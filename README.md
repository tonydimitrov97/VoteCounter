# VoteCounter

This program is a finale to the series of vote-counting applications.  This
iteration uses a TCP protocol to implement client-server connections.  The
server holds the DAG structure (created with an input file) that contains each
voting region and their respective vote counts.  The client sends commands to
the server and the server's DAG is updated accordingly.  The client and server
send messages to one another to communicate whether or not a certain operations
were performed successfully.  The server is threaded so that multiple clients
can be connected to the server at once, but only one can perform operations on
the server's DAG.

How to compile: Run the make command from the directory that client.c, server.c,
makeargv.h, and pa4_util is in.

How to use from shell: 'make' to compile the 2 executables or 'make <filename>'
						to make a specific executable
'./client <input-file> <server-ip-address> <port-number>' to execute client
'.server <input-file> <port-number>' to execute server

Error codes:
0: Not enough arguments
1: Failed to connect to server
2: Error opening file
3: Error allocating memory
4: Socket Error
