#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "cabecera.h"

pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static char *operaciones[9] = {"CON\r\n","LSD\r\n", "DEL", "CRE", "OPN", "WRT", "REA", "CLO", "BYE\r\n"};
int fd = 1;

extern char *bitmap_worker;
extern ListaDescriptores *descriptores;

int string_to_integer(char *to_convert);
char integer_to_char(int n);


void responder_al_cliente(int conn_s, char mensaje[])
{
  write(conn_s, mensaje, strlen(mensaje));
}

int eleccion_worker()
{
  int i,j,n;
  int aux[NUM_WORKER] = {0};

  for(i=0,j=0;i<NUM_WORKER;i++) {
    if(bitmap_worker[i]== 0) {
      aux[j] = i;
      j++;
    }
  }
  if(j==0)
    return -1;
  i = rand() % j;
  n = aux[i];

  pthread_mutex_lock(&m);
  bitmap_worker[n] = 1;
  pthread_mutex_unlock(&m);

  return n;
}

void PS_operadorLSD(DescriptorColas *cola, long conn_s)
{
  char buffer[MAXSIZE_TEXT];
  Msj *msj;

  enviar(cola->worker,'l','0','m',NULL);
  msj = recibir(cola->disp);
  sprintf(buffer,"OK");
  strcat(buffer,msj->nombre);
  strcat(buffer,"\n");
  responder_al_cliente(conn_s, buffer);
  liberar_msj(msj);
}

void PS_operadorDEL(DescriptorColas *cola, long conn_s)
{
  char *token;
  char delims[] = " ";
  Msj *msj;

  if (((token = strtok(NULL, delims)) != NULL) && (strcmp(token, "\r\n") != 0)
      && (strtok(NULL, delims) == NULL)) {
    enviar(cola->worker,'d','0','m',token);
    msj = recibir(cola->disp);
    switch(msj->dato) {
      case '0':
        responder_al_cliente(conn_s, "OK\n");
        break;

      case '1':
        responder_al_cliente(conn_s, "ERROR EL ARCHIVO NO EXISTE\n");
        break;

      default:
        responder_al_cliente(conn_s, "ERROR EL ARCHIVO ESTA ABIERTO\n");
        break;
              
    }
    liberar_msj(msj);
  }
  else {
    responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
  }
}

void PS_operadorCRE(DescriptorColas *cola, long conn_s)
{
  char *token;
  char delims[] = " ";
  Msj *msj;

  if (((token = strtok(NULL, delims)) != NULL) && (strcmp(token, "\r\n") != 0)
      && (strtok(NULL, delims) == NULL)) {
    enviar(cola->worker,'c','0','m',token);
    msj = recibir(cola->disp);
    switch(msj->dato) {
      case '0':
        responder_al_cliente(conn_s, "OK\n");
        break;

      case '1':
        responder_al_cliente(conn_s, "ERROR EL ARCHIVO YA EXISTE\n");
        break;

      case '2':
        responder_al_cliente(conn_s, "ERROR INTENTE DE NUEVO\n");
        break;
    }
    liberar_msj(msj);
  }
  else {
    responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
  }
}

void PS_operadorOPN(DescriptorColas *cola, long conn_s, int worker)
{
  char *token;
  char delims[] = " ";
  char buffer[10];
  Msj *msj;

  if (((token = strtok(NULL, delims)) != NULL) && (strcmp(token, "\r\n") != 0) && (strtok(NULL, delims) == NULL)) {
    if (buscar_descriptor(descriptores, token, 0, 0) != NULL) {
      responder_al_cliente(conn_s, "ERROR EL ARCHIVO ESTA ABIERTO\n");
    }
    else {
      enviar(cola->worker,'o','0','m',token);
      msj = recibir(cola->disp);
      switch(msj->dato) {
        case '0':
          sprintf(buffer,"OK FD %d\n", nuevo_descriptor(descriptores, token, msj->contador - '0', worker));
          responder_al_cliente(conn_s, buffer);
          break;

        case '1':
          responder_al_cliente(conn_s, "ERROR EL ARCHIVO ESTA ABIERTO\n");
          break;

        case '2':
          responder_al_cliente(conn_s, "ERROR EL ARCHIVO NO EXISTE\n");
          break;
      }
      liberar_msj(msj);
    }
  }
  else {
    responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
  }
}

void PS_operadorWRT(DescriptorColas *cola, long conn_s, int worker)
{
  int i = 0, fd, size;
  char *token, *size_token;
  char delims[] = " ";
  DescriptorArchivo *archivo;
  Msj *msj;

  if(((token  = strtok(NULL, delims)) != NULL ) && (strcmp(token, "FD") == 0)) {
    fd  = string_to_integer(strtok(NULL, delims));
    if(((token  = strtok(NULL, delims)) != NULL) && (strcmp(token, "SIZE") == 0)) {
      size_token = strtok(NULL, delims);
      size  = string_to_integer(size_token);
      if(((token  = strtok(NULL, delims)) != NULL) && (fd > 0) && (size > 0))
        i = 1;
      if((token == NULL) && (string_to_integer(sacar_nueva_linea(size_token)) == 0) && (fd > 0))
        i = 2;
    }
  }

  switch(i) {
    case 1:
      if((archivo = buscar_descriptor(descriptores, NULL, fd, 1)) == NULL) {
        responder_al_cliente(conn_s, "ERROR FD INCORRECTO\n");
      }
      else {
        if(archivo->worker_a == worker) {
          token = cola_cadena(token);
          if(strlen(token) == size) {
            enviarWR(cola->worker,'w',integer_to_char(archivo->worker_c),'m',size,archivo->nombre,token);
            msj = recibir(cola->disp);
            responder_al_cliente(conn_s, "OK\n");
            liberar_msj(msj);
          }
          else {
            responder_al_cliente(conn_s, "ERROR TAMAÑO INCORRECTO\n");
          }
        }
        else {
          responder_al_cliente(conn_s, "ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
        }
      }
      break;

    case 2:
      if((archivo = buscar_descriptor(descriptores, NULL, fd, 1)) == NULL) {
        responder_al_cliente(conn_s, "ERROR FD INCORRECTO\n");
      }
      else {
        responder_al_cliente(conn_s, "OK\n");
      }
      break;

    case 0:
      responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
      break;
  }
}

void PS_operadorREA(DescriptorColas *cola, long conn_s, int worker)
{
  int i = 0, fd, size;
  char *token;
  char buffer[MAXSIZE_TEXT], delims[] = " ";
  DescriptorArchivo *archivo;
  Msj *msj;

  if(((token  = strtok(NULL, delims)) != NULL ) && (strcmp(token, "FD") == 0)) {
    fd  = string_to_integer(strtok(NULL, delims));
    if(((token  = strtok(NULL, delims)) != NULL) && (strcmp(token, "SIZE") == 0)) {
      size  = string_to_integer(sacar_nueva_linea(strtok(NULL, delims)));
      if(((strtok(NULL, delims)) == NULL) && (fd > 0) && (size >= 0))
        i = 1;
    }
  }
  if(i) {
    if((archivo = buscar_descriptor(descriptores, NULL, fd, 1)) == NULL) {
      responder_al_cliente(conn_s, "ERROR FD INCORRECTO\n");
    }
    else {
      if(archivo->worker_a == worker) {
        enviarWR(cola->worker,'r',integer_to_char(archivo->worker_c),'m', size, archivo->nombre, NULL);
        msj = recibir(cola->disp);
        if(msj->nombre == NULL) {
          responder_al_cliente(conn_s, "OK SIZE 0\n");
        }
        else {
          sprintf(buffer,"OK SIZE %d %s\n",msj->otrodato, msj->nombre);
          responder_al_cliente(conn_s, buffer);
        }
        //liberar_msj(msj);
      }
      else {
        responder_al_cliente(conn_s, "ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
      }
    }
  }
  else {
    responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
  }
}

void PS_operadorCLO(DescriptorColas *cola, long conn_s, int worker)
{
  int i = 0, fd;
  char *token;
  char delims[] = " ";
  DescriptorArchivo *archivo;
  Msj *msj;

  if(((token = strtok(NULL, delims)) != NULL ) && (strcmp(token, "FD") == 0) &&
     ((token = strtok(NULL, delims)) != NULL) && (strcmp(token, "\r\n") != 0)) {
    if(((strtok(NULL, delims)) == NULL) && ((fd = string_to_integer(sacar_nueva_linea(token))) > 0))
      i = 1;
  }
  if(i) {
    if((archivo = buscar_descriptor(descriptores, NULL, fd, 1)) == NULL) {
      responder_al_cliente(conn_s, "ERROR FD INCORRECTO\n");
    }
    else {
      if(archivo->worker_a == worker) {
        enviar(cola->worker,'s',integer_to_char(archivo->worker_c),'m',archivo->nombre);
        msj = recibir(cola->disp);
        borrar_descriptor(descriptores, archivo->nombre);
        responder_al_cliente(conn_s, "OK\n");
        liberar_msj(msj);
      }
      else {
        responder_al_cliente(conn_s, "ERROR EL ARCHIVO FUE ABIERTO POR OTRO USUARIO\n");
      }  
    }
  }
  else {
    responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
  }
}

void PS_operadorBYE(DescriptorColas *cola, long conn_s, int worker)
{
  DescriptorArchivo *archivo;
  Msj *msj;

  archivo =  descriptores->inicio;
  while(archivo != NULL) {
    if(archivo->worker_a == worker) {
      enviar(cola->worker,'s',integer_to_char(archivo->worker_c),'m',archivo->nombre);
      msj = recibir(cola->disp);
      liberar_msj(msj);
      archivo = borrar_descriptor(descriptores, archivo->nombre);
      continue;
    }
    archivo = archivo->proximo;
  }
  responder_al_cliente(conn_s, "OK\n");
  close(conn_s);
}

int evaluar_pedido(char *pedido, long conn_s, int worker, DescriptorColas* cola)
{
  int i;
  char *token;
  char delims[] = " ";

  token = strtok(pedido, delims);
  for(i = 0; i <= 8; i++) {
    if (strcmp (token, operaciones[i]) == 0) {
      switch(i) {
        case 0:       // CON
          responder_al_cliente(conn_s, "ERROR YA ESTA CONECTADO\n");
          break;

        case 1:       // LSD
          PS_operadorLSD(cola, conn_s);
          break;

        case 2:       // DEL
          PS_operadorDEL(cola, conn_s);
          break;

        case 3:       // CRE
          PS_operadorCRE(cola, conn_s);
          break;

        case 4:       // OPN
          PS_operadorOPN(cola, conn_s, worker);
          break;

        case 5:       // WRT
          PS_operadorWRT(cola, conn_s, worker);
          break;

        case 6:       // REA
          PS_operadorREA(cola, conn_s, worker);
          break;

        case 7:       // CLO
          PS_operadorCLO(cola, conn_s, worker);
          break;

        case 8:       // BYE
          PS_operadorBYE(cola, conn_s, worker);
          return 1;
          break;
      }
    i = 0;
    break;
    }
  }

  if(i > 0) {
    responder_al_cliente(conn_s, "ERROR DE SINTAXIS\n");
  }
  return 0;
}

int iniciar_conexion(char *a, long conn_s)
{
  char con[6] = "CON\r\n";
  char buffer[10];
  int i;

  if(strcmp (a, con) == 0) {
    i = eleccion_worker();
    if(i == -1) {
      responder_al_cliente(conn_s, "ERROR SERVIDOR SATURADO\n");
    }
    else {
      sprintf(buffer,"OK ID %d\n", i);
      responder_al_cliente(conn_s, buffer);
      return i;
    }
  }
  return -1;
}

void *proceso_socket(void *arg)
{
  long conn_s = *(long *)arg;
  char pedido[MAXSIZE_TEXT], cola_w[7];
  int res, worker;
  DescriptorColas *cola;

  printf("Un nuevo cliente\n");
  while(1) {
    res=read(conn_s,pedido,MAXSIZE_TEXT);
    if (res<=0) {
      close(conn_s);
      break;
    }
    pedido[res]='\0';
    if((worker = iniciar_conexion(pedido, conn_s)) >= 0) {
      printf("Un nuevo cliente conectado: worker nº %d\n",worker);
      cola = nueva_cola_mensaje(worker, 'd');
      sprintf(cola_w, "/cola%d", worker);
      cola->worker = abrir(cola_w);
      while(1) {
        res=read(conn_s,pedido,MAXSIZE_TEXT);
        if(res<=0) { //Ante un cierre inesperado deben cerrarse todos los archivos del cliente.
          PS_operadorBYE(cola, conn_s, worker);
          break;
        }
        pedido[res]='\0';
        if(evaluar_pedido(pedido, conn_s, worker, cola))
          break;
      }
      close(conn_s);

      pthread_mutex_lock(&m);
      bitmap_worker[worker] = 0;
      pthread_mutex_unlock(&m);

      borrar_cola_mensaje(cola, worker, 'd');
      printf("Cliente desconectado: worker nº %d\n",worker);
      break;
    }
  }
  close(conn_s);
  return NULL;
}

int string_to_integer(char *s)
{
    char* p = s;
    errno = 0;
    unsigned int val;

    val = strtoul(s, &p, 10);
    if (errno != 0 || s == p || *p != 0)
      return -1;
    return val;
}

char integer_to_char(int n)
{
  return (char) (((int)'0') + n);
}
