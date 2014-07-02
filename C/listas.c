#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"

extern ListaDescriptores *descriptores;
extern int fd;

pthread_mutex_t m;

//Agrega un archivo a la lista de archivos, al inicio
int nuevo_archivo(ListaArchivos *lista, char *nombre)
{
  Archivo *ptr;

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
  if(lista->t == 0)
    lista->fin = ptr;
  lista->inicio = ptr;
  lista->t++;
  return 0;
}

int eliminar_archivo(ListaArchivos *lista, char *nombre)
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

Archivo *buscar_archivo(ListaArchivos *lista, char *nombre)
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

ListaArchivos *crear_lista_archivos(int n)
{
  ListaArchivos *lista;

  if ((lista = (ListaArchivos *)malloc (sizeof(ListaArchivos))) == NULL)
    printf("Error al crear el buffer del worker nº %d\n",n);

  lista->inicio = NULL;
  lista->fin = NULL;
  lista->t = 0;
  lista->worker = (char)(((int)'0')+n-1);

  return lista;
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

void imprimir_archivos (ListaArchivos *lista)
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

//El acceso a la lista de descriptores debe ser atómica

int nuevo_descriptor(ListaDescriptores *des, char *nombre, int worker_c, int worker_a)
{
  DescriptorArchivo *ptr;
  
  if((ptr = (DescriptorArchivo*)malloc(sizeof(DescriptorArchivo)))== NULL)
    return -1;
  if ((ptr->nombre = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    return -1;
  strcpy(ptr->nombre, nombre);
  ptr->worker_c = worker_c;
  ptr->worker_a = worker_a;

  pthread_mutex_lock(&m);
  ptr->fd = fd;
  fd++;
  ptr->proximo = des->inicio;
  des->inicio = ptr;
  des->t++;
  pthread_mutex_unlock(&m);

  return ptr->fd;
}

int borrar_descriptor(ListaDescriptores *des, char *nombre)
{
  DescriptorArchivo *ptr = des->inicio;
  DescriptorArchivo *tmp = NULL;

  pthread_mutex_lock(&m);
  if(des->t==1) {
    des->inicio = NULL;
    des->fin = NULL;
    des->t--;
    free(ptr->nombre);
    free(ptr);
    pthread_mutex_unlock(&m);
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
      pthread_mutex_unlock(&m);
      return 0;
    }
    else {
      tmp = ptr;
      ptr = ptr->proximo;
    }
  }
  pthread_mutex_unlock(&m);

  return -1;
}

//el tipo puede ser:
//- 0: busca por nombre
//- >0: busca por fd
DescriptorArchivo *buscar_descriptor(ListaDescriptores *des, char *dato, int descript, int tipo)
{
  DescriptorArchivo *ptr = des->inicio;

  pthread_mutex_lock(&m);
  if(tipo) {
    while(ptr != NULL) {
      if(descript == ptr->fd) {
        pthread_mutex_unlock(&m);
        return ptr;
      }
      ptr = ptr->proximo;
    }
  }
  else {
    while(ptr != NULL) {
      if(strcmp(dato, ptr->nombre) == 0) {
        pthread_mutex_unlock(&m);
        return ptr;
      }
      ptr = ptr->proximo;
    }
  }
  pthread_mutex_unlock(&m);

  return NULL;
}

ListaDescriptores *crear_lista_descriptores()
{
  ListaDescriptores *des;

  if ((des = (ListaDescriptores *)malloc (sizeof(ListaDescriptores))) == NULL)
    printf("error al crear el buffer de descriptores\n");

  des->inicio = NULL;
  des->fin = NULL;
  des->t = 0;

  return des;
}

void imprimir_descriptores(ListaDescriptores *des)
{
  DescriptorArchivo *actual = des->inicio;

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

char* concatenar_archivos(ListaArchivos *lista)
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
