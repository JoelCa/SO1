#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cabecera.h"

mqd_t nueva(char *nombre)
{
  struct mq_attr atributos;
  mqd_t cola;

  atributos.mq_maxmsg = MAXMSG_COLA;
  atributos.mq_msgsize = MAXSIZE_COLA;
  atributos.mq_flags = 0;

  cola = mq_open(nombre, O_CREAT | O_RDWR, S_IRWXU, &atributos);
  if (cola == (mqd_t)-1)
    perror ("mq_open");
  return cola;
}

mqd_t abrir(char *nombre)
{
  mqd_t cola;

  cola = mq_open(nombre, O_RDWR);
  if (cola == (mqd_t)-1)
    perror("mq_open");
  return cola;
}

int enviar(mqd_t cola, char *mensaje)
{
  if(mq_send(cola, mensaje, MAXSIZE_COLA, 0) == -1) 
    perror("mq_send");
  return 0;
}

int recibir(mqd_t cola, char *buf)
{
  if (mq_receive(cola, buf, MAXSIZE_COLA, NULL) == -1) 
    perror("mq_receive");
  return 0;
}

int cerrar(mqd_t cola)
{
  mq_close(cola);
  return 0;
}

int borrar(mqd_t cola, char *nombre_cola)
{
  cerrar(cola);
  mq_unlink(nombre_cola);
  return 0;
}

int atributos(mqd_t cola, char *nombre)
{
  struct mq_attr mqa;

  if (mq_getattr(cola, &mqa) < 0)
    perror("mq_getattr");
  else {
    printf("MQD: %s tiene %ld msjs (max %ld, max tamaño %ld, modo %s)\n",
    nombre, mqa.mq_curmsgs, mqa.mq_maxmsg, mqa.mq_msgsize, (mqa.mq_flags & O_NONBLOCK) ? "sin-bloqueo" : "bloqueo");
  }
  return 0;
}

void crear_colas(void)
{
  mqd_t mqd;

  mq_unlink("/cola1");
  mq_unlink("/cola2");
  mq_unlink("/cola3");
  mq_unlink("/cola4");
  mq_unlink("/cola5");
  mq_unlink("/cola6");
  mq_unlink("/cola7");
  mq_unlink("/cola8");
  mq_unlink("/cola9");
  mq_unlink("/cola0");

  mqd = nueva("/cola1");
  cerrar(mqd);
  mqd = nueva("/cola2");
  cerrar(mqd);
  mqd = nueva("/cola3");
  cerrar(mqd);
  mqd = nueva("/cola4");
  cerrar(mqd);
  mqd = nueva("/cola5");
  cerrar(mqd);
}

////
void enviar_esp(mqd_t mqd, char tipo, char contador, char dato, char *nombre)
{
  Msj *msj;

  if((msj = (Msj *)malloc(sizeof(Msj))) == NULL)
    printf("error al enviar mensaje\n");

  msj->tipo = tipo;
  msj->contador = contador;
  msj->dato = dato;
  msj->otrodato = '0';
  msj->nombre = nombre;
  msj->texto = NULL;
  enviar(mqd, (char *)msj);
  //liberar_msj(&msj);
}

void enviar_espwr(mqd_t mqd, char tipo, char contador, char dato, int otrodato, char *nombre, char *texto)
{
  Msj *msj = (Msj *)malloc(sizeof(Msj));

  msj->tipo = tipo;
  msj->contador = contador;
  msj->dato = dato;
  msj->otrodato = otrodato;
  msj->nombre = nombre;
  msj->texto = texto;
  enviar(mqd, (char *)msj);
}

void reenvio_msj(mqd_t mqd, Msj *msj, char tipo)
{
  switch(tipo) {
    case 'w':
      enviar_espwr(mqd, 'w', msj->contador-1, msj->dato, msj->otrodato, msj->nombre, msj->texto);
      break;
    case 'r':
      enviar_espwr(mqd, 'r', msj->contador-1, msj->dato, msj->otrodato, msj->nombre, msj->texto);
      break;
    default:
      enviar_esp(mqd,tipo,msj->contador-1,msj->dato,msj->nombre);
      break;
  }
}

Msj* recibir_esp(mqd_t mqd)
{
  char *buff;

  if ((buff = (char *)malloc(MAXSIZE_COLA*sizeof(char))) == NULL)
    printf("Error no es posible recibir un msj por el anillo\n");

  recibir(mqd, buff);
  //printf("recibio algo\n");
  return (Msj *)buff;
}

void liberar_msj(Msj **msj)
{
  free(*msj);
  msj = NULL;
}

void imprimir_msj(Msj *msj)
{
  printf("%c\n",msj->tipo);
  printf("%c\n",msj->contador);
  printf("%c\n",msj->dato);
  printf("%c\n",msj->otrodato);
  if(msj->nombre == NULL)
    printf("sin nombre\n");
  else
    printf("%s\n",msj->nombre);
  if(msj->texto == NULL)
    printf("sin texto\n");
  else
    printf("%s\n",msj->texto);
  printf("\n");
}
