servidor: main.o dispatcher.o worker.o mensaje.o listas.o
	gcc -o servidor main.o dispatcher.o worker.o mensaje.o listas.o -Wall -Wextra -lpthread -lrt

main.o: main.c cabecera.h
	gcc -g -c main.c

dispatcher.o: dispatcher.c cabecera.h
	gcc -g -c dispatcher.c

worker.o: worker.c cabecera.h
	gcc -g -c worker.c

mensaje.o: mensaje.c cabecera.h
	gcc -g -c mensaje.c

listas.o: listas.c cabecera.h
	gcc -g -c listas.c

clean: 
	rm servidor main.o dispatcher.o worker.o mensaje.o listas.o
