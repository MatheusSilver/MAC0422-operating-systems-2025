#!/bin/bash

# Verifica se é um derivado do Debian, não garanto nada se tu é um usuário de algo mais diferenciado
if ! command -v apt-get &>/dev/null; then
  echo "Este script foi feito para distribuições baseadas em Debian/Ubuntu."
  exit 1
fi


sudo apt-get update

echo "Instalando pacotes de compilação básicos (gcc, make, etc.)..."
sudo apt-get install -y build-essential

echo "Instalando GNU Readline (uspsh)..."
sudo apt-get install -y libreadline-dev

echo "Instalando POSIX Threads (ep1, ep2, ep4)..."
sudo apt-get install -y libc6-dev

echo "Instalando dateutils (ep4)..."
sudo apt-get install -y dateutils

echo "Instalando gnuplot (ep4)..."
sudo apt-get install -y gnuplot

echo "Instalando lsof (ep4)..."
sudo apt-get install -y lsof

echo
echo "Agora basta executar 'make' (ou compilar manualmente) com cada programa."
