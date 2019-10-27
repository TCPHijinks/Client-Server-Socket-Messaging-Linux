all: server client

server: server.c
	gcc -o server server.c -pthread -Wall -std=c99

client: client.c
	gcc -o client client.c -Wall -std=c99