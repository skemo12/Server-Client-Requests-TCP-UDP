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

#define BUFLEN 1572	  // dimensiunea maxima a calupului de date
#define IDLEN 10	  // Dimensiunea maxima a unui id

// Interpretare mesaje trimise de clientii UDP
typedef struct udp_message
{
	char topic[50];		// Topicul unui mesaj
	uint8_t type;		// Tipul de date ale unui mesaj
	char content[1500]; // content-ul mesajului
} udp_message;

// Interpretare mesaje trimise de server sau primite de la server
typedef struct server_message
{
	uint32_t number_of_bytes; // Numarul de octeti ce trebuiesc primiti/trimisi
	struct sockaddr_in addr;  // Adresa sender-ului inital (UDP client)
	udp_message message;	  // Continutul mesajului UDP
} server_message;

typedef struct command
{
	uint32_t number_of_bytes; // Numarul de octeti ce trebuiesc primiti/trimisi
	char message[65];		  // Comanda ce trebuie interpretata
} command;

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */
#define DIE(assertion, call_description)  \
	do                                    \
	{                                     \
		if (assertion)                    \
		{                                 \
			fprintf(stdout, "(%s, %d): ", \
					__FILE__, __LINE__);  \
			perror(call_description);     \
			exit(EXIT_FAILURE);           \
		}                                 \
	} while (0)

// Primeste un mesaj intreg, pana cand numarul de octeti primiti este egal cu
// message->number_of_bytes. De asemeanea, functia se asigura ca s-au primit
// numarul de bytes ce trebuiesc primiti.
void recv_full_message(int sockfd, char *buffer, int curr_size)
{
	int ret = 0;
	while (curr_size < 4)
	{
		ret = recv(sockfd, buffer + curr_size, 4 - curr_size, 0);
		DIE(ret < 0, "recv");
		curr_size += ret;
	}

	server_message *message = (server_message *)buffer;
	while (curr_size != message->number_of_bytes)
	{
		ret = recv(sockfd, buffer + curr_size,
				   message->number_of_bytes - curr_size, 0);
		DIE(ret < 0, "recv");
		curr_size += ret;
	}
}

// Functie folosita pentru a trimite un mesaj de eroare in cazul in care
// un subscriber este rulat gresit
void usage(int argc, int number_of_arguments, char *file)
{
	if (argc < number_of_arguments)
	{
		fprintf(stderr, "Usage: %s ID SERVER_ADDRESS SERVER_PORT\n", file);
		exit(0);
	}
}

// Functia ce returneaza numarul de octeti necesar in functie de tipul de mesaj
// ce trimis
uint32_t bytes_needed(uint8_t type, char *content)
{
	uint32_t total_bytes = 4 + sizeof(struct sockaddr_in) + 50 + 1;
	switch (type)
	{
	case 0:
		total_bytes += 5;
		break;
	case 1:
		total_bytes += 2;
		break;
	case 2:
		total_bytes += 6;
		break;
	case 3:
		total_bytes += strlen(content);
		break;
	default:
		break;
	}
	return total_bytes;
}
#endif
