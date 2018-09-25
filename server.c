#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MAX_CLIENTS	100
#define BUFLEN 256

// structura pentru a pastra datele din fisier
typedef struct {
	char lastName[13];
	char firstName[13];
	int cardNo;
	int pin;
	char secretPass[9];
	double balance;
	char blocked;
} card;

// lista de clienti activi
typedef struct activeUserStruc {
	card* cardInfo;
	// i reprezinta indexul din vectorul de carduri
	int i;
	// clientId este socket-ul folosit de client
	int clientId;
	char loggedIn;
	char numOfTries;
	char tryingToUnlock;

	struct activeUserStruc* next;	
} activeUser, *activeUsersList;

// lista de transferuri 
typedef struct pendingTransferStruc {
	card* sender;
	card* receiver;
	int senderId;
	double transferredSum;

	struct pendingTransferStruc* next;
} pendingTransfer, *pendingTransfersList;

void error(char *msg) {
	perror(msg);
	exit(1);
}

// citeste datele din fisier si intoarce un vector
// de structuri card
card* readData(char* fileName, int* n) {
	FILE* file;
	int i;
	card* cards;

	file = fopen(fileName, "r");

	if(file == NULL) {
		error("No such input file");
	}

	fscanf(file, "%d", n);

	cards = malloc((*n) * sizeof(card));

	if(cards == NULL) {
		error("Not enough memory");
	}

	for(i = 0; i < *n; i++) {
		fscanf(file, "%s ", cards[i].lastName);
		fscanf(file, "%s ", cards[i].firstName);
		fscanf(file, "%d ", &(cards[i].cardNo));
		fscanf(file, "%d ", &(cards[i].pin));
		fscanf(file, "%s ", cards[i].secretPass);
		fscanf(file, "%lf", &(cards[i].balance));

		cards[i].blocked = 0;
	}

	fclose(file);
	return cards;
}


// Actualizeaza lista de clienti logati.
// Intoarce cod de eroare in caz de esec,
// sau indexul cardului din vector pe care s-a logat clientul in caz de succes.

// daca exista cardul, dar pinul e gresit, tot il pune in lista cu useri activi

// cardurile blocate raman in lista
int loginCommand(activeUsersList* activeUsers, card* cards, int cardsNo, 
	int cardNo, int pin, int clientId) {
	int i = 0;
	activeUsersList aux;
	// nu exista alti clienti logati
	if(*activeUsers == NULL) {
		// cauta cardul in vector
		for(i = 0; i < cardsNo; i++) {
			if(cards[i].cardNo == cardNo) {
				*activeUsers = malloc(sizeof(activeUser));
				(*activeUsers)->cardInfo = &(cards[i]);
				(*activeUsers)->i = i;
				(*activeUsers)->clientId = clientId;
				(*activeUsers)->loggedIn = 0;
				(*activeUsers)->numOfTries = 0;
				(*activeUsers)->next = NULL;
				(*activeUsers)->tryingToUnlock = 0;

				break;
			}
		}

		// nu exita card cu numarul acesta
		if(i == cardsNo) {
			return -4;
		}

		// daca exista

		// daca pinul e bun
		if(cards[i].pin == pin) {
			(*activeUsers)->loggedIn = 1;
			return i;
		// daca pinul e gresit
		} else {
			(*activeUsers)->numOfTries++;
			return -3;
		}
	// daca exista alti clienti logati
	} else {
		// parcurge lista cu carduri
		aux = *activeUsers;
		for(; aux != NULL; aux = aux->next) {
			// exista deja cineva logat pe cardul acesta
			if(aux->cardInfo->cardNo == cardNo && aux->loggedIn == 1) {
				return -2;
			} else if(aux->cardInfo->cardNo == cardNo && aux->loggedIn == 0) {
				// daca acest card e blocat
				if(aux->cardInfo->blocked) {
					return -5;
				}
				// daca incearca altcineva sa se logheze cu cardul asta
				if(aux->clientId != clientId) {
					aux->clientId = clientId;
					aux->numOfTries = 0;
				}

				break;
			}
		}

		// daca a gasit cardul in lista si nu e nimeni logat pe el
		if(aux != NULL) {
			// daca pinul e bun
			if(aux->cardInfo->pin == pin) {
				aux->loggedIn = 1;
				return aux->i;
			}
			// daca pinul nu e bun si e a 3-a incercare
			aux->numOfTries++;

			// cardul este blocat
			if(aux->numOfTries == 3) {
				aux->loggedIn = 0;
				aux->cardInfo->blocked = 1;
				return -5;
			} else {
				return -3;
			}
		}

		// daca nu a gasit cardul in lista

		// cauta cardul in vector
		for(i = 0; i < cardsNo; i++) {
			if(cards[i].cardNo == cardNo) {
				aux = malloc(sizeof(activeUser));
				aux->cardInfo = &(cards[i]);
				aux->i = i;
				aux->clientId = clientId;
				aux->loggedIn = 0;
				aux->numOfTries = 0;
				aux->next = *activeUsers;
				*activeUsers = aux;

				break;
			}
		}

		// nu exita card cu numarul acesta
		if(i == cardsNo) {
			return -4;
		}

		// daca exista

		// daca pinul e bun
		if(cards[i].pin == pin) {
			(*activeUsers)->loggedIn = 1;
			return i;
		// daca pinul e gresit
		} else {
			(*activeUsers)->numOfTries++;
			return -3;
		}

	}
}

// elimina din lista de clienti activi clientul cu clientId si care
// e logged in
void logoutCommand(activeUsersList* activeUsers, int clientId) {
	activeUsersList aux = *activeUsers;
	activeUsersList ant = NULL;

	for(; aux != NULL; ant = aux, aux = aux->next) {
		// l-a gasit in lista si il elimina
		if(aux->clientId == clientId && aux->loggedIn) {
			// il elimina pe primul din lista
			if(ant == NULL) {
				*activeUsers = aux->next;
			} else {
				ant->next = aux->next;
			}

			free(aux);

			return;
		}
	}
}

double listsoldCommand(activeUsersList activeUsers, int clientId) {
	for(; activeUsers != NULL; activeUsers = activeUsers->next) {
		// a gasit cardul cu care e logat clientul
		if(activeUsers->clientId == clientId && activeUsers->loggedIn) {
			return activeUsers->cardInfo->balance;
		}
	}

	return -1;
}

// intoarce cod de eroare, sau indexul cardului catre care se transfera,
// in caz de succes
int transferCommand(activeUsersList activeUsers, 
	pendingTransfersList* pendingTransfers, card* cards, int cardsNo,
	int cardNoToTransfer, double sumToTransfer, int senderId) {

	int i;
	pendingTransfersList transfer;

	for(i = 0; i < cardsNo; i++) {
		if(cards[i].cardNo == cardNoToTransfer) {
			break;
		}
	}

	// daca nu a gasit niciun card cu numarul dat ca parametru
	if(i == cardsNo) {
		return -4;
	}

	for(; activeUsers != NULL; activeUsers = activeUsers->next) {
		if(activeUsers->clientId == senderId) {
			// daca sender nu are destui bani
			if(activeUsers->cardInfo->balance - sumToTransfer < 0) {
				return -8;
			} else {
				break;
			}
		}
	}

	transfer = malloc(sizeof(pendingTransfer));

	transfer->sender = activeUsers->cardInfo;
	transfer->receiver = &(cards[i]);
	transfer->senderId = senderId;
	transfer->transferredSum = sumToTransfer;

	// daca e primul pending transaction
	if(*pendingTransfers == NULL) {
		transfer->next = NULL;
		*pendingTransfers = transfer;
	} else {
		transfer->next = *pendingTransfers;
		*pendingTransfers = transfer;
	}

	return i;
}

// anuleaza transferul
void cancelTransfer(pendingTransfersList* pendingTransfers, int senderId) {
	pendingTransfersList transfer = *pendingTransfers;
	pendingTransfersList ant = NULL;

	// cauta pending transfer in lista
	for(; transfer != NULL; ant = transfer, transfer = transfer->next) {
		if(transfer->senderId == senderId) {
			if(ant == NULL) {
				*pendingTransfers = transfer->next;
			} else {
				ant->next = transfer->next;
			}

			free(transfer);

			return;
		}
	}
}

// realizeaza transferul
void confirmTransfer(pendingTransfersList* pendingTransfers, int senderId) {
	pendingTransfersList transfer = *pendingTransfers;
	pendingTransfersList ant = NULL;

	for(; transfer != NULL; ant = transfer, transfer = transfer->next) {
		// a gasit transferul
		if(transfer->senderId == senderId) {
			// modifica soldurile
			transfer->sender->balance -= transfer->transferredSum;
			transfer->receiver->balance += transfer->transferredSum;

			if(ant == NULL) {
				*pendingTransfers = transfer->next;
			} else {
				ant->next = transfer->next;
			}

			free(transfer);

			return;
		}
	}
}

// elimina din lista de active users pe cel cu clientId
void removeActiveUser(activeUsersList* activeUsers, int clientId) {
	activeUsersList user = *activeUsers;
	activeUsersList ant = NULL;

	for(; user != NULL; ant = user, user = user->next) {
		if(user->clientId == clientId) {
			if(ant == NULL) {
				*activeUsers = user->next;
			} else {
				ant->next = user->next;
			}

			free(user);

			return;
		}
	}
}

void sendQuitToClients(activeUsersList activeUsers, char* command) {
	for(; activeUsers != NULL; activeUsers = activeUsers->next) {
		// daca e logat
		if(activeUsers->loggedIn) {
			send(activeUsers->clientId, command, strlen(command), 0);
			close(activeUsers->clientId);
		}
	}
}

// elibereaza memoria
void freeMemory(activeUsersList *activeUsers, 
	pendingTransfersList* pendingTransfers, card* cards) {
	activeUsersList auxUser;
	pendingTransfersList auxTransfer;

	while(*activeUsers != NULL) {
		auxUser = *activeUsers;
		*activeUsers = (*activeUsers)->next;

		free(auxUser);
	}

	while(*pendingTransfers != NULL) {
		auxTransfer = *pendingTransfers;
		*pendingTransfers = (*pendingTransfers)->next;

		free(auxTransfer);
	}

	free(cards);
}

// verifica daca un card e blocat
// intoarce cod de eroare daca nu il gaseste, sau nu e blocat,
// 0, altfel
int checkLockedCard(activeUsersList* activeUsers, card* cards, 
	int cardsNo, int cardNo) {
	int i;
	// intai verifica daca se afla in lista de carduri active
	activeUsersList activeUser = *activeUsers;

	for(; activeUser != NULL; activeUser = activeUser->next) {
		// daca a gasit cardul
		if(activeUser->cardInfo->cardNo == cardNo) {
			// daca nu e blocat 
			//(e cineva logat pe el sau doar a incercat sa se logheze)
			if(!(activeUser->cardInfo->blocked)) {
				return -6;
			} else if(activeUser->tryingToUnlock){
				return -7;
			} else {
				activeUser->tryingToUnlock = 1;
				break;
			}
		}
	}

	// daca nu l-a gasit in lista de carduri active 
	// il cauta in vectorul de carduri
	if(activeUser == NULL) {
		for(i = 0; i < cardsNo; i++) {
			// cardul nu e blocat
			if(cards[i].cardNo == cardNo) {
				return -6;
			}
		}

		// daca nu l-a gasit nici in vector inseamna ca nu exista
		return -4;
	}

	// in caz de succes intoarce 0
	return 0;
}

// deblocheaza cardul
int unlockCard(activeUsersList activeUsers, int cardNo, char* secretPass) {
	for(; activeUsers != NULL; activeUsers = activeUsers->next) {
		// cardul cu numarul corect
		if(activeUsers->cardInfo->cardNo == cardNo) {
			// daca parola secreta e buna
			if(!strcmp(activeUsers->cardInfo->secretPass, secretPass)) {
				activeUsers->cardInfo->blocked = 0;
				activeUsers->tryingToUnlock = 0;
				activeUsers->numOfTries = 0;
				return 0;
			// parola secreta incorecta
			} else {
				activeUsers->tryingToUnlock = 0;
				return -7;
			}
		}
	}

	return -4;
}

int main(int argc, char *argv[]) {
	int listenSocketFd, newSocketFd, clientLen;
	char buffer[BUFLEN];
	char tmpBuffer[BUFLEN];
	char command[BUFLEN];
	struct sockaddr_in server, client;
	int n, i;
	int crtFd;
	activeUsersList activeUsers = NULL;
	pendingTransfersList pendingTransfers = NULL;
	char* token;
	int cardNo;
	int pin;
	int result;
	int transferResult;
	int cardNoToTransfer;
	double sumToTransfer;
	fd_set readFds;
	fd_set tmpFds; 
	int maxFd;
	int cardsNo;
	card* cards;
	// variabile pentru conexiunea UDP
	int UDPSocketFd;
	struct sockaddr_in unlockClientStruc;
	unsigned int addrlen = sizeof(server);
	int checkLockedCardRes;
	int unlockCardRes;
	char secretPass[9];

	if(argc < 3) {
		error("Not enough arguments");
	}

	cards = readData(argv[2], &cardsNo);

	// goleste multimea de descriptori de citire (read_fds) si multimea tmp_fds 
	FD_ZERO(&readFds);
	FD_ZERO(&tmpFds);

	// socket petru conexiune TCP IBANK	 
	listenSocketFd = socket(PF_INET, SOCK_STREAM, 0);

	if(listenSocketFd < 0) {
		error("ERROR opening socket");
	}

	// socket pentru conexiune UDP UNLOCK
	UDPSocketFd = socket(PF_INET, SOCK_DGRAM, 0);

	if(UDPSocketFd < 0) {
		error("ERROR opening socket");
	}

	// Setare struct sockaddr_in server
	memset((char*) &server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	server.sin_port = htons(atoi(argv[1]));
	
	// bind pentru protocolul TCP
	if(bind(listenSocketFd, (struct sockaddr*) &server, 
		sizeof(struct sockaddr)) < 0) {
		error("ERROR on binding");
	}

	// bind pentru protocolul UDP
	if(bind(UDPSocketFd, (struct sockaddr*) &server, 
		sizeof(struct sockaddr)) < 0) {
		error("ERROR on binding");
	}

	listen(listenSocketFd, MAX_CLIENTS);

	// adauga socketul pe care se asculta conexiuni TCP in multimea read_fds
	FD_SET(listenSocketFd, &readFds);
	// 0 pentru citire de la tastatura
	FD_SET(0, &readFds);
	// adauga socketul UDP in set
	FD_SET(UDPSocketFd, &readFds);

	maxFd = MAX(listenSocketFd, UDPSocketFd);

	clientLen = sizeof(client);

	while(1) {
		tmpFds = readFds; 
		if(select(maxFd + 1, &tmpFds, NULL, NULL, NULL) == -1) 
			error("ERROR in select");
	
		for(crtFd = 0; crtFd <= maxFd; crtFd++) {
			if(FD_ISSET(crtFd, &tmpFds)) {
				// citeste de la tastatura
				if(crtFd == 0) {
					memset(command, 0 , BUFLEN);
					fgets(command, BUFLEN-1, stdin);

					// sterge \n de la sfarsit (ugly but it works)
					if(command[strlen(command) - 1] == '\n') {
						command[strlen(command) - 1] = '\0';
					}

					// daca e comanda de quit
					if(!strcmp(command, "quit")) {
						// anunta toti clientii ca se inchide serverul
						sendQuitToClients(activeUsers, command);
						// elibereaza memoria
						freeMemory(&activeUsers, &pendingTransfers, cards);

						close(listenSocketFd);
						close(UDPSocketFd);

						return 0;
					}
				// conexiune noua
				} else if(crtFd == listenSocketFd) {
					// actiunea serverului: accept()
					if((newSocketFd = accept(listenSocketFd, 
						(struct sockaddr *)&client, &clientLen)) == -1) {
						error("ERROR in accept");
					} 
					else {
						// adauga noul socket intors de accept() la multimea 
						// descriptorilor de citire
						FD_SET(newSocketFd, &readFds);
						if(newSocketFd > maxFd) { 
							maxFd = newSocketFd;
						}
					}
				// daca e cerere pe socket UDP
				} else if(crtFd == UDPSocketFd) {
					memset(buffer, 0, BUFLEN);

					recvfrom(UDPSocketFd, buffer, BUFLEN, 0, 
						(struct sockaddr*)(&unlockClientStruc), &addrlen);
						
					memcpy(tmpBuffer, buffer, strlen(buffer) + 1);

					token = strtok(tmpBuffer, " \n");

					// daca e comanda unlock
					if(!strcmp(token, "unlock")) {
						cardNo = atoi(strtok(NULL, " \n"));
						checkLockedCardRes = checkLockedCard(&activeUsers, cards,
						 cardsNo, cardNo);

						memset(buffer, 0, BUFLEN);

						// cod de eroare
						if(checkLockedCardRes < 0) {
							sprintf(buffer, "%d", checkLockedCardRes);
						// numar card valid
						} else {
							sprintf(buffer, "Trimite parola secreta");
						}

						sendto(UDPSocketFd, buffer, strlen(buffer), 0, 
							(struct sockaddr*)(&unlockClientStruc), addrlen);
					// a primit parola secreta
					} else {
						sscanf(buffer, "%d %s", &cardNo, secretPass);

						unlockCardRes = unlockCard(activeUsers, cardNo, secretPass);

						memset(buffer, 0, BUFLEN);

						// cod de eroare
						if(unlockCardRes < 0) {
							sprintf(buffer, "%d", unlockCardRes);
						// parola secreta valida
						} else {
							sprintf(buffer, "Card deblocat");
						}

						sendto(UDPSocketFd, buffer, strlen(buffer), 0, 
							(struct sockaddr*)(&unlockClientStruc), addrlen);
					}
				// a primit date pe un socket TCP
				} else {
					memset(buffer, 0, BUFLEN);
					if((n = recv(crtFd, buffer, sizeof(buffer), 0)) <= 0) {
						if(n == 0) {
							//conexiunea s-a inchis
							printf("server: socket %d hung up\n", crtFd);
						} else {
							error("ERROR in recv");
						}
						close(crtFd); 
						// elimina socketul din set
						FD_CLR(crtFd, &readFds);
					} 
					else {
						memcpy(tmpBuffer, buffer, strlen(buffer) + 1);

						token = strtok(tmpBuffer, " \n");
						
						// daca e comanda de login
						if(!strcmp(token, "login")) {
							cardNo = atoi(strtok(NULL, " \n"));
							pin = atoi(strtok(NULL, " \n"));

							result = loginCommand(&activeUsers, cards, cardsNo, 
								cardNo, pin, crtFd);

							memset(buffer, 0, BUFLEN);

							// daca e cod de eroare
							if(result < 0) {
								sprintf(buffer, "%d", result);
							// daca s-a efectuat cu succes login
							} else {
								sprintf(buffer, "Welcome %s %s", 
									cards[result].lastName, cards[result].firstName);
							}

							n = send(crtFd, buffer, strlen(buffer), 0);
						// daca e comanda de logout
						} else if(!strcmp(token, "logout")) {
							logoutCommand(&activeUsers, crtFd);

							memset(buffer, 0, BUFLEN);
							sprintf(buffer, "Clientul a fost deconectat");

							n = send(crtFd, buffer, strlen(buffer), 0);
						// daca e comanda listsold
						} else if(!strcmp(token, "listsold")) {
							memset(buffer, 0, BUFLEN);
							sprintf(buffer, "%.2lf", 
								listsoldCommand(activeUsers, crtFd));

							n = send(crtFd, buffer, strlen(buffer), 0);
						// daca e comanda transfer
						} else if(!strcmp(token, "transfer")) {
							memset(buffer, 0, BUFLEN);

							sscanf(tmpBuffer + strlen(token) + 1, "%d %lf", 
								&cardNoToTransfer, &sumToTransfer);

							transferResult =  transferCommand(activeUsers, 
								&pendingTransfers, cards, cardsNo, cardNoToTransfer,
								sumToTransfer, crtFd);

							// daca e cod de eroare
							if(transferResult < 0) {
								sprintf(buffer, "%d", transferResult);
							// daca transferul se poate realiza
							} else {
								sprintf(buffer, "Transfer %.2lf catre %s %s? [y/n]",
								 sumToTransfer, cards[transferResult].lastName, 
									cards[transferResult].firstName);
							}

							n = send(crtFd, buffer, strlen(buffer), 0);
						// daca mesajul primit e confirmare transfer
						} else if(!strcmp(token, "y")) {
							memset(buffer, 0, BUFLEN);
							confirmTransfer(&pendingTransfers, crtFd);

							sprintf(buffer, "Transfer realizat cu succes");

							n = send(crtFd, buffer, strlen(buffer), 0);
						// quit de la client
						} else if(!strcmp(token, "quit")) {
							// elimina din lista de active logins
							removeActiveUser(&activeUsers, crtFd);

							close(crtFd); 
							// elimina socketul din set
							FD_CLR(crtFd, &readFds);
						// anulare transfer
						} else {
							memset(buffer, 0, BUFLEN);
							cancelTransfer(&pendingTransfers, crtFd);

							sprintf(buffer, "-9");

							n = send(crtFd, buffer, strlen(buffer), 0);
						}
					}
				} 
			}
		}
	}
   
	return 0; 
}


