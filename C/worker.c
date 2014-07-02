#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

extern pthread_barrier_t barrier; 

//IDEA VIEJA: antes de que cada worker reciba un mensaje:
//Abre tanto la cola del dispatcher como la cola de su vecino, y las mantiene abiertas.
//ACTUALIZACIÓN: Ahora el worker NO crea la cola del dispatcher, solo la abre cada vez
//que le llega un mensaje (como antes)

void reenvio_por_anillo(DescriptorColas *cola, Msj *msj, ListaArchivos *lista, char *candidato)
{
  char *buff;
  Archivo *aux;
  char worker;
  int size;
  
  worker = lista->worker;

  switch(msj->tipo) {
    case 'l': //LSD
      if((buff = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
        printf("LSD: error\n");  
      buff = concatenar_archivos(lista);
      strcat(buff, msj->nombre);
      enviar(cola->anillo, 'l', msj->contador-1, 'f', buff);
      break;

    case 'd': //DEL
      switch(msj->dato) {
        case 'f':
          if((aux = buscar_archivo(lista, msj->nombre)) != NULL) {
            if(aux->estado == 0) {
              eliminar_archivo(lista,msj->nombre);
              enviar(cola->anillo, 'd', msj->contador-1, 't', msj->nombre);
            }
            else {
              enviar(cola->anillo, 'd', msj->contador-1, 'e', msj->nombre);
            }
          }
          else
            reenvio_msj(cola->anillo, msj, 'd');
          break;

        default:
          reenvio_msj(cola->anillo, msj, 'd');
          break;
      }
      break;

    case 'c': //CRE
      switch(msj->dato) {
        case 'f':
          if(buscar_archivo(lista, msj->nombre) != NULL) {
            if(candidato == NULL)
              enviar(cola->anillo, 'c',msj->contador-1,'t',msj->nombre);
            else {
              if(strcmp(candidato, msj->nombre) == 0)
                enviar(cola->anillo,'c',msj->contador-1,'e',msj->nombre);
              else
                enviar(cola->anillo,'c',msj->contador-1,'t',msj->nombre);
            }
          }
          else {
            reenvio_msj(cola->anillo, msj, 'c');
          }        
          break;

        default:
          reenvio_msj(cola->anillo, msj, 'c');
          break;
      }
      break;

    case 'o': //OPN
      switch(msj->dato) {
        case 'f':
          aux = buscar_archivo(lista, msj->nombre);
          if(aux != NULL) {
            if(modificar_estado(aux))
              enviar(cola->anillo,'o',msj->contador-1,worker,msj->nombre);
            else
              enviar(cola->anillo,'o',msj->contador-1,'e',msj->nombre);
          }
          else
            enviar(cola->anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;

        case 'e':
          enviar(cola->anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;

        default:
          enviar(cola->anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;
      }
      break;

    case 'w': //WRT
      switch(msj->dato) {
        case 't':
          reenvio_msj(cola->anillo, msj, 'w');
          break;

        default:
          if(worker == msj->dato) {
            aux = buscar_archivo(lista, msj->nombre);
            strcat(aux->texto, msj->texto);
            aux->tam += msj->otrodato;
            enviar(cola->anillo, 'w', msj->contador-1, 't', NULL);
          }
          else
            reenvio_msj(cola->anillo, msj, 'w');
          break;
      }
      break;

    case 'r': //REA
      switch(msj->dato) {
        case 't':
          reenvio_msj(cola->anillo, msj, 'r');
          break;

        default:
          if(worker == msj->dato) {
            aux = buscar_archivo(lista, msj->nombre);
            if(aux->indice > aux->tam) {
              enviar(cola->anillo, 'r', msj->contador-1, 't', NULL);
            }
            else {
              size = MIN(msj->otrodato, aux->tam - aux->indice);
              if((buff = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
                printf("REA: error\n");
              memcpy(buff, &(aux->texto)[aux->indice], size);
              buff[size] = '\0';
              aux->indice +=  size;
              enviarWR(cola->anillo, 'r', msj->contador-1, 't', size, buff, NULL);
            }
          }
          else
            reenvio_msj(cola->anillo, msj, 'r');
          break;
      }
      break;

    case 's': //CLO
      switch(msj->dato) {
        case 't':
          reenvio_msj(cola->anillo, msj, 's');
          break;

        default:
          if(worker == msj->dato) {
            aux = buscar_archivo(lista, msj->nombre);
            aux->estado = 0;
            aux->indice = 0;
            enviar(cola->anillo,'s',msj->contador-1,'t',NULL);
          }
          else
            reenvio_msj(cola->anillo, msj, 's');
          break;
      }
      break;
  }
}


int evaluar_msj(DescriptorColas *cola, Msj *msj, ListaArchivos *lista, char *candidato)
{
  if(msj->contador == '1') {
    switch(msj->tipo) {
      case 'l': //LSD
        enviar(cola->disp, 'l', '0', 'f', msj->nombre);
        break;

      case 'd': //DEL
        switch(msj->dato) {
          case 't':
            enviar(cola->disp, 'd', '0', '0', NULL);
            break;
          case 'f':
            enviar(cola->disp, 'd', '0', '1', NULL);
            break;
          default:
            enviar(cola->disp, 'd', '0', '2', NULL);
            break;
        }
        break;

      case 'c': //CRE
        switch(msj->dato) {
          case 't':
            enviar(cola->disp, 'c', '0', '1', NULL);
            break;
          case 'f':
            nuevo_archivo(lista, candidato);
            enviar(cola->disp, 'c', '0', '0', NULL);
            break;
          case 'e':
            enviar(cola->disp, 'c', '0', '2', NULL);
            break;
        }
        break;

      case 'o': //OPN
        switch(msj->dato) {
          case 'f':
            enviar(cola->disp, 'o', '0', '2', NULL);
            break;
          case 'e':
            enviar(cola->disp, 'o', '0', '1', NULL);
            break;
          default:
            enviar(cola->disp, 'o', msj->dato, '0', NULL);
            break;
            
        }
        break;

      case 'w': //WRT
        switch(msj->dato) {
          case 't':
            enviar(cola->disp, 'w', '0', '0', NULL);
            break;
        }
        break;

      case 'r': //REA
        switch(msj->dato) {
          case 't':
            enviarWR(cola->disp, 'r', '0', '0', msj->otrodato, msj->nombre, NULL);
            break;
        }
        break;

      case 's': //CLO
        switch(msj->dato) {
          case 't':
            enviar(cola->disp,'s', '0', '0', NULL);
            break;
        }
        break;
    }
    return 1;
  }

  reenvio_por_anillo(cola, msj, lista, NULL);
  liberar_msj(msj);
  return 0;
}


int recibir_msjs(DescriptorColas *cola, ListaArchivos *lista, char *candidato)
{
  Msj *msj;

  while(1) {
    msj = recibir(cola->worker);
    printf("llego al worker inicial\n");
    imprimir_cola(cola);
    imprimir_msj(msj);
    if(evaluar_msj(cola, msj, lista, candidato))
      break;
  }
  return 0;
}


void operadorLSD(DescriptorColas *cola, ListaArchivos *lista)
{
  char *nombres;

  if((nombres = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    printf("LSD: error\n");
  nombres = concatenar_archivos(lista);
  enviar(cola->anillo,'l', '5', 'f', nombres);
  recibir_msjs(cola, lista, NULL);
  free(nombres);
}

void operadorDEL(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;

  if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
    if(arch->estado == 0) {
      eliminar_archivo(lista, msj->nombre);
      enviar(cola->disp, 'd', '0', '0', NULL);
    }
    else
      enviar(cola->disp, 'd', '0', 'e', NULL);
  }
  else {
    enviar(cola->anillo,'d','5','f',msj->nombre);
    recibir_msjs(cola, lista, NULL);
  }
}

void operadorCRE(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  if(buscar_archivo(lista, msj->nombre) != NULL) {
    enviar(cola->disp, 'c', '0', '1', NULL);
  }
  else {
    enviar(cola->anillo,'c','5','f',msj->nombre);
    recibir_msjs(cola, lista, msj->nombre);
  }
}

void operadorOPN(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;
  
  if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
    if(modificar_estado(arch)) 
      enviar(cola->disp, 'o', lista->worker, '0', NULL);
    else
      enviar(cola->disp, 'o', '0', '1', NULL);
  }
  else {
    enviar(cola->anillo,'o','5','f',msj->nombre);
    recibir_msjs(cola, lista, NULL);
  }
}

void operadorWRT(int worker, DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;
  
  //printf("EN WORKER %d:\nel msj->contador es: %c\n", worker, msj->contador);
  if(msj->contador -'0' == worker) {
    arch = buscar_archivo(lista, msj->nombre);
    strcat(arch->texto, msj->texto);
    arch->tam += msj->otrodato;
    enviar(cola->disp, 'w', '0', '0', NULL);
  }
  else {
    enviarWR(cola->anillo,'w','5',msj->contador, msj->otrodato, msj->nombre, msj->texto);
    recibir_msjs(cola, lista, NULL);
  }
}

void operadorREA(int worker, DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  int size;
  char *texto;
  Archivo *arch;

  if(msj->contador -'0' == worker) {
    arch = buscar_archivo(lista, msj->nombre);
    if(arch->indice > arch->tam)
      enviar(cola->disp, 'r', '0', '0', NULL);
    else {
      size = MIN(msj->otrodato, arch->tam - arch->indice);
      if((texto = (char *) malloc(size * sizeof (char))) == NULL)
        printf("REA: error\n");
      memcpy(texto, &(arch->texto)[arch->indice], size);
      texto[size] = '\0';
      arch->indice += size;
      enviarWR(cola->disp,'r', '0', '0', size, texto, NULL);
      //free(texto);
    }
  }
  else {
    enviarWR(cola->anillo,'r','5',msj->contador, msj->otrodato, msj->nombre, NULL);
    recibir_msjs(cola, lista, NULL);
  }
}

void operadorCLO(int worker, DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;

  if(msj->contador -'0' == worker) {
    printf("ACA en worker %d\n", worker);
    arch = buscar_archivo(lista, msj->nombre);
    arch->estado = 0;
    arch->indice = 0;
    printf("por enviar al dispatcher\n");
    enviar(cola->disp, 's', '0', '0', NULL);
    printf("enviado\n");
  }
  else {
    enviar(cola->anillo,'s','5',msj->contador,msj->nombre);
    recibir_msjs(cola, lista, NULL);
  }
}


void *fs (void * arg)
{
  int worker;
  char cola_d[7], cola_a[7];
  Msj *msj;
  DescriptorColas *cola;
  ListaArchivos *lista;

  worker = *(int *)arg;
  //crear_cola(worker);
  lista = crear_lista_archivos(worker);

  worker--;
  sprintf(cola_d, "/cola%d", (worker+N_WORKER)%(2*N_WORKER));
  sprintf(cola_a, "/cola%d", (worker+1)%N_WORKER);

  cola = nueva_cola_mensaje(worker, 'w');
  
  //creo que esta barrera sirve
  //para que un worker abra una cola, despues de que halla sido creada
  pthread_barrier_wait(&barrier);
  
  cola->anillo = abrir(cola_a);

  printf("la cola dispatcher del worker %d: %s\n", worker, cola_d);
  printf("la cola anillo del worker %d: %s\n", worker, cola_a);

  while(1) {
    msj = recibir(cola->worker);
    imprimir_msj(msj);
    printf("la cola del worker %d\n", worker);
    imprimir_cola(cola);
    if(msj->dato == 'm') {
      cola->disp = abrir(cola_d);
      switch(msj->tipo) {
        case 'l': //LSD
          operadorLSD(cola, lista);
          break;

        case 'd': //DEL
          operadorDEL(cola, lista, msj);
          break;

        case 'c': //CRE
          operadorCRE(cola, lista, msj);
          break;

        case 'o': //OPN
          operadorOPN(cola, lista, msj);
          break;

        case 'w': //WRT
          operadorWRT(worker, cola, lista, msj);
          break;

        case 'r': //REA
          operadorREA(worker, cola, lista, msj);
          break;

        case 's': //CLO
          operadorCLO(worker, cola, lista, msj);
          break;
      }
      cerrar(cola->disp);
    }
    else {
      reenvio_por_anillo(cola, msj, lista, NULL);
    }
  }

  borrar_cola_mensaje(cola, worker, 'w');
}
