#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include "cabecera.h"

ListaDescriptores *descriptores;
pthread_barrier_t barrier;
char *bitmap_worker;

int main(int argc, char **argv)
{
  int i,list_s;
  int num[N_WORKER] = {0};
  long conn_s = -1;
  struct sockaddr_in servaddr;
  pthread_t work, clientes;
  char * pEnd;
  unsigned port;


  if(argc > 2) {
    printf("servidor: error argumento incorrecto\n");
    return -1;
  }

  if(argc == 2) {
    errno = 0;
    port = strtol(argv[1],&pEnd,10);

    if((errno == ERANGE && (port == LONG_MAX || port == LONG_MIN))
       || (errno != 0 && port == 0)) {
      perror("strtol");
      return -1;
    }

    if ((pEnd == argv[1]) || (port <= 0)) {
      printf("servidor: error argumento incorrecto\n");
      return -1;
    }
  }
  else
    port = 8000;

  descriptores = crear_lista_descriptores();

  pthread_barrier_init(&barrier,NULL,N_WORKER);

  bitmap_worker = (char *) calloc(N_WORKER, sizeof(char));

  srand(time(NULL));

  if((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
    return -1;
  }

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port);
  
  if(bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error calling bind()\n");
    return -1; 
  }
  
  if( listen(list_s, 10) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error calling listen()\n");
    return -1;													
  }
  for(i=0;i<N_WORKER;i++) {
    num[i] = i+1;
    pthread_create(&work, NULL, fs, num+i);
  }
  printf("Servidor aceptando clientes\n");
  while (1) {
    if((conn_s = accept(list_s, NULL, NULL) ) < 0 ) {
      fprintf(stderr, "ECHOSERV: Error calling accept()\n");
      return -1;
    }
    pthread_create(&clientes, NULL, handle_client,(void *)&conn_s);
  }
  pthread_join(clientes, NULL);
  close(list_s);
  pthread_barrier_destroy(&barrier);
  return 0;
}
