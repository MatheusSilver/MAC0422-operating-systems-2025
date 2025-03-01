CC = gcc
CFLAGS = -Wall -Wextra -pedantic
LDFLAGS = -lreadline
OBJ_USPSH = uspsh.o
EXE_USPSH = uspsh

all: uspsh

uspsh: $(OBJ_USPSH)
	$(CC) $(CFLAGS) -o $@ $(OBJ_USPSH) $(LDFLAGS)

run_sh: $(EXE_USPSH)
	./$(EXE_USPSH)

clean:
	rm -f $(EXE_USPSH) $(OBJ_USPSH)