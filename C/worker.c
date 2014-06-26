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

void reenvio_por_anillo(mqd_t anillo, Msj *msj, Lista *lista, char *candidato)
{
  char *buff;
  Archivo *aux;
  char worker;
  int size;
  
  worker = (lista->cola)[5]-1;

  switch(msj->tipo) {
    case 'l': //LSD
      if((buff = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
        printf("LSD: error\n");  
      buff = concatenar_archivos(lista);
      strcat(buff, msj->nombre);
      enviar(anillo, 'l', msj->contador-1, 'f', buff);
      break;

    case 'd': //DEL
      switch(msj->dato) {
        case 'f':
          if((aux = buscar_archivo(lista, msj->nombre)) != NULL) {
            if(aux->estado == 0) {
              eliminar_archivo(lista,msj->nombre);
              enviar(anillo, 'd', msj->contador-1, 't', msj->nombre);
            }
            else {
              enviar(anillo, 'd', msj->contador-1, 'e', msj->nombre);
            }
          }
          else
            reenvio_msj(anillo, msj, 'd');
          break;

        default:
          reenvio_msj(anillo, msj, 'd');
          break;
      }
      break;

    case 'c': //CRE
      switch(msj->dato) {
        case 'f':
          if(buscar_archivo(lista, msj->nombre) != NULL) {
            if(candidato == NULL)
              enviar(anillo, 'c',msj->contador-1,'t',msj->nombre);
            else {
              if(strcmp(candidato, msj->nombre) == 0)
                enviar(anillo,'c',msj->contador-1,'e',msj->nombre);
              else
                enviar(anillo,'c',msj->contador-1,'t',msj->nombre);
            }
          }
          else {
            reenvio_msj(anillo, msj, 'c');
          }        
          break;

        default:
          reenvio_msj(anillo, msj, 'c');
          break;
      }
      break;

    case 'o': //OPN
      switch(msj->dato) {
        case 'f':
          aux = buscar_archivo(lista, msj->nombre);
          if(aux != NULL) {
            if(modificar_estado(aux))
              enviar(anillo,'o',msj->contador-1,worker,msj->nombre);
            else
              enviar(anillo,'o',msj->contador-1,'e',msj->nombre);
          }
          else
            enviar(anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;

        case 'e':
          enviar(anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;

        default:
          enviar(anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;
      }
      break;

    case 'w': //WRT
      switch(msj->dato) {
        case 't':
          reenvio_msj(anillo, msj, 'w');
          break;

        default:
          if(worker == msj->dato) {
            aux = buscar_archivo(lista, msj->nombre);
            strcat(aux->texto, msj->texto);
            aux->tam += msj->otrodato;
            enviar(anillo, 'w', msj->contador-1, 't', NULL);
          }
          else
            reenvio_msj(anillo, msj, 'w');
          break;
      }
      break;

    case 'r': //REA
      switch(msj->dato) {
        case 't':
          reenvio_msj(anillo, msj, 'r');
          break;

        default:
          if(worker == msj->dato) {
            aux = buscar_archivo(lista, msj->nombre);
            if(aux->indice > aux->tam) {
              enviar(anillo, 'r', msj->contador-1, 't', NULL);
            }
            else {
              size = MIN(msj->otrodato, aux->tam - aux->indice);
              if((buff = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
                printf("REA: error\n");
              memcpy(buff, &(aux->texto)[aux->indice], size);
              buff[size] = '\0';
              aux->indice +=  size;
              enviarWR(anillo, 'r', msj->contador-1, 't', size, buff, NULL);
            }
          }
          else
            reenvio_msj(anillo, msj, 'r');
          break;
      }
      break;

    case 's': //CLO
      switch(msj->dato) {
        case 't':
          reenvio_msj(anillo, msj, 's');
          break;

        default:
          if(worker == msj->dato) {
            aux = buscar_archivo(lista, msj->nombre);
            aux->estado = 0;
            aux->indice = 0;
            enviar(anillo,'s',msj->contador-1,'t',NULL);
          }
          else
            reenvio_msj(anillo, msj, 's');
          break;
      }
      break;
  }
}


int evaluar_msj(mqd_t proc_socket, mqd_t anillo, Msj *msj, Lista *lista, char *candidato)
{
  if(msj->contador == '1') {
    switch(msj->tipo) {
      case 'l': //LSD
        enviar(proc_socket, 'l', '0', 'f', msj->nombre);
        break;

      case 'd': //DEL
        switch(msj->dato) {
          case 't':
            enviar(proc_socket, 'd', '0', '0', NULL);
            break;
          case 'f':
            enviar(proc_socket, 'd', '0', '1', NULL);
            break;
          default:
            enviar(proc_socket, 'd', '0', '2', NULL);
            break;
        }
        break;

      case 'c': //CRE
        switch(msj->dato) {
          case 't':
            enviar(proc_socket, 'c', '0', '1', NULL);
            break;
          case 'f':
            nuevo_archivo(lista, candidato);
            enviar(proc_socket, 'c', '0', '0', NULL);
            break;
          case 'e':
            enviar(proc_socket, 'c', '0', '2', NULL);
            break;
        }
        break;

      case 'o': //OPN
        switch(msj->dato) {
          case 'f':
            enviar(proc_socket, 'o', '0', '2', NULL);
            break;
          case 'e':
            enviar(proc_socket, 'o', '0', '1', NULL);
            break;
          default:
            enviar(proc_socket, 'o', msj->dato, '0', NULL);
            break;
            
        }
        break;

      case 'w': //WRT
        switch(msj->dato) {
          case 't':
            enviar(proc_socket, 'w', '0', '0', NULL);
            break;
        }
        break;

      case 'r': //REA
        switch(msj->dato) {
          case 't':
            enviarWR(proc_socket, 'r', '0', '0', msj->otrodato, msj->nombre, NULL);
            break;
        }
        break;

      case 's': //CLO
        switch(msj->dato) {
          case 't':
            enviar(proc_socket,'s', '0', '0', NULL);
            break;
        }
        break;
    }
    return 1;
  }

  reenvio_por_anillo(anillo, msj, lista, NULL);
  liberar_msj(msj);
  return 0;
}


int recibir_msjs(Colas dc, Lista *lista, char *candidato)
{
  Msj *msj;

  while(1) {
    msj = recibir(dc.cola_worker);
    if(evaluar_msj(dc.cola_disp, dc.cola_anillo, msj, lista, candidato))
      break;
  }
  return 0;
}


void operadorLSD(Colas dc, Lista *lista)
{
  char *nombres;

  if((nombres = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
    printf("LSD: error\n");
  nombres = concatenar_archivos(lista);
  enviar(dc.cola_anillo,'l', '5', 'f', nombres);
  recibir_msjs(dc, lista, NULL);
  free(nombres);
}

void operadorDEL(Colas dc, Lista *lista, Msj *msj)
{
  Archivo *arch;

  if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
    if(arch->estado == 0) {
      eliminar_archivo(lista, msj->nombre);
      enviar(dc.cola_disp, 'd', '0', '0', NULL);
    }
    else
      enviar(dc.cola_disp, 'd', '0', 'e', NULL);
  }
  else {
    enviar(dc.cola_anillo,'d','5','f',msj->nombre);
    recibir_msjs(dc, lista, NULL);
  }
}

void operadorCRE(Colas dc, Lista *lista, Msj *msj)
{
  if(buscar_archivo(lista, msj->nombre) != NULL) {
    enviar(dc.cola_disp, 'c', '0', '1', NULL);
  }
  else {
    enviar(dc.cola_anillo,'c','5','f',msj->nombre);
    recibir_msjs(dc, lista, msj->nombre);
  }
}

void operadorOPN(Colas dc, Lista *lista, Msj *msj)
{
  Archivo *arch;
  
  if((arch = buscar_archivo(lista, msj->nombre)) != NULL) {
    if(modificar_estado(arch)) 
      enviar(dc.cola_disp, 'o', lista->cola[5] - 1, '0', NULL);
    else
      enviar(dc.cola_disp, 'o', '0', '1', NULL);
  }
  else {
    enviar(dc.cola_anillo,'o','5','f',msj->nombre);
    recibir_msjs(dc, lista, NULL);
  }
}

void operadorWRT(int worker, Colas dc, Lista *lista, Msj *msj)
{
  Archivo *arch;
  
  if(msj->contador -'0' == worker) {
    arch = buscar_archivo(lista, msj->nombre);
    strcat(arch->texto, msj->texto);
    arch->tam += msj->otrodato;
    enviar(dc.cola_disp, 'w', '0', '0', NULL);
  }
  else {
    enviarWR(dc.cola_anillo,'w','5',msj->contador, msj->otrodato, msj->nombre, msj->texto);
    recibir_msjs(dc, lista, NULL);
  }
}

void operadorREA(int worker, Colas dc, Lista *lista, Msj *msj)
{
  int size;
  char *texto;
  Archivo *arch;

  if(msj->contador -'0' == worker) {
    arch = buscar_archivo(lista, msj->nombre);
    if(arch->indice > arch->tam)
      enviar(dc.cola_disp, 'r', '0', '0', NULL);
    else {
      size = MIN(msj->otrodato, arch->tam - arch->indice);
      if((texto = (char *) malloc(size * sizeof (char))) == NULL)
        printf("REA: error\n");
      memcpy(texto, &(arch->texto)[arch->indice], size);
      texto[size] = '\0';
      arch->indice += size;
      enviarWR(dc.cola_disp,'r', '0', '0', size, texto, NULL);
      //free(texto);
    }
  }
  else {
    enviarWR(dc.cola_anillo,'r','5',msj->contador, msj->otrodato, msj->nombre, NULL);
    recibir_msjs(dc, lista, NULL);
  }
}

void operadorCLO(int worker, Colas dc, Lista *lista, Msj *msj)
{
  Archivo *arch;

  if(msj->contador -'0' == worker) {
    arch = buscar_archivo(lista, msj->nombre);
    arch->estado = 0;
    arch->indice = 0;
    enviar(dc.cola_disp, 's', '0', '0', NULL);
  }
  else {
    enviar(dc.cola_anillo,'s','5',msj->contador,msj->nombre);
    recibir_msjs(dc, lista, NULL);
  }
}


void *fs (void * arg)
{
  int worker;
  char *cola,*anillo, disp[7];
  Msj *msj;
  Colas dc;
  Lista *lista;
  mqd_t cola_worker, cola_anillo, cola_disp;

  worker = *(int *)arg;
  crear_cola(worker);
  lista = crear_lista(worker);

  sprintf(disp, "/cola%d", (worker+N_WORKER)%(2*N_WORKER));
  cola = lista->cola;
  anillo = lista->anillo;
  worker--;

  pthread_barrier_wait(&barrier);

  cola_worker = abrir(cola);
  cola_anillo = abrir(anillo);

  while(1) {
    msj = recibir(cola_worker);
    //imprimir_msj(msj);
    if(msj->dato == 'm') {
      cola_disp = abrir(disp);
      dc.cola_worker = cola_worker;
      dc.cola_disp = cola_disp;
      dc.cola_anillo = cola_anillo;
      switch(msj->tipo) {
        case 'l': //LSD
          operadorLSD(dc, lista);
          break;

        case 'd': //DEL
          operadorDEL(dc, lista, msj);
          break;

        case 'c': //CRE
          operadorCRE(dc, lista, msj);
          break;

        case 'o': //OPN
          operadorOPN(dc, lista, msj);
          break;

        case 'w': //WRT
          operadorWRT(worker, dc, lista, msj);
          break;

        case 'r': //REA
          operadorREA(worker, dc, lista, msj);
          break;

        case 's': //CLO
          operadorCLO(worker, dc, lista, msj);
          break;
      }
      cerrar(cola_disp);
    }
    else 
      reenvio_por_anillo(cola_anillo,msj,lista,NULL);
  }
  cerrar(cola_worker);
  cerrar(cola_disp); //estaba comentada
  cerrar(cola_anillo);
  mq_unlink(disp);
  mq_unlink(cola);
}
