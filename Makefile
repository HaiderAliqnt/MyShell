CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

TARGET = mysh

SRC = src/main.c \
      src/shell.c \
      src/parser.c \
      src/process.c \
      src/builtins.c \
      src/redirection.c \
      lib/linenoise/linenoise.c

OBJ = $(SRC:.c=.o)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
