#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

// --- PARÂMETROS DA SIMULAÇÃO ---
#define TEMPO_TOTAL (60 * 5) // Simulação de 5 minutos
#define MAX_AVIOES 500       // Limite de aviões a serem criados

// --- CONTROLE GLOBAL ---
volatile int simulacaoAtiva = 1;

// --- MÉTRICAS E MUTEXES ---
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

// --- RECURSOS DO AEROPORTO (SEMÁFOROS) ---
sem_t semPistas;
sem_t semPortoes;
sem_t semTorre;

// --- ESTRUTURA DO AVIÃO ---
struct aviao {
    int id;
    int tipo; // 0 doméstico, 1 internacional
    int tempoDeOperacao;
    int entrouEmAlerta;
    pthread_mutex_t mutexAviao;
    int emEstadoCritico; 
    int anunciouPrioridade; 
};

// --- ESTRUTURA PARA O MONITOR ---
struct monitor_args {
    struct aviao* avioes;
    int numAvioesCriados;
};

// Função auxiliar para esperar um semáforo com timeout em segundos
int sem_wait_seconds(sem_t *s, int seconds) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) return -1;
    ts.tv_sec += seconds;
    while (1) {
        int r = sem_timedwait(s, &ts);
        if (r == 0) return 0;
        if (errno == EINTR) continue;
        return -1;
    }
}

int pousar(struct aviao *a) {
    int obteveTorre = 0;
    int obtevePista = 0;
    if (!simulacaoAtiva) return -1;

    time_t inicioEspera = time(NULL);

    while (!(obtevePista && obteveTorre) && simulacaoAtiva) {
        
        if (a->tipo == 0) { 
            int espera = time(NULL) - inicioEspera;
            if (espera >= 60 && !a->entrouEmAlerta) {
                pthread_mutex_lock(&mutexContadores);
                if (!a->entrouEmAlerta) {
                    a->entrouEmAlerta = 1;
                    avioesAlerta++;
                    a->emEstadoCritico = 1; 
                    printf("Aviao %d (Domestico) em alerta critico (esperando ha %ds) [pouso]\n", a->id, espera);
                    fflush(stdout);
                }
                pthread_mutex_unlock(&mutexContadores);
            }
            if (espera >= 90) {
                pthread_mutex_lock(&mutexContadores);
                avioesCaidos++;
                pthread_mutex_unlock(&mutexContadores);
                printf("!!! Aviao %d (Domestico) CAIU apos 90s esperando por recursos para pouso.\n", a->id);
                fflush(stdout);
                if (obteveTorre) sem_post(&semTorre);
                if (obtevePista) sem_post(&semPistas);
                return -1;
            }
        }

        // --- ESTRATÉGIA DE ALOCAÇÃO ---
        
        // Estratégia para Internacionais (sempre prioritária)
        if (a->tipo == 1) {
            if (!obtevePista) {
                if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
            }
            if (obtevePista && !obteveTorre) {
                if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
                else {
                    sem_post(&semPistas);
                    obtevePista = 0;
                }
            }
        } 
        // Estratégia para Domésticos (muda se ficar crítico)
        else { 
            if (a->emEstadoCritico == 1) { // Lógica Prioritária para Doméstico Crítico
                if (a->anunciouPrioridade == 0) {
                     printf("--- Aviao %d (Domestico CRITICO) tentando pouso prioritario ---\n", a->id);
                     fflush(stdout);
                     a->anunciouPrioridade = 1;
                }
                // Tenta pegar o que falta, usando a ordem Pista -> Torre
                if (!obtevePista) {
                    if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
                }
                if (obtevePista && !obteveTorre) {
                    if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
                    else {
                        sem_post(&semPistas);
                        obtevePista = 0;
                    }
                }
            } else { // Lógica Padrão para Doméstico normal
                // Tenta pegar o que falta, usando a ordem Torre -> Pista
                if (!obteveTorre) {
                    if (sem_wait_seconds(&semTorre, 1) == 0) obteveTorre = 1;
                }
                if (obteveTorre && !obtevePista) {
                    if (sem_wait_seconds(&semPistas, 1) == 0) obtevePista = 1;
                    else {
                        sem_post(&semTorre);
                        obteveTorre = 0;
                    }
                }
            }
        }
    }

    if (!(obtevePista && obteveTorre)) {
        if (obtevePista) sem_post(&semPistas);
        if (obteveTorre) sem_post(&semTorre);
        return -1;
    }

    printf("Aviao %d (%s) iniciou o pouso.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    fflush(stdout);
    sleep(a->tempoDeOperacao);
    printf("Aviao %d (%s) terminou o pouso.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    fflush(stdout);

    sem_post(&semTorre);
    sem_post(&semPistas);
    return 0;
}

int desembarcar(struct aviao *a) {
    int obteveTorre = 0;
    int obtevePortao = 0;
    if (!simulacaoAtiva) return -1;
    
    if (a->tipo == 1) { 
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
    } else { 
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
                    printf("Aviao %d (Domestico) esperando para desembarcar (esperando ha %ds)\n", a->id, espera);
                    fflush(stdout);
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
    fflush(stdout);
    sleep(a->tempoDeOperacao);
    printf("Aviao %d (%s) terminou o desembarque.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    fflush(stdout);

    sem_post(&semTorre);
    sem_post(&semPortoes);
    return 0;
}

int decolar(struct aviao *a) {
    int obteveTorre = 0;
    int obtevePista = 0;
    int obtevePortao = 0;
    if (!simulacaoAtiva) return -1;
    
    if (a->tipo == 1) { 
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
    } else { 
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
                   printf("Aviao %d (Domestico) esperando para decolar (esperando ha %ds)\n", a->id, espera);
                   fflush(stdout);
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
    fflush(stdout);
    sleep(a->tempoDeOperacao);
    printf("Aviao %d (%s) terminou a decolagem.\n", a->id, a->tipo == 0 ? "Domestico" : "Internacional");
    fflush(stdout);

    sem_post(&semTorre);
    sem_post(&semPistas);
    sem_post(&semPortoes);
    return 0;
}

void *threadAviao(void *arg) {
    struct aviao *a = (struct aviao *)arg;
    struct timeval tstart, tend;
    gettimeofday(&tstart, NULL);
    pthread_mutex_lock(&mutexOperacao);
    avioesEmOperacao++;
    if (avioesEmOperacao > picoAvioesSimultaneos) picoAvioesSimultaneos = avioesEmOperacao;
    pthread_mutex_unlock(&mutexOperacao);
    if (pousar(a) != 0) {
        pthread_mutex_lock(&mutexOperacao);
        avioesEmOperacao--;
        pthread_mutex_unlock(&mutexOperacao);
        return NULL;
    }
    if (desembarcar(a) != 0) {
        pthread_mutex_lock(&mutexOperacao);
        avioesEmOperacao--;
        pthread_mutex_unlock(&mutexOperacao);
        return NULL;
    }
    if (decolar(a) != 0) {
        pthread_mutex_lock(&mutexOperacao);
        avioesEmOperacao--;
        pthread_mutex_unlock(&mutexOperacao);
        return NULL;
    }
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

void* threadMonitor(void* arg) {
    struct monitor_args* args = (struct monitor_args*)arg;
    struct aviao* avs = args->avioes;
    while (simulacaoAtiva) {
        sleep(5);
        pthread_mutex_lock(&mutexMonitor);
        int num_avioes = args->numAvioesCriados;
        for (int i = 0; i < num_avioes; i++) {
            pthread_mutex_lock(&avs[i].mutexAviao);
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
    margs.numAvioesCriados = 0;
    pthread_t monitorThread;
    pthread_create(&monitorThread, NULL, threadMonitor, &margs);
    time_t inicio = time(NULL);
    int i = 0;
    printf("Iniciando simulacao por %d segundos...\n", TEMPO_TOTAL);
    fflush(stdout);
    while (time(NULL) - inicio < TEMPO_TOTAL && i < MAX_AVIOES && simulacaoAtiva) {
        avioes[i].id = i + 1;
        avioes[i].tipo = rand() % 2;
        avioes[i].tempoDeOperacao = rand() % 3 + 1;
        avioes[i].entrouEmAlerta = 0;
        avioes[i].emEstadoCritico = 0;
        avioes[i].anunciouPrioridade = 0;
        pthread_mutex_lock(&mutexContadores);
        totalAvioes++;
        if (avioes[i].tipo == 0) totalDomesticos++;
        else totalInternacionais++;
        pthread_mutex_unlock(&mutexContadores);
        pthread_create(&threads[i], NULL, threadAviao, &avioes[i]);
        i++;
        margs.numAvioesCriados = i;
        sleep(rand() % 4);
    }
    printf("\n>>> TEMPO DE SIMULACAO ESGOTADO. Nao serao criados novos avioes. Aguardando operacoes em andamento... <<<\n\n");
    fflush(stdout);
    simulacaoAtiva = 0;
    for (int j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
    }
    pthread_join(monitorThread, NULL);
    char nome_arquivo[100];
    sprintf(nome_arquivo, "relatorio_%dp_%dg_%dt.txt", numPistas, numPortoes, capacidadeTorre);
    FILE *arquivo_relatorio;
    arquivo_relatorio = fopen(nome_arquivo, "w");
    if (arquivo_relatorio == NULL) {
        fprintf(stderr, "Erro ao criar o arquivo de relatorio '%s'.\n", nome_arquivo);
    }
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
    if (arquivo_relatorio != NULL) {
        fprintf(arquivo_relatorio, "--- RELATORIO FINAL ---\n");
        fprintf(arquivo_relatorio, "Configuracao: %d Pistas, %d Portoes, %d Torre(s) de Controle\n", numPistas, numPortoes, capacidadeTorre);
        fprintf(arquivo_relatorio, "----------------------------------------\n");
        fprintf(arquivo_relatorio, "Total de avioes criados: %d\n", totalAvioes);
        fprintf(arquivo_relatorio, " - Domesticos: %d\n", totalDomesticos);
        fprintf(arquivo_relatorio, " - Internacionais: %d\n", totalInternacionais);
        fprintf(arquivo_relatorio, "----------------------------------------\n");
        fprintf(arquivo_relatorio, "Total de avioes que completaram o ciclo: %d\n", avioesSucesso);
        fprintf(arquivo_relatorio, "Total de avioes que cairam (starvation): %d\n", avioesCaidos);
        fprintf(arquivo_relatorio, "Total de avioes que entraram em alerta critico: %d\n", avioesAlerta);
        fprintf(arquivo_relatorio, "Deadlocks detectados pelo monitor: %d\n", deadlocksOcorridos);
        fprintf(arquivo_relatorio, "----------------------------------------\n");
        fprintf(arquivo_relatorio, "Tempo total de simulacao (sec): %ld\n", time(NULL) - inicio);
        fprintf(arquivo_relatorio, "Pico de avioes simultaneos em operacao: %d\n", picoAvioesSimultaneos);
    }
    if (avioesSucesso > 0) {
        double media = somaTemposOperacao / (double)avioesSucesso;
        printf("Tempo medio de ciclo por aviao (sucesso): %.2f ms\n", media);
        if (arquivo_relatorio != NULL) {
            fprintf(arquivo_relatorio, "Tempo medio de ciclo por aviao (sucesso): %.2f ms\n", media);
        }
    } else {
        printf("Nenhum aviao completou as operacoes com sucesso.\n");
        if (arquivo_relatorio != NULL) {
            fprintf(arquivo_relatorio, "Nenhum aviao completou as operacoes com sucesso.\n");
        }
    }
    printf("--- FIM DO RELATORIO ---\n");
    if (arquivo_relatorio != NULL) {
        fprintf(arquivo_relatorio, "--- FIM DO RELATORIO ---\n");
        fclose(arquivo_relatorio);
        printf("\nRelatorio final tambem foi salvo em '%s'\n", nome_arquivo);
    }
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