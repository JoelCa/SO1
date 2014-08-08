#include <mqueue.h>

#define NUM_WORKER 5
#define MAXSIZE_TEXT 512

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
//void imprimir_archivos(ListaArchivos *lista);

//Operaciones sobre tokens
char *cola_cadena(char *token);
char *sacar_nueva_linea(char *nombre);

//Operaciones sobre descriptores de archivos
ListaDescriptores *crear_lista_descriptores();
int nuevo_descriptor(ListaDescriptores *des, char *nombre, int worker_c, int worker_a);
DescriptorArchivo *borrar_descriptor(ListaDescriptores *des, char *nombre);
DescriptorArchivo *buscar_descriptor(ListaDescriptores *des, char *dato, int descript, int tipo);
//void imprimir_descriptores(ListaDescriptores *des);


//mensaje.c

//Operadores sobre las colas de mensajes
DescriptorColas *nueva_cola_mensaje(int worker, char tipo);
void borrar_cola_mensaje(DescriptorColas* cola, int worker, char tipo);
mqd_t abrir(char *nombre);
int cerrar(mqd_t cola);
//void imprimir_cola(DescriptorColas* cola);

//Operaciones sobre los mensajes
void enviar(mqd_t mqd, char tipo, char contador, char dato, char *nombre);
void enviarWR(mqd_t mqd, char tipo, char contador, char dato, int otrodato, char *nombre, char *texto);
void reenvio_msj(mqd_t mqd, Msj *msj, char tipo);
Msj* recibir(mqd_t mqd);
void liberar_msj(Msj *msj);
//void imprimir_msj(Msj *msj);
