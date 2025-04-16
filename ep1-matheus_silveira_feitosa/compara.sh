#!/bin/bash

# Script: compare_processes.sh
# Uso: ./compare_processes.sh arquivo1 arquivo2

# Verifica se dois argumentos foram fornecidos
if [ $# -ne 2 ]; then
    echo "Erro: Forneça dois arquivos para comparação"
    echo "Uso: $0 arquivo1 arquivo2"
    exit 1
fi

# Função para ordenar processos pelo nome
sort_processes() {
    sort -t ' ' -k1.5n "$1" # Ordena pelo número após 'proc' (campo 1, posição 5)
}

# Cria arquivos temporários ordenados
sorted_file1=$(mktemp)
sorted_file2=$(mktemp)

sort_processes "$1" > "$sorted_file1"
sort_processes "$2" > "$sorted_file2"

# Compara os arquivos ordenados
echo "======================================"
echo "Diferenças entre $1 e $2:"
echo "======================================"
diff -y --suppress-common-lines "$sorted_file1" "$sorted_file2"

# Limpeza
rm "$sorted_file1" "$sorted_file2"