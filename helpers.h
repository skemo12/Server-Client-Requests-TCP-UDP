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
#include <unordered_map>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

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

#define BUFLEN		2048	// dimensiunea maxima a calupului de date
#define COMMANDLEN	65		// dimensiunea maxima a unei comenzi de la tastatura
#define IDLEN	11

void receive_complete_message(int sockfd, char *buffer,
									int current_size, int message_size)
{
	while (current_size != message_size)
	{
		current_size += recv(sockfd, buffer + current_size,
								message_size - current_size, 0);
		DIE(current_size < 0, "recv");
	}
}
#endif
