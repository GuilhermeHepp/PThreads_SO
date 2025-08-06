#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* 3 pistas - exclusivas
5 portões de embarque - exclusivos
1 torre de controle - compatilhado, mas só, atendem no maximo 2 opreações simultâneas
Cada avião (thread) passará pelas seguintes operações:
1. Pouso
• Requer: 1 pista + 1 torre de controle
• Liberação: pista e torre após pouso
• Após o pouso, solicita um portão para desembarque
2. Desembarque
• Requer: 1 portão + 1 torre de controle
• Liberação: torre e, após um tempo, o portão
• Após desembarque, o avião aguarda para decolar
3. Decolagem
• Requer: 1 portão + 1 pista + 1 torre de controle
• Liberação: todos após a decolagem
A principal fonte de deadlocks está na diferença de ordem de alocação de recursos entre voos
domésticos e internacionais. Além disso, voos internacionais possuem prioridade em relação aos
domésticos). 
*/






