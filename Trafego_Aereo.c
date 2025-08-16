#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define TEMPO_TOTAL (60 * 2) // Simulação de 2 minutos
#define MAX_AVIOES 100

// controle global
volatile int simulacaoAtiva = 1;

// métricas e mutexes
int avioesEmOperacao = 0;
int picoAvioesSimultaneos = 0;
long long somaTemposOperacao = 0;
pthread_mutex_t mutexOperacao;

int totalAvioes = 0;
int totalDomesticos = 0;
int totalInternacionais = 0;
int avioesSucesso = 0;
int avioesCaidos = 0;
int avioesAlerta = 0;
pthread_mutex_t mutexContadores;

int deadlocksOcorridos = 0;
pthread_mutex_t mutexMonitor;

// semáforos recursos
sem_t semPistas;
sem_t semPortoes;
sem_t semTorre;

// avião
struct aviao {
    int id;
    int tipo; // 0 doméstico, 1 internacional
    int tempoDeOperacao;
    int entrouEmAlerta;
    time_t tempoDeEsperaTorre; // 0 se não está esperando
    pthread_mutex_t mutexAviao;
};

// args monitor
struct monitor_args {
    struct aviao* avioes;
    pthread_t* threads;
    int numAvioesCriados;
};

// helper: sem_timedwait por segundos (retorna 0 se obteve, -1 e errno==ETIMEDOUT se timeout)
int sem_wait_seconds(sem_t *s, int seconds) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) return -1;
    ts.tv_sec += seconds;
    while (1) {
        int r = sem_timedwait(s, &ts);
        if (r == 0) return 0;
        if (errno == EINTR) continue; // retry if interrupted
        return -1; // either ETIMEDOUT or other error
    }
}

// MODIFICAÇÃO: A função agora retorna int (0 para sucesso, -1 para falha/queda)
int pousar(struct aviao *a) {
    int obteveTorre = 0;
    int obtevePista = 0;

    // Se a simulação já terminou, não tenta nem iniciar a operação.
    if (!simulacaoAtiva) return -1;

    if (a->tipo == 1) { // Internacional: Pista -> Torre
        time_t inicio = time(NULL);
        while (!(obtevePista && obteveTorre) && simulacaoAtiva) {
            if (!obtevePista) {
                if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
            }
            if (obtevePista && !obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
                else { // Não conseguiu torre, libera pista para evitar deadlock
                    sem_post(&semPistas);
                    obtevePista = 0;
                }
            }
            
            // Lógica de alerta para voos internacionais também, para registro
            if (time(NULL) - inicio >= 60 && !a->entrouEmAlerta) {
                pthread_mutex_lock(&mutexContadores);
                if (!a->entrouEmAlerta) { // Double check para evitar race condition
                    a->entrouEmAlerta = 1;
                    avioesAlerta++;
                    printf("Aviao %d (Internacional) em alerta critico [pouso]\n", a->id);
                }
                pthread_mutex_unlock(&mutexContadores);
            }
        }
    } else { // Doméstico: Torre -> Pista
        time_t inicio = time(NULL);
        while (!(obteveTorre && obtevePista) && simulacaoAtiva) {
            if (!obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
            }
            if (obteveTorre && !obtevePista) {
                if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
                else { // Não conseguiu pista, libera torre para evitar deadlock
                    sem_post(&semTorre);
                    obteveTorre = 0;
                }
            }

            int espera = time(NULL) - inicio;
            if (espera >= 60 && !a->entrouEmAlerta) {
                pthread_mutex_lock(&mutexContadores);
                if(!a->entrouEmAlerta) {
                    a->entrouEmAlerta = 1;
                    avioesAlerta++;
                    printf("Aviao %d (Domestico) em alerta critico (esperando ha %ds) [pouso]\n", a->id, espera);
                }
                pthread_mutex_unlock(&mutexContadores);
            }
            if (espera >= 90) {
                pthread_mutex_lock(&mutexContadores);
                avioesCaidos++;
                pthread_mutex_unlock(&mutexContadores);
                printf("!!! Aviao %d (Domestico) CAIU apos 90s esperando por recursos para pouso.\n", a->id);
                if (obteveTorre) sem_post(&semTorre);
                // MODIFICAÇÃO: Retorna -1 para sinalizar a falha catastrófica
                return -1;
            }
        }
    }

    // Se saiu do loop porque a simulação acabou, libera o que conseguiu e retorna falha
    if (!(obtevePista && obteveTorre)) {
        if (obtevePista) sem_post(&semPistas);
        if (obteveTorre) sem_post(&semTorre);
        return -1;
    }

    printf("Aviao %d (%s) iniciou o pouso.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    sleep(a->tempoDeOperacao);
    printf("Aviao %d (%s) terminou o pouso.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");

    sem_post(&semTorre);
    sem_post(&semPistas);

    return 0; // Sucesso
}

// MODIFICAÇÃO: A função agora retorna int
int desembarcar(struct aviao *a) {
    int obteveTorre = 0;
    int obtevePortao = 0;

    if (!simulacaoAtiva) return -1;
    
    if (a->tipo == 1) { // Internacional: Portão -> Torre
        while (!(obtevePortao && obteveTorre) && simulacaoAtiva) {
            if (!obtevePortao) {
                if (sem_wait_seconds(&semPortoes, 1) == 0) obtevePortao = 1;
            }
            if (obtevePortao && !obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
                else {
                    sem_post(&semPortoes);
                    obtevePortao = 0;
                }
            }
        }
    } else { // Doméstico: Torre -> Portão
         time_t inicio = time(NULL);
        while (!(obteveTorre && obtevePortao) && simulacaoAtiva) {
            if (!obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
            }
            if (obteveTorre && !obtevePortao) {
                if (sem_wait_seconds(&semPortoes, 1) == 0) obtevePortao = 1;
                else {
                    sem_post(&semTorre);
                    obteveTorre = 0;
                }
            }
            int espera = time(NULL) - inicio;
            if (espera >= 60 && !a->entrouEmAlerta) {
                 pthread_mutex_lock(&mutexContadores);
                 if(!a->entrouEmAlerta){
                    a->entrouEmAlerta = 1;
                    avioesAlerta++;
                    printf("Aviao %d (Domestico) em alerta critico (esperando ha %ds) [desembarque]\n", a->id, espera);
                 }
                pthread_mutex_unlock(&mutexContadores);
            }
        }
    }

    if (!(obtevePortao && obteveTorre)) {
        if (obtevePortao) sem_post(&semPortoes);
        if (obteveTorre) sem_post(&semTorre);
        return -1;
    }

    printf("Aviao %d (%s) iniciou o desembarque.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    sleep(a->tempoDeOperacao);
    printf("Aviao %d (%s) terminou o desembarque.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");

    sem_post(&semTorre);
    // De acordo com o PDF, o portão é liberado "após um tempo", aqui liberamos junto com a torre.
    sem_post(&semPortoes);

    return 0; // Sucesso
}

// MODIFICAÇÃO: A função agora retorna int e a lógica de alocação foi corrigida
int decolar(struct aviao *a) {
    int obteveTorre = 0;
    int obtevePista = 0;
    int obtevePortao = 0;

    if (!simulacaoAtiva) return -1;
    
    // Ordem para Internacional: Portão -> Pista -> Torre
    if (a->tipo == 1) { // Internacional
        while (!(obtevePortao && obtevePista && obteveTorre) && simulacaoAtiva) {
             if (!obtevePortao) {
                if (sem_wait_seconds(&semPortoes, 1) == 0) obtevePortao = 1;
            }
            if (obtevePortao && !obtevePista) {
                if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
                else {
                    sem_post(&semPortoes);
                    obtevePortao = 0;
                }
            }
            if (obtevePortao && obtevePista && !obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
                else {
                    sem_post(&semPistas);
                    sem_post(&semPortoes);
                    obtevePista = 0;
                    obtevePortao = 0;
                }
            }
        }
    } else { // Doméstico
        // Ordem para Doméstico: Torre -> Portão -> Pista
        time_t inicio = time(NULL);
        while (!(obteveTorre && obtevePortao && obtevePista) && simulacaoAtiva) {
            if (!obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
            }
            if (obteveTorre && !obtevePortao) {
                if (sem_wait_seconds(&semPortoes, 1) == 0) obtevePortao = 1;
                else {
                    sem_post(&semTorre);
                    obteveTorre = 0;
                }
            }
            if (obteveTorre && obtevePortao && !obtevePista) {
                if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
                else {
                    sem_post(&semPortoes);
                    sem_post(&semTorre);
                    obtevePortao = 0;
                    obteveTorre = 0;
                }
            }
            
            int espera = time(NULL) - inicio;
            if (espera >= 60 && !a->entrouEmAlerta) {
                pthread_mutex_lock(&mutexContadores);
                if(!a->entrouEmAlerta){
                   a->entrouEmAlerta = 1;
                   avioesAlerta++;
                   printf("Aviao %d (Domestico) em alerta critico (esperando ha %ds) [decolagem]\n", a->id, espera);
                }
                pthread_mutex_unlock(&mutexContadores);
            }
        }
    }

    if (!(obtevePortao && obtevePista && obteveTorre)) {
        if (obtevePortao) sem_post(&semPortoes);
        if (obtevePista) sem_post(&semPistas);
        if (obteveTorre) sem_post(&semTorre);
        return -1;
    }

    printf("Aviao %d (%s) iniciou a decolagem.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    sleep(a->tempoDeOperacao);
    printf("Aviao %d (%s) terminou a decolagem.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");

    sem_post(&semTorre);
    sem_post(&semPistas);
    sem_post(&semPortoes);
    return 0; // Sucesso
}

// thread do avião
void *threadAviao(void *arg) {
    struct aviao *a = (struct aviao *)arg;

    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);

    pthread_mutex_lock(&mutexOperacao);
    avioesEmOperacao++;
    if (avioesEmOperacao > picoAvioesSimultaneos) picoAvioesSimultaneos = avioesEmOperacao;
    pthread_mutex_unlock(&mutexOperacao);

    // MODIFICAÇÃO: Verifica o retorno de cada função. Se falhar, encerra a thread.
    if (pousar(a) != 0) {
        // Se pousar falhou (caiu ou simulação acabou), a thread termina aqui.
        pthread_mutex_lock(&mutexOperacao);
        avioesEmOperacao--;
        pthread_mutex_unlock(&mutexOperacao);
        return NULL;
    }
    
    if (desembarcar(a) != 0) {
        // Se desembarcar falhou (simulação acabou), a thread termina.
        pthread_mutex_lock(&mutexOperacao);
        avioesEmOperacao--;
        pthread_mutex_unlock(&mutexOperacao);
        return NULL;
    }
    
    if (decolar(a) != 0) {
        // Se decolar falhou (simulação acabou), a thread termina.
        pthread_mutex_lock(&mutexOperacao);
        avioesEmOperacao--;
        pthread_mutex_unlock(&mutexOperacao);
        return NULL;
    }

    // Se chegou até aqui, todas as operações foram um sucesso.
    gettimeofday(&tend, NULL);
    long long tempo = (tend.tv_sec - tstart.tv_sec) * 1000LL + (tend.tv_usec - tstart.tv_usec) / 1000;

    pthread_mutex_lock(&mutexContadores);
    avioesSucesso++;
    somaTemposOperacao += tempo;
    pthread_mutex_unlock(&mutexContadores);

    pthread_mutex_lock(&mutexOperacao);
    avioesEmOperacao--;
    pthread_mutex_unlock(&mutexOperacao);

    return NULL;
}

// Monitor de deadlock.
void* threadMonitor(void* arg) {
    struct monitor_args* args = (struct monitor_args*)arg;
    struct aviao* avs = args->avioes;
    
    while (simulacaoAtiva) {
        sleep(5); // Checa a cada 5 segundos
        pthread_mutex_lock(&mutexMonitor);
        int num_avioes = args->numAvioesCriados;
        for (int i = 0; i < num_avioes; i++) {
            pthread_mutex_lock(&avs[i].mutexAviao);
            if (avs[i].tempoDeEsperaTorre > 0) {
                time_t waited = time(NULL) - avs[i].tempoDeEsperaTorre;
                // Um tempo muito longo de espera pode indicar um deadlock não resolvido pela estratégia cooperativa.
                if (waited > 120) { 
                    printf("\n>>> MONITOR: Deadlock potencial detectado! Aviao %d esperando ha mais de 120s. Encerrando simulacao.\n", avs[i].id);
                    pthread_mutex_lock(&mutexContadores);
                    deadlocksOcorridos++;
                    pthread_mutex_unlock(&mutexContadores);

                    simulacaoAtiva = 0; // Força o encerramento de tudo
                    
                    pthread_mutex_unlock(&avs[i].mutexAviao);
                    pthread_mutex_unlock(&mutexMonitor);
                    return NULL;
                }
            }
            pthread_mutex_unlock(&avs[i].mutexAviao);
        }
        pthread_mutex_unlock(&mutexMonitor);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <num_pistas> <num_portoes> <capacidade_torre>\n", argv[0]);
        return 1;
    }
    
    srand(time(NULL));

    int numPistas = atoi(argv[1]);
    int numPortoes = atoi(argv[2]);
    int capacidadeTorre = atoi(argv[3]);

    sem_init(&semPistas, 0, numPistas);
    sem_init(&semPortoes, 0, numPortoes);
    sem_init(&semTorre, 0, capacidadeTorre);

    pthread_mutex_init(&mutexContadores, NULL);
    pthread_mutex_init(&mutexOperacao, NULL);
    pthread_mutex_init(&mutexMonitor, NULL);

    pthread_t* threads = malloc(sizeof(pthread_t) * MAX_AVIOES);
    struct aviao* avioes = malloc(sizeof(struct aviao) * MAX_AVIOES);
    if (!threads || !avioes) { perror("malloc"); return 1; }

    for (int j = 0; j < MAX_AVIOES; j++) {
        pthread_mutex_init(&avioes[j].mutexAviao, NULL);
    }

    struct monitor_args margs;
    margs.avioes = avioes;
    margs.threads = threads;
    margs.numAvioesCriados = 0;

    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, threadMonitor, &margs);

    time_t inicio = time(NULL);
    int i = 0;

    while (time(NULL) - inicio < TEMPO_TOTAL && i < MAX_AVIOES && simulacaoAtiva) {
        avioes[i].id = i + 1;
        avioes[i].tipo = rand() % 2;
        avioes[i].tempoDeOperacao = rand() % 3 + 1; // Operações de 1 a 3 segundos
        avioes[i].entrouEmAlerta = 0;
        avioes[i].tempoDeEsperaTorre = 0;

        pthread_mutex_lock(&mutexContadores);
        totalAvioes++;
        if (avioes[i].tipo == 0) totalDomesticos++;
        else totalInternacionais++;
        pthread_mutex_unlock(&mutexContadores);

        pthread_create(&threads[i], NULL, threadAviao, &avioes[i]);
        i++;
        margs.numAvioesCriados = i;

        sleep(rand() % 4); // Novo avião a cada 0-3 segundos
    }

    printf("\n>>> TEMPO DE SIMULACAO ESGOTADO. Nao serao criados novos avioes. Aguardando operacoes em andamento... <<<\n\n");

    // MODIFICAÇÃO: Sinaliza o fim da simulação ANTES de esperar as threads terminarem.
    simulacaoAtiva = 0;

    for (int j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
    }

    // Agora que as threads de avião terminaram, aguarda o monitor
    pthread_join(monitorThread, NULL);

    printf("\n--- RELATORIO FINAL ---\n");
    printf("Configuracao: %d Pistas, %d Portoes, %d Torre(s) de Controle\n", numPistas, numPortoes, capacidadeTorre);
    printf("----------------------------------------\n");
    printf("Total de avioes criados: %d\n", totalAvioes);
    printf(" - Domesticos: %d\n", totalDomesticos);
    printf(" - Internacionais: %d\n", totalInternacionais);
    printf("----------------------------------------\n");
    printf("Total de avioes que completaram o ciclo: %d\n", avioesSucesso);
    printf("Total de avioes que cairam (starvation): %d\n", avioesCaidos);
    printf("Total de avioes que entraram em alerta critico: %d\n", avioesAlerta);
    printf("Deadlocks detectados pelo monitor: %d\n", deadlocksOcorridos);
    printf("----------------------------------------\n");
    printf("Tempo total de simulacao (sec): %ld\n", time(NULL) - inicio);
    printf("Pico de avioes simultaneos em operacao: %d\n", picoAvioesSimultaneos);
    if (avioesSucesso > 0) {
        double media = somaTemposOperacao / (double)avioesSucesso;
        printf("Tempo medio de ciclo por aviao (sucesso): %.2f ms\n", media);
    } else {
        printf("Nenhum aviao completou as operacoes com sucesso.\n");
    }
    printf("--- FIM DO RELATORIO ---\n");

    // limpeza
    for (int j = 0; j < MAX_AVIOES; j++) pthread_mutex_destroy(&avioes[j].mutexAviao);
    free(threads);
    free(avioes);
    sem_destroy(&semPistas);
    sem_destroy(&semPortoes);
    sem_destroy(&semTorre);
    pthread_mutex_destroy(&mutexContadores);
    pthread_mutex_destroy(&mutexOperacao);
    pthread_mutex_destroy(&mutexMonitor);

    return 0;
}
