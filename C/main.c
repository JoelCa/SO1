#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"   

#define N_WORKER 5

ListaDes *des= NULL;
pthread_barrier_t barrier;

int main()
{
  int i,list_s;
  int num[5] = {1,2,3,4,5};
  long conn_s = -1;
  struct sockaddr_in servaddr;
  pthread_t work, clientes;

  crear_buff_des();
  pthread_barrier_init(&barrier,NULL,N_WORKER);

  if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
    return -1;
  }
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(8001);
  
  if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error calling bind()\n");
    return -1; 
  }
  
  if ( listen(list_s, 10) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error calling listen()\n");
    return -1;													
  }
  for(i=0;i<N_WORKER;i++)
    pthread_create(&work, NULL, fs, num+i);
  printf("Servidor aceptando clientes\n");
  while (1) {
    if ((conn_s = accept(list_s, NULL, NULL) ) < 0 ) {
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
