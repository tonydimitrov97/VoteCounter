/*
dimit037, mulli231
date: 04/30/18
Lecture Section: Both lecture section 10
name: Antonio Dimitrov, John Mulligan
id: 4969287, 4214411
Extra credit: Yes
*/
#define _BSD_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "makeargv.h"

#ifndef PA4_UTIL_H
#define PA4_UTIL_H

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

#define SERVER_ARGS 2
#define CLIENT_ARGS 3
#define MAX_CLIENTS 5
#define MAX_CANDIDATE_NAME 100
#define MAX_CHILDREN 5
#define MAX_REGION_NAME 15
#define MAX_CANDIDATES 100
#define MAX_REGIONS 100
#define MAX_DAG_LINE_SIZE (MAX_REGION_NAME * (MAX_CHILDREN + 1) + 6)
#define BUF_SIZE 256

#define CLOSED 0
#define OPEN 1
#define DONE 2

/******************************************************************************
* Error checking functions
******************************************************************************/
void fileCheck(FILE* file) {
	if (file == NULL) {
		perror("Error opening file");
		exit(2);
	}
}

void mallocCheck(void* ptr) {
	if (ptr == NULL) {
		perror("Error allocating dynamic memory");
		exit(3);
	}
}

void socketCheck(int i) {
	if (i == -1) {
		perror("Socket error");
		exit(4);
	}
}

/******************************************************************************
* Candidate Linked List Struct and Functions
******************************************************************************/

// Used to hold a candidate's name and the number of votes the candidate has.
struct Candidate {
	char name[MAX_CANDIDATE_NAME];
	int count;
};

// Used to create a list of candidates.
struct Node {
	struct Candidate data;
	struct Node* next;
};

// Add a candidate to the list.
void insertCandidate(struct Node **ll, struct Candidate c) {
	if (*ll == NULL){
		*ll = malloc(sizeof(struct Node));
		mallocCheck(*ll);
		(*ll)->data = c;
		(*ll)->next = NULL;
	} else {
		struct Node* temp = *ll;
		struct Node* prev = NULL;
		while(temp != NULL) {
			if (strcmp(temp->data.name, c.name) == 0) {
				temp->data.count += c.count;
				return;
			}
			prev = temp;
			temp = temp->next;
		}
		struct Node* newNode = malloc(sizeof(struct Node));
		mallocCheck(newNode);
		newNode->data = c;
		newNode->next = NULL;
		prev->next = newNode;
	}
}

// Add a candidate to the list.
int removeCandidate(struct Node **ll, struct Candidate c) {
	if (*ll == NULL){
		return 1;
	} else {
		struct Node* temp = *ll;
		while(temp != NULL) {
			if (strcmp(trimwhitespace(temp->data.name), c.name) == 0) {
				if(temp->data.count - c.count > -1) {
					temp->data.count = temp->data.count - c.count;
					return 0;
				} else {
					return 1;
				}
			}
			temp = temp->next;
		}
		return 1;
	}
}

// Print the contents of the candidate list.  Used for debugging.
void printCandidateList(struct Node *ll) {
	struct Node *temp = ll;
	while (temp != NULL){
		printf("%s:", temp->data.name);
		if (temp->next != NULL)
			printf("%d,", temp->data.count);
		else
			printf("%d", temp->data.count);
		temp = temp->next;
	}
	printf("\n");
}

// Print the contents of the candidate list to string.
void candidateListToString(struct Node *ll, char** str) {
	strcpy(*str, "");
	struct Node *temp = ll;
	while (temp != NULL){
		strcat(*str, temp->data.name);
		strcat(*str, ":");
		char tempStr[10];
		sprintf(tempStr, "%d", temp->data.count);
		strcat(*str, tempStr);
		if (temp->next != NULL)
			strcat(*str, ",");
		temp = temp->next;
	}
	strcat(*str, "\0");
}

// Finds the candidates with the highest count.  Stores the "winning"
// candidates name in winner
void findWinner(struct Node* ll, char** winner) {
	int max = 0;
	struct Node* temp = ll;
	while (temp != NULL) {
		if (temp->data.count > max) {
			strcpy(*winner, temp->data.name);
			max = temp->data.count;
		}
		temp = temp->next;
	}
}

// Delete all candidates from the list.
void deleteCandidateList(struct Node **ll) {
	struct Node* prev = NULL;
	struct Node* cur = *ll;
	while(cur != NULL) {
		prev = cur;
		cur = cur->next;
		free(prev);
	}
	*ll = NULL;
}

// Copy the contents of one list to another.
void copyCandidateList(struct Node** lhs, struct Node* rhs) {
	if (*lhs != NULL)
		deleteCandidateList(lhs);

	struct Node* temp = rhs;
	while(temp != NULL) {
		struct Candidate c;
		strcpy(c.name, temp->data.name);
		c.count = temp->data.count;
		insertCandidate(lhs, c);
		temp = temp->next;
	}
}

/******************************************************************************
* Node Struct and Functions
******************************************************************************/
typedef struct node{
	char name[MAX_REGION_NAME];
	struct Node* cand_list;
	int children[MAX_CHILDREN];
	int parent;
	int* num_children;
	int poll_status;
	int id;
}node_t;

// Used to properly initialize a node for polling.
void initNode(node_t* r) {
	r->cand_list = NULL;
	r->parent = -1;
	r->num_children = malloc(sizeof(int));
	*(r->num_children) = 0;
	r->poll_status = CLOSED;
}

// Returns the index of a region in an array based on its name.  Returns -1
// if the region is not found in the array.
int findRegionIndex(char* str, node_t** r, int* num_regions) {
	for (int i = 0; i < *num_regions; i++) {
		if (strcmp(r[i]->name, str) == 0)
			return i;
	}
	return -1;
}

// Recursively opens a region's polls, along with its ancestors.
void openPolls(node_t** r, int index) {
	r[index]->poll_status = OPEN;
	for (int i = 0; i < *(r[index]->num_children); i++)
		openPolls(r, r[index]->children[i]);
}

// Recursively closes a region's polls, along with its ancestors.
void closePolls(node_t** r, int index) {
	r[index]->poll_status = DONE;
	for (int i = 0; i < *(r[index]->num_children); i++)
		closePolls(r, r[index]->children[i]);
}

// Recursively adds candidate vote counts to a region.
void aggregateVotes(node_t** r, int index, struct Node* n_list) {
	struct Node* temp = n_list;
	while(temp != NULL) {
		insertCandidate(&r[index]->cand_list, temp->data);
		temp = temp->next;
	}
	if (r[index]->parent != -1) {
		aggregateVotes(r, r[index]->parent, n_list);
	}
}

// Recursively removes candidate vote counts from a region.
void deaggregateVotes(node_t** r, int index, struct Node* n_list) {
	struct Node* temp = n_list;
	while(temp != NULL) {
		removeCandidate(&r[index]->cand_list, temp->data);
		temp = temp->next;
	}
	if (r[index]->parent != -1) {
		deaggregateVotes(r, r[index]->parent, n_list);
	}
}

/******************************************************************************
* Server Data Structure Functions
******************************************************************************/

// Returns the index of a candidate in an array based on its name.  Returns -1
// if the candidate is not in the array.
int findCandidateIndex(node_t** n, int n_size, char* name) {
	for (int i = 0; i < n_size; i++)
		if (strcmp(n[i]->name, name) == 0)
			return i;
	return -1;
}

// Creates an array of regions based on the contents of the given file.
void parseInput(char *filename, node_t **n, int* n_size) {
	FILE* file = fopen(filename, "r");
	fileCheck(file);

	// Temporary variables for creating string arrays for each line
	// of the input file
	char line[MAX_DAG_LINE_SIZE];
	char** regions = malloc(sizeof(char*) * MAX_REGIONS);
	mallocCheck(regions);
	for (int i = 0; i < MAX_REGIONS; i++) {
		regions[i] = malloc(sizeof(char) * MAX_REGION_NAME);
		mallocCheck(regions[i]);
	}

	// To keep track the actual size of the nodes array
	int size = 0;

	// Get first line to start filling nodes array
	fgets(line, sizeof(line), file);
	int num_children = makeargv(line, ":", &regions);
	int parent_id = size;
	strcpy(n[parent_id]->name, trimwhitespace(regions[0]));	// add parent region
	*(n[parent_id]->num_children) = 0;	// no children to start
	size++;

	// Adding the children to the nodes array
	for (int i = 1; i < num_children; i++) {
		strcpy(n[size]->name, trimwhitespace(regions[i]));
		n[size]->parent = parent_id;
		*(n[size]->num_children) = 0;
		n[parent_id]->children[*(n[parent_id]->num_children)] = size;
		*(n[parent_id]->num_children) = *(n[parent_id]->num_children) + 1;
		size++;
	}

	// Get the rest of the lines
	while(fgets(line, sizeof(line), file)) {
		// Split line into seperate region names
		num_children = makeargv(line, ":", &regions);
		parent_id = findCandidateIndex(n, size, regions[0]);
		// Adding the children to the nodes array
		for (int i = 1; i < num_children; i++) {
			strcpy(n[size]->name, trimwhitespace(regions[i]));
			n[size]->parent = parent_id;
			*(n[size]->num_children) = 0;
			n[parent_id]->children[*(n[parent_id]->num_children)] = size;
			*(n[parent_id]->num_children) = *(n[parent_id]->num_children) + 1;
			size++;
		}
	}
	free(regions);
	*n_size = size;
}// end parseInput

// Used by each client thread whenever a connection to the server is established.
struct ClientArgs {
	char input_region[MAX_REGION_NAME];
	char data[BUF_SIZE];
	char command[BUF_SIZE];
	node_t** regions;
	int* num_regions;
	int client_fd;
	char ip[INET_ADDRSTRLEN];
	int port_number;
};

// Open the polls of a region based on the specifications of the client.
void* openPollsClient (void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;

	// Deciding what to send to client based on data given.
	printf("Request received from client at %s:%d, OP %s\n", cargs.ip, cargs.port_number, cargs.input_region);
	int region_index = findRegionIndex(cargs.input_region, cargs.regions, cargs.num_regions);
	char buffer[BUF_SIZE];
	if (region_index < 0) {
		sprintf(buffer, "NR;%s", cargs.input_region);
	} else if (cargs.regions[region_index]->poll_status == OPEN) {
		sprintf(buffer, "PF;%s;open", cargs.input_region);
	} else if (cargs.regions[region_index]->poll_status == DONE) {
		sprintf(buffer, "RR;%s", cargs.input_region);
	} else {
		openPolls(cargs.regions, region_index);
		sprintf(buffer, "SC;");
	}
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Close the polls of a region based on the specifications of the client.
void* closePollsClient (void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;

	// Deciding what to send to client based on data given.
	printf("Request received from client at %s:%d, CP %s\n", cargs.ip, cargs.port_number, cargs.input_region);
	int region_index = findRegionIndex(cargs.input_region, cargs.regions, cargs.num_regions);
	char buffer[BUF_SIZE];
	if (region_index < 0) {
		sprintf(buffer, "NR;%s", cargs.input_region);
	} else if (cargs.regions[region_index]->poll_status == CLOSED ||
		cargs.regions[region_index]->poll_status == DONE) {
		sprintf(buffer, "PF;%s;closed", cargs.input_region);
	} else {
		closePolls(cargs.regions, region_index);
		sprintf(buffer, "SC;");
	}
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Add a list of candidate vote counts to a region based on the specifications
// of the client.
void* addVotesClient (void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;

	// Deciding what to send to client based on data given.
	printf("Request received from client at %s:%d, AV %s %s\n", cargs.ip, cargs.port_number, cargs.input_region, cargs.data);
	int region_index = findRegionIndex(cargs.input_region, cargs.regions, cargs.num_regions);
	char buffer[BUF_SIZE];
	if (region_index < 0) {
		sprintf(buffer, "NR;%s", cargs.input_region);
	} else if (cargs.regions[region_index]->poll_status == CLOSED
		|| cargs.regions[region_index]->poll_status == DONE) {
		sprintf(buffer, "RC;%s", cargs.input_region);
	} else if (*(cargs.regions[region_index]->num_children) != 0) {
		sprintf(buffer, "NL;%s", cargs.input_region);
	} else {
		char** voting_data = malloc(sizeof(char*) * MAX_CANDIDATES);
		for (int i = 0; i < MAX_CANDIDATES; i++)
			voting_data[i] = malloc(sizeof(char) * BUF_SIZE);

		char** vote_data = malloc(sizeof(char*) * 2);
		for (int i = 0; i < 2; i++)
			vote_data[i] = malloc(sizeof(char) * MAX_CANDIDATE_NAME);

		// Insert each candidate into the list or add counts to already
		// existing candidates.
		struct Node* temp_list = NULL;
		int data_size = makeargv(cargs.data, ",", &voting_data);
		for (int i = 0; i < data_size; i++) {
			makeargv(voting_data[i], ":", &vote_data);
			struct Candidate c;
			strcpy(c.name, trimwhitespace(vote_data[0]));
			c.count = atoi(vote_data[1]);
			insertCandidate(&(cargs.regions[region_index]->cand_list), c);
			insertCandidate(&temp_list, c);
		}
		aggregateVotes(cargs.regions, cargs.regions[region_index]->parent,
			temp_list);

		sprintf(buffer, "SC;");
		free(voting_data);
		free(vote_data);
	}
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Remove the candidate vote counts from a region based on the specifications
// of the client.
void* removeVotesClient (void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);

	// Deciding what to send to client based on data given.
	printf("Request received from client at %s:%d, RV %s %s\n", cargs.ip, cargs.port_number, cargs.input_region, cargs.data);
	int region_index = findRegionIndex(cargs.input_region, cargs.regions, cargs.num_regions);
	char buffer[BUF_SIZE];
	if (region_index < 0) {
		sprintf(buffer, "NR;%s", cargs.input_region);
	} else if (cargs.regions[region_index]->poll_status == CLOSED
		|| cargs.regions[region_index]->poll_status == DONE) {
		sprintf(buffer, "RC;%s", cargs.input_region);
	} else if (*(cargs.regions[region_index]->num_children) != 0) {
		sprintf(buffer, "NL;%s", cargs.input_region);
	} else {
		char** voting_data = malloc(sizeof(char*) * MAX_CANDIDATES);
		for (int i = 0; i < MAX_CANDIDATES; i++)
			voting_data[i] = malloc(sizeof(char) * BUF_SIZE);

		char** vote_data = malloc(sizeof(char*) * 2);
		for (int i = 0; i < 2; i++)
			vote_data[i] = malloc(sizeof(char) * MAX_CANDIDATE_NAME);

		// Save the candidate list in case subtraction is invalid
		int data_size = makeargv(cargs.data, ",", &voting_data);
		struct Node* saved_list = NULL;
		struct Node* list_to_remove = NULL;
		int valid = 0;
		strcpy(buffer, "IS;");
		copyCandidateList(&saved_list, cargs.regions[region_index]->cand_list);

		// Create list of "illegal" candidates
		for (int i = 0; i < data_size; i++) {
			makeargv(voting_data[i], ":", &vote_data);
			struct Candidate c;
			strcpy(c.name, trimwhitespace(vote_data[0]));
			c.count = atoi(vote_data[1]);
			insertCandidate(&list_to_remove, c);
			if (removeCandidate(&(cargs.regions[region_index]->cand_list), c)) {
				valid++;
				strcat(buffer, c.name);
				strcat(buffer, ",");
			}
		}
		if (valid == 0) {	// If subtraction is valid, get remove votes
			deaggregateVotes(cargs.regions, cargs.regions[region_index]->parent,
				list_to_remove);
			strcpy(buffer, "SC;\0");
		} else {			// otherwise revert the candidate list back to normal
			deleteCandidateList(&(cargs.regions[region_index]->cand_list));
			copyCandidateList(&(cargs.regions[region_index]->cand_list), saved_list);
			buffer[strlen(buffer) - 1] = '\0';
		}
		deleteCandidateList(&list_to_remove);
		free(voting_data);
		free(vote_data);
	}
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	int s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Count the votes of a region specified by the client.
void* countVotesClient (void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;

	// Deciding what to send to client based on data given.
	printf("Request received from client at %s:%d, CV %s\n", cargs.ip, cargs.port_number, cargs.input_region);
	int region_index = findRegionIndex(cargs.input_region, cargs.regions, cargs.num_regions);
	char buffer[BUF_SIZE];
	if (region_index < 0) {
		sprintf(buffer, "NR;%s", cargs.input_region);
	} else {
		if (cargs.regions[region_index]->cand_list != NULL) {
			char* results = malloc(BUF_SIZE);
			mallocCheck(results);
			candidateListToString(cargs.regions[region_index]->cand_list, &results);
			sprintf(buffer, "SC;%s", results);
			free(results);
		} else {
			sprintf(buffer, "SC;No Votes");
		}
	}
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Return the winner of the election.
void* returnWinnerClient (void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;

	// Deciding what to send to client based on data given.
	printf("Request received from client at %s:%d, RW\n", cargs.ip, cargs.port_number);
	char buffer[BUF_SIZE];

	// Checking to see if any of the polls are open/closed.
	int poll_status = 2;
	for (int i = 0; i < *(cargs.num_regions) && poll_status == 2; i++) {
		if (cargs.regions[i]->poll_status != DONE)
			poll_status = cargs.regions[i]->poll_status;
	}
	if (poll_status == 1) {
		sprintf(buffer, "RO;");
	} else if (poll_status == 0) { sprintf(buffer, "RC;");
	} else {
		if (cargs.regions[0]->cand_list != NULL) {
			char* winner = malloc(MAX_CANDIDATE_NAME);
			findWinner(cargs.regions[0]->cand_list, &winner);
			sprintf(buffer, "SC;Winner:%s", winner);
			free(winner);
		} else {
			sprintf(buffer, "SC;No Votes");
		}
	}
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Add a region to the rgions array as a leaf node specified by the client.
void* addRegionClient(void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;

	printf("Request received from client at %s:%d, AR\n", cargs.ip, cargs.port_number);
	char buffer[BUF_SIZE];
	int region_index = findRegionIndex(cargs.input_region, cargs.regions, cargs.num_regions);
	if (region_index < 0)
		sprintf(buffer, "NR;%s", cargs.input_region);
	else {
		if (*(cargs.num_regions) < MAX_REGIONS) {
			cargs.regions[region_index]->children[*(cargs.regions[region_index]->num_children)] = *(cargs.num_regions);
			*(cargs.regions[region_index]->num_children) = *(cargs.regions[region_index]->num_children) + 1;
			strcpy(cargs.regions[*(cargs.num_regions)]->name, cargs.data);
			cargs.regions[*(cargs.num_regions)]->parent = region_index;
			*(cargs.num_regions) = *(cargs.num_regions) + 1;
			sprintf(buffer, "SC;%s;%s", cargs.input_region, cargs.data);
		} else {
			sprintf(buffer, "UE;Too many regions");
		}
	}

	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Function to send response to client if it gave the server an invalid command.
void* unhandledCommandClient(void* arg) {
	struct ClientArgs cargs = *((struct ClientArgs*) arg);
	int s;
	char buffer[BUF_SIZE];
	sprintf(buffer, "UC;%s", cargs.command);
	// Temp variables to hold received and formatted data.
	char buf[BUF_SIZE];
	char** codes = malloc(sizeof(char*) * 3);
	mallocCheck(codes);
	for (int i = 0; i < 3; i++) {
		codes[i] = malloc(sizeof(char) * BUF_SIZE);
		mallocCheck(codes[i]);
	}

	// Formatting response to client.
	int num_codes = makeargv(buffer, ";", &codes);
	sprintf(buf, "Sending response to client at %s:%d, ", cargs.ip, cargs.port_number);
	strcat(buf, codes[0]);
	for (int i = 1; i < num_codes; i++) {
		strcat(buf, " ");
		strcat(buf, codes[i]);
	}
	strcat(buf, "\0");
	printf("%s\n", buf);

	free(codes);
	s = send(cargs.client_fd, buffer, strlen(buffer) + 1, 0);
	socketCheck(s);
	return NULL;
}

// Main function to execute certain functions based on commands sent by the client.
void* threadFun (void* arg) {
	pthread_mutex_lock(&lock);
	struct ClientArgs cargs = *((struct ClientArgs*) arg);

	char buf[BUF_SIZE];
	int read_size;
	// Get everything the client sends.
	while(read_size = recv(cargs.client_fd, buf, sizeof(buf), 0) > 0) {
		buf[strlen(buf)] = '\0';
		char** data = malloc(sizeof(char*) * 3);
		for (int i = 0; i < 3; i++)
			data[i] = malloc(sizeof(char) * BUF_SIZE);
		makeargv(buf, ";", &data);
		sprintf(cargs.input_region, "%s", trimwhitespace(data[1]));

		// Deciding which function to run based on command received from client.
		if(strcmp(data[0], "OP") == 0) {	// Open polls
			openPollsClient(&cargs);
		} else if (strcmp(data[0], "CP") == 0) {	// Close polls
			closePollsClient(&cargs);
		} else if (strcmp(data[0], "AV") == 0) {	// Add votes
			sprintf(cargs.data, "%s", trimwhitespace(data[2]));
			addVotesClient(&cargs);
		} else if (strcmp(data[0], "RV") == 0) {	// Remove votes
			sprintf(cargs.data, "%s", trimwhitespace(data[2]));
			removeVotesClient(&cargs);
		} else if (strcmp(data[0], "CV") == 0) {	// Count votes
			countVotesClient(&cargs);
		} else if (strcmp(data[0], "RW") == 0) {	// Return winner
			returnWinnerClient(&cargs);
		} else if (strcmp(data[0], "AR") == 0) {
			sprintf(cargs.data, "%s", trimwhitespace(data[2]));
			addRegionClient(&cargs);
		} else {	// Unhandled command
			sprintf(cargs.command, "%s", trimwhitespace(data[0]));
			unhandledCommandClient((void*)&cargs);
		}
		free(data);
	}
	printf("Closed connection with client at %s:%d\n", cargs.ip, cargs.port_number);
	close(cargs.client_fd);
	pthread_mutex_unlock(&lock);
	return NULL;
}

#endif  // PA4_UTIL_H
