# Protocoale de comunicatii:
# Laborator 8: Multiplexare
# Makefile

CFLAGS = -Wall -g

# Portul pe care asculta serverul (de completat)
PORT = 3333

# Adresa IP a serverului (de completat)
IP_SERVER = 127.0.0.1

all: server subscriber udpserver udpclient

# Compileaza server.c
server: server.cpp

# Compileaza client.c
subscriber: subscriber.c

# Compileaza UDPClient.c
udpclient: UDPClient.c 

# Compileaza UDPServer.c
udpserver: UDPServer.c

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server: server
	./server ${PORT}

# Ruleaza clientul
run_subscriber: subscriber
	./subscriber C1 ${IP_SERVER} ${PORT}

run_udp:
	python3 pcom_hw2_udp_client/udp_client.py --mode manual --source-address ${IP_SERVER} --source-port ${PORT}

clean:
	rm -f server subscriber
