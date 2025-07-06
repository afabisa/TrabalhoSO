#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define MAX_CAPACITY 20
#define SOFA_CAPACITY 4

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t customer1_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t customer2_cond = PTHREAD_COND_INITIALIZER;

int total_in_barbershop = 0;
int sofa_count = 0;
int queue1_size = 0;
int queue2_size = 0;

typedef struct {
    pthread_cond_t sem1;
    pthread_cond_t sem2;
    pthread_cond_t sem3; // para pagamento
    pthread_cond_t sem4; // para recibo
    pthread_mutex_t lock;
    bool sem1_ready;
    bool sem2_ready;
    bool sem3_ready;
    bool sem4_ready;
    int id;
} CustomerData;

CustomerData *queue1[MAX_CAPACITY];
CustomerData *queue2[MAX_CAPACITY];

int num_barbers = 3;
int num_customers = 25;

void balk(int id) {
    printf("Cliente %d desistiu (barbearia cheia).\n", id);
    pthread_exit(NULL);
}

void *customer(void *arg) {
    int id = *(int *)arg;
    free(arg);

    usleep(rand() % 300000); // atraso aleatório

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

    pthread_mutex_lock(&mutex);
    if (total_in_barbershop >= MAX_CAPACITY) {
        pthread_mutex_unlock(&mutex);
        balk(id);
    }
    total_in_barbershop++;
    queue1[queue1_size++] = &data;
    printf("Cliente %d entrou na barbearia (total: %d)\n", id, total_in_barbershop);
    pthread_cond_broadcast(&customer1_cond);
    pthread_mutex_unlock(&mutex);

    // Espera ser chamado para tentar sofá
    pthread_mutex_lock(&data.lock);
    while (!data.sem1_ready)
        pthread_cond_wait(&data.sem1, &data.lock);
    pthread_mutex_unlock(&data.lock);

    pthread_mutex_lock(&mutex);
    while (sofa_count >= SOFA_CAPACITY) {
        pthread_mutex_unlock(&mutex);
        usleep(1000);
        pthread_mutex_lock(&mutex);
    }
    sofa_count++;
    printf("Cliente %d sentou no sofá (ocupado: %d)\n", id, sofa_count);
    pthread_mutex_unlock(&mutex);

    usleep(rand() % 200000 + 100000); // tempo no sofá

    pthread_mutex_lock(&mutex);
    queue2[queue2_size++] = &data;
    pthread_cond_broadcast(&customer2_cond);
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&data.lock);
    while (!data.sem2_ready)
        pthread_cond_wait(&data.sem2, &data.lock);
    pthread_mutex_unlock(&data.lock);

    pthread_mutex_lock(&mutex);
    sofa_count--;
    pthread_mutex_unlock(&mutex);

    printf("Cliente %d sendo atendido por um barbeiro\n", id);
    usleep(rand() % 300000 + 200000); // corte de cabelo

    // pagamento
    pthread_mutex_lock(&data.lock);
    data.sem3_ready = true;
    pthread_cond_signal(&data.sem3);
    while (!data.sem4_ready)
        pthread_cond_wait(&data.sem4, &data.lock);
    pthread_mutex_unlock(&data.lock);

    pthread_mutex_lock(&mutex);
    total_in_barbershop--;
    pthread_mutex_unlock(&mutex);

    printf("Cliente %d saiu da barbearia\n", id);
    pthread_exit(NULL);
}

void *barber(void *arg) {
    int id = *(int *)arg;
    free(arg);

    while (1) {
        CustomerData *cust;

        pthread_mutex_lock(&mutex);
        while (queue1_size == 0)
            pthread_cond_wait(&customer1_cond, &mutex);
        cust = queue1[0];
        for (int i = 0; i < queue1_size - 1; i++)
            queue1[i] = queue1[i + 1];
        queue1_size--;
        pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&cust->lock);
        cust->sem1_ready = true;
        pthread_cond_signal(&cust->sem1);
        pthread_mutex_unlock(&cust->lock);

        pthread_mutex_lock(&mutex);
        while (queue2_size == 0)
            pthread_cond_wait(&customer2_cond, &mutex);
        cust = queue2[0];
        for (int i = 0; i < queue2_size - 1; i++)
            queue2[i] = queue2[i + 1];
        queue2_size--;
        pthread_mutex_unlock(&mutex);

        pthread_mutex_lock(&cust->lock);
        cust->sem2_ready = true;
        pthread_cond_signal(&cust->sem2);
        pthread_mutex_unlock(&cust->lock);

        // espera pagamento
        pthread_mutex_lock(&cust->lock);
        while (!cust->sem3_ready)
            pthread_cond_wait(&cust->sem3, &cust->lock);
        usleep(rand() % 100000); // processamento do pagamento
        printf("Barbeiro %d recebeu pagamento de cliente %d\n", id, cust->id);
        cust->sem4_ready = true;
        pthread_cond_signal(&cust->sem4);
        pthread_mutex_unlock(&cust->lock);

        usleep(rand() % 100000); // descanso
    }
}

int main() {
    printf("Digite o número de barbeiros: ");
    scanf("%d", &num_barbers);
    printf("Digite o número de clientes: ");
    scanf("%d", &num_customers);

    srand(time(NULL)); // aleatoriedade

    pthread_t barbers[num_barbers];
    pthread_t customers[num_customers];

    for (int i = 0; i < num_barbers; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&barbers[i], NULL, barber, id);
    }

    for (int i = 0; i < num_customers; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&customers[i], NULL, customer, id);
    }

    for (int i = 0; i < num_customers; i++)
        pthread_join(customers[i], NULL);

    printf("\nTodos os clientes foram atendidos ou desistiram.\n");

    return 0;
}
