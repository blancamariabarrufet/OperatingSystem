CC = gcc
CFLAGS = -Wall -Wextra -pthread

all: clean maester

maester: maester.c config.c console.c inventory.c commands.c network.c client_handler.c server_handler.c config.h console.h inventory.h commands.h network.h client_handler.h server_handler.h
	$(CC) $(CFLAGS) -o maester maester.c config.c console.c inventory.c commands.c network.c client_handler.c server_handler.c

clean:
	rm -f maester

.PHONY: all clean
