#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"
#include <netinet/tcp.h>
#include <unordered_map>
#include <string>
#include <vector>
#include <sstream>

using namespace std;

typedef struct udp_message {
	char topic[50];
	uint8_t tip_date;
	char content[1500];
} udp_message;

class TCPClient {       // The class
  public:             // Access specifier
    int socket;        // Attribute (int variable)
    string id;  // Attribute (string variable)
	vector<string> topics;
};

class Topics {
	public:
		string topic;
		int sf;
};

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}
int max(int x, int y)
{
    if (x > y)
        return x;
    else
        return y;
}
int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockTCP, sockUDP, newSockTCP, newSockUDP, portno;
	char buffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen;

	fd_set tcp_fds;		// multimea de citire tcp folosita in select()
	fd_set udp_fds;		// multimea de citire tcp folosita in select()
	fd_set sel_sock;
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea tcp_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (tcp_fds) si multimea temporara (tmp_fds)
	FD_ZERO(&tcp_fds);
	FD_ZERO(&udp_fds);
	FD_ZERO(&tmp_fds);

	sockTCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	DIE(sockTCP < 0, "socket");
	DIE(sockUDP < 0, "socket");
	int flag = 1;	
	int result = setsockopt(sockTCP,            /* socket affected */
                        IPPROTO_TCP,     /* set option at TCP level */
                        TCP_NODELAY,     /* name of option */
                        (char *) &flag,  /* the cast is historical cruft */
                        sizeof(int));    /* length of option value */

	DIE(result < 0, "TCP NODELAY");
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockTCP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");
	ret = listen(sockTCP, MAX_CLIENTS);
	DIE(ret < 0, "listen");


	ret = bind(sockUDP, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");



	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) in multimea read_fds
	FD_SET(sockTCP, &tcp_fds);
	FD_SET(sockUDP, &udp_fds);

	FD_SET(sockTCP, &sel_sock);
	FD_SET(sockUDP, &sel_sock);
	FD_SET(STDIN_FILENO, &sel_sock);

	fdmax = max(sockUDP, sockTCP);
	fdmax = max(fdmax, STDIN_FILENO);
	unordered_map<string, int> id_sock;
	unordered_map<int, string> sock_id;
	unordered_map<string, vector<Topics> > topics_socks;
	while (1) {
		tmp_fds = sel_sock; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		unordered_map<string, vector<Topics> >:: iterator it;
		for (it = topics_socks.begin(); it != topics_socks.end(); it++)
		{
			vector<Topics>::iterator x;
			for (x = it->second.begin(); x != it->second.end(); x++)
			{
				// printf("%s\n", x->topic.c_str());
			}
			
		}
		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			DIE(n < 0, "read");
			if (strncmp(buffer, "exit", 4) == 0) {
				unordered_map<string, int>:: iterator itr;
				for (itr = id_sock.begin(); itr != id_sock.end(); itr++)
				{
					// printf("closing socket %d\n", itr->second);
					close(itr->second);
				}
				
				break;
			}
			FD_CLR(STDIN_FILENO, &tmp_fds);

		}
		if (FD_ISSET(sockTCP, &tmp_fds)) {
			// a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
			// pe care serverul o accepta
			clilen = sizeof(cli_addr);
			newSockTCP = accept(sockTCP, (struct sockaddr *) &cli_addr, &clilen);
			DIE(newSockTCP < 0, "accept");
			int flag = 1;	
			int result = setsockopt(newSockTCP,            /* socket affected */
								IPPROTO_TCP,     /* set option at TCP level */
								TCP_NODELAY,     /* name of option */
								(char *) &flag,  /* the cast is historical cruft */
								sizeof(int));    /* length of option value */
			DIE(result < 0, "TCP NODELAY");
			memset(buffer, 0, sizeof(buffer));
			n = recvfrom(newSockTCP, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
			if (id_sock.find(buffer) != id_sock.end())
			{
				printf("Client %s already connected\n", buffer);
				FD_CLR(sockTCP, &tmp_fds);
				close(newSockTCP);
				continue;
			}
			
			id_sock[buffer] = newSockTCP;
			sock_id[newSockTCP] = buffer;
			// se adauga noul socket intors de accept() la multimea descriptorilor de citire
			FD_SET(newSockTCP, &sel_sock);
			if (newSockTCP > fdmax) { 
				fdmax = newSockTCP;
			}
			// printf("accepted\n");
			char id[10];
			strncpy(id, buffer, 10);
			printf("New client %s connected from %s:%d\n", buffer,
					inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
			FD_CLR(sockTCP, &tmp_fds);
		}
		if (FD_ISSET(sockUDP, &tmp_fds)) {
			// printf("UDP\n");
			// a venit o cerere de conexiune pe socketul UDP,
			// pe care serverul o accepta
			clilen = sizeof(cli_addr);
			memset(buffer, 0, BUFLEN);
			n = recvfrom(sockUDP, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
			DIE(n <= 0, "recvfrom");
			buffer[n] = '\0';
			// printf("Client udp: %s\n", buffer); 
			FD_CLR(sockUDP, &tmp_fds);
		}  
		for (i = 0; i <= fdmax + 1; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				// s-au primit date pe unul din socketii de client,
				// asa ca serverul trebuie sa le receptioneze
				memset(buffer, 0, BUFLEN);
				n = recv(i, buffer, sizeof(buffer) - 1, 0);
				DIE(n < 0, "recv");

				if (n == 0) {
					// conexiunea s-a inchis
					printf("Client %s disconnected\n", sock_id[i].c_str());
					
					// se scoate din multimea de citire socketul inchis 
					FD_CLR(i, &sel_sock);
					close(i);
				} else {
					
					
					// printf ("S-a primit de la clientul de pe socketul %d mesajul: %s\n", i, buffer);
					string str(buffer);
					istringstream message(str);
					string word;
					string client = sock_id[i];
					if (message >> word)
					{
						/* take first word, subscribe/ unsccribe */
						// printf("right message\n");
						// string command = message.substr(0, message.find(" "));
						// printf("word %s\n", word.c_str());
						if (word.compare("subscribe") == 0)
						{
							if(message >> word) {
								string topicToSubscribeTo = word;
								if(message >> word) {
									// printf("word topic %s\n", topicToSubscribeTo.c_str());

									// printf("word flag %s\n", word.c_str());
									Topics topic;
									topic.sf = stoi(word);
									topic.topic = topicToSubscribeTo;
									topics_socks[client].push_back(topic);
									// printf("verif %s\n", topics_socks[std::to_string(i)][0].topic.c_str());
								}
							}
						}

						if (word.compare("unsubscribe") == 0)
						{
							if(message >> word) {
								// printf("word topic %s\n", word.c_str());
								vector<Topics>::iterator x;
								for (x = topics_socks[std::to_string(i)].begin(); x != topics_socks[std::to_string(i)].end(); x++)
								{
									if (x->topic.compare(word) == 0)
									{
										break;
									}
									
								}
								topics_socks[client].erase(x);
							}
						}
						
					}
					
					// Lab 8
					// char c = buffer[0];
					// int client = atoi(&c);
					// printf("Client is %d\n", client);
					// n = send(sockets[client - 1], buffer, sizeof(buffer) - 1, MSG_NOSIGNAL);
					// DIE(n < 0, "send reply");
					
				}
				// }
			}
		}
	}

	close(sockTCP);

	return 0;
}
