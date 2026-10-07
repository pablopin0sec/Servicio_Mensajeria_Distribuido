#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include "plantilla.h"

#define MAX_USUARIOS 100     // máximo de usuarios registrados
#define MAX_MENSAJES 200     // máximo de mensajes pendientes por usuario
#define MAX_NOMBRE 256       // tamaño máximo del nombre de usuario
#define MAX_MENSAJE 256      // tamaño máximo del mensaje (incluido '\0')
#define MAX_FICHERO 256      // tamaño máximo del nombre de fichero
#define MAX_IP 16            // tamaño máximo de una IP (xxx.xxx.xxx.xxx\0)
#define BACKLOG 100          // máximo de conexiones pendientes en listen


struct mensaje {
    unsigned int id;
    char remitente[MAX_NOMBRE];
    char contenido[MAX_MENSAJE];
    char fichero[MAX_FICHERO];
};

struct info_cliente {
    char nombre [MAX_NOMBRE];
    int estado;
    char IP [MAX_IP];
    int PUERTO;
    unsigned int contador_mensajes;
    struct mensaje mensajes_pendientes [MAX_MENSAJES];
    int num_mensajes_pendientes;
};

struct thread_args {
    int client_sock;
    struct sockaddr_in client_dir;
    CLIENT *client_rpc;;
};