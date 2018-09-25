build: client.o server.o
	gcc client.o -o client
	gcc server.o -o server
client.o: client.c
	gcc -c client.c
server.o: server.c
	gcc -c server.c
clean:
	rm -f *.o client server