#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

// --- Parâmetros de Configuração (a serem definidos por entrada interativa/arquivo) ---
int MAX_CUSTOMERS_IN_SHOP = 20; // n
int NUM_SOFA_SEATS = 4;
int NUM_BARBERS = 3;
int MAX_HAIRCUTS_PER_CUSTOMER = 1; // Para fins de simulação
int SIMULATION_DURATION_SECONDS = 30; // Duração da simulação

// --- Variáveis Globais ---
int customers_in_shop = 0; // corresponde a 'customers' 
int customers_on_sofa = 0;
int customers_in_barber_chair = 0;

// Mutexes
pthread_mutex_t shop_mutex;       // Protege customers_in_shop e queue1 
pthread_mutex_t queue2_mutex;     // Protege queue2 
pthread_mutex_t cash_register_mutex; // Garante apenas um pagamento por vez 

// Variáveis de Condição
pthread_cond_t customer_enters_shop_cv; // Sinalizada quando um cliente entra, barbeiros podem esperar aqui por um cliente disponível 
pthread_cond_t sofa_available_cv;       // Sinalizada quando um assento no sofá fica livre 
pthread_cond_t barber_available_cv;     // Sinalizada quando um barbeiro está livre (e pronto para cortar cabelo) 
pthread_cond_t payment_initiated_cv;    // Sinalizada pelo cliente ao pagar 
pthread_cond_t payment_accepted_cv;     // Sinalizada pelo barbeiro quando o pagamento é aceito 

// Filas (usando listas ligadas ou similar para tamanho dinâmico)
// Em uma implementação real em C, estas seriam provavelmente listas ligadas de structs
// que contêm um pthread_cond_t para cada cliente esperando.
typedef struct CustomerWaiting {
    int customer_id;
    pthread_cond_t self_cond; // Variável de condição para o cliente esperar
    struct CustomerWaiting *next;
} CustomerWaiting;

CustomerWaiting *queue1_head = NULL; // Clientes esperando por um assento no sofá 
CustomerWaiting *queue1_tail = NULL;

CustomerWaiting *queue2_head = NULL; // Clientes esperando por uma cadeira de barbeiro 
CustomerWaiting *queue2_tail = NULL;

// Função dummy balk (cliente sai se a loja estiver cheia)
void balk(int customer_id) {
    printf("Cliente %d: Loja cheia, desistindo.\n", customer_id);
}

// Função da thread do cliente
void *customer(void *arg) {
    int customer_id = *(int *)arg;
    pthread_cond_t my_sofa_cond;     // Condição para o cliente esperar pelo sofá
    pthread_cond_t my_barber_cond;   // Condição para o cliente esperar pela cadeira do barbeiro
    pthread_cond_init(&my_sofa_cond, NULL);
    pthread_cond_init(&my_barber_cond, NULL);

    printf("Cliente %d: Chegou à barbearia.\n", customer_id);

    // 1. enterShop() 
    pthread_mutex_lock(&shop_mutex);
    if (customers_in_shop == MAX_CUSTOMERS_IN_SHOP) { [cite: 49]
        pthread_mutex_unlock(&shop_mutex);
        balk(customer_id); [cite: 51]
        pthread_cond_destroy(&my_sofa_cond);
        pthread_cond_destroy(&my_barber_cond);
        return NULL;
    }
    customers_in_shop++; [cite: 52]
    printf("Cliente %d: Entrou na loja. Clientes na loja: %d\n", customer_id, customers_in_shop);
    
    // Adiciona-se à queue1 e sinaliza um barbeiro que um cliente está esperando por um assento no sofá.
    // O barbeiro espera por uma CV global e o cliente precisa se adicionar à fila *antes*
    // do barbeiro pegá-lo. O barbeiro então sinalizará a CV específica do cliente.
    CustomerWaiting *new_q1_node = (CustomerWaiting *)malloc(sizeof(CustomerWaiting));
    if (new_q1_node == NULL) {
        perror("malloc para new_q1_node falhou");
        exit(EXIT_FAILURE);
    }
    new_q1_node->customer_id = customer_id;
    new_q1_node->self_cond = my_sofa_cond; // Armazena referência para my_sofa_cond
    new_q1_node->next = NULL;
    
    if (queue1_tail == NULL) {
        queue1_head = new_q1_node;
        queue1_tail = new_q1_node;
    } else {
        queue1_tail->next = new_q1_node;
        queue1_tail = new_q1_node;
    }
    // original: queuel.append(self.sem1) 

    pthread_cond_signal(&customer_enters_shop_cv); // Sinaliza que um cliente está disponível 
    pthread_mutex_unlock(&shop_mutex);

    // Espera por sinal do barbeiro para sentar no sofá (original `self.sem1.wait()`) 
    pthread_mutex_lock(&shop_mutex); // Readquire mutex para esperar na variável de condição
    pthread_cond_wait(&my_sofa_cond, &shop_mutex);
    pthread_mutex_unlock(&shop_mutex);

    // 2. sitOnSofa() 
    pthread_mutex_lock(&shop_mutex); // Usa shop_mutex para proteger customers_on_sofa também
    while (customers_on_sofa == NUM_SOFA_SEATS) {
        printf("Cliente %d: Sofá cheio, esperando por um assento.\n", customer_id);
        pthread_cond_wait(&sofa_available_cv, &shop_mutex);
    }
    customers_on_sofa++;
    printf("Cliente %d: Sentou no sofá. Clientes no sofá: %d\n", customer_id, customers_on_sofa);
    pthread_mutex_unlock(&shop_mutex);


    // Prepara-se para getHairCut
    // Adiciona-se à queue2 e sinaliza o barbeiro que um cliente está esperando por uma cadeira
    pthread_mutex_lock(&queue2_mutex); // Usa queue2_mutex
    CustomerWaiting *new_q2_node = (CustomerWaiting *)malloc(sizeof(CustomerWaiting));
    if (new_q2_node == NULL) {
        perror("malloc para new_q2_node falhou");
        exit(EXIT_FAILURE);
    }
    new_q2_node->customer_id = customer_id;
    new_q2_node->self_cond = my_barber_cond; // Armazena referência para my_barber_cond
    new_q2_node->next = NULL;

    if (queue2_tail == NULL) {
        queue2_head = new_q2_node;
        queue2_tail = new_q2_node;
    } else {
        queue2_tail->next = new_q2_node;
        queue2_tail = new_q2_node;
    }
    // original: queue2.append(self.sem2) 

    pthread_cond_signal(&barber_available_cv); // Sinaliza que um cliente está disponível para um corte de cabelo 
    pthread_mutex_unlock(&queue2_mutex);

    // Espera por sinal do barbeiro para sentar na cadeira de barbeiro (original `self.sem2.wait()`) 
    pthread_mutex_lock(&queue2_mutex); // Readquire mutex para esperar
    pthread_cond_wait(&my_barber_cond, &queue2_mutex);
    pthread_mutex_unlock(&queue2_mutex);

    // 3. getHairCut() 
    pthread_mutex_lock(&shop_mutex); // Protegendo `customers_in_barber_chair`
    customers_in_barber_chair++;
    printf("Cliente %d: Cortando cabelo. Barbeiros ocupados: %d\n", customer_id, customers_in_barber_chair);
    pthread_mutex_unlock(&shop_mutex);

    // Simula tempo de corte de cabelo
    usleep(rand() % 500000 + 100000); // 0.1 a 0.6 segundos

    pthread_mutex_lock(&shop_mutex);
    customers_in_barber_chair--;
    pthread_mutex_unlock(&shop_mutex);
    printf("Cliente %d: Corte de cabelo finalizado.\n", customer_id);

    // Sinaliza que o sofá está disponível após o corte de cabelo, pois ele deixa o sofá para cortar o cabelo.
    // Esta é uma correção ao comportamento declarado no livro. O diagrama do livro implica
    // que o cliente sai do sofá *depois* de sair da queue1, *antes* de sentar na cadeira do barbeiro.
    // O "sitInBarberChair()" é o que realmente libera o sofá.
    // A dica diz "Quando ele sai daquela fila [queue2], ele corta o cabelo, paga e então sai." [cite: 103]
    // E "sofa.signal()" está após `self.sem2.wait()`.
    // Então, `sofa.signal()` acontece quando o cliente se move do sofá para a cadeira do barbeiro.
    pthread_mutex_lock(&shop_mutex);
    customers_on_sofa--;
    pthread_cond_signal(&sofa_available_cv); // Sinaliza que o assento do sofá está livre
    printf("Cliente %d: Saiu do sofá. Clientes no sofá: %d\n", customer_id, customers_on_sofa);
    pthread_mutex_unlock(&shop_mutex);


    // 4. pay() 
    pthread_mutex_lock(&cash_register_mutex);
    printf("Cliente %d: Indo pagar.\n", customer_id);
    pthread_cond_signal(&payment_initiated_cv); // Sinaliza barbeiro que cliente está pronto para pagar 
    pthread_cond_wait(&payment_accepted_cv, &cash_register_mutex); // Espera pelo barbeiro aceitar o pagamento 
    printf("Cliente %d: Pagamento aceito.\n", customer_id);
    pthread_mutex_unlock(&cash_register_mutex);

    // Sair da loja
    pthread_mutex_lock(&shop_mutex);
    customers_in_shop--; [cite: 95]
    printf("Cliente %d: Saiu da loja. Clientes na loja: %d\n", customer_id, customers_in_shop);
    pthread_mutex_unlock(&shop_mutex);

    pthread_cond_destroy(&my_sofa_cond);
    pthread_cond_destroy(&my_barber_cond);
    free(arg); // Libera memória alocada para customer_id
    return NULL;
}

// Função da thread do barbeiro
void *barber(void *arg) {
    int barber_id = *(int *)arg;
    printf("Barbeiro %d: Começou a trabalhar.\n", barber_id);

    while (1) {
        // Espera por um cliente para entrar na loja e estar na queue1
        pthread_mutex_lock(&shop_mutex);
        while (queue1_head == NULL) {
            printf("Barbeiro %d: Esperando por cliente na queue1.\n", barber_id);
            pthread_cond_wait(&customer_enters_shop_cv, &shop_mutex); [cite: 108]
        }

        // Desenfileira cliente da queue1
        CustomerWaiting *customer_node_q1 = dequeue_customer(&queue1_head, &queue1_tail);
        if (customer_node_q1 == NULL) { // Não deveria acontecer se a condição do loop for atendida
             pthread_mutex_unlock(&shop_mutex);
             continue;
        }
        printf("Barbeiro %d: Sinalizando Cliente %d para sentar no sofá (da queue1).\n", barber_id, customer_node_q1->customer_id);
        pthread_cond_signal(&customer_node_q1->self_cond); // Sinaliza cliente para sentar no sofá 
        pthread_mutex_unlock(&shop_mutex);
        free(customer_node_q1); // Libera o nó usado para o enfileiramento

        // Depois que o cliente é sinalizado para o sofá, o barbeiro espera que o cliente esteja na queue2
        pthread_mutex_lock(&queue2_mutex);
        while (queue2_head == NULL) {
            printf("Barbeiro %d: Esperando por cliente na queue2.\n", barber_id);
            pthread_cond_wait(&barber_available_cv, &queue2_mutex); [cite: 119]
        }

        // Desenfileira cliente da queue2
        CustomerWaiting *customer_node_q2 = dequeue_customer(&queue2_head, &queue2_tail);
        if (customer_node_q2 == NULL) {
            pthread_mutex_unlock(&queue2_mutex);
            continue;
        }
        printf("Barbeiro %d: Sinalizando Cliente %d para sentar na cadeira de barbeiro (da queue2).\n", barber_id, customer_node_q2->customer_id);
        pthread_cond_signal(&customer_node_q2->self_cond); // Sinaliza cliente para sentar na cadeira de barbeiro 
        pthread_mutex_unlock(&queue2_mutex);
        free(customer_node_q2); // Libera o nó usado para o enfileiramento

        // Simula corte de cabelo (implicitamente, a thread do cliente lida com isso)
        // O barbeiro é considerado "ocupado" durante este tempo.

        // Espera o cliente iniciar o pagamento
        pthread_mutex_lock(&cash_register_mutex);
        printf("Barbeiro %d: Esperando pagamento do cliente.\n", barber_id);
        pthread_cond_wait(&payment_initiated_cv, &cash_register_mutex); [cite: 134]

        // Aceita pagamento
        printf("Barbeiro %d: Aceitando pagamento.\n", barber_id);
        usleep(rand() % 200000 + 50000); // Simula tempo de processamento do pagamento
        pthread_cond_signal(&payment_accepted_cv); // Sinaliza cliente que o pagamento foi aceito 
        pthread_mutex_unlock(&cash_register_mutex);

        // Barbeiro fica livre e volta a esperar pelo próximo cliente
        printf("Barbeiro %d: Terminou com o cliente, pronto para o próximo.\n", barber_id);
    }
    free(arg); // Libera memória alocada para barber_id
    return NULL;
}

int main() {
    srand(time(NULL));

    // Inicializa mutexes e variáveis de condição
    pthread_mutex_init(&shop_mutex, NULL);
    pthread_mutex_init(&queue2_mutex, NULL);
    pthread_mutex_init(&cash_register_mutex, NULL);

    pthread_cond_init(&customer_enters_shop_cv, NULL);
    pthread_cond_init(&sofa_available_cv, NULL);
    pthread_cond_init(&barber_available_cv, NULL);
    pthread_cond_init(&payment_initiated_cv, NULL);
    pthread_cond_init(&payment_accepted_cv, NULL);

    // Obtém parâmetros do usuário (exemplo, substituir por método de entrada real) 
    printf("Digite o número máximo de clientes na loja (n): ");
    scanf("%d", &MAX_CUSTOMERS_IN_SHOP);
    printf("Digite o número de assentos no sofá: ");
    scanf("%d", &NUM_SOFA_SEATS);
    printf("Digite o número de barbeiros: ");
    scanf("%d", &NUM_BARBERS);
    printf("Digite a duração da simulação em segundos: ");
    scanf("%d", &SIMULATION_DURATION_SECONDS);

    // Cria threads de barbeiro
    pthread_t barber_threads[NUM_BARBERS];
    for (int i = 0; i < NUM_BARBERS; ++i) {
        int *barber_id = malloc(sizeof(int));
        if (barber_id == NULL) {
            perror("Falha ao alocar memória para barber_id");
            exit(EXIT_FAILURE);
        }
        *barber_id = i + 1;
        if (pthread_create(&barber_threads[i], NULL, barber, barber_id) != 0) {
            perror("Falha ao criar thread de barbeiro");
            exit(EXIT_FAILURE);
        }
    }

    // Cria threads de cliente (chegada contínua para simulação)
    pthread_t customer_thread;
    int customer_count = 0;
    time_t start_time = time(NULL);

    while (time(NULL) - start_time < SIMULATION_DURATION_SECONDS) {
        int *customer_id = malloc(sizeof(int));
        if (customer_id == NULL) {
            perror("Falha ao alocar memória para customer_id");
            exit(EXIT_FAILURE);
        }
        *customer_id = ++customer_count;
        if (pthread_create(&customer_thread, NULL, customer, customer_id) != 0) {
            perror("Falha ao criar thread de cliente");
            exit(EXIT_FAILURE);
        }
        pthread_detach(customer_thread); // Desanexa para limpar os recursos automaticamente

        usleep(rand() % 500000 + 50000); // Novo cliente chega a cada 0.05 a 0.55 segundos
    }

    // Permite algum tempo para os clientes existentes terminarem (opcional)
    sleep(5);

    // Limpeza (em uma aplicação real, você juntaria as threads de barbeiro ou usaria um mecanismo de desligamento)
    // Para esta simulação, os barbeiros continuarão a ser executados até o término do programa.
    // Para terminar corretamente, seria necessário um modo de sinalizar os barbeiros para sair do loop.

    pthread_mutex_destroy(&shop_mutex);
    pthread_mutex_destroy(&queue2_mutex);
    pthread_mutex_destroy(&cash_register_mutex);

    pthread_cond_destroy(&customer_enters_shop_cv);
    pthread_cond_destroy(&sofa_available_cv);
    pthread_cond_destroy(&barber_available_cv);
    pthread_cond_destroy(&payment_initiated_cv);
    pthread_cond_destroy(&payment_accepted_cv);

    // Libera quaisquer nós restantes nas filas (se a simulação terminar abruptamente)
    CustomerWaiting *current = queue1_head;
    while(current != NULL) {
        CustomerWaiting *next = current->next;
        pthread_cond_destroy(&current->self_cond);
        free(current);
        current = next;
    }
    current = queue2_head;
    while(current != NULL) {
        CustomerWaiting *next = current->next;
        pthread_cond_destroy(&current->self_cond);
        free(current);
        current = next;
    }

    return 0;
}
