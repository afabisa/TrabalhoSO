#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

/*
 * =================================================================================
 * BARBEARIA DE HILZER
 * =================================================================================
 * Implementação em C com pthreads de uma solução dinâmica e robusta para o
 * problema da barbearia.
 *
 * Funcionalidades:
 * - Configuração Dinâmica: Número de barbeiros, vagas no sofá e capacidade da
 * loja são definidos pelo usuário em tempo de execução.
 * - Geração Contínua: Um gerador de clientes cria novas threads em intervalos
 * aleatórios, simulando um fluxo real.
 * - Parada Graciosa: O programa pode ser interrompido de forma segura com Ctrl+C,
 * permitindo que os clientes atuais finalizem seus ciclos.
 * - Sincronização Segura:
 * - Filas FIFO para garantir a ordem de atendimento.
 * - Sincronização de pagamento 1-para-1 entre um barbeiro e seu cliente
 * específico, usando variáveis de condição privadas para evitar
 * condições de corrida.
 * =================================================================================
 */

// --- Estruturas de Fila e Cliente ---
typedef struct NoCliente {
    pthread_cond_t cond_pessoal_cliente;     // Para ser chamado para a cadeira
    pthread_cond_t cond_transacao_pagamento; // CV PRIVADA para o rendezvous do pagamento
    int pagamento_concluido;                 // Flag de status para a transação
    long id;
    struct NoCliente *proximo;
} NoCliente;

typedef struct Fila {
    NoCliente *inicio;
    NoCliente *fim;
    int tamanho;
} Fila;

// --- Variáveis Globais de Configuração (definidas pelo usuário) ---
int NUM_BARBEIROS;
int VAGAS_SOFA;
int TOTAL_CAPACIDADE;

// --- Primitivas de Sincronização e Estado Global ---
pthread_mutex_t mutex;
pthread_cond_t cond_barbeiro_dormindo; // Para acordar barbeiros quando a fila de corte não está vazia
pthread_cond_t cond_caixa_livre;       // Para serializar o acesso ao caixa

NoCliente *cliente_no_caixa = NULL; // Ponteiro que identifica QUEM está no caixa
int caixa_ocupado = 0;
int clientes_na_loja = 0;
int clientes_no_sofa = 0;

Fila fila_espera_corte; // Fila para clientes no sofá, esperando por um barbeiro
Fila fila_em_pe;        // Fila para clientes esperando por uma vaga no sofá

// --- Flag de Parada ---
volatile sig_atomic_t programa_deve_parar = 0;

// --- Funções de Apoio da Fila ---
void inicializar_fila(Fila *f) {
    f->inicio = NULL;
    f->fim = NULL;
    f->tamanho = 0;
}

void enfileirar(Fila *f, NoCliente *no) {
    no->proximo = NULL;
    if (f->fim == NULL) {
        f->inicio = no;
        f->fim = no;
    } else {
        f->fim->proximo = no;
        f->fim = no;
    }
    f->tamanho++;
}

NoCliente* desenfileirar(Fila *f) {
    if (f->inicio == NULL) {
        return NULL;
    }
    NoCliente *no = f->inicio;
    f->inicio = f->inicio->proximo;
    if (f->inicio == NULL) {
        f->fim = NULL;
    }
    f->tamanho--;
    return no;
}


// --- Lógica das Threads ---

void *funcao_cliente(void *arg) {
    NoCliente meu_no;
    meu_no.id = (long)arg;
    pthread_cond_init(&meu_no.cond_pessoal_cliente, NULL);
    pthread_cond_init(&meu_no.cond_transacao_pagamento, NULL);
    meu_no.pagamento_concluido = 0;

    pthread_mutex_lock(&mutex);

    if (clientes_na_loja >= TOTAL_CAPACIDADE) {
        printf("[Cliente %ld] Foi embora, loja cheia.\n", meu_no.id);
        pthread_mutex_unlock(&mutex);
        pthread_cond_destroy(&meu_no.cond_pessoal_cliente);
        pthread_cond_destroy(&meu_no.cond_transacao_pagamento);
        return NULL;
    }
    clientes_na_loja++;
    printf("[Cliente %ld] Entrou na loja. (Total: %d)\n", meu_no.id, clientes_na_loja);

    if (clientes_no_sofa < VAGAS_SOFA) {
        clientes_no_sofa++;
        enfileirar(&fila_espera_corte, &meu_no);
        printf("[Cliente %ld] Sentou no sofá e aguarda corte. (Sofá: %d/%d, Fila Corte: %d)\n", meu_no.id, clientes_no_sofa, VAGAS_SOFA, fila_espera_corte.tamanho);
        pthread_cond_signal(&cond_barbeiro_dormindo);
    } else {
        enfileirar(&fila_em_pe, &meu_no);
        printf("[Cliente %ld] Fica em pé aguardando vaga no sofá. (Fila em Pé: %d)\n", meu_no.id, fila_em_pe.tamanho);
        pthread_cond_wait(&meu_no.cond_pessoal_cliente, &mutex);
        printf("[Cliente %ld] Conseguiu vaga no sofá e agora aguarda corte. (Sofá: %d/%d, Fila Corte: %d)\n", meu_no.id, clientes_no_sofa, VAGAS_SOFA, fila_espera_corte.tamanho);
        pthread_cond_signal(&cond_barbeiro_dormindo);
    }

    pthread_cond_wait(&meu_no.cond_pessoal_cliente, &mutex);
    
    clientes_no_sofa--;
    printf("[Cliente %ld] Chamado! Levantou do sofá. (Sofá: %d/%d)\n", meu_no.id, clientes_no_sofa, VAGAS_SOFA);

    if (fila_em_pe.tamanho > 0) {
        NoCliente *proximo_da_fila_em_pe = desenfileirar(&fila_em_pe);
        clientes_no_sofa++;
        enfileirar(&fila_espera_corte, proximo_da_fila_em_pe);
        printf("    (Cliente %ld passa o bastão: chama cliente %ld da fila em pé para o sofá)\n", meu_no.id, proximo_da_fila_em_pe->id);
        pthread_cond_signal(&proximo_da_fila_em_pe->cond_pessoal_cliente);
    }
    
    pthread_mutex_unlock(&mutex);

    printf("[Cliente %ld] Cortando o cabelo...\n", meu_no.id);
    sleep(rand() % 3 + 1); // Corte dura de 1 a 3 segundos

    pthread_mutex_lock(&mutex);
    
    while (caixa_ocupado) {
        pthread_cond_wait(&cond_caixa_livre, &mutex);
    }
    
    caixa_ocupado = 1;
    cliente_no_caixa = &meu_no;
    printf("[Cliente %ld] Chegou ao caixa.\n", meu_no.id);
    pthread_cond_signal(&meu_no.cond_transacao_pagamento);

    while (meu_no.pagamento_concluido == 0) {
        pthread_cond_wait(&meu_no.cond_transacao_pagamento, &mutex);
    }
    
    printf("[Cliente %ld] Pagamento confirmado. Liberando o caixa.\n", meu_no.id);

    caixa_ocupado = 0;
    cliente_no_caixa = NULL;
    pthread_cond_signal(&cond_caixa_livre);
    
    clientes_na_loja--;
    printf("[Cliente %ld] Saiu da loja. (Total na loja: %d)\n", meu_no.id, clientes_na_loja);
    pthread_mutex_unlock(&mutex);

    pthread_cond_destroy(&meu_no.cond_pessoal_cliente);
    pthread_cond_destroy(&meu_no.cond_transacao_pagamento);
    return NULL;
}

void *funcao_barbeiro(void *arg) {
    long id = (long)arg;
    NoCliente *cliente_atendido;

    while (1) {
        pthread_mutex_lock(&mutex);
        while (fila_espera_corte.tamanho == 0) {
            printf("[Barbeiro %ld] Dormindo...\n", id);
            pthread_cond_wait(&cond_barbeiro_dormindo, &mutex);
        }
        
        cliente_atendido = desenfileirar(&fila_espera_corte);
        printf("[Barbeiro %ld] Chamando cliente %ld da fila.\n", id, cliente_atendido->id);
        pthread_cond_signal(&cliente_atendido->cond_pessoal_cliente);
        
        pthread_mutex_unlock(&mutex);

        printf("[Barbeiro %ld] Cortando cabelo do cliente %ld...\n", id, cliente_atendido->id);
        sleep(rand() % 3 + 1); // Corte dura de 1 a 3 segundos

        pthread_mutex_lock(&mutex);

        printf("[Barbeiro %ld] Finalizou o corte, aguardando cliente %ld no caixa.\n", id, cliente_atendido->id);
        while (cliente_no_caixa != cliente_atendido) {
            pthread_cond_wait(&cliente_atendido->cond_transacao_pagamento, &mutex);
        }

        printf("[Barbeiro %ld] Sincronizado com cliente %ld no caixa. Processando pagamento.\n", id, cliente_atendido->id);
        
        cliente_atendido->pagamento_concluido = 1;
        pthread_cond_signal(&cliente_atendido->cond_transacao_pagamento);
        
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

void tratar_sinal(int signum) {
    if (signum == SIGINT) {
        printf("\n\nSINAL DE PARADA (Ctrl+C) RECEBIDO.\n");
        printf("O programa não aceitará novos clientes. Aguardando clientes atuais finalizarem...\n");
        programa_deve_parar = 1;
    }
}

int main() {
    printf("--- Configuração da Barbearia ---\n");
    printf("Digite o número de barbeiros: ");
    scanf("%d", &NUM_BARBEIROS);
    printf("Digite o número de vagas no sofá: ");
    scanf("%d", &VAGAS_SOFA);
    printf("Digite a capacidade total da loja: ");
    scanf("%d", &TOTAL_CAPACIDADE);

    if (NUM_BARBEIROS <= 0 || VAGAS_SOFA < 0 || TOTAL_CAPACIDADE <= 0) {
        printf("Erro: os parâmetros devem ser números positivos.\n");
        return 1;
    }

    signal(SIGINT, tratar_sinal);

    srand(time(NULL));
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond_barbeiro_dormindo, NULL);
    pthread_cond_init(&cond_caixa_livre, NULL);
    inicializar_fila(&fila_espera_corte);
    inicializar_fila(&fila_em_pe);
    
    printf("\n--- Simulação Iniciada --- (Pressione Ctrl+C para parar de gerar novos clientes)\n\n");

    pthread_t barbeiros[NUM_BARBEIROS];
    for (long i = 0; i < NUM_BARBEIROS; i++) {
        pthread_create(&barbeiros[i], NULL, funcao_barbeiro, (void*)(i + 1));
    }

    long id_cliente = 1;
    while (!programa_deve_parar) {
        pthread_t nova_thread_cliente;

        if (pthread_create(&nova_thread_cliente, NULL, funcao_cliente, (void*)id_cliente) != 0) {
            perror("Falha ao criar a thread do cliente");
            continue;
        }

        pthread_detach(nova_thread_cliente);
        id_cliente++;

        sleep(rand() % 3); // Intervalo de chegada de clientes entre 0 e 2 segundos
    }
    
    printf("\n--- Gerador de clientes finalizado. As threads dos barbeiros permanecerão ativas para atender os clientes restantes. ---\n");
    
    // A thread principal aguarda indefinidamente pelas threads dos barbeiros.
    // Como os barbeiros estão em loop infinito, este join nunca retornará.
    // O programa terminará quando for fechado externamente.
    for(int i=0; i < NUM_BARBEIROS; i++){
        pthread_join(barbeiros[i], NULL);
    }

    // Liberação de recursos (não será alcançado neste modelo simples)
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond_barbeiro_dormindo);
    pthread_cond_destroy(&cond_caixa_livre);

    return 0;
}
