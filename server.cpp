#include "helpers.h"
using namespace std;

// Clasa folosita pentru a descrie un topic
class Topic
{
public:
	string topic_name; // Numele unui topic
	bool sf;		   // Tag-ul SF pentru un topic
};

// Clasa folosita pentru a descrie un client
class Subscriber
{
public:
	int socket;							  // Socketul de comunicare
	bool connected;						  // True pentru conectat, fals altfel
	string id;							  // Id-ul unui client
	vector<Topic> topics;				  // Lista de topics la care este abonat
	vector<server_message> saved_packets; // Pachete pierdute de client

	// Afiseaza conectarea clientului
	void print_connect(struct sockaddr_in cli_addr)
	{
		printf("New client %s connected from %s:%d\n", id.c_str(),
			   inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
	}

	// Afiseaza deconectarea clientului
	void print_disconnect()
	{
		printf("Client %s disconnected.\n", id.c_str());
	}

	// Trimite pachetele salvate pentru topic-urile cu SF == 1 cat timp
	// clientul a fost deconectat, daca exista
	void send_saved_packets()
	{
		int n;
		for (auto packet : saved_packets)
		{
			n = send(socket, &packet, packet.number_of_bytes, 0);
			DIE(n < 0, "send");
		}
		saved_packets.clear();
	}

	// Executa o command de subscriber, de tipul subscribe/unsubscribe
	void execute_command(char *command)
	{
		// Parsarea propozitie
		istringstream command_spliter(command);
		// Un cuvant de proprozitie
		string word;
		if (command_spliter >> word)
		{
			if (word.compare("subscribe") == 0)
			{
				string topic_name;
				if (command_spliter >> topic_name && command_spliter >> word)
				{
					Topic topic;
					topic.sf = false;
					if (word == "1")
					{
						topic.sf = true;
					}

					topic.topic_name = topic_name;
					topics.push_back(topic);
				}
				else
				{
					fprintf(stderr, "Not a valid subscribe command.\n");
				}
			}

			if (word.compare("unsubscribe") == 0)
			{
				if (command_spliter >> word)
				{
					remove_if(topics.begin(), topics.end(),
							  [&word](Topic topic)
							  { return topic.topic_name == word; });
				}
				else
				{
					fprintf(stderr, "No topic given for unsubscribe.\n");
				}
			}
		}
	}
	// Opreste conexiunea cu un client
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

// Verifica daca un id este unic si inchide conexiunea in caz contrar
bool id_unique(map<int, Subscriber> *clients, int new_sock, char *id)
{
	auto found = find_if(clients->begin(), clients->end(),
						 [id](pair<const int, Subscriber> client)
						 { return client.second.id.compare(id) == 0; });

	if (found != clients->end() && found->second.connected)
	{
		printf("Client %s already connected.\n", id);
		int ret = close(new_sock);
		DIE(ret < 0, "close");
		return false;
	}

	return true;
}

// Salveaza un client in baza de date
void save_client(map<int, Subscriber> *clients, int new_sock, char *id,
				 int *fdmax, fd_set *sel_sock)
{

	Subscriber new_client;
	new_client.connected = true;
	new_client.id.append(id);
	new_client.socket = new_sock;

	auto old_client = find_if(clients->begin(), clients->end(),
							  [id](pair<const int, Subscriber> client)
							  { return client.second.id.compare(id) == 0; });

	if (old_client != clients->end())
	{
		new_client.topics = old_client->second.topics;
		new_client.saved_packets = old_client->second.saved_packets;
		clients->erase(old_client);
	}

	(*clients)[new_sock] = new_client;
	FD_SET(new_sock, sel_sock);
	if (new_sock > *fdmax)
	{
		*fdmax = new_sock;
	}
}

// Interpreteaza un mesaj UDP si trimite/salveaza mesajul la subscriberii la
// acel topic
void parse_udp_message(char *buffer, map<int, Subscriber> *clients,
					   struct sockaddr_in cli_addr)
{
	int ret;
	udp_message *message = (udp_message *)buffer;

	server_message toSend;
	memcpy(&toSend.message, buffer, sizeof(udp_message));
	memset(&toSend.addr, 0, sizeof(toSend.addr));
	toSend.addr.sin_family = AF_INET;
	toSend.addr = cli_addr;
	toSend.number_of_bytes = bytes_needed(message->type, message->content);
	for (auto &client : *clients)
	{
		auto topic = find_if(client.second.topics.begin(),
							 client.second.topics.end(),
							 [&message](Topic el)
							 {
								 return el.topic_name
											.compare(message->topic) == 0;
							 });

		if (topic != client.second.topics.end())
		{
			if (client.second.connected)
			{
				ret = send(client.second.socket, &toSend,
						   toSend.number_of_bytes, 0);
				DIE(ret < 0, "send");
			}
			else
			{
				if (topic->sf)
				{
					client.second.saved_packets.push_back(toSend);
				}
			}
		}
	}
}

// Verifica daca am primit un mesaj de tipul exits
bool check_exit_signal(char *buffer, map<int, Subscriber> *clients)
{
	int ret;
	if (strncmp(buffer, "exit", 4) == 0)
	{
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
	// Declarari de variabile si setarea STDOUT-ului
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	int sock_TCP, sock_UDP, new_sock, portno, ret;
	char buffer[BUFLEN];
	command *command_parser;

	struct sockaddr_in serv_addr, cli_addr;
	socklen_t clilen = sizeof(cli_addr);
	socklen_t sockaddr_len = sizeof(struct sockaddr);
	// Hashmap-ul folosit pentru stocarea clientilor socket -> Client
	map<int, Subscriber> clients;

	// Initializare multimi de socketi, una fiind o copie
	fd_set sel_sock, copy_fds;
	int fdmax; // valoare maxima fd din multimea tcp_fds

	// Verificam rularea corecta
	usage(argc, 2, argv[0]);

	// se goleste multimea de descriptori de citire (tcp_fds) si copia
	FD_ZERO(&sel_sock);
	FD_ZERO(&copy_fds);

	// Creeare celor 2 socketi TCP si UDP
	sock_TCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sock_UDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	DIE(sock_TCP < 0, "socket");
	DIE(sock_UDP < 0, "socket");
	// Dezactivare nagle
	int flag = 1;
	int result = setsockopt(sock_TCP, IPPROTO_TCP, TCP_NODELAY,
							(char *)&flag, sizeof(int));

	DIE(result < 0, "TCP_NODELAY");
	// Transformarea portului din string in int
	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	// Pregatirea datelor serverului
	memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// Facem si bind si listen pe socketul de acceptare conexiuni TCP
	ret = bind(sock_TCP, (struct sockaddr *)&serv_addr, sockaddr_len);
	DIE(ret < 0, "bind");
	ret = listen(sock_TCP, 1);
	DIE(ret < 0, "listen");

	// Facem bind pe socket-ul pe care primim mesaje de la clientii UDP
	ret = bind(sock_UDP, (struct sockaddr *)&serv_addr, sockaddr_len);
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

	while (1)
	{
		// Restauram copia
		copy_fds = sel_sock;
		// Asteptam un mesaj un pe un socketfd(inclusiv STDIN)
		ret = select(fdmax + 1, &copy_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		// Am primit un mesaj de la tastatura
		if (FD_ISSET(STDIN_FILENO, &copy_fds))
		{
			// Citim mesajul si vedem daca este un mesaj de tipul "exit"
			memset(buffer, 0, sizeof(buffer));
			ret = read(0, buffer, sizeof(buffer));
			DIE(ret < 0, "read");
			if (check_exit_signal(buffer, &clients))
			{
				break;
			}
			else
				fprintf(stderr, "Invalid input command\n");
			FD_CLR(STDIN_FILENO, &copy_fds);
		}
		// Am primit o cerere de conexiune pe socket-ul TCP
		if (FD_ISSET(sock_TCP, &copy_fds))
		{
			// a venit o cerere de conexiune, pe care serverul o accepta
			clilen = sizeof(cli_addr);
			new_sock = accept(sock_TCP, (struct sockaddr *)&cli_addr, &clilen);
			DIE(new_sock < 0, "accept");
			flag = 1;
			ret = setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY,
							 (char *)&flag, sizeof(int));
			DIE(ret < 0, "TCP_NODELAY");

			// Primim ID-ul clientului intr-un mesaj TCP
			memset(buffer, 0, sizeof(buffer));
			ret = recv(new_sock, buffer, sizeof(uint32_t), 0);
			DIE(ret < 0, "recv");
			command_parser = (command *)buffer;
			recv_full_message(new_sock, buffer, ret);

			// Verificam daca id-ul este unic
			if (id_unique(&clients, new_sock, command_parser->message))
			{
				// Salvam noul client, afisam ca este conectat si trimitem
				// eventualele packete salvate
				save_client(&clients, new_sock, command_parser->message,
							&fdmax, &sel_sock);
				clients[new_sock].print_connect(cli_addr);
				clients[new_sock].send_saved_packets();
			}

			FD_CLR(sock_TCP, &copy_fds);
		}
		// Am primit un mesaj pe un socket UDP
		if (FD_ISSET(sock_UDP, &copy_fds))
		{
			memset(buffer, 0, BUFLEN);
			memset(&cli_addr, 0, sizeof(cli_addr));
			clilen = sizeof(struct sockaddr);
			ret = recvfrom(sock_UDP, buffer, sizeof(buffer), 0,
						   (struct sockaddr *)&cli_addr, &clilen);
			DIE(ret < 0, "recvfrom");
			parse_udp_message(buffer, &clients, cli_addr);
			FD_CLR(sock_UDP, &copy_fds);
		}
		// Verificam daca am primit un mesaj de la un client
		for (auto &client : clients)
		{
			if (FD_ISSET(client.second.socket, &copy_fds))
			{
				// s-au primit date pe unul din socketii de client,
				// asa ca serverul trebuie sa le receptioneze
				memset(buffer, 0, sizeof(command));
				ret = recv(client.second.socket, buffer, sizeof(uint32_t), 0);
				DIE(ret < 0, "recv");
				if (ret == 0)
				{
					// conexiunea s-a inchis
					client.second.client_stop(&sel_sock);
				}
				else
				{
					// Ne asiguram ca mesajul este complet si verificam comanda
					recv_full_message(client.second.socket, buffer, ret);
					command_parser = (command *)buffer;
					client.second.execute_command(command_parser->message);
				}
			}
		}
	}
	// Inchideri socketi
	ret = close(sock_TCP);
	DIE(ret < 0, "close");
	ret = close(sock_UDP);
	DIE(ret < 0, "close");

	return 0;
}
