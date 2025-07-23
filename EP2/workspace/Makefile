CC = gcc
CFLAGS = -Wall -Wextra -pedantic
LDFLAGS = -pthread 

OBJ_EP2 = ep2.o
EXE_EP2 = ep2

all: ep2

ep2: $(OBJ_EP2)
	$(CC) $(CFLAGS) -o $@ $(OBJ_EP2) $(LDFLAGS)

clean:
	rm -f $(EXE_EP2) $(OBJ_EP2)