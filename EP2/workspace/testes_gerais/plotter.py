# Outro script que pedi pro Gepeto fazer pra gerar gráficos pois novamente
# Não sou mais químico para usar Google Planilhas, e mim por enquanto, ainda ter preguiça de aprender a usar MatPlotLib

import argparse
import re
import glob
import os
import pandas as pd
import matplotlib.pyplot as plt
from scipy import stats
import math


def parse_filename(fn):
    m = re.match(r".*_d(?P<d>\d+)_k(?P<k>\d+)_(?P<dir>pe|pi)\.txt$", os.path.basename(fn))
    if not m:
        raise ValueError(f"Filename {fn} does not match expected pattern")
    return int(m.group('d')), int(m.group('k')), m.group('dir')


def read_raw(fn):
    """Lê valores brutos de tempo e memória de um arquivo"""
    times, mems = [], []
    with open(fn) as f:
        for line in f:
            parts = line.split()
            if len(parts) != 2:
                continue
            t, m = parts
            times.append(float(t))
            mems.append(float(m))
    return times, mems


def summarize(values):
    """Retorna média e intervalo de confiança 95% para a lista de valores"""
    n = len(values)
    if n == 0:
        return 0, 0, 0
    mean = sum(values) / n
    sem = stats.sem(values)
    t_crit = stats.t.ppf(0.975, df=n-1)
    margin = sem * t_crit
    return mean, mean - margin, mean + margin


def plot_group(df, group_var, fixed_var, out_prefix):
    direction = out_prefix.split('_')[1]
    dir_desc = {'pi': 'Abordagem Ingênua', 'pe': 'Abordagem Eficiente'}[direction]

    fixed_vals = sorted(df[fixed_var].unique())
    var_vals = sorted(df[group_var].unique())

    if group_var == 'k':
        terms = ['poucos', 'normal', 'muitos']
    else:
        terms = ['pequena', 'média', 'grande']
    desc_map = {v: terms[i] for i, v in enumerate(var_vals)}

    for fixed in fixed_vals:
        sub = df[df[fixed_var] == fixed]
        times, mems = [], []
        labels = [f"{v}\n{desc_map[v]}" for v in var_vals]

        for v in var_vals:
            row = sub[sub[group_var] == v]
            if row.empty:
                times.append(0); mems.append(0)
            else:
                times.append(row['mean_time'].iloc[0]); mems.append(row['mean_memory'].iloc[0])

        x = list(range(len(var_vals)))
        width = 0.35
        fig, ax1 = plt.subplots()
        ax2 = ax1.twinx()

        ax1.bar([i - width/2 for i in x], times, width, label='Tempo médio (s)')
        ax2.bar([i + width/2 for i in x], mems, width, alpha=0.7, label='Memória média (KB)')

        xlabel = 'Número de ciclistas' if group_var == 'k' else 'Tamanho da pista (m)'
        ax1.set_xlabel(xlabel)
        ax1.set_xticks(x)
        ax1.set_xticklabels(labels)
        ax1.set_ylabel('Tempo médio (s)')
        ax2.set_ylabel('Memória média (KB)')

        max_t = max(times) if times else 0
        max_m = max(mems) if mems else 0
        if max_t > 0:
            ax1.set_ylim(0, max_t * 1.2)
        if max_m > 0:
            ax2.set_ylim(0, max_m * 1.2)

        fixed_label = f"Pista de {fixed}m" if fixed_var == 'd' else f"{fixed} ciclistas"
        var_label = 'Nº de Ciclistas' if group_var == 'k' else 'Tamanho da pista'
        plt.title(f"{fixed_label} variando {var_label}")

        # Combina legendas de tempo e memória no canto superior esquerdo, empilhadas verticalmente
        h1, l1 = ax1.get_legend_handles_labels()
        h2, l2 = ax2.get_legend_handles_labels()
        handles = h1 + h2
        labels = l1 + l2
        ax1.legend(handles, labels, loc='upper left', ncol=1)

        plt.tight_layout()

        out_file = f"{out_prefix}_{fixed_var}{fixed}_vs_{group_var}.png"
        plt.savefig(out_file)
        plt.close(fig)
        print(f"Saved {out_file}")


def main():
    parser = argparse.ArgumentParser(description='Plot média de tempo e memória de arquivos de métricas')
    parser.add_argument('patterns', nargs='+', help='Padrões de arquivo (ex.: RESULTS_d*.txt)')
    args = parser.parse_args()

    files = []
    for pat in args.patterns:
        matched = glob.glob(pat)
        if not matched:
            print(f"Warning: nenhum arquivo para padrão '{pat}'")
        files.extend(matched)
    if not files:
        print("Nenhum arquivo para processar."); return

    # Impressão de média e IC95 para cada arquivo
    for fn in sorted(files):
        try:
            times, mems = read_raw(fn)
            mt, lt, ut = summarize(times)
            mm, lm, um = summarize(mems)
            print(f"{os.path.basename(fn)}: Tempo médio = {mt:.3f} s [ {lt:.3f}; {ut:.3f} ], Memória média = {mm:.3f} KB [ {lm:.3f}; {um:.3f} ]")
        except Exception as e:
            print(f"Erro em {fn}: {e}")

    records = []
    for fn in sorted(files):
        try:
            d, k, direction = parse_filename(fn)
        except ValueError as e:
            print(e); continue
        times, mems = read_raw(fn)
        mean_t, _, _ = summarize(times)
        mean_m, _, _ = summarize(mems)
        records.append({'d': d, 'k': k, 'direction': direction,
                        'mean_time': mean_t, 'mean_memory': mean_m})

    df = pd.DataFrame(records)
    for direction in ['pe', 'pi']:
        sub = df[df['direction'] == direction]
        if sub.empty:
            print(f"Nenhum dado para direção {direction}"); continue
        plot_group(sub, 'k', 'd', f'plot_{direction}_d_fixed')
        plot_group(sub, 'd', 'k', f'plot_{direction}_k_fixed')

if __name__ == '__main__':
    main()