#include "helpers.h"
using namespace std;

typedef struct udp_message {
	char topic[50];
	uint8_t tip_date;
	char content[1501];
} udp_message;

typedef struct message_for_tcp {
	struct sockaddr_in addr;
	udp_message message;
} message_for_tcp;

typedef struct save_packets {
	message_for_tcp message;
	string id;
} save_packets;

class TCPClient {
	public:            
		int socket;
		bool connected;
		string id;
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

void send_saved_packets(vector<save_packets> *point_packets, char *id, int sock)
{
	if ((*point_packets).begin() != (*point_packets).end())
	{	
		for (auto packet : (*point_packets))
		{
			if (packet.id.compare(id) == 0)
			{
				int n = send(sock, &packet, sizeof(message_for_tcp), 0);
				DIE(n < 0, "send");
			}
			
		}
		
	}
	(*point_packets).erase(
		remove_if(
			(*point_packets).begin(),
			(*point_packets).end(),
			[&id](save_packets i) { return i.id.compare(id) == 0; }),
	(*point_packets).end());
}

bool id_unique(unordered_map<string, int> *id_sock, int new_sock, char *id)
{
	if ((*id_sock).find(id) != (*id_sock).end())
	{
		printf("Client %s already connected.\n", id);
		close(new_sock);
		return false;
	}
	return true;
}

void save_client(unordered_map<string, int> *id_sock, int new_sock, char *id,
			int *fdmax, fd_set *sel_sock, unordered_map<int, string> *sock_id) 
{
		(*id_sock)[id] = new_sock;
		(*sock_id)[new_sock] = id;
		FD_SET(new_sock, sel_sock);
		if (new_sock > *fdmax) { 
			*fdmax = new_sock;
		}
}

void print_client_connected(char *id, struct sockaddr_in cli_addr)
{
	printf("New client %s connected from %s:%d\n", id,
			inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
}

void execute_command(char *command, unordered_map<int, string> *sock_id,
				unordered_map<string, vector<Topics> > *id_topics, int socket)
{
	istringstream command_spliter(command);
	string word;
	string client = (*sock_id)[socket];
	if (command_spliter >> word)
	{
		/* take first word, subscribe/ unsccribe */
		// printf("right message\n");
		// string command = message.substr(0, message.find(" "));
		// printf("word %s\n", word.c_str());
		if (word.compare("subscribe") == 0)
		{
			if(command_spliter >> word) {
				string topicToSubscribeTo = word;
				if(command_spliter >> word) {
					// printf("word topic %s\n", topicToSubscribeTo.c_str());

					// printf("word flag %s\n", word.c_str());
					Topics topic;
					topic.sf = stoi(word);
					topic.topic = topicToSubscribeTo;
					(*id_topics)[client].push_back(topic);
					// printf("verif %s\n", topics_socks[std::to_string(i)][0].topic.c_str());
				}
			}
		}

		if (word.compare("unsubscribe") == 0)
		{
			if(command_spliter >> word) {
				// printf("word topic %s\n", word.c_str());
				vector<Topics>::iterator topic_iterator;
				for (topic_iterator = (*id_topics)[client].begin(); 
					topic_iterator != (*id_topics)[client].end();
					topic_iterator++)
				{
					if (topic_iterator->topic.compare(word) == 0)
					{
						break;
					}
					
				}
				if (topic_iterator != (*id_topics)[client].end())
				{
					(*id_topics)[client].erase(topic_iterator);
				}
				
			}
			else 
			{
				fprintf(stderr, "No topic given for unsubscribe.\n");
			}
		}
		
	}
}

void client_stop(unordered_map<string, int> *id_sock,
			unordered_map<int, string> *sock_id, int socket, fd_set *sockets)
{
	printf("Client %s disconnected.\n", (*sock_id)[socket].c_str());
	
	// se scoate din multimea de citire socketul inchis 
	FD_CLR(socket, sockets);
	close(socket);
	
	(*id_sock).erase((*sock_id)[socket]);
	(*sock_id).erase(socket);
}

void send_udp_message_to_clients(char *buffer,
							unordered_map<string, vector<Topics> > *id_topics,
							unordered_map<string, int> (*id_sock), 
							struct sockaddr_in cli_addr,
							vector<save_packets> *saved_packets)
{
	int ret;
	udp_message *message = (udp_message *) buffer;
	
	message_for_tcp toSend;
	toSend.message = *message;
	memset(&toSend.addr, 0, sizeof(toSend.addr));
	toSend.addr.sin_family = AF_INET;
	toSend.addr = cli_addr;
	// printf("Sender %s port %d\n", inet_ntoa(toSend.addr.sin_addr), toSend.addr.sin_port);
	for (auto c_topics : (*id_topics))
	{
		auto topic = find_if(c_topics.second.begin(), c_topics.second.end(),
			[&message] (Topics el){
					return el.topic.compare(message->topic) == 0;
				});
	
		if (topic != c_topics.second.end())
		{
			if ((*id_sock).find(c_topics.first) != (*id_sock).end())
			{
				// printf("founded!%d\n", counter++);
				ret = send((*id_sock)[c_topics.first], &toSend,
							sizeof(message_for_tcp), 0);
				DIE(ret < 0, "send");
			}
			else
			{
				if (topic->sf == 1)
				{
					save_packets packet;
					packet.id = c_topics.first;
					packet.message = toSend;
					(*saved_packets).push_back(packet);
				}
				
			}

			

		}
		
	}

	
}
int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sockTCP, sockUDP, new_sock, portno;
	char buffer[BUFLEN], command[COMMANDLEN];

	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret;
	socklen_t clilen = sizeof(cli_addr);
	socklen_t sockaddr_len = sizeof(struct sockaddr);

	// Structuri de date pentru salvarea datelor folosite
	// Asociere id -> socket (sunt pastrati doar clientii care sunt conectati)
	unordered_map<string, int> id_sock;
	// Asociere socket -> id (sunt pastrati doar clientii care sunt conectati)
	unordered_map<int, string> sock_id;
	// Asociere id -> lista de topicuri la care clientul ID este abonat
	unordered_map<string, vector<Topics> > id_topics;
	// Lista de pachete reconectare
	vector<save_packets> saved_packets;

	// Initializare multimi de socketi
	fd_set sel_sock;
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea tcp_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (tcp_fds)
	// si multimea temporara (tmp_fds)
	FD_ZERO(&sel_sock);
	FD_ZERO(&tmp_fds);

	sockTCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	DIE(sockTCP < 0, "socket");
	DIE(sockUDP < 0, "socket");
	int flag = 1;	
	int result = setsockopt(sockTCP, IPPROTO_TCP, TCP_NODELAY,     
                        (char *) &flag, sizeof(int));    

	DIE(result < 0, "TCP_NODELAY");
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockTCP, (struct sockaddr *) &serv_addr, sockaddr_len);
	DIE(ret < 0, "bind");
	ret = listen(sockTCP, 1);
	DIE(ret < 0, "listen");

	ret = bind(sockUDP, (struct sockaddr *) &serv_addr, sockaddr_len);
	DIE(ret < 0, "bind");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) 
	// in multimea sel_sock. De asemenea se adauga si input de la stdin si cel
	// de sockudp
	FD_SET(sockTCP, &sel_sock);
	FD_SET(sockUDP, &sel_sock);
	FD_SET(STDIN_FILENO, &sel_sock);

	// Socket-ul maxim
	fdmax = max(sockUDP, sockTCP);
	fdmax = max(fdmax, STDIN_FILENO);

	while (1) {
		tmp_fds = sel_sock; 
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds))
		{
			memset(buffer, 0, sizeof(buffer));
			n = read(0, buffer, sizeof(buffer) - 1);
			DIE(n < 0, "read");
			if (strncmp(buffer, "exit", 4) == 0) {
				for (auto client_socket : id_sock)
				{
					close(client_socket.second);
				}
				break;
			}
			FD_CLR(STDIN_FILENO, &tmp_fds);

		}
		if (FD_ISSET(sockTCP, &tmp_fds)) {
			// a venit o cerere de conexiune, pe care serverul o accepta
			clilen = sizeof(cli_addr);
			new_sock = accept(sockTCP, (struct sockaddr *) &cli_addr, &clilen);
			DIE(new_sock < 0, "accept");
			int flag = 1;	
			int result = setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY,     
								(char *) &flag, sizeof(int));    
			DIE(result < 0, "TCP_NODELAY");

			memset(buffer, 0, sizeof(buffer));
			n = recv(new_sock, buffer, IDLEN, 0);
			if (id_unique(&id_sock, new_sock, buffer))
			{
				save_client(&id_sock, new_sock, buffer, &fdmax,
							&sel_sock, &sock_id);
				print_client_connected(buffer, cli_addr);
				send_saved_packets(&saved_packets, buffer, new_sock);	
			}
			
			FD_CLR(sockTCP, &tmp_fds);
		}
		if (FD_ISSET(sockUDP, &tmp_fds)) 
		{
			memset(buffer, 0, BUFLEN);
			memset(&cli_addr, 0, sizeof(cli_addr));
			clilen = sizeof(struct sockaddr);
			n = recvfrom(sockUDP, buffer, sizeof(buffer), 0,
					(struct sockaddr *) &cli_addr, &clilen);
			DIE(n < 0, "recvfrom");
			send_udp_message_to_clients(buffer, &id_topics, &id_sock,
										cli_addr, &saved_packets);
			FD_CLR(sockUDP, &tmp_fds);
		}  
		for (i = 0; i <= fdmax + 1; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				// s-au primit date pe unul din socketii de client,
				// asa ca serverul trebuie sa le receptioneze
				memset(command, 0, COMMANDLEN);
				n = recv(i, command, COMMANDLEN, 0);
				DIE(n < 0, "recv");

				if (n == 0) {
					// conexiunea s-a inchis
					client_stop(&id_sock, &sock_id, i, &sel_sock);
					
				} else {
					// printf("S-a primit de la clientul de pe socketul %d mesajul: lungime %d %s\n", i, n, command);
					receive_complete_message(i, command, n, COMMANDLEN);
					execute_command(command, &sock_id, &id_topics, i);
					
				}
			}
		}
	}

	close(sockTCP);
	close(sockUDP);

	return 0;
}
