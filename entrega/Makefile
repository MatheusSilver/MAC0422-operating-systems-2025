CC = gcc
CFLAGS = -Wall -Wextra
LDFLAGS = -lreadline
OBJ_USPSH = uspsh.o

all: uspsh

uspsh: $(OBJ_USPSH)
	$(CC) $(CFLAGS) -o $@ $(OBJ_USPSH) $(LDFLAGS)
	rm -f uspsh.o

clean:
	rm -f uspsh $(OBJ_USPSH)