CC = gcc
CFLAGS = -Wall -Wextra -pedantic
LDFLAGS_SHELL = -lreadline
LDFLAGS_SIMULATOR = -lpthread

OBJ_USPSH = uspsh.o
EXE_USPSH = uspsh

OBJ_EP1 = ep1.o
EXE_EP1 = ep1

all: uspsh ep1

uspsh: $(OBJ_USPSH)
	$(CC) $(CFLAGS) -o $@ $(OBJ_USPSH) $(LDFLAGS_SHELL)

run_sh: $(EXE_USPSH)
	./$(EXE_USPSH)

ep1: $(OBJ_EP1)
	$(CC) $(CFLAGS) -o $@ $(OBJ_EP1) $(LDFLAGS_SIMULATOR)

run_ep1: $(EXE_EP1)
	./$(EXE_EP1)

clean:
	rm -f $(EXE_USPSH) $(OBJ_USPSH) $(EXE_EP1) $(OBJ_EP1)