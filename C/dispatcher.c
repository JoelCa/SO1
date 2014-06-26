#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "cabecera.h"

int is_natural(char *to_convert);

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

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
  char con[6] = "CON\r\n";
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

void dispatcherLSD(mqd_t mqd_w, mqd_t mqd_d, long conn_s)
{
  char buffer[MAXSIZE_COLA];
  Msj *msj;

  enviar(mqd_w,'l','0','m',NULL);
  msj = recibir(mqd_d);
  sprintf(buffer,"OK");
  strcat(buffer,msj->nombre);
  strcat(buffer,"\n");
  write(conn_s,buffer,strlen(buffer));
  liberar_msj(msj);
}

void dispatcherDEL(mqd_t mqd_w, mqd_t mqd_d, long conn_s)
{
  char *token;
  char delims[] = " ";
  char buffer[MAXSIZE_COLA];
  Msj *msj;

  if (((token = strtok(NULL, delims)) != NULL) && (strcmp(token, "\r\n") != 0)
      && (strtok(NULL, delims) == NULL)) {
    enviar(mqd_w,'d','0','m',token);
    msj = recibir(mqd_d);
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
    liberar_msj(msj);
  }
  else {
    sprintf(buffer,"ERROR DE SINTAXIS\n");
    write(conn_s,buffer,strlen(buffer));
  }
}

void dispatcherCRE(mqd_t mqd_w, mqd_t mqd_d, long conn_s)
{
  char *token;
  char delims[] = " ";
  char buffer[MAXSIZE_COLA];
  Msj *msj;

  if (((token = strtok(NULL, delims)) != NULL) && (strcmp(token, "\r\n") != 0)
      && (strtok(NULL, delims) == NULL)) {
    enviar(mqd_w,'c','0','m',token);
    msj = recibir(mqd_d);
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
    liberar_msj(msj);
  }
  else {
    sprintf(buffer,"ERROR DE SINTAXIS\n");
    write(conn_s,buffer,strlen(buffer));
  }
}

void dispatcherOPN(mqd_t mqd_w, mqd_t mqd_d, long conn_s)
{
  return;
}

int proc_socket(char *a, long conn_s, int worker, mqd_t mqd_w, mqd_t mqd_d)
{
  char delims[] = " ", buffer[MAXSIZE_COLA];
  char *result = NULL, *buff;
  int i, fd0, size;
  Msj *msj;
  Descriptor *elem;
  
  result = strtok(a, delims);
  for(i = 0; i <= 8; i++) {
    if (strcmp (result, operaciones[i]) == 0) {
      switch(i) {
        case 0:      // CON
          sprintf(buffer,"ERROR YA ESTA CONECTADO\n");
          write(conn_s,buffer,strlen(buffer));
          break;

        case 1:      // LSD
          dispatcherLSD(mqd_w, mqd_d, conn_s);
          break;

        case 2:      // DEL
          dispatcherDEL(mqd_w, mqd_d, conn_s);
          break;

        case 3:      // CRE
          dispatcherCRE(mqd_w, mqd_d, conn_s);
          break;

        case 4:      // OPN
          if (((result = strtok(NULL, delims)) != NULL) && (strcmp(result, "\r\n") != 0) && (strtok(NULL, delims) == NULL)) {
            if (busca_des(des, result, 0, 0) != NULL) {
              sprintf(buffer,"ERROR EL ARCHIVO ESTA ABIERTO\n");
              write(conn_s,buffer,strlen(buffer));
            }
            else {
              enviar(mqd_w,'o','0','m',result);
              msj = recibir(mqd_d);
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
              liberar_msj(msj);
            }
          }
          else {
            sprintf(buffer,"ERROR DE SINTAXIS\n");
            write(conn_s,buffer,strlen(buffer));
          }
          break;
        case 5:     //WRT
         i = 0;
          if(((result  = strtok(NULL, delims)) != NULL ) && (strcmp(result, "FD") == 0)) {
            fd0  = is_natural(strtok(NULL, delims));
            if(((result  = strtok(NULL, delims)) != NULL) && (strcmp(result, "SIZE") == 0)) {
              buff = strtok(NULL, delims);
              size  = is_natural(buff);
              if(((result  = strtok(NULL, delims)) != NULL) && (fd0 > 0) && (size > 0))
                i = 1;
              if((result == NULL) && (is_natural(sacar_nueva_linea(buff)) == 0) && (fd0 > 0))
                i = 2;
            }
          }
          switch(i) {
            case 1:
              if((elem = busca_des(des, NULL, fd0, 1)) == NULL) {
                sprintf(buffer,"ERROR FD INCORRECTO\n");
                write(conn_s,buffer,strlen(buffer));
              }
              else {
                if(elem->worker_a == worker) {
                  result = cola_cadena(result);
                  if(strlen(result) == size) {
                    enviarWR(mqd_w,'w',(char)(((int)'0')+elem->worker_c),'m',size,elem->nombre,result);
                    msj = recibir(mqd_d);
                    sprintf(buffer,"OK\n");
                    write(conn_s,buffer,strlen(buffer));
                    liberar_msj(msj);
                  }
                  else {
                    sprintf(buffer,"ERROR TAMAÑO INCORRECTO\n");
                    write(conn_s,buffer,strlen(buffer));
                  }
                }
                else {
                  sprintf(buffer,"ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
                  write(conn_s,buffer,strlen(buffer));
                }
              }
              break;
            case 2:
              if((elem = busca_des(des, NULL, fd0, 1)) == NULL) {
                sprintf(buffer,"ERROR FD INCORRECTO\n");
                write(conn_s,buffer,strlen(buffer));
              }
              else {
                sprintf(buffer,"OK\n");
                write(conn_s,buffer,strlen(buffer));
              }
              break;
            case 0:
              sprintf(buffer,"ERROR DE SINTAXIS\n");
              write(conn_s,buffer,strlen(buffer));
              break;
          }
          break;
        case 6:      // REA
          i = 0;
          if(((result  = strtok(NULL, delims)) != NULL ) && (strcmp(result, "FD") == 0)) {
            fd0  = is_natural(strtok(NULL, delims));
            if(((result  = strtok(NULL, delims)) != NULL) && (strcmp(result, "SIZE") == 0)) {
              size  = is_natural(sacar_nueva_linea(strtok(NULL, delims)));
              if(((strtok(NULL, delims)) == NULL) && (fd0 > 0) && (size >= 0))
                i = 1;
            }
          }
          if(i) {
            if((elem = busca_des(des, NULL, fd0, 1)) == NULL) {
              sprintf(buffer,"ERROR FD INCORRECTO\n");
              write(conn_s,buffer,strlen(buffer));
            }
            else {
              if(elem->worker_a == worker) {
                enviarWR(mqd_w,'r',(char)(((int)'0')+elem->worker_c),'m', size, elem->nombre, NULL);
                msj = recibir(mqd_d);
                if(msj->nombre == NULL) {
                  sprintf(buffer,"OK SIZE 0\n");
                  write(conn_s,buffer,strlen(buffer));
                }
                else {
                  sprintf(buffer,"OK SIZE %d %s\n",msj->otrodato, msj->nombre);
                  write(conn_s,buffer,strlen(buffer));
                }
                //liberar_msj(msj);
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
          i = 0;
          if(((result  = strtok(NULL, delims)) != NULL ) && (strcmp(result, "FD") == 0) &&
              ((result  = strtok(NULL, delims)) != NULL) && (strcmp(result, "\r\n") != 0)) {
            if(((strtok(NULL, delims)) == NULL) && ((fd0 = is_natural(sacar_nueva_linea(result))) > 0))
              i = 1;
          }
          if(i) {
            elem = busca_des(des, NULL, fd0, 1);
            if(elem == NULL) {
              sprintf(buffer,"ERROR FD INCORRECTO\n");
              write(conn_s,buffer,strlen(buffer));
            }
            else {
              if(elem->worker_a == worker) {
                enviar(mqd_w,'s',(char)(((int)'0')+elem->worker_c),'m',elem->nombre);
                msj = recibir(mqd_d);
                del_descriptor(des, elem->nombre);
                sprintf(buffer,"OK\n");
                write(conn_s,buffer,strlen(buffer));
                liberar_msj(msj);
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
              enviar(mqd_w,'s',(char)(((int)'0')+elem->worker_c),'m',elem->nombre);
              msj = recibir(mqd_d);
              del_descriptor(des, elem->nombre);
              liberar_msj(msj);
            }
            elem = elem->proximo;
          }
          sprintf(buffer,"OK\n");
          write(conn_s,buffer,strlen(buffer));
          close(conn_s);
          pthread_mutex_lock(&m);
          nro_worker[worker] = worker+1;
          pthread_mutex_unlock(&m);
          return 1;
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
  return 0;
}

void *handle_client(void *arg)
{
  long conn_s = *(long *)arg;
  char buffer[MAXSIZE_COLA];
  int res, worker;
  mqd_t mqd_w, mqd_d;
  Colas *dc;

  printf("Un nuevo cliente\n");
  while(1) {
    res=read(conn_s,buffer,MAXSIZE_COLA);
    if (res<=0) {
      close(conn_s);
      break;
    }
    buffer[res]='\0';
    if((worker = respuesta(buffer, conn_s)) >= 0) {
      printf("Un nuevo cliente conectado: worker nº %d\n",worker);
      mqd_w = abrir(colas[worker]);
      mqd_d = nueva(colas[worker + N_WORKER]);
      while(1) {
        res=read(conn_s,buffer,MAXSIZE_COLA);
        if(res<=0) {
          sprintf(buffer, "BYE\r\n");
          //dc = descriptor_cola(mqd_w, mqd_d, NULL);
          proc_socket(buffer, conn_s, worker, mqd_w, mqd_d);
          break;
        }
        buffer[res]='\0';
        if(proc_socket(buffer, conn_s, worker, mqd_w, mqd_d))
          break;
      }
      close(conn_s);
      pthread_mutex_lock(&m);
      nro_worker[worker] = worker+1;
      pthread_mutex_unlock(&m);
      cerrar(mqd_w);
      borrar(mqd_d,colas[worker + N_WORKER]);
      printf("Cliente desconectado: worker nº %d\n",worker);
      break;
    }
  }
  close(conn_s);
  return NULL;
}

//similar a atoi, regresa -1 si no es una string válida
int is_natural(char *to_convert)
{
    char* p = to_convert;
    errno = 0;
    unsigned int val;

    val = strtoul(to_convert, &p, 10);
    if (errno != 0 || to_convert == p || *p != 0)
      return -1;
    return val;
}
