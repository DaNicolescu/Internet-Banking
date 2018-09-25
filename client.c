#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define BUFLEN 256
#define IBANK "IBANK>"
#define UNLOCK "UNLOCK>"

void error(char *msg) {
	perror(msg);
	exit(0);
}

// creeaza fisierul de log
FILE* createLogFile() {
	int processId;
	char processIdString[12];
	char* logName;
	FILE* logFile;

	processId = getpid();
	sprintf(processIdString, "%d", processId);
	logName = malloc(12 + strlen(processIdString));
	strcpy(logName, "client-");
	strcat(logName, processIdString);
	strcat(logName, ".log");

	logFile = fopen(logName, "w");

	free(logName);

	return logFile;
}

// scrie in fisierul de log si la consola

// logFile: fisier de log
// command: comanda data de client la server
// result: mesajul dat de server
// service: IBANK sau UNLOCK
// code: cod de eroare (primit tot de la server)
void updateLog(FILE* logFile, char* command, char* result, char* service,
	int code) {
	// caz special pentru quit
	if(command != NULL && !strcmp(command, "quit")) {
		fprintf(logFile, "%s\n", command);

		return;
	}
	// cod de eroare (very bad)
	if(code == -10) {
		fprintf(logFile, "Eroare la apel %s\n", result);
		printf("Eroare la apel %s\n", result);

		fclose(logFile);
		exit(0);
	}

	fprintf(logFile, "%s", command);

	// code < 0, atunci e cod de eroare
	if(code < 0) {
		fprintf(logFile, "%s %d : ", service, code);
		printf("%s %d : ", service, code);

		switch(code) {
			case -1 :
				fprintf(logFile, "Clientul nu este autentificat\n");
				printf("Clientul nu este autentificat\n");
				break;
			case -2 : 
				fprintf(logFile, "Sesiune deja deschisa\n");
				printf("Sesiune deja deschisa\n");
				break;
			case -3 :
				fprintf(logFile, "Pin gresit\n");
				printf("Pin gresit\n");
				break;
			case -4 : 
				fprintf(logFile, "Numar card inexistent\n");
				printf("Numar card inexistent\n");
				break;
			case -5 : 
				fprintf(logFile, "Card blocat\n");
				printf("Card blocat\n");
				break;
			case -6 :
				fprintf(logFile, "Operatie esuata\n");
				printf("Operatie esuata\n");
				break;
			case -7 :
				fprintf(logFile, "Deblocare esuata\n");
				printf("Deblocare esuata\n");
				break;
			case -8 :
				fprintf(logFile, "Fonduri insuficiente\n");
				printf("Fonduri insuficiente\n");
				break;
			case -9 :
				fprintf(logFile, "Operatie anulata\n");
				printf("Operatie anulata\n");
				break;
			default :
				error("Wrong code");
		}
	} else {
		fprintf(logFile, "%s ", service);
		printf("%s ", service);

		fprintf(logFile, "%s\n", result);
		printf("%s\n", result);
	}
}

// inchide clientul in cazul in care serverul s-a inchis
void serverClosed(FILE* logFile, int clientSocketFd, int unlockSocketFd) {
	printf("server inchis\n");
	// inchide fisier de log
	fclose(logFile);
	// inchide socket
	close(clientSocketFd);
	close(unlockSocketFd);

	exit(0);
}

int main(int argc, char *argv[]) {
	// socket pentru conexiunea TCP
	int clientSocketFd;
	// socket pentru UDP
	int unlockSocketFd;
	int maxFd;
	int n;
	struct sockaddr_in server;
	char buffer[BUFLEN];
	char tmpBuffer[BUFLEN];
	char command[BUFLEN];
	char tmpCommand[BUFLEN];
	// fisier de log
	FILE* logFile;
	int loginRes;
	// string folosit pentru parsare
	char* token;
	char loggedIn = 0;
	int cardNo;
	unsigned int addrlen = sizeof(server);
	int udpRes;
	int transferResult;

	if(argc < 3) {
		error("Not enough arguments");
	}

	// creeaza fisier de log
	logFile = createLogFile();
	
	// socket pentru conexiunea TCP
	clientSocketFd = socket(PF_INET, SOCK_STREAM, 0);
	if(clientSocketFd < 0) {
		updateLog(logFile, NULL, "socket", NULL, -10);
	}

	// socket pentru UDP
	unlockSocketFd = socket(PF_INET, SOCK_DGRAM, 0);

	if(unlockSocketFd < 0) {
		updateLog(logFile, NULL, "socket", NULL, -10);
	}
	
	// seteaza serverul (struct sockaddr_in)
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(argv[2]));
	inet_aton(argv[1], &server.sin_addr);

	// set de citire folosit in select()
	fd_set readFds;	
	fd_set tmpFds;
	FD_SET(clientSocketFd, &readFds);
	// adauga 0 pentru citire de la tastatura
	FD_SET(0, &readFds);
	// adauga socketul UDP in setul de citire
	FD_SET(unlockSocketFd, &readFds);
	
	if(connect(clientSocketFd, (struct sockaddr*) &server, 
		sizeof(server)) < 0) {
		updateLog(logFile, NULL, "connect", NULL, -10); 
	}

	// adauga socketul TCP
	FD_SET(clientSocketFd, &readFds);
	maxFd = MAX(clientSocketFd, unlockSocketFd);
	
	while(1) {
		tmpFds = readFds;
		if(select(maxFd + 1, &tmpFds, NULL, NULL, NULL) == -1) {
			updateLog(logFile, NULL, "select", NULL, -10);
		}

		// primeste mesaj pe UDP
		if(FD_ISSET(unlockSocketFd, &tmpFds)) {
			memset(buffer, 0, BUFLEN);

			recvfrom(unlockSocketFd, buffer, BUFLEN, 0, 
				(struct sockaddr*)(&server), 
				&addrlen);

			// daca e cod de eroare
			if(sscanf(buffer, "%d", &udpRes) == 1) {
				updateLog(logFile, command, NULL, UNLOCK, udpRes);
			} else {
				// daca mesajul este Card deblocat
				if(!strcmp(buffer, "Card deblocat")) {
					updateLog(logFile, command, buffer, UNLOCK, 0);
				// altfel, trebuie trimisa parola secreta
				} else {
					updateLog(logFile, command, buffer, UNLOCK, 0);

					memset(command, 0, BUFLEN);
					memset(buffer, 0, BUFLEN);

					// citeste parola secreta de la tastatura
					fgets(command, BUFLEN-1, stdin);

					sprintf(buffer, "%d %s", cardNo, command);

					// o trimite la server
					sendto(unlockSocketFd, buffer, strlen(buffer), 0, 
						(struct sockaddr*)(&server), addrlen);
				}
			}
		// primeste mesaj pe TCP
		} else if(FD_ISSET(clientSocketFd, &tmpFds)) {
			memset(buffer, 0, BUFLEN);
			
			n = recv(clientSocketFd, buffer, sizeof(buffer), 0);

			// server inchis
			if(n == 0) {
				serverClosed(logFile, clientSocketFd, unlockSocketFd);
			}

			// server inchis
			if(!strcmp(buffer, "quit")) {
				serverClosed(logFile, clientSocketFd, unlockSocketFd);
			// daca a fost data comanda login
			} else if(!strcmp(token, "login")) {
				// daca e cod de eroare
				if(sscanf(buffer, "%d", &loginRes) == 1) {
					updateLog(logFile, command, NULL, IBANK, loginRes);
				// altfel, e login cu succes
				} else {
					loggedIn = 1;
					updateLog(logFile, command, buffer, IBANK, 0);
				}
			// daca a fost data o comanda de logout
			} else if(!strcmp(token, "logout")) {
				loggedIn = 0;
				updateLog(logFile, command, buffer, IBANK, 0);
			// daca a fost data comanda listsold
			} else if(!strcmp(token, "listsold")) {
				updateLog(logFile, command, buffer, IBANK, 0);
			// daca a fost data comanda transfer
			} else if(!strcmp(token, "transfer")) {
				// daca in buffer e doar un numar, atunci e cod de eroare
				if(sscanf(buffer, "%d", &transferResult) == 1) {
					updateLog(logFile, command, buffer, IBANK, transferResult);
				// altfel, transferul este posibil
				} else {
					updateLog(logFile, command, buffer, IBANK, 0);
					// citeste de la tastatura
					memset(command, 0 , BUFLEN);
					fgets(command, BUFLEN-1, stdin);

					n = send(clientSocketFd, command, strlen(command), 0);
					if(n < 0) {
						updateLog(logFile, NULL, "send", NULL, -10);
					}
				}
			// daca transferul a avut loc cu succes
			} else if(!strcmp(token, "Transfer realizat cu succes")) {
				updateLog(logFile, command, buffer, IBANK, 0);
			} 
		} else {
			// citeste de la tastatura
			memset(command, 0, BUFLEN);
			fgets(command, BUFLEN-1, stdin);

			memcpy(tmpCommand, command, strlen(command) + 1);
			// obtine primul cuvant
			token = strtok(tmpCommand, " \n");

			// daca e comanda de login
			if(!strcmp(token, "login")) {
				// daca e deja logat nu mai trimite mesaj catre server
				if(loggedIn) {
					updateLog(logFile, command, NULL, IBANK, -2);

					continue;
				}

				cardNo = atoi(strtok(NULL, " \n"));
			// daca e comanda de logout
			} else if(!strcmp(token, "logout")) {
				// daca nu era logat nu mai trimite mesaj catre server
				if(!loggedIn) {
					updateLog(logFile, command, NULL, IBANK, -1);

					continue;
				}
				cardNo = -1;
			// daca e comanda de listsold
			} else if(!strcmp(token, "listsold")) {
				// idaca nu e logat
				if(!loggedIn) {
					updateLog(logFile, command, NULL, IBANK, -1);

					continue;
				}
			// daca e comanda de transfer
			} else if(!strcmp(token, "transfer")) {
				// daca nu e logat
				if(!loggedIn) {
					updateLog(logFile, command, NULL, IBANK, -1);

					continue;
				}
			// daca e comanda de quit
			} else if(!strcmp(token, "quit")) {
				n = send(clientSocketFd, command, strlen(command), 0);
				if(n < 0) {
					updateLog(logFile, NULL, "send", NULL, -10);
				} else {
					updateLog(logFile, "quit", NULL, NULL, 0);
				}

				// inchide fisier de log
				fclose(logFile);
				// inchide socket
				close(clientSocketFd);
				close(unlockSocketFd);

				return 0;
			// daca e comanda de unlock
			} else if(!strcmp(token, "unlock")) {
				memset(buffer, 0, BUFLEN);

				sprintf(buffer, "%s %d", command, cardNo);

				// o trimite la server (pe socket UDP)
				sendto(unlockSocketFd, buffer, strlen(buffer), 0, 
					(struct sockaddr*)(&server), addrlen);

				continue;
			}

			// trimite mesaj la server (pe socket TCP)
			n = send(clientSocketFd, command, strlen(command), 0);
			if(n < 0) {
				updateLog(logFile, NULL, "send", NULL, -10);
			}
		}
	}

	return 0;
}