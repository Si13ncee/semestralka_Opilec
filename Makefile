CC = gcc
CFLAGS = -Wall -g
LDFLAGS =

SERVER_SRC = server.c
CLIENT_SRC = client.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) -o server $(SERVER_SRC)

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o client $(CLIENT_SRC)

clean:
	rm -f server client
