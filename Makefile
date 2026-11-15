CC = gcc
CFLAGS = -Wall -Iinclude
LDFLAGS = -lreadline

SRC = src/main.c src/shell.c src/execute.c
OBJ = obj/main.o obj/shell.o obj/execute.o

bin/myshell: $(OBJ)
	mkdir -p bin
	$(CC) $(OBJ) -o bin/myshell $(LDFLAGS)

obj/%.o: src/%.c include/shell.h
	mkdir -p obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf obj bin
