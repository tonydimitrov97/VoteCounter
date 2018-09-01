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
	if (argc < CLIENT_ARGS + 1) {
		printf("Wrong number of args, expected %d, given %d\n", CLIENT_ARGS, argc - 1);
		exit(0);
	}

	// Create server socket.
	int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	socketCheck(server_fd);

	// Specify an address to connect to.
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[3]));
	server_addr.sin_addr.s_addr = inet_addr(argv[2]);

	// Connecting to server.
	if (connect(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) == 0) {
		printf("Initiated connection with server at %s:%s\n", argv[2], argv[3]);

		// Open command file for reading, and to find the size of the file.
		FILE* command_file = fopen(argv[1], "r");
		fileCheck(command_file);

		// Get the size of the file
		fseek(command_file, 0, SEEK_END);
		unsigned long file_size = ftell(command_file);
		file_size++;
		rewind(command_file);

		// Create a buffer.
		char line[file_size];

		// Temp variables for file I/O.
		char** buffer = malloc(sizeof(char*) * 3);
		mallocCheck(buffer);
		for (int i = 0; i < 3; i++) {
			buffer[i] = malloc(sizeof(char) * 32);
			mallocCheck(buffer[i]);
		}

		// Getting commands from the file.
		while(fgets(line, sizeof(line), command_file) && strlen(line) > 1) {
			strcpy(line, trimwhitespace(line));
			int size = makeargv(line, " ", &buffer);
			char* command = NULL;;

			// Checking to see which command to send to the server, formatting the
			// command and then sending it to the server.
			if (strcmp("Return_Winner", buffer[0]) == 0) {
				command = malloc(MAX_REGION_NAME + 5);
				mallocCheck(command);
				strcpy(command, "RW;");
				for (int i = 0; i < MAX_REGION_NAME; i++)
					strcat(command, " ");
				strcat(command, ";\0");
				printf("Sending request to server: RW\n");
				write(server_fd, command, strlen(command) + 1);
				free(command);
			} else if (strcmp("Count_Votes", buffer[0]) == 0) {
				command = malloc(MAX_REGION_NAME + 5);
				mallocCheck(command);
				strcpy(command, "CV;");
				strcat(command, buffer[1]);
				for (int i = 0; i < MAX_REGION_NAME - strlen(buffer[1]); i++)
					strcat(command, " ");
				strcat(command, ";\0");
				printf("Sending request to server: CV %s\n", buffer[1]);
				write(server_fd, command, strlen(command) + 1);
				free(command);
			} else if (strcmp("Open_Polls", buffer[0]) == 0) {
				command = malloc(MAX_REGION_NAME + 5);
				mallocCheck(command);
				strcpy(command, "OP;");
				strcat(command, buffer[1]);
				for (int i = 0; i < MAX_REGION_NAME - strlen(buffer[1]); i++)
					strcat(command, " ");
				strcat(command, ";\0");
				write(server_fd, command, strlen(command) + 1);
				printf("Sending request to server: OP %s\n", buffer[1]);
				free(command);
			} else if (strcmp("Close_Polls", buffer[0]) == 0) {
				command = malloc(MAX_REGION_NAME + 5);
				mallocCheck(command);
				strcpy(command, "CP;");
				strcat(command, buffer[1]);
				for (int i = 0; i < MAX_REGION_NAME - strlen(buffer[1]); i++)
					strcat(command, " ");
				strcat(command, ";\0");
				printf("Sending request to server: CP %s\n", buffer[1]);
				write(server_fd, command, strlen(command) + 1);
				free(command);
			} else if (strcmp("Add_Votes", buffer[0]) == 0) {
				// Get an offset in order to construct file path.
				int offset = 0;
				for (int i = (strlen(argv[1]) - 1); argv[1][i] != '/' && i > -1; i--)
					offset = i;

				// Copy partial path.
				char path[offset + strlen(buffer[2]) + 3];
				memset(path, '\0', offset + strlen(buffer[2]) + 3);
				strcpy(path, "./");
				for (int i = 2; i <= offset + 1; i++)
					path[i] = argv[1][i-2];

				// Copy file name into path.
				strcpy(path, trimwhitespace(path));
				strcat(path, trimwhitespace(buffer[2]));
				FILE* votes_file = fopen(path, "r");
				fileCheck(votes_file);

				// Temp variables for adding candidates.
				char name[MAX_CANDIDATE_NAME];
				struct Node* list = NULL;
				struct Candidate c;

				// Insert candidates into the list.
				while(fgets(name, sizeof(name), votes_file)) {
					strcpy(c.name, trimwhitespace(name));
					c.count = 1;
					insertCandidate(&list, c);
				}

				// Turn candidate list into string.
				char* vote_counts = malloc(BUF_SIZE);
				mallocCheck(vote_counts);
				candidateListToString(list, &vote_counts);

				// Formatting command to send to the server.
				command = malloc(BUF_SIZE);
				mallocCheck(command);
				strcpy(command, "AV;");
				strcat(command, buffer[1]);
				for (int i = 0; i < MAX_REGION_NAME - strlen(buffer[1]); i++)
					strcat(command, " ");
				strcat(command, ";");
				strcat(command, vote_counts);
				strcat(command, "\0");
				printf("Sending request to server: AV %s %s\n", buffer[1], vote_counts);
				write(server_fd, command, strlen(command) + 1);
				free(vote_counts);
				free(command);
			} else if (strcmp("Remove_Votes", buffer[0]) == 0) {
				// Get an offset in order to construct file path.
				int offset = 0;
				for (int i = (strlen(argv[1]) - 1); argv[1][i] != '/' && i > -1; i--)
					offset = i;

				// Copy partial path.
				char path[offset + strlen(buffer[2]) + 3];
				memset(path, '\0', offset + strlen(buffer[2]) + 3);
				strcpy(path, "./");
				for (int i = 2; i <= offset + 1; i++)
					path[i] = argv[1][i-2];

				// Copy file name into path.
				strcpy(path, trimwhitespace(path));
				strcat(path, trimwhitespace(buffer[2]));
				FILE* votes_file = fopen(path, "r");
				fileCheck(votes_file);

				// Temp variables for adding candidates.
				char name[MAX_CANDIDATE_NAME];
				struct Node* list = NULL;
				struct Candidate c;

				// Insert candidates into the list.
				while(fgets(name, sizeof(name), votes_file)) {
					strcpy(c.name, trimwhitespace(name));
					c.count = 1;
					insertCandidate(&list, c);
				}

				// Turn candidate list into string.
				char* vote_counts = malloc(BUF_SIZE);
				mallocCheck(vote_counts);
				candidateListToString(list, &vote_counts);

				// Formatting command to send to the server.
				command = malloc(BUF_SIZE);
				mallocCheck(command);
				strcpy(command, "RV;");
				strcat(command, buffer[1]);
				for (int i = 0; i < MAX_REGION_NAME - strlen(buffer[1]); i++)
					strcat(command, " ");
				strcat(command, ";");
				strcat(command, vote_counts);
				strcat(command, "\0");
				printf("Sending request to server: RV %s %s\n", buffer[1], vote_counts);
				write(server_fd, command, strlen(command) + 1);
				free(vote_counts);
				free(command);
			} else if (strcmp("Add_Region", buffer[0]) == 0) {
				command = malloc(BUF_SIZE);
				mallocCheck(command);
				strcpy(command, "AR;");
				strcat(command, buffer[1]);
				for (int i = 0; i < MAX_REGION_NAME - strlen(buffer[1]); i++)
					strcat(command, " ");
				strcat(command, ";");
				strcat(command, buffer[2]);
				strcat(command, "\0");
				printf("Sending request to server: AR %s %s\n", buffer[1], buffer[2]);
				write(server_fd, command, strlen(command) + 1);
				free(command);
			} else {
				send(server_fd, buffer[0], strlen(buffer[0]) + 1, 0);
			}

			// Temp variables to hold received and formatted data.
			char buf[BUF_SIZE];
			char** codes = malloc(sizeof(char*) * 3);
			mallocCheck(codes);
			for (int i = 0; i < 3; i++) {
				codes[i] = malloc(sizeof(char) * BUF_SIZE);
				mallocCheck(codes[i]);
			}

			// Receive response from server.
			int r = recv(server_fd, buf, BUF_SIZE, 0);
			socketCheck(r);
			int num_codes = makeargv(buf, ";", &codes);
			strcpy(buf, "Received response from server: ");
			strcat(buf, codes[0]);
			for (int i = 1; i < num_codes; i++) {
				strcat(buf, " ");
				strcat(buf, codes[i]);
			}
			strcat(buf, "\0");
			printf("%s\n", buf);

			free(codes);
		}
		// Close the file.
		printf("Closed connection with server at %s:%s\n", argv[2], argv[3]);
		close(server_fd);
		free(buffer);
	} else {
		perror("Failed to connect to server");
		exit(1);
	}
	return 0;
}
