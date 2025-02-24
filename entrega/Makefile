CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
OBJ_USPSH = uspsh.o
OBJ_EP1 = ep1.o

# Regra padrão para compilar todos os executáveis
all: uspsh ep1

# Compila o uspsh
uspsh: $(uspsh.o)
	$(CC) $(CFLAGS) -o uspsh $(OBJ_USPSH)

# Compila o ep1
ep1: $(OBJ_EP1)
	$(CC) $(CFLAGS) -o ep1 $(OBJ_EP1) -lpthread

# Gera os arquivos .o para cada código-fonte
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpeza dos arquivos gerados
clean:
	rm -f *.o uspsh ep1
