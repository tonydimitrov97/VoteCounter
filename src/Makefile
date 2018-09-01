all: client server

server:
	gcc -std=c99 -pthread server.c -o server

client:
	gcc -std=c99 client.c -o client

clean:
	rm client server
