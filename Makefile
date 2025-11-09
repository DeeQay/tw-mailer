# Makefile for TW-Mailer Project
# Compiles both client and server applications

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g
LDFLAGS = 

# Source files
SERVER_SRC = twmailer-server.c
CLIENT_SRC = twmailer-client.c

# Executable names
SERVER_BIN = twmailer-server
CLIENT_BIN = twmailer-client

# Targets
.PHONY: all clean

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) $(LDFLAGS)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC) $(LDFLAGS)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
	rm -rf mailspool
	rm -f *.o
