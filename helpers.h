#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netinet/tcp.h>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>



#define BUFLEN		2048	// dimensiunea maxima a calupului de date
#define COMMANDLEN	65		// dimensiunea maxima a unei comenzi de la tastatura
#define IDLEN	11 // Dimensiunea maxima a unui id

// Interpretare mesaje trimise de clientii UDP
typedef struct udp_message {
	char topic[50];
	uint8_t tip_date;
	char content[1500];
} udp_message;

// Interpretare mesaje trimise de server sau primite de la server
typedef struct server_message {
	struct sockaddr_in addr;
	udp_message message;
} server_message;

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stdout, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

// Primeste un mesaj intreg, pana cand numarul de octeti primiti este egal cu
// full_size
void recv_full_message(int sockfd, char *buffer, int curr_size, int full_size)
{
	int ret;
	while (curr_size != full_size)
	{
		ret = recv(sockfd, buffer + curr_size,
								full_size - curr_size, 0);
		DIE(ret < 0, "recv");
		curr_size += ret;
	}
}

// Functie folosita pentru a trimite un mesaj de eroare in cazul in care 
// un subscriber este rulat gresit
void usage(int argc, int number_of_arguments, char *file)
{
	if (argc < number_of_arguments) {
		fprintf(stderr, "Usage: %s ID SERVER_ADDRESS SERVER_PORT\n", file);
		exit(0);
	}
	
}
#endif
