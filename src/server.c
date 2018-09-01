/*
dimit037, mulli231
date: 04/30/18
Lecture Section: Both lecture section 10
name: Antonio Dimitrov, John Mulligan
id: 4969287, 4214411
Extra credit: Yes
*/
#include "pa4_util.h"

int main(int argc, char** argv) {
	// Checking argument count.
	if (argc < SERVER_ARGS + 1) {
		printf("Wrong number of args, expected %d, given %d\n", SERVER_ARGS, argc - 1);
		exit(0);
	}
	struct ClientArgs cargs;
	cargs.num_regions = malloc(sizeof(int));
	mallocCheck(cargs.num_regions);

	cargs.regions = malloc(sizeof(node_t*) * MAX_REGIONS);
	mallocCheck(cargs.regions);

	// Create DAG based on input file given.
	for (int i = 0; i < MAX_REGIONS; i++) {
		cargs.regions[i] = malloc(sizeof(node_t));
		mallocCheck(cargs.regions[i]);
		initNode(cargs.regions[i]);
	}
	parseInput(argv[1], cargs.regions, cargs.num_regions);

	// Create client socket.
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	socketCheck(server_fd);

	// Specify an address for clients to connect to.
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[2]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// Bind it to a local address.
	int b = bind(server_fd, (struct sockaddr*) &addr, sizeof(addr));
	socketCheck(b);

	// Listen on this socket.
	int l = listen(server_fd, MAX_CLIENTS);
	socketCheck(l);

	// Accept incoming connections.
	struct sockaddr_in clientAddress;
	socklen_t size = sizeof(struct sockaddr_in);

	// Print the waiting status of the server.
	printf("Listening on port %d\n", ntohs(addr.sin_port));

	// Server runs infinitely.
	while(cargs.client_fd = accept(server_fd, (struct sockaddr*) &clientAddress, (socklen_t*) &size)) {
		// Create a thread, and make sure the ClientArgs struct contains the client's
		// IP address and port number.
		pthread_t clientThread;
		struct in_addr ipAddress = clientAddress.sin_addr;
		inet_ntop(AF_INET, &ipAddress, cargs.ip, INET_ADDRSTRLEN);
		cargs.port_number = ntohs(clientAddress.sin_port);
		pthread_create(&clientThread, NULL, &threadFun, (void*)&cargs);
		pthread_join(clientThread, NULL);
	}

	close(server_fd);
	free(cargs.num_regions);
	for (int i = 0; i < MAX_REGIONS; i++)
		deleteCandidateList(&cargs.regions[i]->cand_list);

	return 0;
}
