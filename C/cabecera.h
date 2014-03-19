#include <mqueue.h>

#define MAXMSG_COLA 5
#define MAXSIZE_COLA 512

//listas.c
typedef struct archivo_ {
  char *nombre;
  char *texto;
  int estado;
  int indice;
  int tam;
  struct archivo_ *proximo;
} Archivo;

typedef struct lista_ {
  Archivo *inicio;
  Archivo *fin;
  int t;
  char *cola;
  char *anillo;
} Lista;

typedef struct des_{
  char *nombre;
  int worker_c;
  int worker_a;
  int fd;
  struct des_ *proximo;
} Descriptor;

typedef struct ListaDes {
  Descriptor *inicio;
  Descriptor *fin;
  int t;
} ListaDes;

//mensajes.c
typedef struct msj_{
  char tipo;
  char contador;
  char dato;
  int otrodato;
  char *nombre;
  char *texto;
} Msj;


//listas.c
void crear_listas();
int ins_lista(Lista *lista, char *nombre);
char* concatenar_archivos(Lista *lista);
void visualizacion (Lista *lista);
int del_lista(Lista *lista, char *nombre);
int modificar_estado(Archivo *elem);
Archivo *busca_lista(Lista *lista, char *nombre);
///
void crear_buff_des();
int ins_descriptor(ListaDes *des, char *nombre, int worker_c, int worker_a);
int del_descriptor(ListaDes *des, char *nombre);
void imprimir_des(ListaDes *des);
Descriptor *busca_des(ListaDes *des, char *dato, int descript, int tipo);
///
char *cola_cadena(char *token);
char *sacar_nueva_linea(char *nombrex);


//mensajes.c
void crear_colas();
mqd_t nueva(char *nombre);
mqd_t abrir(char *nombre);
int cerrar(mqd_t cola);
int borrar(mqd_t cola, char *nombre_cola);
int atributos(mqd_t cola, char *nombre);
void imprimir_msj(Msj *msj);
void enviar_esp(mqd_t mqd, char tipo, char contador, char dato, char *nombre);
void liberar_msj(Msj **msj);
void enviar_espwr(mqd_t mqd, char tipo, char contador, char dato, int otrodato, char *nombre, char *texto);
Msj* recibir_esp(mqd_t mqd);
void reenvio_msj(mqd_t mqd, Msj *msj, char tipo);


//workers.c
void *fs (void * arg);

//dispatcher.c
void *handle_client(void *arg);
