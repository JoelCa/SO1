servidor: main.o dispatcher.o worker.o mensaje.o listas.o
	gcc -o servidor main.o dispatcher.o worker.o mensaje.o listas.o -lpthread -lrt

main.o: main.c cabecera.h
	gcc -c main.c

dispatcher.o: dispatcher.c cabecera.h
	gcc -c dispatcher.c

worker.o: worker.c cabecera.h
	gcc -c worker.c

mensaje.o: mensaje.c cabecera.h
	gcc -c mensaje.c

listas.o: listas.c cabecera.h
	gcc -c listas.c

clean: 
	rm -f servidor main.o dispatcher.o worker.o mensaje.o listas.o
