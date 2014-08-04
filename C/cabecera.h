#include <mqueue.h>

#define MAXMSG_COLA 5
#define MAXSIZE_COLA 512
#define N_WORKER 5 //NO cambiar

typedef struct colas {
  mqd_t worker;
  mqd_t disp;
  mqd_t anillo;
} DescriptorColas;

typedef struct des_{
  char *nombre;
  int worker_c;
  int worker_a;
  int fd;
  struct des_ *proximo;
} DescriptorArchivo;

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
  int t;
  char worker;
} ListaArchivos;

typedef struct ListaDes {
  DescriptorArchivo *inicio;
  int t;
} ListaDescriptores;

typedef struct msj_{
  char tipo;
  char contador;
  char dato;
  int otrodato;
  char *nombre;
  char *texto;
} Msj;


//listas.c

//Operaciones sobre archivos
ListaArchivos *crear_lista_archivos(int n);
int nuevo_archivo(ListaArchivos *lista, char *nombre);
int eliminar_archivo(ListaArchivos *lista, char *nombre);
Archivo *buscar_archivo(ListaArchivos *lista, char *nombre);
int modificar_estado(Archivo *arch);
char* concatenar_archivos(ListaArchivos *lista);
void imprimir_archivos(ListaArchivos *lista);


//Operaciones sobre descriptores de archivos
ListaDescriptores *crear_lista_descriptores();
int nuevo_descriptor(ListaDescriptores *des, char *nombre, int worker_c, int worker_a);
DescriptorArchivo *borrar_descriptor(ListaDescriptores *des, char *nombre);
DescriptorArchivo *buscar_descriptor(ListaDescriptores *des, char *dato, int descript, int tipo);
void imprimir_descriptores(ListaDescriptores *des);

///

char *cola_cadena(char *token);
char *sacar_nueva_linea(char *nombre);


//Operadores sobre las colas de mensajes
DescriptorColas *nueva_cola_mensaje(int worker, char tipo);
void borrar_cola_mensaje(DescriptorColas* cola, int worker, char tipo);
void imprimir_cola(DescriptorColas* cola);

//mensajes.c
mqd_t crear_cola(int cola);
mqd_t nueva(char *nombre);
mqd_t abrir(char *nombre);
int cerrar(mqd_t cola);
int borrar(char *nombre_cola);
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
