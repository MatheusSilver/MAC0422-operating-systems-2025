#!/bin/bash

# A essa altura do campeonato, já deu pra perceber que eu literal, pedi (Dessa vez pro deepseek)
# Nessa época eu achava o deepseek melhor, para adaptar o outro shell de testes usado no EP2.
# Em essencia, os dois são praticamente a mesma coisa, só muda a forma de pegar o tempo.

# --- CONFIGURAÇÃO ---
# Vetores de algoritmos (índice e nome para saída legível)
algos=(1 2 3 4)
names=("FIRST_FIT" "NEXT_FIT" "BEST_FIT" "WORST_FIT")

# Vetor de arquivos de trace
traces=(trace-firstfit trace-nextfit trace-bestfit trace-worstfit)

# Arquivo CSV onde os resultados brutos serão armazenados
output_csv="results.csv"

# Quantidade de repetições por combinação
runs=30

# Valor t para 95% de confiança com df = 29 (~2.045)
t_value=2.045

# Arquivo de estatísticas finais
stats_txt="stats.txt"

# --- LIMPA ARQUIVOS ANTIGOS ---
> "$output_csv"
> "$stats_txt"

# Escreve cabeçalho no CSV
echo "algo,trace,time,failures" >> "$output_csv"

# --- COLETA DE DADOS ---
for i in "${!algos[@]}"; do
  algo=${algos[$i]}
  algname=${names[$i]}

  for trace in "${traces[@]}"; do

    echo "Executando $algname sobre $trace..."

    for run in $(seq 1 $runs); do
      # marca tempo antes
      start=$(date +%s.%N)

      # executa o programa, captura só a última linha (número de falhas)
      failures=$(./ep3 "$algo" ep3-exemplo01.pgm "$trace" saida.pgm | tail -n1)

      # marca tempo depois
      end=$(date +%s.%N)

      # calcula duração em segundos (float)
      runtime=$(echo "$end - $start" | bc)

      # grava no CSV
      echo "$algname,$trace,$runtime,$failures" >> "$output_csv"
    done

  done
done

# --- CÁLCULO DE MÉDIA e INTERVALO DE CONFIANÇA 95% ---
echo "Estatísticas (95% CI, t=$t_value, df=$((runs-1))):" >> "$stats_txt"
echo >> "$stats_txt"

for algname in "${names[@]}"; do
  for trace in "${traces[@]}"; do

    # usa AWK para calcular média, desvio padrão amostral e CI
    awk -v alg="$algname" -v trc="$trace" -v t="$t_value" -F',' '
      BEGIN {
        count=0; sum=0; sumsq=0;
      }
      $1==alg && $2==trc {
        count++;
        x=$3;
        sum += x;
        sumsq += x*x;
      }
      END {
        if (count > 1) {
          mean = sum / count;
          # desvio padrão amostral
          sd = sqrt( (sumsq - sum*sum/count) / (count - 1) );
          ci = t * sd / sqrt(count);
          printf("%-10s | %-16s | mean = %8.6f s | CI95%% = ±%6.6f s\n",
                 alg, trc, mean, ci);
        }
      }
    ' "$output_csv" >> "$stats_txt"

  done
done

# --- FINAL ---
echo
echo "Coleta finalizada."
echo "Resultados brutos em: $output_csv"
echo "Resumo estatístico em: $stats_txt"
