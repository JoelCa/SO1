#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "cabecera.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

extern pthread_barrier_t barrier; 

int esperar_respuesta(DescriptorColas *cola, ListaArchivos *lista, char *candidato);

const int workers = (char) (((int)'0') + NUM_WORKER);

void operadorLSD(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  char *nombres;

  if((nombres = (char *) malloc(MAXSIZE_TEXT * sizeof (char))) == NULL)
    printf("LSD: error\n");
  nombres = concatenar_archivos(lista);
  if(msj->dato != 'm') {
    strcat(nombres, msj->nombre);
    enviar(cola->anillo, 'l', msj->contador-1, 'f', nombres);
  }
  else {
    enviar(cola->anillo,'l',workers,'f', nombres);
    esperar_respuesta(cola, lista, NULL);
    free(nombres);
  }
}

void operadorDEL(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;

  switch(msj->dato) {
    case 'm':
      if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
        if(arch->estado == 0) {
          eliminar_archivo(lista, msj->nombre);
          enviar(cola->disp, 'd', '0', '0', NULL);
        }
        else
          enviar(cola->disp, 'd', '0', 'e', NULL);
      }
      else {
        enviar(cola->anillo,'d',workers,'f',msj->nombre);
        esperar_respuesta(cola, lista, NULL);
      }
      break;

    case 'f':
      if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
        if(arch->estado == 0) {
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
}


void operadorCRE(DescriptorColas *cola, ListaArchivos *lista, Msj *msj, char *candidato)
{
  switch(msj->dato) {
    case 'm':
      if(buscar_archivo(lista, msj->nombre) != NULL) {
        enviar(cola->disp, 'c', '0', '1', NULL);
      }
      else {
        enviar(cola->anillo,'c',workers,'f',msj->nombre);
        esperar_respuesta(cola, lista, msj->nombre);
      }
      break;

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
}

void operadorOPN(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;

  switch(msj->dato) {
    case 'm':
      if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
        if(modificar_estado(arch)) 
          enviar(cola->disp, 'o', lista->worker, '0', NULL);
        else
          enviar(cola->disp, 'o', '0', '1', NULL);
      }
      else {
        enviar(cola->anillo,'o',workers,'f',msj->nombre);
        esperar_respuesta(cola, lista, NULL);
      }
      break;

    case 'f':
      if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
        if(modificar_estado(arch))
          enviar(cola->anillo,'o',msj->contador-1,lista->worker,msj->nombre);
        else
          enviar(cola->anillo,'o',msj->contador-1,'e',msj->nombre);
      }
      else
        enviar(cola->anillo,'o',msj->contador-1,msj->dato,msj->nombre);
      break;

    default:
      reenvio_msj(cola->anillo, msj, 'o');
      break;
  }
}

void escribir_archivo(ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;

  arch = buscar_archivo(lista, msj->nombre);
  strcat(arch->texto, msj->texto);
  arch->tam += msj->otrodato; 
}

void operadorWRT(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  char worker = lista->worker;

  switch(msj->dato) {
    case 'm':
      if(msj->contador == worker) {
        escribir_archivo(lista, msj);
        enviar(cola->disp, 'w', '0', '0', NULL);
      }
      else {
        enviarWR(cola->anillo,'w',workers,msj->contador, msj->otrodato, msj->nombre, msj->texto);
        esperar_respuesta(cola, lista, NULL);
      }
      break;

    case 't':
      reenvio_msj(cola->anillo, msj, 'w');
      break;

    default:
      if(worker == msj->dato) {
        escribir_archivo(lista, msj);
        enviar(cola->anillo, 'w', msj->contador-1, 't', NULL);
      }
      else
        reenvio_msj(cola->anillo, msj, 'w');
      break;
  }
}

void leer_archivo(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  int size;
  char *texto;
  Archivo *arch;

  arch = buscar_archivo(lista, msj->nombre);
  if(arch->indice > arch->tam) {
    if(msj->dato == 'm')
      enviar(cola->disp, 'r', '0', '0', NULL);
    else {
      enviar(cola->anillo, 'r', msj->contador-1, 't', NULL);
    }
  }
  else {
    size = MIN(msj->otrodato, arch->tam - arch->indice);
    if((texto = (char *) malloc((size+1) * sizeof (char))) == NULL)
      printf("REA: error\n");
    memcpy(texto, &(arch->texto)[arch->indice], size);
    texto[size] = '\0';
    arch->indice += size;
    if(msj->dato == 'm')
      enviarWR(cola->disp,'r', '0', '0', size, texto, NULL);
    else
      enviarWR(cola->anillo, 'r', msj->contador-1, 't', size, texto, NULL);
  
  }
}

void operadorREA(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  char worker = lista->worker;

  switch(msj->dato) {
    case 'm':
      if(msj->contador == worker)
        leer_archivo(cola, lista, msj);
      else {
        enviarWR(cola->anillo,'r',workers,msj->contador, msj->otrodato, msj->nombre, NULL);
        esperar_respuesta(cola, lista, NULL);
      }
      break;

    case 't':
      reenvio_msj(cola->anillo, msj, 'r');
      break;

    default:
      if(msj->dato == worker)
        leer_archivo(cola, lista, msj);
      else
        reenvio_msj(cola->anillo, msj, 'r');
      break;
  }
}

void cerrar_archivo(ListaArchivos *lista, Msj *msj)
{
  Archivo *arch;

  arch = buscar_archivo(lista, msj->nombre);
  arch->estado = 0;
  arch->indice = 0;
}

void operadorCLO(DescriptorColas *cola, ListaArchivos *lista, Msj *msj)
{
  char worker = lista->worker;

  switch(msj->dato) {
    case 'm':
      if(msj->contador  == worker) {
        cerrar_archivo(lista, msj);
        enviar(cola->disp, 's', '0', '0', NULL);
      }
      else {
        enviar(cola->anillo,'s',workers,msj->contador,msj->nombre);
        esperar_respuesta(cola, lista, NULL);
      }
      break;

    case 't':
      reenvio_msj(cola->anillo, msj, 's');
      break;

    default:
      if(msj->dato == worker) {
        cerrar_archivo(lista, msj);
        enviar(cola->anillo,'s',msj->contador-1,'t',NULL);
      }
      else
        reenvio_msj(cola->anillo, msj, 's');
      break;
  }

}

//A partir del mensaje que obtiene del anillo, genera otro mensaje para enviarlo por allí,
//con el tratamiento correspondiente.
void envio_por_anillo(DescriptorColas *cola, Msj *msj, ListaArchivos *lista, char *candidato)
{
  switch(msj->tipo) {
    case 'l': //LSD
      operadorLSD(cola, lista, msj);
      break;

    case 'd': //DEL
      operadorDEL(cola, lista, msj);
      break;

    case 'c': //CRE
      operadorCRE(cola, lista, msj, candidato);
      break;

    case 'o': //OPN
      operadorOPN(cola, lista, msj);
      break;

    case 'w': //WRT
      operadorWRT(cola, lista, msj);
      break;

    case 'r': //REA
      operadorREA(cola, lista, msj);
      break;

    case 's': //CLO
      operadorCLO(cola, lista, msj);
      break;
  }
}


//Evalúa si el mensaje que recibe por el anillo, es la respuesta a su consulta, o no.
//Si lo es, envía la respuesta al proceso socket correspondiente.
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

  envio_por_anillo(cola, msj, lista, NULL);
  liberar_msj(msj);
  return 0;
}

//Una vez que el worker envía su consulta por el anillo, espera la respuesta.
int esperar_respuesta(DescriptorColas *cola, ListaArchivos *lista, char *candidato)
{
  Msj *msj;

  while(1) {
    msj = recibir(cola->worker);
    /*printf("------\nMensaje del worker %c, que espera una respuesta:\n", lista->worker);
    imprimir_msj(msj);
    printf("------\n");*/
    if(evaluar_msj(cola, msj, lista, candidato))
      break;
  }
  return 0;
}

void *fs (void * arg)
{
  int worker;
  char cola_d[7], cola_a[7];
  Msj *msj;
  DescriptorColas *cola;
  ListaArchivos *lista;

  worker = *(int *)arg;

  lista = crear_lista_archivos(worker);

  worker--;
  sprintf(cola_d, "/cola%d", (worker+NUM_WORKER)%(2*NUM_WORKER));
  sprintf(cola_a, "/cola%d", (worker+1)%NUM_WORKER);

  cola = nueva_cola_mensaje(worker, 'w');
  
  //Esta barrera sirve para que el worker abra la cola de mensajes de su
  //vecino, luego de que halla sido creada.
  pthread_barrier_wait(&barrier);
  
  cola->anillo = abrir(cola_a);

  while(1) {
    msj = recibir(cola->worker);
    /*printf("------\nmensaje del worker %d:\n", worker);
    imprimir_msj(msj);
    printf("conjunto de colas del worker %d:\n", worker);
    imprimir_cola(cola);
    printf("------\n");*/
    if(msj->dato == 'm') {
      cola->disp = abrir(cola_d);
      switch(msj->tipo) {
        case 'l': //LSD
          operadorLSD(cola, lista, msj);
          break;

        case 'd': //DEL
          operadorDEL(cola, lista, msj);
          break;

        case 'c': //CRE
          operadorCRE(cola, lista, msj, NULL);
          break;

        case 'o': //OPN
          operadorOPN(cola, lista, msj);
          break;

        case 'w': //WRT
          operadorWRT(cola, lista, msj);
          break;

        case 'r': //REA
          operadorREA(cola, lista, msj);
          break;

        case 's': //CLO
          operadorCLO(cola, lista, msj);
          break;
      }
      cerrar(cola->disp);
    }
    else {
      envio_por_anillo(cola, msj, lista, NULL);
    }
  }

  borrar_cola_mensaje(cola, worker, 'w');
}
