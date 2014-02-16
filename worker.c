#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cabecera.h"

#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))

extern char *colas[10];
extern ListaDes *des;

typedef struct des_cola {
  mqd_t mqd_cola;
  mqd_t mqd_d;
  mqd_t mqd_anillo;
} DesCola;

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
        printf("Error\n");  
      buff = concatenar_archivos(lista);
      strcat(buff, msj->nombre);
      enviar_esp(anillo, 'l', msj->contador-1, 'f', buff);
      break;
    case 'd': //DEL
      switch(msj->dato) {
        case 'f':
          aux = busca_lista(lista, msj->nombre);
          if(aux != NULL) {
            if(aux->estado == 0) {
              del_lista(lista,msj->nombre);
              enviar_esp(anillo, 'd', msj->contador-1, 't', msj->nombre);
            }
            else {
              enviar_esp(anillo, 'd', msj->contador-1, 'e', msj->nombre);
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
          if(busca_lista(lista, msj->nombre) != NULL) {
            if(candidato == NULL)
              enviar_esp(anillo, 'c',msj->contador-1,'t',msj->nombre);
            else {
              if(strcmp(candidato, msj->nombre) == 0)
                enviar_esp(anillo,'c',msj->contador-1,'e',msj->nombre);
              else
                enviar_esp(anillo,'c',msj->contador-1,'t',msj->nombre);
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
          aux = busca_lista(lista, msj->nombre);
          if(aux != NULL) {
            if(modificar_estado(aux))
              enviar_esp(anillo,'o',msj->contador-1,worker,msj->nombre);
            else
              enviar_esp(anillo,'o',msj->contador-1,'e',msj->nombre);
          }
          else
            enviar_esp(anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;
        case 'e':
          enviar_esp(anillo,'o',msj->contador-1,msj->dato,msj->nombre);
          break;
        default:
          enviar_esp(anillo,'o',msj->contador-1,msj->dato,msj->nombre);
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
            aux = busca_lista(lista, msj->nombre);
            strcat(aux->texto, msj->texto);
            aux->tam += msj->otrodato;
            enviar_esp(anillo, 'w', msj->contador-1, 't', NULL);
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
            aux = busca_lista(lista, msj->nombre);
            if(aux->indice > aux->tam) {
              enviar_esp(anillo, 'r', msj->contador-1, 't', NULL);
            }
            else {
              size = MIN(msj->otrodato, aux->tam - aux->indice);
              if((buff = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
                printf("Error\n");
              memcpy(buff, &(aux->texto)[aux->indice], size);
              buff[size] = '\0';
              aux->indice +=  size;
              enviar_espwr(anillo, 'r', msj->contador-1, 't', size, buff, NULL);
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
            aux = busca_lista(lista, msj->nombre);
            aux->estado = 0;
            aux->indice = 0;
            enviar_esp(anillo,'s',msj->contador-1,'t',NULL);
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
  switch(msj->tipo) {
    case 'l': //LSD
      if(msj->contador == '1') {
        enviar_esp(proc_socket, 'l', '0', 'f', msj->nombre);
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, NULL);
      break;
    case 'd': //DEL
      if(msj->contador == '1') {
        switch(msj->dato) {
          case 't':
            enviar_esp(proc_socket, 'd', '0', '0', NULL);
            break;
          case 'f':
            enviar_esp(proc_socket, 'd', '0', '1', NULL);
            break;
          default:
            enviar_esp(proc_socket, 'd', '0', '2', NULL);
            break;
        }
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, NULL);
      break;
    case 'c': //CRE
      if(msj->contador == '1') {
        switch(msj->dato) {
          case 't':
            enviar_esp(proc_socket, 'c', '0', '1', NULL);
            break;
          case 'f':
            ins_lista(lista, candidato);
            enviar_esp(proc_socket, 'c', '0', '0', NULL);
            break;
          case 'e':
            enviar_esp(proc_socket, 'c', '0', '2', NULL);
            break;
        }
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, candidato);
      break;
    case 'o': //OPN
      if(msj->contador == '1') {
        switch(msj->dato) {
          case 'f':
            enviar_esp(proc_socket, 'o', '0', '2', NULL);
            break;
          case 'e':
            enviar_esp(proc_socket, 'o', '0', '1', NULL);
            break;
          default:
            enviar_esp(proc_socket, 'o', msj->dato, '0', NULL);
            break;
            
        }
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, NULL);
      break;
    case 'w': //WRT
      if(msj->contador == '1') {
        switch(msj->dato) {
          case 't':
            enviar_esp(proc_socket, 'w', '0', '0', NULL);
            break;
        }
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, NULL);
      break;
    case 'r': //REA
      if(msj->contador == '1') {
        switch(msj->dato) {
          case 't':
            enviar_espwr(proc_socket, 'r', '0', '0', msj->otrodato, msj->nombre, NULL);
            break;
        }
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, NULL);
      break;
    case 's': //CLO
      if(msj->contador == '1') {
        switch(msj->dato) {
          case 't':
            enviar_esp(proc_socket,'s', '0', '0', NULL);
            break;
        }
        return 1;
      }
      else
        reenvio_por_anillo(anillo, msj, lista, NULL);
      break;
  }
  return 0;
  liberar_msj(&msj);
}

int recibir_msjs(DesCola ds, Lista *lista, char *candidato)
{
  Msj *msj;

  while(1) {
    msj = recibir_esp(ds.mqd_cola);
    if(evaluar_msj(ds.mqd_d, ds.mqd_anillo, msj, lista, candidato))
      break;
  }
  return 0;
}

void *fs (void * arg)
{
  char *cola,*anillo,*elem;
  int worker, size;
  mqd_t mqd_cola, mqd_anillo, mqd_d;
  Lista *lista = (Lista *) arg;
  Msj *msj;
  Archivo *res;
  DesCola dc;
  char buff[MAXSIZE_COLA];

  cola = lista->cola;
  anillo = lista->anillo;
  worker = cola[5] - '0' - 1;

  mqd_cola = abrir(cola);
  mqd_anillo = abrir(anillo);

  while(1) {
    msj = recibir_esp(mqd_cola);
    if(msj->dato == 'm') {
      mqd_d = abrir(colas[worker+5]);
      dc.mqd_cola = mqd_cola;
      dc.mqd_d = mqd_d;
      dc.mqd_anillo = mqd_anillo;
      switch(msj->tipo) {
        case 'l': //LSD
          if((elem = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
            printf("Error\n");
          elem = concatenar_archivos(lista);
          enviar_esp(mqd_anillo,'l', '5', 'f', elem);
          recibir_msjs(dc, lista, NULL);
          break;
        case 'd': //DEL
          res = busca_lista(lista, msj->nombre);
          if(res != NULL) {
            if(res->estado == 0) {
              del_lista(lista, msj->nombre);
              enviar_esp(mqd_d, 'd', '0', '0', NULL);
            }
            else
              enviar_esp(mqd_d, 'd', '0', 'e', NULL);
          }
          else {
            enviar_esp(mqd_anillo,'d','5','f',msj->nombre);
            recibir_msjs(dc, lista, NULL);
          }
          break;
        case 'c': //CRE
          if(busca_lista(lista, msj->nombre) != NULL) {
            enviar_esp(mqd_d, 'c', '0', '1', NULL);
          }
          else {
            enviar_esp(mqd_anillo,'c','5','f',msj->nombre);
            recibir_msjs(dc, lista, msj->nombre);
          }
          break;
        case 'o': //OPN
          res = busca_lista(lista, msj->nombre);
          if(res != NULL) {
            if(modificar_estado(res)) 
              enviar_esp(mqd_d, 'o', cola[5]-1, '0', NULL);
            else
              enviar_esp(mqd_d, 'o', '0', '1', NULL);
          }
          else {
            enviar_esp(mqd_anillo,'o','5','f',msj->nombre);
            recibir_msjs(dc, lista, NULL);
          }
          break;
        case 'w': //WRT
          if(msj->contador -'0' == worker) {
            res = busca_lista(lista, msj->nombre);
            strcat(res->texto, msj->texto);
            res->tam += msj->otrodato;
            enviar_esp(mqd_d, 'w', '0', '0', NULL);
          }
          else {
            enviar_espwr(mqd_anillo,'w','5',msj->contador, msj->otrodato, msj->nombre, msj->texto);
            recibir_msjs(dc, lista, NULL);
          }
          break;
        case 'r': //REA
          if(msj->contador -'0' == worker) {
            res = busca_lista(lista, msj->nombre);
            if(res->indice > res->tam)
              enviar_esp(mqd_d, 'r', '0', '0', NULL);
            else {
              size = MIN(msj->otrodato, res->tam - res->indice);
              if((elem = (char *) malloc(MAXSIZE_COLA * sizeof (char))) == NULL)
                printf("Error\n");
              memcpy(elem, &(res->texto)[res->indice], size);
              elem[size] = '\0';
              res->indice += size;
              enviar_espwr(mqd_d,'r', '0', '0', size, elem, NULL);
            }
          }
          else {
            enviar_espwr(mqd_anillo,'r','5',msj->contador, msj->otrodato, msj->nombre, NULL);
            recibir_msjs(dc, lista, NULL);
          }
          break;
        case 's': //CLO
          if(msj->contador -'0' == worker) {
            res = busca_lista(lista, msj->nombre);
            res->estado = 0;
            res->indice = 0;
            enviar_esp(mqd_d, 's', '0', '0', NULL);
          }
          else {
            enviar_esp(mqd_anillo,'s','5',msj->contador,msj->nombre);
            recibir_msjs(dc, lista, NULL);
          }
          break;
      }
    cerrar(mqd_d);
    }
    else 
      reenvio_por_anillo(mqd_anillo,msj,lista,NULL);
  }
  cerrar(mqd_cola);
}
