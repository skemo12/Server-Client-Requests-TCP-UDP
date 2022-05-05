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

typedef struct save_packet {
	message_for_tcp message;
	string id;
} save_packet;

class Topic {
	public:
		string topic;
		int sf;
};

class Subscriber {
	public:            
		int socket;
		bool connected;
		string id;
		vector<Topic> topics;
		vector<save_packet> point_packets;

		void print_connect(struct sockaddr_in cli_addr)
		{
			printf("New client %s connected from %s:%d\n", id.c_str(),
					inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
		}

		void print_disconnect() 
		{
			printf("Client %s disconnected.\n", id.c_str());
		}

		void send_saved_packets()
		{
			int n;
			for (auto packet : point_packets)
			{
				n = send(socket, &packet, sizeof(message_for_tcp), 0);
				DIE(n < 0, "send");
			}
			point_packets.clear();
		}

		void execute_command(char *command)
		{
			istringstream command_spliter(command);
			string word;
			if (command_spliter >> word)
			{
				/* take first word, subscribe/ unsccribe */
				// printf("right message\n");
				// string command = message.substr(0, message.find(" "));
				// printf("word %s\n", word.c_str());
				if (word.compare("subscribe") == 0)
				{
					string name;
					if(command_spliter >> name && command_spliter >> word) {

						// printf("word topic %s\n", name.c_str());

						// printf("word flag %s\n", word.c_str());
						Topic topic;
						topic.sf = stoi(word);
						topic.topic = name;
						topics.push_back(topic);
						
					}
					else 
					{
						fprintf(stderr, "Not a valid subscribe command.\n");
					}
				}

				if (word.compare("unsubscribe") == 0)
				{
					if(command_spliter >> word) {
						// printf("word topic %s\n", word.c_str());
						remove_if(topics.begin(), topics.end(),
						[&word] (Topic topic) {return topic.topic == word;});
					}
					else 
					{
						fprintf(stderr, "No topic given for unsubscribe.\n");
					}
				}
				
			}
		}

		void client_stop(fd_set *sockets)
		{	
			// se scoate din multimea de citire socketul inchis 
			FD_CLR(socket, sockets);
			int ret = close(socket);
			DIE(ret < 0, "close");
			connected = false;
			print_disconnect();
		}


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

bool id_unique(map<int, Subscriber> *clients, int new_sock, char *id)
{
	auto found = find_if((*clients).begin(), (*clients).end(), 
			[id] (pair<const int, Subscriber> client) 
			{return client.second.id.compare(id) == 0;});
	
	if (found != clients->end() && found->second.connected)
	{
		printf("Client %s already connected.\n", id);
		int ret = close(new_sock);
		DIE(ret < 0, "close");
		return false;
	}

	return true;
}

void save_client(map<int, Subscriber> *clients, int new_sock, char *id,
			int *fdmax, fd_set *sel_sock) 
{
	
	Subscriber new_client;
	new_client.connected = true;
	new_client.id.append(id);
	new_client.socket = new_sock;

	auto old_client = find_if((*clients).begin(), (*clients).end(), 
	[id] (pair<const int, Subscriber> client) 
	{return client.second.id.compare(id) == 0;});

	if (old_client != (*clients).end())
	{
		new_client.topics = old_client->second.topics;
		new_client.point_packets = old_client->second.point_packets;
		(*clients).erase(old_client);
	}

	
	(*clients)[new_sock] = new_client;
	FD_SET(new_sock, sel_sock);
	if (new_sock > *fdmax) { 
		*fdmax = new_sock;
	}
}

void parse_udp_message(char *buffer, map<int, Subscriber> *clients, 
									struct sockaddr_in cli_addr)
{
	int ret;
	udp_message *message = (udp_message *) buffer;
	
	message_for_tcp toSend;
	toSend.message = *message;
	memset(&toSend.addr, 0, sizeof(toSend.addr));
	toSend.addr.sin_family = AF_INET;
	toSend.addr = cli_addr;
	// printf("Sender %s port %d\n", inet_ntoa(toSend.addr.sin_addr), toSend.addr.sin_port);
	for (auto client = clients->begin(); client != clients->end(); client++)
	{
		auto topic = find_if(client->second.topics.begin(),
							client->second.topics.end(),
			[&message] (Topic el){
					return el.topic.compare(message->topic) == 0;
				});
	
		if (topic != client->second.topics.end())
		{
			if (client->second.connected)
			{
				// printf("founded!\n");
				ret = send(client->second.socket, &toSend,
							sizeof(message_for_tcp), 0);
				DIE(ret < 0, "send");
			}
			else
			{
				if (topic->sf == 1)
				{
					save_packet packet;
					packet.id = client->second.id;
					packet.message = toSend;
					client->second.point_packets.push_back(packet);
				}
				
			}

		}
		
	}

}

bool check_exit_signal(char *buffer, map<int, Subscriber> *clients) 
{
	int ret;
	if (strncmp(buffer, "exit", 4) == 0) {
		for (auto client : *clients)
		{
			if (client.second.connected)
			{
				ret = close(client.second.socket);
				DIE(ret < 0, "close");
			}
			
		}
		return true;
	}
	return false;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sock_TCP, sock_UDP, new_sock, portno, i, ret;
	char buffer[BUFLEN], command[COMMANDLEN];

	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen = sizeof(cli_addr);
	socklen_t sockaddr_len = sizeof(struct sockaddr);
	// Hashmap-ul folosit pentru stocarea clientilor socket -> Client
	map<int, Subscriber> clients;

	// Initializare multimi de socketi, una fiind o copie
	fd_set sel_sock, copy_fds;
	int fdmax;			// valoare maxima fd din multimea tcp_fds

	if (argc < 2) {
		usage(argv[0]);
	}

	// se goleste multimea de descriptori de citire (tcp_fds) si copia
	FD_ZERO(&sel_sock);
	FD_ZERO(&copy_fds);

	// Creeare celor 2 socketi TCP si UDP
	sock_TCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sock_UDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	DIE(sock_TCP < 0, "socket");
	DIE(sock_UDP < 0, "socket");
	int flag = 1;	
	int result = setsockopt(sock_TCP, IPPROTO_TCP, TCP_NODELAY,     
                        (char *) &flag, sizeof(int));    

	DIE(result < 0, "TCP_NODELAY");
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sock_TCP, (struct sockaddr *) &serv_addr, sockaddr_len);
	DIE(ret < 0, "bind");
	ret = listen(sock_TCP, 1);
	DIE(ret < 0, "listen");

	ret = bind(sock_UDP, (struct sockaddr *) &serv_addr, sockaddr_len);
	DIE(ret < 0, "bind");

	// se adauga noul file descriptor (socketul pe care se asculta conexiuni) 
	// in multimea sel_sock. De asemenea se adauga si input de la stdin si cel
	// de sockudp
	FD_SET(sock_TCP, &sel_sock);
	FD_SET(sock_UDP, &sel_sock);
	FD_SET(STDIN_FILENO, &sel_sock);

	// Socket-ul maxim
	fdmax = max(sock_UDP, sock_TCP);
	fdmax = max(fdmax, STDIN_FILENO);

	while (1) {
		copy_fds = sel_sock; 
		ret = select(fdmax + 1, &copy_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &copy_fds))
		{
			memset(buffer, 0, sizeof(buffer));
			ret = read(0, buffer, sizeof(buffer));
			DIE(ret < 0, "read");
			if (check_exit_signal(buffer, &clients))
			{
				break;
			}
			FD_CLR(STDIN_FILENO, &copy_fds);
		}
		if (FD_ISSET(sock_TCP, &copy_fds)) {
			// a venit o cerere de conexiune, pe care serverul o accepta
			clilen = sizeof(cli_addr);
			new_sock = accept(sock_TCP, (struct sockaddr *) &cli_addr, &clilen);
			DIE(new_sock < 0, "accept");
			flag = 1;	
			ret = setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY,     
								(char *) &flag, sizeof(int));    
			DIE(ret < 0, "TCP_NODELAY");

			memset(buffer, 0, sizeof(buffer));
			ret = recv(new_sock, buffer, IDLEN, 0);
			if (id_unique(&clients, new_sock, buffer))
			{
				save_client(&clients, new_sock, buffer, &fdmax, &sel_sock);
				clients[new_sock].print_connect(cli_addr);
				clients[new_sock].send_saved_packets();	
			}
			
			FD_CLR(sock_TCP, &copy_fds);
		}
		if (FD_ISSET(sock_UDP, &copy_fds)) 
		{
			memset(buffer, 0, BUFLEN);
			memset(&cli_addr, 0, sizeof(cli_addr));
			clilen = sizeof(struct sockaddr);
			ret = recvfrom(sock_UDP, buffer, sizeof(buffer), 0,
					(struct sockaddr *) &cli_addr, &clilen);
			DIE(ret < 0, "recvfrom");
			parse_udp_message(buffer, &clients, cli_addr);
			FD_CLR(sock_UDP, &copy_fds);
			
		}  
		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &copy_fds)) {
				// s-au primit date pe unul din socketii de client,
				// asa ca serverul trebuie sa le receptioneze
				memset(command, 0, COMMANDLEN);
				ret = recv(i, command, COMMANDLEN, 0);
				DIE(ret < 0, "recv");

				if (ret == 0) {
					// conexiunea s-a inchis
					clients[i].client_stop(&sel_sock);
					
				} else {
					// Ne asiguram ca mesajul este complet si verificam comanda
					recv_full_message(i, command, ret, COMMANDLEN);
					clients[i].execute_command(command);
				}
			}
		}
	}

	ret = close(sock_TCP);
	DIE(ret < 0, "close");
	ret = close(sock_UDP);
	DIE(ret < 0, "close");

	return 0;
}
