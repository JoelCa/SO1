#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cabecera.h"

#define MAXMSG_QUEUE 5

//-------------------------------------------------------
//Operaciones que actuan sobre la cola de mensajes POSIX.
//-------------------------------------------------------

mqd_t nueva(char *nombre)
{
  struct mq_attr atributos;
  mqd_t cola;

  atributos.mq_maxmsg = MAXMSG_QUEUE;
  atributos.mq_msgsize = sizeof(Msj);
  atributos.mq_flags = 0;

  if ((cola = mq_open(nombre, O_CREAT | O_RDWR, S_IRWXU, &atributos)) == (mqd_t)-1)
    perror ("mq_open");
  return cola;
}

mqd_t abrir(char *nombre)
{
  mqd_t cola;

  if ((cola = mq_open(nombre, O_RDWR)) == (mqd_t)-1)
    perror("mq_open");
  return cola;
}

int enviar_msj(mqd_t cola, char *mensaje)
{
  if(mq_send(cola, mensaje, sizeof(Msj), 0) == -1) 
    perror("mq_send");
  return 0;
}

int recibir_msj(mqd_t cola, char *buf)
{
  if (mq_receive(cola, buf, sizeof(Msj), NULL) == -1) 
    perror("mq_receive");
  return 0;
}

int cerrar(mqd_t cola)
{
  mq_close(cola);
  return 0;
}

int borrar(char *nombre_cola)
{
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

//Retorna el mqd de la cola creada (no la cierra).
mqd_t crear_cola(int cola)
{
  mqd_t mqd;
  char *buff;
  
  if((buff = (char *) malloc(7*sizeof(char))) == NULL)
     printf("Error al crear cola de mensajes");
  sprintf(buff, "/cola%d", cola);
  mq_unlink(buff);
  mqd = nueva(buff);
  return mqd;
}

//-------------------------------------
//Operaciones sobre la estructura: Msj.
//-------------------------------------

void enviar(mqd_t mqd, char tipo, char contador, char dato, char *nombre)
{
  Msj *msj;

  if((msj = (Msj *) calloc(1, sizeof(Msj))) == NULL)
    printf("Error al enviar mensaje\n");
  msj->tipo = tipo;
  msj->contador = contador;
  msj->dato = dato;
  msj->otrodato = '0';
  msj->nombre = nombre;
  msj->texto = NULL;
  enviar_msj(mqd, (char *)msj);
  //free(msj);
}

void enviarWR(mqd_t mqd, char tipo, char contador, char dato, int otrodato, char *nombre, char *texto)
{
  Msj *msj = (Msj *)calloc(1, sizeof(Msj));

  msj->tipo = tipo;
  msj->contador = contador;
  msj->dato = dato;
  msj->otrodato = otrodato;
  msj->nombre = nombre;
  msj->texto = texto;
  enviar_msj(mqd, (char *)msj);
}

void reenvio_msj(mqd_t mqd, Msj *msj, char tipo)
{
  switch(tipo) {
    case 'w':
      enviarWR(mqd, 'w', msj->contador-1, msj->dato, msj->otrodato, msj->nombre, msj->texto);
      break;
    case 'r':
      enviarWR(mqd, 'r', msj->contador-1, msj->dato, msj->otrodato, msj->nombre, msj->texto);
      break;
    default:
      enviar(mqd,tipo,msj->contador-1,msj->dato,msj->nombre);
      break;
  }
}

Msj* recibir(mqd_t mqd)
{
  char *buff;

  if ((buff = (char *)malloc(sizeof(Msj))) == NULL)
    printf("Error no es posible recibir un msj por el anillo\n");
  recibir_msj(mqd, buff);
  return (Msj *)buff;
}

void liberar_msj(Msj *msj)
{
  if(msj == NULL)
    return;
  free(msj->nombre);
  free(msj->texto);
  free(msj);
  msj = NULL;
}

void imprimir_msj(Msj *msj)
{
  printf("El tipo: %c\n",msj->tipo);
  printf("El contador: %c\n",msj->contador);
  printf("El dato: %c\n",msj->dato);
  printf("El otrodato: %d\n",msj->otrodato);
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

//-------------------------------------------------
//Operaciones sobre la estructura: DescriptorColas.
//-------------------------------------------------

DescriptorColas* nueva_cola_mensaje(int worker, char tipo)
{
  DescriptorColas *cola = (DescriptorColas*)calloc(1, sizeof(DescriptorColas));

  switch(tipo) {
    case 'w':
      cola->worker = crear_cola(worker);
      break;

    case 'd':
      cola->disp = crear_cola((worker+NUM_WORKER)%(2*NUM_WORKER));
      break;
  }
  return cola;
}

void borrar_cola_mensaje(DescriptorColas* cola, int worker, char tipo)
{
  char nombre[7];

  switch(tipo) {
    case 'w':
      cerrar(cola->worker);
      cerrar(cola->anillo);
      sprintf(nombre, "/cola%d", worker);
      borrar(nombre);
      break;

    case 'd':
      cerrar(cola->disp);
      cerrar(cola->worker);
      sprintf(nombre, "/cola%d", (worker+NUM_WORKER)%(2*NUM_WORKER));
      borrar(nombre);
      break;
  }
  free(cola);
}

void imprimir_cola(DescriptorColas* cola)
{
  if(cola != NULL) {
    if(cola->worker != 0)
      printf("Cola worker: %d\n",(int)cola->worker);
    if(cola->anillo != 0)
      printf("Cola anillo: %d\n",(int)cola->anillo);
    if(cola->disp != 0)
      printf("Cola dispatcher: %d\n",(int)cola->disp);
    printf("\n");
  }
}
