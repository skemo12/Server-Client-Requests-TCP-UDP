#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#include <netinet/tcp.h>

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iomanip>

typedef struct udp_message {
	char topic[50];
	uint8_t tip_date;
	char content[1500];
} udp_message;

typedef struct server_message {
	struct sockaddr_in addr;
	udp_message message;
} server_message;

void usage(char *file)
{
	fprintf(stderr, "Usage: %s id_client server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	char command[65];

	if (argc < 3) {
		usage(argv[0]);
	}

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");
	int flag = 1;	
	int result = setsockopt(sockfd,            /* socket affected */
                        IPPROTO_TCP,     /* set option at TCP level */
                        TCP_NODELAY,     /* name of option */
                        (char *) &flag,  /* the cast is historical cruft */
                        sizeof(int));    /* length of option value */
	DIE(result < 0, "TCP NODELAY");
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	n = send(sockfd, argv[1], strlen(argv[1]), MSG_NOSIGNAL);
	DIE(n < 0, "send");
	fdmax = sockfd;
	while (1) {
  		// se citeste de la stdin
		tmp_fds = read_fds; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			memset(command, 0, sizeof(command));
			n = read(STDIN_FILENO, command, sizeof(command));
			DIE(n < 0, "read");

			if (strncmp(command, "exit", 4) == 0) {
				break;
			}

			// se trimite mesaj la server
			// printf("message %s printf size %d\n", buffer, strlen(buffer));
			n = send(sockfd, command, sizeof(command), 0);
			DIE(n < 0, "send");
			if (command[0] == 's')
			{
				printf("Subscribed to topic.\n");
			}

			if (command[0] == 'u')
			{
				printf("Unsubscribed from topic.\n");
			}
			FD_CLR(STDIN_FILENO, &tmp_fds);
		}
		
		if (FD_ISSET(sockfd, &tmp_fds))
		{
			memset(buffer, 0, sizeof(buffer));
			n = recv(sockfd, buffer, sizeof(server_message), 0);
			DIE(n < 0, "recv");
			if (n == 0)
			{
				break;
			}
			int len = n;
			while (len != sizeof(server_message))
			{
				n = recv(sockfd, buffer + len, sizeof(server_message) - len, 0);
				len += n;
			}
			server_message *ser_mess = (server_message *) buffer;
			printf("%s:%d - %s - ", inet_ntoa(ser_mess->addr.sin_addr),
					ntohs(ser_mess->addr.sin_port), ser_mess->message.topic);
			udp_message *message = (udp_message *) &ser_mess->message;
			if (message->tip_date == 0)
			{
				printf("INT - ");
				uint32_t *test = (uint32_t *) (message->content + 1);
				uint8_t sign = message->content[0];
				if (sign == 1)
				{
					printf("-%d\n", ntohl(*test));
				} else 
				{
					printf("%d\n", ntohl(*test));
				}
				
			}

			if (message->tip_date == 1)
			{
				printf("SHORT_REAL - ");
				uint16_t *test = (uint16_t *) (message->content);
				if (ntohs(*test) % 100 == 0)
				{
					printf("%d\n", ntohs(*test) / 100);
				}
				else 
				{
					printf("%d.%02d\n", ntohs(*test) / 100, ntohs(*test) % 100);
				}
			}

			if (message->tip_date == 2)
			{
				printf("FLOAT - ");

				uint32_t *test = (uint32_t *) (message->content + 1);
				uint8_t sign = message->content[0];

				uint8_t *power = (uint8_t *) (message->content + 1 + sizeof(uint32_t));
				int divider = pow(10, *power);
				if (sign == 1)
				{
					if (ntohl(*test) % divider == 0)
					{
						printf("-%d\n", ntohl(*test) / divider);
					}
					else 
					{
						printf("-%d.%0*d\n", ntohl(*test) / divider, *power, ntohl(*test) % divider);
					}
				} else 
				{
					if (ntohl(*test) % divider == 0)
					{
						printf("%d\n", ntohl(*test) / divider);
					}
					else 
					{
						printf("%d.%0*d\n", ntohl(*test) / divider, *power, ntohl(*test) % divider);
					}
				}
			}
			if (message->tip_date == 3)
			{
				printf("STRING - ");
				printf("%s\n", message->content);

			}
			FD_CLR(sockfd, &tmp_fds);

		}



	}

	close(sockfd);

	return 0;
}
