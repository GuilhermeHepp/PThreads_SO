# Simulador de Controle de Tráfego Aéreo com PThreads

**Autor:** Guilherme Hepp da Fonseca  
**Disciplina:** Sistemas Operacionais  
**Instituição:** Universidade Federal de Pelotas (UFPel)  
**Professor:** Rafael Burlamaqui Amaral

---

##  Visão Geral

Este projeto é uma simulação de um sistema de controle de tráfego aéreo em um aeroporto internacional com alta demanda, desenvolvido como trabalho final para a disciplina de Sistemas Operacionais. A aplicação foi escrita em **Linguagem C** e utiliza a biblioteca **PThreads** para gerenciar a concorrência entre múltiplas aeronaves (threads) que competem por recursos limitados no aeroporto.

O objetivo principal é modelar, analisar e resolver problemas clássicos de concorrência, como **Deadlock** e **Starvation**, em um cenário prático e complexo.

## Principais Funcionalidades

* **Simulação Multi-threaded:** Cada avião é representado por uma thread PThreads independente, executando seu ciclo de vida (pouso, desembarque, decolagem).
* **Gerenciamento de Recursos com Semáforos:** Os recursos do aeroporto (pistas, portões e torre) são modelados como semáforos contadores para garantir exclusão mútua e controlar o acesso.
* **Prevenção de Deadlock:** Implementa uma estratégia de "tentativa e recuo" (*release-and-retry*) utilizando `sem_timedwait`, que quebra a condição de "Reter e Esperar" (*Hold-and-Wait*), garantindo que o sistema nunca trave em um impasse.
* **Análise e Mitigação de Starvation:** Simula o fenômeno de *starvation* (fome), onde voos de menor prioridade (domésticos) são prejudicados. Inclui uma política de **escalonamento de prioridade dinâmico**, que eleva a prioridade de um avião em estado crítico para tentar evitar a falha operacional ("queda").
* **Configuração Flexível:** A quantidade de recursos do aeroporto pode ser definida através de argumentos de linha de comando, permitindo a fácil realização de diferentes experimentos.
* **Relatório Detalhado:** Ao final de cada simulação, um relatório completo com métricas de desempenho (sucessos, quedas, alertas, picos de uso, etc.) é exibido no terminal e salvo em um arquivo `.txt` com nome dinâmico.

## Conceitos de Sistemas Operacionais Demonstrados

* **Programação Concorrente:** Utilização da biblioteca PThreads para criar e gerenciar múltiplas threads.
* **Exclusão Mútua:** Controle de acesso a recursos compartilhados através de Semáforos.
* **Deadlock:** Demonstração das condições para sua ocorrência e implementação de uma estratégia de **prevenção**.
* **Starvation:** Simulação do problema de fome em processos de baixa prioridade e implementação de uma política de **mitigação** (elevação de prioridade).
* **Sincronização entre Processos:** Uso de `pthread_join` para garantir que a thread principal aguarde a finalização das threads de aviões.

## Tecnologias Utilizadas

* Linguagem C
* Biblioteca PThreads (POSIX Threads)
* Semáforos e Mutexes POSIX
* Compilador GCC em ambiente Windows

## Como Compilar e Executar

### Pré-requisitos

É necessário um ambiente com o compilador `gcc` e a biblioteca `PThreads` instalada. Recomenda-se um sistema operacional Windows. Através do MinGW que instala o pacote pro `gcc`.

### Compilação

Navegue até a pasta do projeto e execute o seguinte comando no terminal:

```bash
gcc Trafego_Aereo.c -o simulador -lpthread
```

### Execução

O programa requer três argumentos de linha de comando para definir a configuração da simulação:

```bash
./simulador <num_pistas> <num_portoes> <capacidade_torre>
```

**Exemplos de Execução:**

* **Cenário de Estresse (configuração base):**
    ```bash
    ./simulador 3 5 2
    ```
* **Cenário Otimizado (com a torre reforçada):**
    ```bash
    ./simulador 3 5 4
    ```

Ao final da execução, um relatório será impresso no terminal e um arquivo (ex: `relatorio_3p_5g_2t.txt`) será gerado no mesmo diretório.
