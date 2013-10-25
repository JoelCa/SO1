#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"

pthread_mutex_t m;

int fd = 1, nro_worker[5] = {1,2,3,4,5};
char *colas[10] = {"/cola1", "/cola2", "/cola3", "/cola4", "/cola5","/cola6","/cola7","/cola8","/cola9","/cola0"};
static char *operaciones[9] = {"CON\r\n","LSD\r\n", "DEL", "CRE", "OPN", "WRT", "REA", "CLO", "BYE\r\n"};
extern ListaDes *des;

int eleccion_worker()
{
  int i,j,n;
  int aux[5];

  srand(time(NULL));
  for(i=0,j=0;i<5;i++)
    if(nro_worker[i]>0) {
      pthread_mutex_lock(&m);
      aux[j] = nro_worker[i];
      pthread_mutex_unlock(&m);
      j++;
    }
  if(j==0)
    return -1;
  i = rand() % j;
  n = aux[i];
  for(i=0;i<5;i++)
    if(nro_worker[i]==n) {
      pthread_mutex_lock(&m);
      nro_worker[i] = 0;
      pthread_mutex_unlock(&m);
      break;
    }
  return n-1;
}

int respuesta(char *a, long conn_s)
{
  char con[5] = "CON\r\n";
    char buffer[24];
  int i;

  if (strcmp (a, con) == 0) {
    i = eleccion_worker();
    if(i == -1) {
      sprintf(buffer,"ERROR SERVIDOR SATURADO\n");
      write(conn_s,buffer,strlen(buffer));
    }
    else {
      sprintf(buffer,"OK ID %d\n", i);
      write(conn_s,buffer,strlen(buffer));
      return i;
    }
  }
  return -1;
}

void proc_socket(char *a, long conn_s, int worker, mqd_t mqd_w, mqd_t mqd_d)
{
  char delims[] = " ", buffer[MAXSIZE_COLA];
  char *result = NULL, *fd = NULL, *size = NULL, *buff;
  int i;
  Msj *msj;
  Descriptor *elem;
  
  //imprimir_des(des);
  result = strtok(a, delims);
  for(i = 0; i <= 8; i++) {
    if (strcmp (result, operaciones[i]) == 0) {
      //atributos(mqd_w,colas[worker]);
      switch(i) {
        case 0:      // CON
          sprintf(buffer,"ERROR YA ESTA CONECTADO\n");
          write(conn_s,buffer,strlen(buffer));
          break;
        case 1:      // LSD
          enviar_esp(mqd_w,'l','0','m',NULL);
		  //atributos(mqd_d,"cola del dispa");
          msj = recibir_esp(mqd_d);
		  //printf("GUARDA: \n");
		  //imprimir_msj(msj);
		  sprintf(buffer,"OK");
		  //printf("LLEGO: %s\n", msj->nombre);
		  strcat(buffer,msj->nombre);
          strcat(buffer,"\n");
          write(conn_s,buffer,strlen(buffer));
          break;
        case 2:      // DEL
          result = strtok(NULL, delims);
          enviar_esp(mqd_w,'d','0','m',result);
          msj = recibir_esp(mqd_d);
          switch(msj->dato) {
            case '0':
              sprintf(buffer,"OK\n");
              write(conn_s,buffer,strlen(buffer));
              break;
            case '1':
              sprintf(buffer,"ERROR EL ARCHIVO NO EXISTE\n");
              write(conn_s,buffer,strlen(buffer));
              break;
            default:
              sprintf(buffer,"ERROR EL ARCHIVO ESTA ABIERTO\n");
              write(conn_s,buffer,strlen(buffer));
              break;
              
          }  
          break;
        case 3:      // CRE
          result = strtok(NULL, delims);
          //printf("se envio un msj al worker\n");
          enviar_esp(mqd_w,'c','0','m',result);
          //recibir(mqd_d, buffer);
          //atributos(mqd_d,"cola del dispa en CRE");
		  msj = recibir_esp(mqd_d);
		  //atributos(mqd_d,"cola del dispa en CRE");
		  //imprimir_msj(msj);
          //printf("recibio del worker: %s", (char *) msj);
          //printf("recibio del worker: %s", buffer);
          //free(buff);
          switch(msj->dato) {
            case '0':
              sprintf(buffer,"OK\n");
              write(conn_s,buffer,strlen(buffer));
              break;
            case '1':
              sprintf(buffer,"ERROR EL ARCHIVO YA EXISTE\n");
              write(conn_s,buffer,strlen(buffer));
              break;
            case '2':
              sprintf(buffer,"ERROR INTENTE DE NUEVO\n");
              write(conn_s,buffer,strlen(buffer));
              break;
          }
          break;
        case 4:      // OPN
          result = strtok(NULL, delims);
		  if(strcmp(result, "\r\n") == 0) {
		    sprintf(buffer,"ERROR DE SINTAXIS\n");
            write(conn_s,buffer,strlen(buffer));
		  }
		  /*printf("TENEMOS:%s\n", result);
		  for(i = 0; result[i] != '\0'; i++)
		    printf("%d ", result[i]);
		  printf("\n");*/
          else {
		    if(busca_des(des, result, 0) != NULL) {
            sprintf(buffer,"ERROR EL ARCHIVO ESTA ABIERTO\n");
            write(conn_s,buffer,strlen(buffer));
		    }
		    else {
            enviar_esp(mqd_w,'o','0','m',result);
            msj = recibir_esp(mqd_d);
            switch(msj->dato) {
              case '0':
                sprintf(buffer,"OK FD %d\n", ins_descriptor(des, result, msj->contador-'0', worker));
                write(conn_s,buffer,strlen(buffer));
                break;
              case '1':
                sprintf(buffer,"ERROR EL ARCHIVO ESTA ABIERTO\n");
                write(conn_s,buffer,strlen(buffer));
                break;
              case '2':
                sprintf(buffer,"ERROR EL ARCHIVO NO EXISTE\n");
                write(conn_s,buffer,strlen(buffer));
                break;
            }
		    }
		  }
          break;
        case 5:     //WRT
          result = strtok(NULL, delims);
          if((result != NULL ) && (strcmp(result, "FD") == 0)) {
            fd  = strtok(NULL, delims);
            //printf("llego hasta ACA1\nCON: %p", fd);
			elem = busca_des(des, fd, 1);
            if(elem == NULL) {
              sprintf(buffer,"ERROR FD INCORRECTO\n");
              write(conn_s,buffer,strlen(buffer));
            }
            else {
              if(elem->worker_a == worker) {
                result  = strtok(NULL, delims);
				//printf("llego hasta ACA\nCON: %p", result);
                if((result != NULL) && (strcmp(result, "SIZE") == 0)) {
                  size  = strtok(NULL, delims);
                  result  = strtok(NULL, delims);
                  if(result == NULL) {
                    sprintf(buffer,"ERROR DE SINTAXIS\n");
                    write(conn_s,buffer,strlen(buffer));
                  }
                  else {
                    result = cola_cadena(result);
                    result = sacar_nueva_linea(result, 'w');
                    if(strlen(result) == atoi(size)) {
                      enviar_espwr(mqd_w,'w',(char)(((int)'0')+elem->worker_c),'m',atoi(size),elem->nombre,result);
                      msj = recibir_esp(mqd_d);
                      sprintf(buffer,"OK\n");
                      write(conn_s,buffer,strlen(buffer));
                    }
                    else {
                      sprintf(buffer,"ERROR TAMAÑO INCORRECTO\n");
                      write(conn_s,buffer,strlen(buffer));
                    }
                  }
                }
                else {
                  sprintf(buffer,"ERROR DE SINTAXIS\n");
                  write(conn_s,buffer,strlen(buffer));
                }

              }
              else {
                sprintf(buffer,"ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
                write(conn_s,buffer,strlen(buffer));
              }
            }
          }
          else {
            sprintf(buffer,"ERROR DE SINTAXIS\n");
            write(conn_s,buffer,strlen(buffer));
          }
          break;
        case 6:      // REA
          result = strtok(NULL, delims);
          if((result != NULL ) && (strcmp(result, "FD") == 0)) {
            fd  = strtok(NULL, delims);
			printf("llego hasta ACA1\nCON: %s\n", fd);
            elem = busca_des(des, fd, 1);
            if(elem == NULL) {
              sprintf(buffer,"ERROR FD INCORRECTO\n");
              write(conn_s,buffer,strlen(buffer));
            }
            else {
              if(elem->worker_a == worker) {
                result  = strtok(NULL, delims);
				printf("llego hasta ACA\nCON: %s\n", result);
                if((result != NULL) && (strcmp(result, "SIZE") == 0)) {
                  size  = strtok(NULL, delims);
                  enviar_espwr(mqd_w,'r',(char)(((int)'0')+elem->worker_c),'m', atoi(size), elem->nombre, NULL);
                  msj = recibir_esp(mqd_d);
                  if(msj->nombre == NULL) {
                    sprintf(buffer,"OK SIZE 0\n");
                    write(conn_s,buffer,strlen(buffer));
                  }
                  else {
                    sprintf(buffer,"OK SIZE %d %s\n",msj->otrodato, msj->nombre);
                    write(conn_s,buffer,strlen(buffer));
                  }
                }
                else {
                  sprintf(buffer,"ERROR DE SINTAXIS\n");
                  write(conn_s,buffer,strlen(buffer));
                }
              }
              else {
                sprintf(buffer,"ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
                write(conn_s,buffer,strlen(buffer));
              }
            }
          }
          else {
            sprintf(buffer,"ERROR DE SINTAXIS\n");
            write(conn_s,buffer,strlen(buffer));
          }
          break;
        case 7:      // CLO
          result = strtok(NULL, delims);
          if((result != NULL ) && (strcmp(result, "FD") == 0)) {
            result = strtok(NULL, delims);
            elem = busca_des(des, result, 1);
            if(elem == NULL) {
              sprintf(buffer,"ERROR FD INCORRECTO\n");
              write(conn_s,buffer,strlen(buffer));
            }
            else {
              if(elem->worker_a == worker) {
                enviar_esp(mqd_w,'s',(char)(((int)'0')+elem->worker_c),'m',elem->nombre);
                msj = recibir_esp(mqd_d);
                del_descriptor(des, elem->nombre);
                sprintf(buffer,"OK\n");
                write(conn_s,buffer,strlen(buffer));
              }
              else {
                sprintf(buffer,"ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
                write(conn_s,buffer,strlen(buffer));
              }
              
            }

          }
          else {
            sprintf(buffer,"ERROR DE SINTAXIS\n");
            write(conn_s,buffer,strlen(buffer));
          }
          break;
        case 8:      // BYE
          elem =  des->inicio;
          while(elem != NULL) {
            if(elem->worker_a == worker) {
              enviar_esp(mqd_w,'s',(char)(((int)'0')+elem->worker_c),'m',elem->nombre);
              msj = recibir_esp(mqd_d);
              del_descriptor(des, elem->nombre);
            }
            elem = elem->proximo;
          }
          sprintf(buffer,"OK\n");
          write(conn_s,buffer,strlen(buffer));
          close(conn_s);
          pthread_mutex_lock(&m);
          nro_worker[worker] = worker+1;
          pthread_mutex_unlock(&m);
          break;
      }
    i = 0;
    break;
    }
  }

  if(i > 0) {
    sprintf(buffer,"ERROR DE SINTAXIS\n");
    write(conn_s,buffer,strlen(buffer));
  }
}

void *handle_client(void *arg)
{
  long conn_s = *(long *)arg;
  char buffer[MAXSIZE_COLA];
  int res, worker;
  mqd_t mqd_w, mqd_d;

  printf("Un nuevo cliente\n");
  while(1) {
    res=read(conn_s,buffer,MAXSIZE_COLA);
    if (res<=0)
    {
      close(conn_s);
      break;
    }
    buffer[res]='\0';
    worker = respuesta(buffer, conn_s);
    if(worker >= 0) {
      printf("Cliente %d conectado\n",worker);
      mqd_w = abrir(colas[worker]);
      mqd_d = nueva(colas[worker+5]);
      while(1) {
        res=read(conn_s,buffer,MAXSIZE_COLA);
        if (res<=0)
        {
          close(conn_s);
          pthread_mutex_lock(&m);
          nro_worker[worker] = worker+1;
          pthread_mutex_unlock(&m);
          break;
        }
        buffer[res]='\0';
        proc_socket(buffer, conn_s, worker, mqd_w, mqd_d);
      }
      cerrar(mqd_w);
      borrar(mqd_d,colas[worker+5]);
      printf("Cliente %d desconectado\n",worker);
      break;
    }
  }
  close(conn_s);
  return NULL;
}
