#!/bin/bash

ARQUIVO_TRACE=( entrada-esperado.txt entrada-inesperado.txt )
EXECUTAVEL="./ep1"

# Lista de escalonadores
declare -A NOMES
NOMES[1]="FCFS"
NOMES[2]="SRTN"
NOMES[3]="Prioridade"

for arquivo in "${ARQUIVO_TRACE[@]}"; do
    for tipo in 1 2 3; do
        echo "==============================="
        echo " Testando ${NOMES[$tipo]} (tipo $tipo)"
        echo "-------------------------------"
        for i in $(seq 30); do
            $EXECUTAVEL $tipo "${arquivo}" "saida${i}-${NOMES[$tipo]}-$arquivo"
        done
        for i in $(seq 30); do
            if [ $((i % 2)) -eq 0 ]; then
                ./compara.sh "saida${i}-${NOMES[$tipo]}-$arquivo" "saida$((i-1))-${NOMES[$tipo]}-$arquivo"
            fi
        done
        echo
    done
done