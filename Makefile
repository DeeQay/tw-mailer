# Makefile for TW-Mailer Pro Project
# Compiles both client and server applications
# Requires: libldap2-dev package (sudo apt install libldap2-dev)

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -pedantic -g
LDFLAGS_SERVER = -lldap -llber
LDFLAGS_CLIENT = 

# Source files
SERVER_SRC = server/twmailer-server.c server/common.c server/blacklist.c server/auth.c server/mailbox.c server/commands.c
CLIENT_SRC = client/twmailer-client.c client/common.c client/input.c client/commands.c

# Executable names
SERVER_BIN = twmailer-server
CLIENT_BIN = twmailer-client

# Targets
.PHONY: all clean

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) $(LDFLAGS_SERVER)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC) $(LDFLAGS_CLIENT)

clean:
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
	rm -rf mailspool
	rm -f *.o
