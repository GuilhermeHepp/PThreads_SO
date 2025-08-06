#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define TEMPO_TOTAL (60 * 5) // tempo total da simulação em segundos
#define MAX_AVIOES 100

// Variáveis globais para contagem de aviões
int totalAvioes = 0;
int totalDomesticos = 0;
int totalInternacionais = 0;
int avioesSucesso = 0;
int avioesCaidos = 0;
int avioesAlerta = 0;

pthread_mutex_t mutexContadores;
// Definições iniciais
pthread_mutex_t mutexPista;
pthread_mutex_t mutexPortao;
sem_t semaforoTorre;

//Estrutura para representar um avião
struct aviao
{
    int id;
    int tipo; // 0 - domestico, 1 - internacional
    int tempoDeOperacao; // tempo que leva para realizar a operação
    int entrouEmAlerta;
};

// Função para simular o pouso
void pousar(struct aviao *avioes)
{
    if(avioes->tipo == 1) {
        //Pouso: Pista → Torre
        pthread_mutex_lock(&mutexPista);
        sem_wait(&semaforoTorre);
        printf("Aviao %d (Tipo: %d) iniciou o pouso.\n", avioes->id, avioes->tipo);
        sleep(avioes->tempoDeOperacao);
        printf("Aviao %d (Tipo: %d) terminou o pouso.\n", avioes->id, avioes->tipo);
        sem_post(&semaforoTorre);
        pthread_mutex_unlock(&mutexPista);
    } else {
        //Pouso: Torre → Pista
        time_t inicio = time(NULL);
        int sucesso = 0;

        while (!sucesso) {
            if (sem_trywait(&semaforoTorre) == 0) {
                pthread_mutex_lock(&mutexPista);
                sucesso = 1;
            } else {
                time_t agora = time(NULL);
                int espera = agora - inicio;

                if (espera >= 90) {
                    printf("Aviao %d (Domestico) caiu apos 90s esperando a torre.\n", avioes->id);
                    pthread_mutex_lock(&mutexContadores);
                    avioesCaidos++;
                    pthread_mutex_unlock(&mutexContadores);
                    pthread_exit(NULL);
                } else if (espera >= 60 && !avioes->entrouEmAlerta) {
                    printf("Aviao %d (Domestico) em alerta critico (esperando torre ha %ds)\n", avioes->id, espera);
                    avioes->entrouEmAlerta = 1;
                    pthread_mutex_lock(&mutexContadores);
                    avioesAlerta++;
                    pthread_mutex_unlock(&mutexContadores);
                }

                sleep(1);
            }
        }

        printf("Aviao %d (Tipo: %d) iniciou o pouso.\n", avioes->id, avioes->tipo);
        sleep(avioes->tempoDeOperacao);
        printf("Aviao %d (Tipo: %d) terminou o pouso.\n", avioes->id, avioes->tipo);
        sem_post(&semaforoTorre);
        pthread_mutex_unlock(&mutexPista);
    }
}
// Função para simular o desembarque
void desembarcar(struct aviao *avioes)
{
    if(avioes->tipo == 1) {
        pthread_mutex_lock(&mutexPortao);
        sem_wait(&semaforoTorre);
        printf("Aviao %d (Tipo: %d) iniciou o desembarque.\n", avioes->id, avioes->tipo);
        sleep(avioes->tempoDeOperacao);
        printf("Aviao %d (Tipo: %d) terminou o desembarque.\n", avioes->id, avioes->tipo);
        sem_post(&semaforoTorre);
        pthread_mutex_unlock(&mutexPortao);
    } else {
        time_t inicio = time(NULL);
        int sucesso = 0;

        while (!sucesso) {
            if (sem_trywait(&semaforoTorre) == 0) {
                pthread_mutex_lock(&mutexPortao);
                sucesso = 1;
            } else {
                time_t agora = time(NULL);
                int espera = agora - inicio;

                if (espera >= 90) {
                    printf("Aviao %d (Domestico) caiu apos 90s esperando a torre.\n", avioes->id);
                    pthread_mutex_lock(&mutexContadores);
                    avioesCaidos++;
                    pthread_mutex_unlock(&mutexContadores);
                    pthread_exit(NULL);
                } else if (espera >= 60 && !avioes->entrouEmAlerta) {
                    printf("Aviao %d (Domestico) em alerta critico (esperando torre ha %ds)\n", avioes->id, espera);
                    avioes->entrouEmAlerta = 1;
                    pthread_mutex_lock(&mutexContadores);
                    avioesAlerta++;
                    pthread_mutex_unlock(&mutexContadores);
                }

                sleep(1);
            }
        }

        printf("Aviao %d (Tipo: %d) iniciou o desembarque.\n", avioes->id, avioes->tipo);
        sleep(avioes->tempoDeOperacao);
        printf("Aviao %d (Tipo: %d) terminou o desembarque.\n", avioes->id, avioes->tipo);
        pthread_mutex_unlock(&mutexPortao);
        sem_post(&semaforoTorre);
    }
}
// Função para simular a decolagem
void decolar(struct aviao *avioes)
{
    if(avioes->tipo == 1) {
        // Decolagem: Portão → Pista → Torre
        pthread_mutex_lock(&mutexPortao);
        pthread_mutex_lock(&mutexPista);  // FALTAVA ISSO AQUI
        sem_wait(&semaforoTorre);
        printf("Aviao %d (Tipo: %d) iniciou a decolagem.\n", avioes->id, avioes->tipo);
        sleep(avioes->tempoDeOperacao); // Simula o tempo de decolagem
        printf("Aviao %d (Tipo: %d) terminou a decolagem.\n", avioes->id, avioes->tipo);
        sem_post(&semaforoTorre);
        pthread_mutex_unlock(&mutexPista);   // Desbloqueia pista
        pthread_mutex_unlock(&mutexPortao);  // Desbloqueia portão
    } 
    else {
        time_t inicio = time(NULL);
        int sucesso = 0;

        while (!sucesso) {
            if (sem_trywait(&semaforoTorre) == 0) {
                pthread_mutex_lock(&mutexPortao);
                pthread_mutex_lock(&mutexPista);
                sucesso = 1;
            } else {
                time_t agora = time(NULL);
                int espera = agora - inicio;

                if (espera >= 90) {
                    printf("Aviao %d (Domestico) caiu apos 90s esperando a torre.\n", avioes->id);
                    pthread_mutex_lock(&mutexContadores);
                    avioesCaidos++;
                    pthread_mutex_unlock(&mutexContadores);
                    pthread_exit(NULL);
                } else if (espera >= 60 && !avioes->entrouEmAlerta) {
                    printf("Aviao %d (Domestico) em alerta critico (esperando torre ha %ds)\n", avioes->id, espera);
                    avioes->entrouEmAlerta = 1;
                    pthread_mutex_lock(&mutexContadores);
                    avioesAlerta++;
                    pthread_mutex_unlock(&mutexContadores);
                }

                sleep(1);
            }
        }

        printf("Aviao %d (Tipo: %d) iniciou a decolagem.\n", avioes->id, avioes->tipo);
        sleep(avioes->tempoDeOperacao);
        printf("Aviao %d (Tipo: %d) terminou a decolagem.\n", avioes->id, avioes->tipo);
        pthread_mutex_unlock(&mutexPista);
        pthread_mutex_unlock(&mutexPortao);
        sem_post(&semaforoTorre);
    }
}

// Thread para cada avião
void *threadAviao(void *arg)
{
    struct aviao *avioes = (struct aviao *)arg;
    pousar(avioes);
    desembarcar(avioes);
    decolar(avioes);

    pthread_mutex_lock(&mutexContadores);
    avioesSucesso++;
    pthread_mutex_unlock(&mutexContadores);
    return NULL;
}


int main (void){
    //CRIA SEMAFOROS E MUTEXES
    pthread_mutex_init(&mutexPista, NULL);
    pthread_mutex_init(&mutexPortao, NULL);
    pthread_mutex_init(&mutexContadores, NULL);
    sem_init(&semaforoTorre, 0, 2); // Inicializa o semáforo da torre com 2 operações simultâneas
    
    //cria threads de aviões randomicamente em tempos distintos
    pthread_t* threads = malloc(sizeof(pthread_t) * MAX_AVIOES);
    struct aviao* avioes = malloc(sizeof(struct aviao) * MAX_AVIOES);

    time_t inicio = time(NULL);
    int i = 0;

    while (time(NULL) - inicio < TEMPO_TOTAL && i < MAX_AVIOES) {
        avioes[i].id = i + 1;
        avioes[i].tipo = rand() % 2;
        avioes[i].tempoDeOperacao = rand() % 5 + 1;
        avioes[i].entrouEmAlerta = 0;

        pthread_mutex_lock(&mutexContadores);
        totalAvioes++;
        if (avioes[i].tipo == 0) totalDomesticos++;
        else totalInternacionais++;
        pthread_mutex_unlock(&mutexContadores);

        pthread_create(&threads[i], NULL, threadAviao, &avioes[i]);

        i++;
        sleep(rand() % 3 + 1);
    }

    for (int j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
    }

    printf("\n--- RELATÓRIO FINAL ---\n");
    printf("Total de aviões criados: %d\n", totalAvioes);
    printf(" - Domésticos: %d\n", totalDomesticos);
    printf(" - Internacionais: %d\n", totalInternacionais);
    printf("Total de aviões que completaram as operações: %d\n", avioesSucesso);
    printf("Total de aviões que entraram em alerta crítico: %d\n", avioesAlerta);
    printf("Total de aviões que caíram: %d\n", avioesCaidos);

    free(threads);
    free(avioes);
    pthread_mutex_destroy(&mutexPista);
    pthread_mutex_destroy(&mutexPortao);
    pthread_mutex_destroy(&mutexContadores);
    sem_destroy(&semaforoTorre);
    return 0;
}
