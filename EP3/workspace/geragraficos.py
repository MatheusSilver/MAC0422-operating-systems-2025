# Essa altura do campeonato, não preciso nem reforçar que sou químico com preguiça de gráficos né?

import matplotlib.pyplot as plt
import pandas as pd
import re

# Nome do arquivo de entrada
filename = 'stats.txt'

# Leitura do arquivo e processamento das linhas
with open(filename, 'r') as f:
    lines = f.readlines()

# Lista para armazenar os dados
data = []

# Regex para extrair as informações
pattern = re.compile(r"(\w+_FIT)\s+\|\s+(\S+)\s+\| mean = ([\d.]+) s \| CI95% = ±([\d.]+) s \| (\d+)")

# Parse das linhas
for line in lines:
    match = pattern.match(line)
    if match:
        alg, trace, mean, ci, failures = match.groups()
        data.append({
            'Algoritmo': alg,
            'Trace': trace,
            'Média': float(mean),
            'CI95': float(ci),
            'Falhas': int(failures)
        })

# Criando um DataFrame
df = pd.DataFrame(data)

# Número de comandos por trace
trace_comandos = {
    'trace-firstfit': 238,
    'trace-nextfit': 165,
    'trace-bestfit': 930,
    'trace-worstfit': 323
}

# Lista dos algoritmos e traces
algoritmos = df['Algoritmo'].unique()
traces = df['Trace'].unique()

# Criar os gráficos de médias (sem erro)
for trace in traces:
    plt.figure(figsize=(8, 5))
    subset = df[df['Trace'] == trace]
    plt.bar(subset['Algoritmo'], subset['Média'], color='orange')
    num_comandos = trace_comandos.get(trace, '?')
    plt.title(f'Média de tempo para {trace} ({num_comandos} comandos)')
    plt.ylabel('Tempo (s)')
    plt.xlabel('Algoritmo')
    plt.grid(True, axis='y', linestyle='--', alpha=0.6)
    plt.tight_layout()
    plt.savefig(f'media_{trace}.png')
    plt.close()

# Criar os gráficos de falhas (y entre 0 e 12)
for trace in traces:
    plt.figure(figsize=(8, 5))
    subset = df[df['Trace'] == trace]
    plt.bar(subset['Algoritmo'], subset['Falhas'], color='orange')
    plt.ylim(0, 12)
    num_comandos = trace_comandos.get(trace, '?')
    plt.title(f'Número de falhas para {trace} ({num_comandos} comandos)')
    plt.ylabel('Falhas')
    plt.xlabel('Algoritmo')
    plt.grid(True, axis='y', linestyle='--', alpha=0.6)
    plt.tight_layout()
    plt.savefig(f'falhas_{trace}.png')
    plt.close()

print("Gráficos salvos com sucesso.")
