#include <mqueue.h>

#define MAXMSG_COLA 5
#define MAXSIZE_COLA 512
#define N_WORKER 5

typedef struct colas {
  mqd_t cola_worker;
  mqd_t cola_disp;
  mqd_t cola_anillo;
} Colas;

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
Lista *crear_lista(int n);
int nuevo_archivo(Lista *lista, char *nombre);
char* concatenar_archivos(Lista *lista);
void visualizacion (Lista *lista);
int eliminar_archivo(Lista *lista, char *nombre);
int modificar_estado(Archivo *elem);
Archivo *buscar_archivo(Lista *lista, char *nombre);
///
void crear_buff_des();
int ins_descriptor(ListaDes *des, char *nombre, int worker_c, int worker_a);
int del_descriptor(ListaDes *des, char *nombre);
void imprimir_des(ListaDes *des);
Descriptor *busca_des(ListaDes *des, char *dato, int descript, int tipo);
///
char *cola_cadena(char *token);
char *sacar_nueva_linea(char *nombrex);
///
//Colas inicializar_cola(mqd_t worker, mqd_t disp, mqd_t anillo);


//mensajes.c
mqd_t crear_cola(int cola);
mqd_t nueva(char *nombre);
mqd_t abrir(char *nombre);
int cerrar(mqd_t cola);
int borrar(mqd_t cola, char *nombre_cola);
int atributos(mqd_t cola, char *nombre);
void imprimir_msj(Msj *msj);
void enviar(mqd_t mqd, char tipo, char contador, char dato, char *nombre);
void liberar_msj(Msj *msj);
void enviarWR(mqd_t mqd, char tipo, char contador, char dato, int otrodato, char *nombre, char *texto);
Msj* recibir(mqd_t mqd);
void reenvio_msj(mqd_t mqd, Msj *msj, char tipo);


//workers.c
void *fs (void * arg);

//dispatcher.c
void *handle_client(void *arg);
