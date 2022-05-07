#include "helpers.h"
using namespace std;

// Verifica ca flag-ul pt SF este unul valid
bool checkValidSF(string word)
{
	return (word == "1" || word == "0");
}

// Printeaza feedback-ul pentru o comanda
void printCommand(char *command)
{
	istringstream parseCommand(command);
	string word;
	if (parseCommand >> word && word == "subscribe")
	{
		printf("Subscribed to topic.\n");
	}
	else
	{
		printf("Unsubscribed from topic.\n");
	}
}

// Verifica ca o comanda de la tastatura este valida
bool validCommand(char *command)
{
	istringstream parseCommand(command);
	string word;
	if (parseCommand >> word && word == "subscribe")
	{
		// verificam lungimea comenzii sa fie egala cu 3
		if (parseCommand >> word && parseCommand >> word && checkValidSF(word))
		{
			return true;
		}
	}
	parseCommand.clear();
	parseCommand.str(command);
	if (parseCommand >> word && word == "unsubscribe" && parseCommand >> word)
	{
		return true;
	}

	return false;
}

// Afiseaza content-ul conform codificarii de INT
void print_content_int(char *content)
{
	printf("INT - ");
	uint32_t *value = (uint32_t *)(content + 1);
	uint8_t sign = content[0];
	if (sign == 1)
	{
		printf("-%d\n", ntohl(*value));
	}
	else
	{
		printf("%d\n", ntohl(*value));
	}
}

// Afiseaza content-ul conform codificarii de SHORT_REAL
void print_content_short_real(char *content)
{
	printf("SHORT_REAL - ");
	uint16_t *value = (uint16_t *)(content);
	if (ntohs(*value) % 100 == 0)
	{
		printf("%d\n", ntohs(*value) / 100);
	}
	else
	{
		printf("%d.%02d\n", ntohs(*value) / 100, ntohs(*value) % 100);
	}
}

// Afiseaza content-ul conform codificarii de FLOAT
void print_content_float(char *content)
{
	printf("FLOAT - ");

	uint32_t *value = (uint32_t *)(content + 1);
	uint8_t sign = content[0];
	uint8_t *power = (uint8_t *)(content + 1 + sizeof(uint32_t));
	int divider = pow(10, *power);

	if (sign == 1)
	{
		if (ntohl(*value) % divider == 0)
		{
			printf("-%d\n", ntohl(*value) / divider);
		}
		else
		{
			printf("-%d.%0*d\n", ntohl(*value) / divider,
				   *power, ntohl(*value) % divider);
		}
	}
	else
	{
		if (ntohl(*value) % divider == 0)
		{
			printf("%d\n", ntohl(*value) / divider);
		}
		else
		{
			printf("%d.%0*d\n", ntohl(*value) / divider,
				   *power, ntohl(*value) % divider);
		}
	}
}

// Afiseaza content-ul conform codificarii de FLOAT
void print_content_string(char *content)
{
	printf("STRING - ");
	printf("%s\n", content);
}

// Interpreteaza un mesaj de la server
void parse_server_message(char *buffer)
{
	server_message *ser_mess = (server_message *)buffer;
	printf("%s:%d - %s - ", inet_ntoa(ser_mess->addr.sin_addr),
		   ntohs(ser_mess->addr.sin_port), ser_mess->message.topic);

	switch (ser_mess->message.type)
	{
	case 0:
	{
		print_content_int(ser_mess->message.content);
		break;
	}
	case 1:
	{
		print_content_short_real(ser_mess->message.content);
		break;
	}
	case 2:
	{
		print_content_float(ser_mess->message.content);
		break;
	}
	case 3:
	{
		print_content_string(ser_mess->message.content);
		break;
	}
	default:
	{
		fprintf(stderr, "Not valid data type.\n");
		break;
	}
	}
}

int main(int argc, char *argv[])
{
	// Declarari de variabile si setarea STDOUT-ului
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	// Variabile folosite
	int sockfd, n, ret, fdmax, portno;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	command command_parser;

	// Verificam rularea corecta
	usage(argc, 4, argv[0]);

	// Pregatim argumentele subscriberul
	portno = atoi(argv[3]);
	DIE(portno < 0, "atoi");
	// Multimea de sockets folosita si o copie a acesteia, golita
	fd_set read_fds, copy_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&copy_fds);

	// Creeare socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");
	// Dezactivare Nagle
	int flag = 1;
	int result = setsockopt(sockfd, IPPROTO_TCP,
							TCP_NODELAY, (char *)&flag, sizeof(int));
	DIE(result < 0, "TCP_NODELAY");

	// Introducem cei 2 socketi folositi de client in multimea de socketi
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	// Pregatim datele serverului si incercam conectarea la server
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");
	ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
	// Trimitem ID-ul clientului la server

	command_parser.number_of_bytes = strlen(argv[1]) + 1 + 4;
	strcpy(command_parser.message, argv[1]);
	n = send(sockfd, &command_parser, command_parser.number_of_bytes, 0);
	DIE(n < 0, "send");

	// Maxim dintre socket-ul tcp si stdin (= 0) este sigur socket-ul tcp
	fdmax = sockfd;

	while (1)
	{
		// Recopiem multimea de socketi, si asteptam un mesaj
		copy_fds = read_fds;
		ret = select(fdmax + 1, &copy_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		// Primim o comanda de la tastatura
		if (FD_ISSET(STDIN_FILENO, &copy_fds))
		{
			// Citim de la tastatura o comanda
			memset(buffer, 0, sizeof(command));
			n = read(STDIN_FILENO, buffer, sizeof(command));
			DIE(n < 0, "read");

			// Daca comanda este exit, iesim din bucla si inchidem clientul
			if (strncmp(buffer, "exit", 4) == 0)
			{
				break;
			}

			// Parsam comanda primita si verificam daca este valida
			if (validCommand(buffer))
			{
				command com;
				com.number_of_bytes = strlen(buffer) + 1 + 4;
				strcpy(com.message, buffer);
				n = send(sockfd, &com, com.number_of_bytes, 0);
				DIE(n < 0, "send");
				// Afisam feedback-ul pentru comanda
				printCommand(buffer);
			}
			else
				fprintf(stderr, "Not a valid input command.\n");

			FD_CLR(STDIN_FILENO, &copy_fds);
		}
		// Am primit un mesaj de la server
		if (FD_ISSET(sockfd, &copy_fds))
		{
			memset(buffer, 0, sizeof(buffer));
			n = recv(sockfd, buffer, sizeof(uint32_t), 0);
			DIE(n < 0, "recv");
			// Daca n == 0, conexiunea cu serverul a fost inchisa, deci inchidem
			// si clientul
			if (n == 0)
			{
				break;
			}

			// Ne asiguram ca primim exact un mesaj intreg de la server
			recv_full_message(sockfd, buffer, n);
			// Verificam mesajul de la server, si afisam in functie de tip.
			parse_server_message(buffer);
			FD_CLR(sockfd, &copy_fds);
		}
	}
	close(sockfd);
	return 0;
}
