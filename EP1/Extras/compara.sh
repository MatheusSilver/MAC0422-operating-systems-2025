#!/bin/bash

# Auxiliar de teste.sh
# Só porque eu estava com preguiça de complementar o outro script.

# Ordenamos os processos pelo nome só pra não ter problema com processos acabando ao mesmo tempo, e sendo escritos em ordens diferentes.
sort_processes() {
    sort -t ' ' -k1.5n "$1"
}

# Cria arquivos temporários ordenados
sorted_orig_file=$(mktemp)
sorted_dest_file=$(mktemp)

sort_processes "$1" > "$sorted_orig_file"
sort_processes "$2" > "$sorted_dest_file"

echo "======================================"
echo "Diferenças entre $1 e $2:"
echo "======================================"
diff -y --suppress-common-lines "$sorted_orig_file" "$sorted_dest_file"

rm "$sorted_orig_file" "$sorted_dest_file"
exit 0