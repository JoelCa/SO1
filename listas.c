#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"

extern Lista *lista1;
extern Lista *lista2;
extern Lista *lista3;
extern Lista *lista4;
extern Lista *lista5;
extern ListaDes *des;
extern int fd;
pthread_mutex_t m;

void inicializacion (Lista *lista, char *cola, char *anillo)
{
  lista->inicio = NULL;
  lista->fin = NULL;
  lista->t = 0;
  lista->cola = cola;
  lista->anillo = anillo;
}

int ins_lista_vacia(Lista *lista, char *nombre)
{
  Archivo *ptr = (Archivo*)malloc(sizeof(Archivo));

  if (ptr == NULL) {
    return -1;}
  if ((ptr->nombre = (char*)malloc(MAXSIZE_COLA * sizeof(char))) == NULL)
    return -1;
  if ((ptr->texto = (char*)malloc(MAXSIZE_COLA * sizeof(char))) == NULL)
    return -1; 
  strcpy(ptr->nombre, nombre);
  ptr->estado = 0;
  ptr->indice = 0;
  ptr->tam = 0;
  ptr->proximo = NULL;
  lista->inicio = ptr;
  lista->fin = ptr;
  lista->t = 1;
  return 0;
}

//agrega un archivo al inicio
int ins_lista(Lista *lista, char *nombre)
{
  Archivo *ptr;

  if(lista->t == 0)
    return (ins_lista_vacia(lista, nombre));
  if((ptr = (Archivo*)malloc(sizeof(Archivo)))== NULL)
    return -1;
  if ((ptr->nombre = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    return -1;
  if ((ptr->texto = (char *)malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    return -1;
  strcpy(ptr->nombre, nombre);
  ptr->estado = 0;
  ptr->indice = 0;
  ptr->tam = 0;
  ptr->proximo = lista->inicio;
  lista->inicio = ptr;
  lista->t++;
  return 0;
}

int del_lista(Lista *lista, char *nombre)
{
  Archivo *ptr = lista->inicio;
  Archivo *tmp = NULL;
  int existe = 0;

  while(ptr != NULL) {
    if(strcmp(nombre, ptr->nombre) == 0) {
      existe = 1;
      break;
    }
    else {
      tmp = ptr;
      ptr = ptr->proximo;
    }
  }
  if(existe == 0)
    return -1;
  if(lista->t==1) {
    lista->inicio = NULL;
    lista->fin = NULL;
    lista->t--;
  }
  else {
    if(ptr->proximo == NULL) {
      tmp->proximo = NULL;
      lista->fin = tmp;
      lista->t--;
    } else if(tmp == NULL) {
      lista->inicio = lista->inicio->proximo;
      lista->t--;

    } else {
      tmp->proximo = tmp->proximo->proximo;
      lista->fin = ptr->proximo;
      lista->t--;
    }
  }
  free(ptr->nombre);
  free(ptr->texto);
  free(ptr);
  return 0;
}

Archivo *busca_lista(Lista *lista, char *nombre)
{
  Archivo *ptr = lista->inicio;

  while(ptr != NULL) {
    if(strcmp(nombre, ptr->nombre) == 0) {
      return ptr;
    }
    ptr = ptr->proximo;
  }
  return NULL;
}


void crear_listas()
{
  if ((lista1 = (Lista *)malloc (sizeof(Lista))) == NULL)
    printf("error al crear el buffer del worker 1\n");
  if ((lista2 = (Lista *)malloc (sizeof(Lista))) == NULL)
        printf("error al crear el buffer del worker 2\n");
  if ((lista3 = (Lista *)malloc (sizeof(Lista))) == NULL)
        printf("error al crear el buffer del worker 3\n");
  if ((lista4 = (Lista *)malloc (sizeof(Lista))) == NULL)
        printf("error al crear el buffer del worker 4\n");
  if ((lista5 = (Lista *)malloc (sizeof(Lista))) == NULL)
        printf("error al crear el buffer del worker 5\n");
  inicializacion(lista1, "/cola1", "/cola2");
  inicializacion(lista2, "/cola2", "/cola3");
  inicializacion(lista3, "/cola3", "/cola4");
  inicializacion(lista4, "/cola4", "/cola5");
  inicializacion(lista5, "/cola5", "/cola1");
}

void imprimir_arch(Archivo *arch)
{
  if(arch == NULL)
    printf("sin archivo\n");
  else {
    printf ("Imprimir archivo:\n%p\n", arch);
    printf ("%s\n", arch->nombre);
    if(arch->texto == NULL)
      printf("sin texto\n");
    else 
      printf ("%s\n", arch->texto);
    printf("%d\n", arch->estado);
    printf("%d\n", arch->indice);
    printf("%d\n", arch->tam);
  }
    printf("\n");
}

void visualizacion (Lista *lista)
{
  Archivo *actual = lista->inicio;

  if(actual == NULL)
    printf("Buffer Vacio\n");
  while (actual != NULL){
    printf ("%p - %s\n", actual, actual->nombre);
    imprimir_arch(actual);
    actual = actual->proximo;
  }
}

//////////

void inicializar_des(ListaDes *des)
{
  des->inicio = NULL;
  des->fin = NULL;
  des->t = 0;
}

int ins_descriptor_vacio(ListaDes *des, char *nombre, int worker_c, int worker_a)
{
  int aux;

  Descriptor *ptr = (Descriptor*)malloc(sizeof(Descriptor));
  
  if(ptr == NULL)
    return -1;
  if ((ptr->nombre = (char *)malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    return -1; 
  strcpy(ptr->nombre, nombre);
  ptr->worker_c = worker_c;
  ptr->worker_a = worker_a;
  ptr->fd = fd;
  ptr->proximo = NULL;
  pthread_mutex_lock(&m);
  aux = fd;
  fd++;
  pthread_mutex_unlock(&m);
  des->inicio = ptr;
  des->fin = ptr;
  des->t = 1;
  return aux;
}

int ins_descriptor(ListaDes *des, char *nombre, int worker_c, int worker_a)
{
  int aux;
  Descriptor *ptr;
  
  if(des->t == 0)
    return (ins_descriptor_vacio(des, nombre, worker_c, worker_a));
  if((ptr = (Descriptor*)malloc(sizeof(Descriptor)))== NULL)
    return -1;
  if ((ptr->nombre = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    return -1;
  strcpy(ptr->nombre, nombre);
  ptr->worker_c = worker_c;
  ptr->worker_a = worker_a;
  ptr->fd = fd;
  pthread_mutex_lock(&m);
  aux = fd;
  fd++;
  pthread_mutex_unlock(&m);
  ptr->proximo = des->inicio;
  des->inicio = ptr;
  des->t++;
  return aux;
}

int del_descriptor(ListaDes *des, char *nombre)
{
  Descriptor *ptr = des->inicio;
  Descriptor *tmp = NULL;

  if(des->t==1) {
    des->inicio = NULL;
    des->fin = NULL;
    des->t--;
    free(ptr->nombre);
    free(ptr);
    return 0;
  }
  while(ptr != NULL) {
    if(strcmp(nombre, ptr->nombre) == 0) {
      if(ptr->proximo == NULL) {
        tmp->proximo = NULL;
        des->fin = tmp;
        des->t--;
      } else if(tmp == NULL) {
        des->inicio = des->inicio->proximo;
        des->t--;
      }
      else {
        tmp->proximo = tmp->proximo->proximo;
        des->fin = ptr->proximo;
        des->t--;
      }
      free(ptr->nombre);
      free(ptr);
      return 0;
    }
    else {
      tmp = ptr;
      ptr = ptr->proximo;
    }
  }
  return -1;
}

//el tipo puede ser:
//- 0: busca por nombre
//- >0: busca por fd
Descriptor *busca_des(ListaDes *des, char *dato, int descript, int tipo)
{
  Descriptor *ptr = des->inicio;

  if(tipo) {
    while(ptr != NULL) {
      if(descript == ptr->fd)
        return ptr;
      ptr = ptr->proximo;
    }
  }
  else {
    while(ptr != NULL) {
      if(strcmp(dato, ptr->nombre) == 0)
        return ptr;
      ptr = ptr->proximo;
    }
  }
  return NULL;
}

void crear_buff_des()
{
    if ((des = (ListaDes *)malloc (sizeof(ListaDes))) == NULL)
        printf("error al crear el buffer de descriptores\n");
    inicializar_des(des);
}

void imprimir_des(ListaDes *des)
{
  Descriptor *actual = des->inicio;

  printf("Estado buff del descriptor:\n");
  if(actual == NULL)
    printf("Buffer del descriptor vacio\n");
  while (actual != NULL){
    printf ("%p\n", actual);
    printf ("%s", actual->nombre);
    printf ("%d\n", actual->worker_c);
    printf ("%d\n", actual->worker_a);
    printf ("%d\n", actual->fd);
    actual = actual->proximo;
  }
  printf("\n");
}

////////

//abre lo cerrado
int modificar_estado(Archivo *elem)
{
  if(elem->estado == 0) {
    elem->estado = 1;
    return 1;
  }
  return 0;
}

char *sacar_nueva_linea(char *nombre)
{
  char *copia;
  int n;

  n = strlen(nombre)-2;
  if((copia = (char *)malloc(n * sizeof (char))) == NULL)
    printf("Error al eliminar nueva linea\n");
  memcpy(copia, nombre, n);
  copia[n] = '\0';
  return copia;
}

char* concatenar_archivos(Lista *lista)
{
  Archivo *ptr = lista->inicio;
  char *nombres, *aux;

  if((nombres = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    printf("Error en la concatenación de nombres de archivos\n");
  while(ptr != NULL) {
    if(ptr->nombre != NULL) {
      aux = sacar_nueva_linea(ptr->nombre);
      strcat(nombres," ");
      strcat(nombres,aux);
    
    }
    ptr = ptr->proximo;
  }
  return nombres;
}

char *cola_cadena(char *token)
{
  char *buff, *buff0, *buff1, *buff2;
  char delims[] = " ";
  int i;

  if((buff = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    printf("Error\n");
  for(i=0;i<MAXSIZE_COLA;i++)
    buff[i] = '\0';
  if((buff0 = strtok(NULL, delims)) == NULL)
    return sacar_nueva_linea(token);
  strcat(buff,token);
  buff2 = buff0;
  while((buff1 = strtok(NULL, delims))!= NULL) {
      strcat(buff, " ");
      strcat(buff, buff0);
      strcat(buff, " ");
    if((buff2 = strtok(NULL, delims)) == NULL) {
      strcat(buff,sacar_nueva_linea(buff1));
      break;
    }
    strcat(buff, buff1);
    buff0 = buff2;
  }
  if(buff1 == NULL) {
    strcat(buff, " ");
    strcat(buff,sacar_nueva_linea(buff2));
  }
  return buff;
}
