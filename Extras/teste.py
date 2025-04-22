import glob
import os
from pathlib import Path
import math
import matplotlib.pyplot as plt

base_dir = Path(os.getcwd())

# 1) Encontre todos os arquivos que começam com "Defsaida30-"
paths = glob.glob('saida-*')

deadlines_met = {}
preemptions   = {}

for path in paths:
    # extrai o sufixo após "Defsaida30-"
    name = os.path.basename(path).replace('saida-','')  # ex: "FCFS", "Prioridade", "SRTN"
    with open(path) as f:
        lines = [l.strip() for l in f if l.strip()]
    # última linha: total de preempções
    preemptions[name] = int(lines[-1])
    # demais linhas: processos; conta quantos '1' no último campo
    met = sum(1 for l in lines[:-1] if l.split()[-1] == '1')
    deadlines_met[name] = met

# 2) Defina a ordem fixa que você quer nos gráficos
algs = ['FCFS', 'SRTN', 'Prioridade']

# 3) Parâmetro de passo para os ticks do eixo Y
y_step = 10

# --- Gráfico 1: deadlines cumpridas (entrada‑esperado) ---
values1 = [deadlines_met[a] for a in algs]
max1    = max(values1)
ymax1   = math.ceil(max1 / y_step) * y_step +y_step  # arredonda pra cima ao múltiplo de y_step

plt.figure(figsize=(6,4))
plt.bar(algs, values1)
plt.title('Cumprimento de Deadlines (entrada‑inesperado) - Máquina B')
plt.ylabel('Número de deadlines cumpridas')
plt.ylim(0, ymax1)
plt.yticks(range(0, ymax1+1, y_step))
plt.tight_layout()
plt.savefig(base_dir / 'deadlines_cumpridas.png')

# --- Gráfico 2: número de preempções (entrada‑esperado) ---
values2 = [preemptions[a] for a in algs]
max2    = max(values2)
ymax2   = math.ceil(max2 / y_step) * y_step +y_step
plt.figure(figsize=(6,4))
plt.bar(algs, values2)
plt.title('Número de Preempções (entrada‑inesperado) - Máquina B')
plt.ylabel('Preempções')
plt.ylim(0, ymax2)
plt.yticks(range(0, ymax2+1, y_step))
plt.tight_layout()
plt.savefig(base_dir / 'preempcoes.png')