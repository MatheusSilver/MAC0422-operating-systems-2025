#!/bin/bash

# Definição dos valores de d, k e p
ds=(100 200 400)
ks=(500)
ps=(i e)

# Número de repetições e valor t para IC 95% com n=30 (gl = 29 → t ≈ 2.045)
# Usando T de Student (vulgo única coisa que tive que especificar pro gepeto dar a fórmula certa)
REPS=30
T_VALUE=2.045

# Arquivo de saída resumo
SUMMARY="summary.txt"
echo "d k p mean_time(s) ci_time(s) mean_mem(KB) ci_mem(KB)" > "$SUMMARY"

# Loop principal
for k in "${ks[@]}"; do
  for d in "${ds[@]}"; do
    for p in "${ps[@]}"; do

      RESULTS_FILE="RESULTS_d${d}_k${k}_p${p}.txt"
      echo "" > "$RESULTS_FILE"

      echo "Executando combinação: d=$d, k=$k, p=$p"
      for ((i=1; i<=REPS; i++)); do
        # Descarta output do programa e envia só o time
        { /usr/bin/time -f "%e %M" ./ep2 "$d" "$k" "$p"; } 2>> "$RESULTS_FILE"
      done

      # A partir do momento que eu vi a palavra "estatística, automaticamente pedi pro Gepeto fazer o cálculo"
      read mean_time ci_time mean_mem ci_mem <<< "$(awk -v n=$REPS -v t=$T_VALUE '
        {
          sum_t += $1;
          sum_m += $2;
          sumsq_t += ($1)^2;
          sumsq_m += ($2)^2;
        }
        END {
          mean_t  = sum_t / n;
          sd_t    = sqrt((sumsq_t - n * mean_t^2) / (n - 1));
          ci_t    = t * sd_t / sqrt(n);
          mean_m  = sum_m / n;
          sd_m    = sqrt((sumsq_m - n * mean_m^2) / (n - 1));
          ci_m    = t * sd_m / sqrt(n);
          # imprime: mean_time, ci_time, mean_mem, ci_mem
          printf("%.6f %.6f %.2f %.2f", mean_t, ci_t, mean_m, ci_m);
        }
      ' "$RESULTS_FILE")"

      echo "$d $k $p $mean_time $ci_time $mean_mem $ci_mem" >> "$SUMMARY"

    done
  done
done

echo "------"
echo "Resumo completo em: $SUMMARY"
