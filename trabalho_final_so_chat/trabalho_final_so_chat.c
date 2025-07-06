#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define MAX_CAPACITY 20     // Capacidade máxima de clientes na barbearia
#define SOFA_CAPACITY 4     // Número de lugares disponíveis no sofá

// Mutex principal da barbearia (protege variáveis globais)
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Variáveis de condição para as filas de entrada (cliente e corte)
pthread_cond_t customer1_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t customer2_cond = PTHREAD_COND_INITIALIZER;

// Variáveis globais de estado
int total_in_barbershop = 0; // Número total de clientes na barbearia
int sofa_count = 0;          // Número atual de clientes sentados no sofá
int queue1_size = 0;         // Tamanho da fila de clientes aguardando no lobby
int queue2_size = 0;         // Tamanho da fila de clientes aguardando corte

// Estrutura que representa um cliente individual
typedef struct {
    // Variáveis de condição individuais do cliente
    pthread_cond_t sem1;  // sinal para ir ao sofá
    pthread_cond_t sem2;  // sinal para ir à cadeira do barbeiro
    pthread_cond_t sem3;  // sinal para pagar
    pthread_cond_t sem4;  // sinal de recibo
    pthread_mutex_t lock; // mutex privado do cliente

    // Flags de sincronização
    bool sem1_ready;
    bool sem2_ready;
    bool sem3_ready;
    bool sem4_ready;

    int id; // identificador do cliente (para logs)
} CustomerData;

// Fila circular de ponteiros para clientes aguardando sofá e cadeira
CustomerData *queue1[MAX_CAPACITY];
CustomerData *queue2[MAX_CAPACITY];

// Números de barbeiros e clientes (configurados em tempo de execução)
int num_barbers = 3;
int num_customers = 25;

// Função chamada quando a barbearia está cheia
void balk(int id) {
    printf("Cliente %d desistiu (barbearia cheia).\n", id);
    pthread_exit(NULL);
}

// -------------------------- THREAD DE CLIENTE --------------------------

void *customer(void *arg) {
    int id = *(int *)arg;
    free(arg); // Libera ponteiro alocado

    usleep(rand() % 300000); // Espera aleatória antes de chegar

    // Inicializa dados individuais do cliente
    CustomerData data;
    pthread_cond_init(&data.sem1, NULL);
    pthread_cond_init(&data.sem2, NULL);
    pthread_cond_init(&data.sem3, NULL);
    pthread_cond_init(&data.sem4, NULL);
    pthread_mutex_init(&data.lock, NULL);
    data.sem1_ready = false;
    data.sem2_ready = false;
    data.sem3_ready = false;
    data.sem4_ready = false;
    data.id = id;

    // Tenta entrar na barbearia
    pthread_mutex_lock(&mutex);
    if (total_in_barbershop >= MAX_CAPACITY) {
        pthread_mutex_unlock(&mutex);
        balk(id);
    }

    // Entra na fila da barbearia
    total_in_barbershop++;
    queue1[queue1_size++] = &data;
    printf("Cliente %d entrou na barbearia (total: %d)\n", id, total_in_barbershop);
    pthread_cond_broadcast(&customer1_cond); // Acorda todos os barbeiros
    pthread_mutex_unlock(&mutex);

    // Espera autorização para tentar o sofá
    pthread_mutex_lock(&data.lock);
    while (!data.sem1_ready)
        pthread_cond_wait(&data.sem1, &data.lock);
    pthread_mutex_unlock(&data.lock);

    // Aguarda vaga no sofá
    pthread_mutex_lock(&mutex);
    while (sofa_count >= SOFA_CAPACITY) {
        pthread_mutex_unlock(&mutex);
        usleep(1000); // pequena espera
        pthread_mutex_lock(&mutex);
    }
    sofa_count++;
    printf("Cliente %d sentou no sofá (ocupado: %d)\n", id, sofa_count);
    pthread_mutex_unlock(&mutex);

    usleep(rand() % 200000 + 100000); // tempo sentado no sofá (100–300ms)

    // Vai para fila de corte
    pthread_mutex_lock(&mutex);
    queue2[queue2_size++] = &data;
    pthread_cond_broadcast(&customer2_cond); // acorda barbeiros
    pthread_mutex_unlock(&mutex);

    // Espera sinal para cadeira do barbeiro
    pthread_mutex_lock(&data.lock);
    while (!data.sem2_ready)
        pthread_cond_wait(&data.sem2, &data.lock);
    pthread_mutex_unlock(&data.lock);

    // Sai do sofá
    pthread_mutex_lock(&mutex);
    sofa_count--;
    pthread_mutex_unlock(&mutex);

    // Corta cabelo
    printf("Cliente %d sendo atendido por um barbeiro\n", id);
    usleep(rand() % 300000 + 200000); // tempo de corte (200–500ms)

    // Pagar
    pthread_mutex_lock(&data.lock);
    data.sem3_ready = true;
    pthread_cond_signal(&data.sem3); // avisa que quer pagar
    while (!data.sem4_ready)
        pthread_cond_wait(&data.sem4, &data.lock); // espera recibo
    pthread_mutex_unlock(&data.lock);

    // Sai da barbearia
    pthread_mutex_lock(&mutex);
    total_in_barbershop--;
    pthread_mutex_unlock(&mutex);

    printf("Cliente %d saiu da barbearia\n", id);
    pthread_exit(NULL);
}

// -------------------------- THREAD DE BARBEIRO --------------------------

void *barber(void *arg) {
    int id = *(int *)arg;
    free(arg);

    while (1) {
        CustomerData *cust;

        // Espera cliente na fila de entrada (queue1)
        pthread_mutex_lock(&mutex);
        while (queue1_size == 0)
            pthread_cond_wait(&customer1_cond, &mutex);
        cust = queue1[0];
        for (int i = 0; i < queue1_size - 1; i++)
            queue1[i] = queue1[i + 1];
        queue1_size--;
        pthread_mutex_unlock(&mutex);

        // Autoriza cliente a tentar sofá
        pthread_mutex_lock(&cust->lock);
        cust->sem1_ready = true;
        pthread_cond_signal(&cust->sem1);
        pthread_mutex_unlock(&cust->lock);

        // Espera cliente pronto para corte (queue2)
        pthread_mutex_lock(&mutex);
        while (queue2_size == 0)
            pthread_cond_wait(&customer2_cond, &mutex);
        cust = queue2[0];
        for (int i = 0; i < queue2_size - 1; i++)
            queue2[i] = queue2[i + 1];
        queue2_size--;
        pthread_mutex_unlock(&mutex);

        // Autoriza cliente a ocupar a cadeira de barbeiro
        pthread_mutex_lock(&cust->lock);
        cust->sem2_ready = true;
        pthread_cond_signal(&cust->sem2);
        pthread_mutex_unlock(&cust->lock);

        // Espera o cliente pedir para pagar
        pthread_mutex_lock(&cust->lock);
        while (!cust->sem3_ready)
            pthread_cond_wait(&cust->sem3, &cust->lock);
        usleep(rand() % 100000); // tempo para processar pagamento
        printf("Barbeiro %d recebeu pagamento de cliente %d\n", id, cust->id);
        cust->sem4_ready = true;
        pthread_cond_signal(&cust->sem4); // envia recibo
        pthread_mutex_unlock(&cust->lock);

        usleep(rand() % 100000); // intervalo entre atendimentos
    }
}

int main() {
    printf("Digite o número de barbeiros: ");
    scanf("%d", &num_barbers);
    printf("Digite o número de clientes: ");
    scanf("%d", &num_customers);

    srand(time(NULL)); // inicializa gerador aleatório

    pthread_t barbers[num_barbers];
    pthread_t customers[num_customers];

    // Cria threads dos barbeiros
    for (int i = 0; i < num_barbers; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&barbers[i], NULL, barber, id);
    }

    // Cria threads dos clientes
    for (int i = 0; i < num_customers; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&customers[i], NULL, customer, id);
    }

    // Espera todos os clientes terminarem
    for (int i = 0; i < num_customers; i++)
        pthread_join(customers[i], NULL);

    printf("\nTodos os clientes foram atendidos ou desistiram.\n");
    return 0;
}
