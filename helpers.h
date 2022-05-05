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

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define BUFLEN		2048	// dimensiunea maxima a calupului de date
#define COMMANDLEN	65		// dimensiunea maxima a unei comenzi de la tastatura
#define IDLEN	11

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stdout, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

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
#endif
