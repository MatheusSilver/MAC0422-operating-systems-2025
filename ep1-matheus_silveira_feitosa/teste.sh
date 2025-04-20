#!/bin/bash

ARQUIVO_TRACE="entrada-inesperado.txt"
EXECUTAVEL="./ep1"


if [ ! -f "$ARQUIVO_TRACE" ]; then
    echo "Arquivo de trace '$ARQUIVO_TRACE' não encontrado."
    exit 1
fi

if [ ! -x "$EXECUTAVEL" ]; then
    echo "Executável '$EXECUTAVEL' não encontrado ou não é executável."
    exit 1
fi

# Lista de escalonadores
declare -A NOMES
NOMES[1]="FCFS"
NOMES[2]="SRTN"
NOMES[3]="Prioridade"

for tipo in 1 2 3; do
    echo "==============================="
    echo " Testando ${NOMES[$tipo]} (tipo $tipo)"
    echo "-------------------------------"
    for i in $(seq 2); do
        $EXECUTAVEL $tipo "$ARQUIVO_TRACE" "saida${i}-${NOMES[$tipo]}" 
    done
    for i in $(seq 2); do
        if [ $((i % 2)) -eq 0 ]; then
            ./compara.sh "saida${i}-${NOMES[$tipo]}" "saida$((i-1))-${NOMES[$tipo]}"
        fi
    done
    echo
done