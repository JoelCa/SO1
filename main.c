#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"

Lista *lista1 = NULL;
Lista *lista2 = NULL;
Lista *lista3 = NULL;
Lista *lista4 = NULL;
Lista *lista5 = NULL;
ListaDes *des= NULL;

int main()
{
  int list_s;
  long conn_s = -1;
  struct sockaddr_in servaddr;
  pthread_t work, clientes;

  crear_colas();
  crear_listas();
  crear_buff_des();
  if ((list_s = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error creating listening socket.\n");
    return -1;
  }
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(8000);
  
  if (bind(list_s, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error calling bind()\n");
    return -1; 
  }
  
  if ( listen(list_s, 10) < 0 ) {
    fprintf(stderr, "ECHOSERV: Error calling listen()\n");
    return -1;													
  }
  pthread_create(&work, NULL, fs, (void *)lista1);
  pthread_create(&work, NULL, fs, (void *)lista2);
  pthread_create(&work, NULL, fs, (void *)lista3);
  pthread_create(&work, NULL, fs, (void *)lista4);
  pthread_create(&work, NULL, fs, (void *)lista5);
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
  return 0;
}
