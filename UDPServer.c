#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h> 
#include <sys/uio.h>
#include <unistd.h>

#define PORT	 50000 
#define MAXLINE 1024 

int main() { 

	int sockfd; 
	char buffer[MAXLINE]; 
	char *hello = "Hello from server"; 
	struct sockaddr_in servaddr, cliaddr; 
	
	// Creating socket file descriptor 
	if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
		perror("socket creation failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	memset(&servaddr, 0, sizeof(servaddr)); 
	memset(&cliaddr, 0, sizeof(cliaddr)); 
	
	// Filling server information 
	servaddr.sin_family = AF_INET; // IPv4 
  // servaddr.sin_addr.s_addr = INADDR_ANY; 
	servaddr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
	servaddr.sin_port = htons(PORT); 
	
	// Bind the socket with the server address 
	if ( bind(sockfd, (const struct sockaddr *)&servaddr, 
			sizeof(servaddr)) < 0 ) { 
		perror("bind failed"); 
		exit(EXIT_FAILURE); 
	} 
	
	int n; 
	// len is an unsinged int value/result, but it is recomanded
	// to use the socklen_t type
	socklen_t len = sizeof(cliaddr); 

	n = recvfrom(sockfd, (char *)buffer, MAXLINE, 
				MSG_WAITALL, (struct sockaddr *) &cliaddr, 
				&len); 
	buffer[n] = '\0';
	printf("Client : %s\n", buffer); 
	sendto(sockfd, (const char *)hello, strlen(hello), 
		0, (const struct sockaddr *) &cliaddr, 
			len); 
	printf("Hello message sent.\n"); 
	
	return 0; 
} 
