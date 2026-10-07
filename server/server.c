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
#include <signal.h>
#include "almacen.h"
#include "sock_utils.h"
#include "plantilla.h"

struct info_cliente clientes [MAX_USUARIOS];
int num_clientes = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int server_sock;


void manejador_sigint(int sig) {
    (void)sig;
    close(server_sock);
    exit(0);
}

char* leer_nombre_usuario (int client_sock){
    char* nombre_usuario = malloc(sizeof(char)*MAX_NOMBRE);
    if (nombre_usuario == NULL) {
        perror("Error al hacer el malloc en leer_nombre_usuario");
        return NULL;
    }
    if (recvString(client_sock, nombre_usuario, MAX_NOMBRE) == -1){
        perror("Error al obtener el nombre del usuario");
        free(nombre_usuario);
        return NULL;
    }
    return nombre_usuario;
}

int find_user_by_name(char* nombre) {
    for (int i = 0; i < num_clientes; i++) {
        if (strcmp(clientes[i].nombre, nombre) == 0) {
            return i;
        }
    }
    return -1;
}

int register_user(int client_sock, char* nombre_usuario) { // Registra un usuario en el sistema

    // 2. Compruebo que no hay usuarios registrados con el mismo nombre
    pthread_mutex_lock(&mutex);
    int existe_usuario = find_user_by_name(nombre_usuario);
    if (existe_usuario != -1){
        pthread_mutex_unlock(&mutex);
        char response = 1;
        sendMessage(client_sock, &response, 1);
        printf("s> REGISTER %s FAIL\n", nombre_usuario);
        return -1;
    }

    // 3. Compruebo que el almacen no este lleno
    if (num_clientes >= MAX_USUARIOS){
        pthread_mutex_unlock(&mutex);
        char response = 2;
        sendMessage(client_sock, &response, 1);
        printf("s> REGISTER %s FAIL\n", nombre_usuario);
        return -1;
    }

    // 4. Aniado al cliente al sistema
    struct info_cliente info;
    strcpy(info.nombre, nombre_usuario);
    info.contador_mensajes = 0;
    info.estado = 0;
    info.num_mensajes_pendientes = 0;
    clientes[num_clientes] = info;
    num_clientes++;
    pthread_mutex_unlock(&mutex);
    char response = 0;
    sendMessage(client_sock, &response, 1);
    printf("s> REGISTER %s OK\n", nombre_usuario);
    return 0;
}

int unregister_user(int client_sock, char* nombre_usuario) { // Da de baja a un usuario del sistema
    // 2. Compruebo hay usuario registrado
    pthread_mutex_lock(&mutex);
    int existe_usuario = find_user_by_name(nombre_usuario);
    if (existe_usuario != -1){
        // Borrar mensajes pendientes del usuario eliminado
        clientes[existe_usuario].num_mensajes_pendientes = 0;
        // Elimino al cliente
        clientes[existe_usuario] = clientes[num_clientes - 1];
        num_clientes--;
        pthread_mutex_unlock(&mutex);
    }
    else {
        pthread_mutex_unlock(&mutex);
        char response = 1;
        sendMessage(client_sock, &response, 1);
        printf("s> UNREGISTER %s FAIL\n", nombre_usuario);
        return -1;
    }

    char response = 0;
    sendMessage(client_sock, &response, 1);
    printf("s> UNREGISTER %s OK\n", nombre_usuario);
    return 0;
}

int connect_user(int client_sock, struct sockaddr_in client_dir, char* nombre_usuario) {

    pthread_mutex_lock(&mutex);
    int indice_usuario = find_user_by_name(nombre_usuario);
    if (indice_usuario == -1) {
        pthread_mutex_unlock(&mutex);
        char response = 1;
        sendMessage(client_sock, &response, 1);
        printf("s> CONNECT %s FAIL\n", nombre_usuario);
        return -1;
    }
    if (clientes[indice_usuario].estado == 1) {
        pthread_mutex_unlock(&mutex);
        char response = 2;
        sendMessage(client_sock, &response, 1);
        printf("s> CONNECT %s FAIL\n", nombre_usuario);
        return -1;
    }

    // 3. Guardo la nueva info
    clientes[indice_usuario].estado = 1;
    char *puerto = malloc(sizeof(char)*8);
    if (puerto == NULL) {
        clientes[indice_usuario].estado = 0;
        pthread_mutex_unlock(&mutex);
        char response = 3;
        sendMessage(client_sock, &response, 1);
        return -1;
    }
    if (recvString(client_sock, puerto, 8) == -1) {
        clientes[indice_usuario].estado = 0;
        free(puerto);
        pthread_mutex_unlock(&mutex);
        char response = 3;
        sendMessage(client_sock, &response, 1);
        return -1;
    }
    char* endptr;
    clientes[indice_usuario].PUERTO = (int)strtol(puerto, &endptr, 10);
    if (*endptr != '\0') {
        clientes[indice_usuario].estado = 0;
        free(puerto);
        pthread_mutex_unlock(&mutex);
        char response = 3;
        sendMessage(client_sock, &response, 1);
        return -1;
    }
    char* ip = inet_ntoa(client_dir.sin_addr);
    strcpy(clientes[indice_usuario].IP, ip);
    char ip_usuario[MAX_IP];
    strcpy(ip_usuario, clientes[indice_usuario].IP);
    int puerto_usuario = clientes[indice_usuario].PUERTO;

    pthread_mutex_unlock(&mutex);
    char response = 0;
    sendMessage(client_sock, &response, 1);
    printf("s> CONNECT %s OK\n", nombre_usuario);
    free(puerto);

    // Enviar mensajes pendientes uno a uno
    pthread_mutex_lock(&mutex);
    int num_pendientes = clientes[indice_usuario].num_mensajes_pendientes;
    pthread_mutex_unlock(&mutex);

    for (int j = 0; j < num_pendientes; j++) {
        pthread_mutex_lock(&mutex);
        unsigned int id = clientes[indice_usuario].mensajes_pendientes[0].id;
        char remitente[MAX_NOMBRE];
        strcpy(remitente, clientes[indice_usuario].mensajes_pendientes[0].remitente);
        char contenido[MAX_MENSAJE];
        strcpy(contenido, clientes[indice_usuario].mensajes_pendientes[0].contenido);
        char fichero[MAX_FICHERO];
        strcpy(fichero, clientes[indice_usuario].mensajes_pendientes[0].fichero);
        pthread_mutex_unlock(&mutex);

        int sock_nuevo = conectar_a_cliente(ip_usuario, puerto_usuario);
        if (sock_nuevo < 0) {
            pthread_mutex_lock(&mutex);
            clientes[indice_usuario].estado = 0;
            pthread_mutex_unlock(&mutex);
            break;
        }

        if (strlen(fichero) > 0) {
            sendString(sock_nuevo, "SEND_MESSAGE_ATTACH");
        } else {
            sendString(sock_nuevo, "SEND_MESSAGE");
        }
        sendString(sock_nuevo, remitente);
        char id_str[20];
        sprintf(id_str, "%u", id);
        sendString(sock_nuevo, id_str);
        sendString(sock_nuevo, contenido);
        if (strlen(fichero) > 0) {
            sendString(sock_nuevo, fichero);
        }
        close(sock_nuevo);

        printf("s> SEND MESSAGE %u FROM %s TO %s\n", id, remitente, nombre_usuario);

        // Buscar indice del remitente
        pthread_mutex_lock(&mutex);
        int indice_remitente = find_user_by_name(remitente);
        pthread_mutex_unlock(&mutex);

        // Notificar remitente con SEND_MESS_ACK si esta conectado
        pthread_mutex_lock(&mutex);
        if (indice_remitente != -1 && clientes[indice_remitente].estado == 1) {
            char ip_remitente[MAX_IP];
            strcpy(ip_remitente, clientes[indice_remitente].IP);
            int puerto_remitente = clientes[indice_remitente].PUERTO;
            pthread_mutex_unlock(&mutex);
            int sock_remitente = conectar_a_cliente(ip_remitente, puerto_remitente);
            if (sock_remitente >= 0) {
                if (strlen(fichero) > 0) {
                    sendString(sock_remitente, "SEND_MESS_ATTACH_ACK");
                    sendString(sock_remitente, id_str);
                    sendString(sock_remitente, fichero);
                } else {
                    sendString(sock_remitente, "SEND_MESS_ACK");
                    sendString(sock_remitente, id_str);
                }
                close(sock_remitente);
            }
        } else {
            pthread_mutex_unlock(&mutex);
        }

        // Borrar mensaje de pendientes
        pthread_mutex_lock(&mutex);
        for (int k = 0; k < clientes[indice_usuario].num_mensajes_pendientes - 1; k++) {
            clientes[indice_usuario].mensajes_pendientes[k] = clientes[indice_usuario].mensajes_pendientes[k+1];
        }
        clientes[indice_usuario].num_mensajes_pendientes--;
        pthread_mutex_unlock(&mutex);
    }

    return 0;
}

int disconnect_user(int client_sock, char* nombre_usuario, struct sockaddr_in client_dir) {
 
    pthread_mutex_lock(&mutex);
    int indice = find_user_by_name(nombre_usuario);
    if (indice == -1) {
        pthread_mutex_unlock(&mutex);
        char response = 1;
        sendMessage(client_sock, &response, 1);
        printf("s> DISCONNECT %s FAIL\n", nombre_usuario);
        return -1;
    }
    if (clientes[indice].estado == 0) {
        pthread_mutex_unlock(&mutex);
        char response = 2;
        sendMessage(client_sock, &response, 1);
        printf("s> DISCONNECT %s FAIL\n", nombre_usuario);
        return -1;
    }
    // Obtengo la direccion IP del cliente
    char* IP_cliente = inet_ntoa(client_dir.sin_addr);
    if (IP_cliente == NULL || strcmp(clientes[indice].IP, IP_cliente) != 0){
        pthread_mutex_unlock(&mutex);
        char response = 3;
        sendMessage(client_sock, &response, 1);
        printf("s> DISCONNECT %s FAIL\n", nombre_usuario);
        return -1;
    }

    clientes[indice].PUERTO = 0;
    strcpy(clientes[indice].IP, "");
    clientes[indice].estado = 0;
    pthread_mutex_unlock(&mutex);
    char response = 0;
    sendMessage(client_sock, &response, 1);
    printf("s> DISCONNECT %s OK\n", nombre_usuario);
    return 0;
}

int send_message(int client_sock, int TIPO, char **fichero_out, char *nombre_remitente) { // Envia un mensaje a un usuario
    char* nombre_fichero = NULL;

    // 1.2 Leo el nombre de usuario al que quiere mandar el mensaje (Destinatario)
    char* nombre_destinatario = leer_nombre_usuario(client_sock);
    if (nombre_destinatario == NULL) {
        return -1;
    }
    // 1.3 Leo el mensaje
    char* mensaje = malloc(sizeof(char)*MAX_MENSAJE);
    if (mensaje == NULL) {
        perror("Error al hacer el malloc en send_message");
        free(nombre_destinatario);
        return -1;
    }
    if (recvString(client_sock, mensaje, MAX_MENSAJE) == -1){
        perror("Error al obtener el mensaje del usuario destinatario");
        free(nombre_destinatario);
        free(mensaje);
        return -1;
    }
    if (TIPO == 1){ // Send attach
        nombre_fichero = malloc(sizeof(char)*MAX_FICHERO);
        if (nombre_fichero == NULL) {
            perror("Error al hacer el malloc en send_attach_message");
            free(nombre_destinatario);
            free(mensaje);
            return -1;
        }
        if (recvString(client_sock, nombre_fichero, MAX_FICHERO) == -1){
            perror("Error al obtener el nombre del fichero");
            free(nombre_destinatario);
            free(mensaje);
            free(nombre_fichero);
            return -1;
        }
    }

    // 2.1 Valido el remitente
    pthread_mutex_lock(&mutex);
    int indice_remitente = find_user_by_name(nombre_remitente);
    pthread_mutex_unlock(&mutex);
    if (indice_remitente == -1) {
        char response = 1;
        sendMessage(client_sock, &response, 1);
        free(nombre_destinatario);
        free(mensaje);
        free(nombre_fichero);
        return -1;
    }

    // 2.2 Valido el destinatario
    pthread_mutex_lock(&mutex);
    int indice_destinatario = find_user_by_name(nombre_destinatario);
    pthread_mutex_unlock(&mutex);
    if (indice_destinatario == -1) {
        char response = 1;
        sendMessage(client_sock, &response, 1);
        free(nombre_destinatario);
        free(mensaje);
        free(nombre_fichero);
        return -1;
    }

    // 3.1 Incremento el contador de mensajes del remitente (ese es el id)
    pthread_mutex_lock(&mutex);
    if (clientes[indice_destinatario].num_mensajes_pendientes >= MAX_MENSAJES) {
        pthread_mutex_unlock(&mutex);
        char response = 2;
        sendMessage(client_sock, &response, 1);
        free(nombre_destinatario);
        free(mensaje);
        free(nombre_fichero);
        return -1;
    }
    clientes[indice_remitente].contador_mensajes ++;
    if (clientes[indice_remitente].contador_mensajes == 0) {
        clientes[indice_remitente].contador_mensajes = 1; // evitar el 0
    }
    unsigned int id = clientes[indice_remitente].contador_mensajes;

    // 3.2 Guardo en mensajes_pendientes del destinatario (id, remitente, contenido)
    int indice_siguiente_mensaje = clientes[indice_destinatario].num_mensajes_pendientes;
    clientes[indice_destinatario].mensajes_pendientes[indice_siguiente_mensaje].id = id;
    strcpy(clientes[indice_destinatario].mensajes_pendientes[indice_siguiente_mensaje].remitente, nombre_remitente);
    strcpy(clientes[indice_destinatario].mensajes_pendientes[indice_siguiente_mensaje].contenido, mensaje);
    if (TIPO == 0) {
        strcpy(clientes[indice_destinatario].mensajes_pendientes[indice_siguiente_mensaje].fichero, "");
    }
    if (TIPO == 1){
        strcpy(clientes[indice_destinatario].mensajes_pendientes[indice_siguiente_mensaje].fichero, nombre_fichero);
    }
    
    // 3.3 Incremento el numero de mensajes pendientes del destinatario
    clientes[indice_destinatario].num_mensajes_pendientes++;

    // 3.4 Me guardo la info del destinatario
    int destinatario_conectado = clientes[indice_destinatario].estado;
    char ip_destinatario [MAX_IP];
    strcpy(ip_destinatario, clientes[indice_destinatario].IP);
    int puerto_destinatario = clientes[indice_destinatario].PUERTO;
    pthread_mutex_unlock(&mutex);

    // 4.1 Responder al remitente con byte 0 + id como string
    char response = 0;
    sendMessage(client_sock, &response, 1);
    char id_str[20];
    sprintf(id_str, "%u", id);
    sendString(client_sock, id_str);

    // 5. Si destinatario esta conectado:
    if (destinatario_conectado == 0){
        printf("s> MESSAGE %u FROM %s TO %s STORED\n", id, nombre_remitente, nombre_destinatario);
        free(nombre_destinatario);
        free(mensaje);
        free(nombre_fichero);
        return 0;
    }
        // 5.1 Abrir conexion a IP:puerto del destinatario
        int sock_destinatario = conectar_a_cliente(ip_destinatario, puerto_destinatario);
        if (sock_destinatario < 0) {
            pthread_mutex_lock(&mutex);
            clientes[indice_destinatario].estado = 0;
            pthread_mutex_unlock(&mutex);
            printf("s> MESSAGE %u FROM %s TO %s STORED\n", id, nombre_remitente, nombre_destinatario);
            free(nombre_destinatario);
            free(mensaje);
            free(nombre_fichero);
            return 0;
        }

        // 5.2 Enviar "SEND_MESSAGE\0"
        if (TIPO == 1){
            if (sendString(sock_destinatario, "SEND_MESSAGE_ATTACH") == -1){
                perror("Error al mandar el mensaje");
            }
        }
        else {
            if (sendString(sock_destinatario, "SEND_MESSAGE") == -1){
                    perror("Error al mandar el mensaje");
            }
        } 

        // 5.3 Enviar remitente
        sendString(sock_destinatario, nombre_remitente);

        // 5.4 Enviar id como string
        sprintf(id_str, "%u", id);
        sendString(sock_destinatario, id_str);

        // 5.5 Enviar mensaje
        sendString(sock_destinatario, mensaje);

        if (TIPO == 1){ // Envio nombre del fichero
            sendString(sock_destinatario, nombre_fichero);
        }

        // 5.6 Cerrar conexion
        close(sock_destinatario);

        // 5.7 Mostrar s> SEND MESSAGE <id> FROM <remitente> TO <destinatario>
        printf("s> SEND MESSAGE %u FROM %s TO %s\n", id, nombre_remitente, nombre_destinatario);

        // 5.8 Borrar mensaje de pendientes
        pthread_mutex_lock(&mutex);
        int ultimo = clientes[indice_destinatario].num_mensajes_pendientes - 1;
        clientes[indice_destinatario].mensajes_pendientes[indice_siguiente_mensaje] = clientes[indice_destinatario].mensajes_pendientes[ultimo];
        clientes[indice_destinatario].num_mensajes_pendientes--;

        // 5.9 Si remitente conectado → notificar con SEND_MESS_ACK + id
        if (clientes[indice_remitente].estado == 1) {
            char ip_remitente[MAX_IP];
            strcpy(ip_remitente, clientes[indice_remitente].IP);
            int puerto_remitente = clientes[indice_remitente].PUERTO;
            pthread_mutex_unlock(&mutex);

            int sock_remitente = conectar_a_cliente(ip_remitente, puerto_remitente);
            if (sock_remitente >= 0) {
                if (TIPO == 1){
                    sendString(sock_remitente, "SEND_MESS_ATTACH_ACK");
                    sendString(sock_remitente, id_str);
                    sendString(sock_remitente, nombre_fichero);
                }
                else {
                    sendString(sock_remitente, "SEND_MESS_ACK");
                    sendString(sock_remitente, id_str);
                }
                close(sock_remitente);
            }
        } 
        else {
            pthread_mutex_unlock(&mutex);
        }

    if (TIPO == 1 && fichero_out != NULL) {
        *fichero_out = strdup(nombre_fichero);
    }
    free(nombre_destinatario);
    free(mensaje);
    free(nombre_fichero);
    return 0;
}

int list_users(int client_sock, char* nombre_usuario) { // Lista los usuarios conectados

    // 2. Miro si el cliente esta registrado
    pthread_mutex_lock(&mutex);
    int indice = find_user_by_name(nombre_usuario);
    if (indice == -1){
        pthread_mutex_unlock(&mutex);
        char response = 2;
        sendMessage(client_sock, &response, 1);
        printf("s> CONNECTEDUSERS FAIL\n");
        return -1;
    }
    if (clientes[indice].estado == 0){ // No Conectado
        pthread_mutex_unlock(&mutex);
        char response = 1;
        sendMessage(client_sock, &response, 1);
        printf("s> CONNECTED USERS FAIL\n");
        return -1;
    }

    // Conectado - Obtengo los usuarios conectados
    char response = 0;
    sendMessage(client_sock, &response, 1);
    printf("s> CONNECTED USERS OK\n");

    // Mando el numero de conectados
    int num_conectados = 0;
    for (int j = 0; j < num_clientes; j++){
        if (clientes[j].estado == 1) num_conectados++;
    }
    char num_string[10];
    sprintf(num_string, "%d", num_conectados);
    sendString(client_sock, num_string);

    // Voy mandando los nombres de los clientes
    for (int j = 0; j < num_clientes; j++){
        if (clientes[j].estado == 1){
            char info[MAX_NOMBRE + MAX_IP + 20];
            sprintf(info, "%s :: %s :: %d",
                    clientes[j].nombre,
                    clientes[j].IP,
                    clientes[j].PUERTO);
            sendString(client_sock, info);
        }
    }
    pthread_mutex_unlock(&mutex);
    return 0;
}

// Funcion principal del hilo
void* handle_client(void* arg) {
    // 1. Obtengo la info del cliente haciendo el cast
    struct thread_args* info_cliente = (struct thread_args*)arg;

    // 2. Redirigo al cliente a la funcion correspondiente
    char* mensaje_recibido = malloc (sizeof(char) * MAX_MENSAJE);
    if (mensaje_recibido == NULL){
        perror("Error al declarar el buffer para recibir el mensaje");
        return NULL;
    }
    if (recvString(info_cliente->client_sock, mensaje_recibido, MAX_MENSAJE) == -1){
        perror ("Error al recibir el mensaje del cliente");
        return NULL;
    }
    // Leo el nombre del usuario
    int resultado  = 0;
    char *usuario = leer_nombre_usuario(info_cliente->client_sock);
    if (usuario == NULL) {
        free(mensaje_recibido);
        free(info_cliente);
        return NULL;
    }

    if (strcmp(mensaje_recibido, "REGISTER") == 0) {
        register_user(info_cliente->client_sock, usuario);
        if (imprimir_log_1(usuario, "REGISTER", &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
            clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
    } 
    else if (strcmp(mensaje_recibido, "UNREGISTER") == 0) {
        unregister_user(info_cliente->client_sock, usuario);
        if (imprimir_log_1(usuario, "UNREGISTER", &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
            clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
    }
    else if (strcmp(mensaje_recibido, "CONNECT") == 0) {
        connect_user(info_cliente->client_sock, info_cliente->client_dir, usuario);
        if (imprimir_log_1(usuario, "CONNECT", &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
            clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
    }
    else if (strcmp(mensaje_recibido, "DISCONNECT") == 0) {
        disconnect_user(info_cliente->client_sock, usuario, info_cliente->client_dir);
        if (imprimir_log_1(usuario, "DISCONNECT", &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
            clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
    }
    else if (strcmp(mensaje_recibido, "USERS") == 0) {
        list_users(info_cliente->client_sock, usuario);
        if (imprimir_log_1(usuario, "USERS", &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
            clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
    }
    else if (strcmp(mensaje_recibido, "SEND") == 0) {
        send_message(info_cliente->client_sock, 0, NULL, usuario);
        if (imprimir_log_1(usuario, "SEND", &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
            clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
    }
    else if (strcmp(mensaje_recibido, "SENDATTACH") == 0) {
        char *fichero = NULL;
        int ret = send_message(info_cliente->client_sock, 1, &fichero, usuario);
        if (ret == 0 && fichero != NULL) {
            char operacion[512];
            snprintf(operacion, sizeof(operacion), "SENDATTACH %s", fichero);
            if (imprimir_log_1(usuario, operacion, &resultado, info_cliente->client_rpc) != RPC_SUCCESS)
                clnt_perror(info_cliente->client_rpc, "Error en imprimir_log_1");
            free(fichero);
        }
    }
    else {
        perror("Operacion no reconocida");
    }

    free(usuario);
    free(mensaje_recibido);
    close(info_cliente->client_sock);  
    free(info_cliente);
    return NULL;

}

int main (int argc, char* argv[]) {
    // 1. Compruebo la entrada
    if (argc != 3) {
        perror("Error en el numero de argumentos");
        return -1;
    }
    if (strcmp(argv[1], "-p") != 0){
        perror("Error en el contenido de los argumentos");
        printf("'%s' no valido\n", argv[1]);
        return -1;
    }
    // 2. Obtengo el puerto
    char* endptr;
    int PUERTO = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0') {
        perror("El puerto pasado por la entrada no es valido");
        return -1;
    }
    
    // 3. Creo el socket
    struct sockaddr_in server_dir, client_dir;
    int client_sock;
    socklen_t server_dir_len = sizeof(server_dir);
    socklen_t client_dir_len = sizeof(client_dir);
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0){
        perror("Error al abrir el socket");
        return -1;
    }
    // 3. Hago reutilizable el puerto
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 4. Hago el bind del socket
    server_dir.sin_family = AF_INET;
    server_dir.sin_port = htons(PUERTO);
    server_dir.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr*)&server_dir, server_dir_len) < 0) {
        perror("Error al hacer el bind del socket");
        return -1;
    }

    // Nuevo paso, creo el cliente RPC
    char* rpc_ip =  getenv("LOG_RPC_IP");
    if (rpc_ip == NULL){
        perror("Error al obtener la variable de entorno LOG_RPC_IP");
        return -1;
    }
    CLIENT* clnt = clnt_create(rpc_ip, SERVIDOR_LOG, VERSION_1, "tcp");
    if (clnt == NULL){
        clnt_pcreateerror(rpc_ip);
        return -1;
    }

    // 5. Me pongo en escucha
    if (listen(server_sock, BACKLOG) < 0){
        perror("Error al ponerme en escucha");
        return -1;
    }

    // 6. Obtengo la IP local y muestro el mensaje
    char hostname [MAX_NOMBRE];
    gethostname(hostname, sizeof(hostname));
    struct hostent *he = gethostbyname(hostname);
    char* localIP = inet_ntoa(*(struct in_addr*)he->h_addr_list[0]);
    printf("s> init server <%s>:<%i>\n", localIP, PUERTO);
    printf("s> \n");



    // Acepto conexiones
    signal(SIGINT, manejador_sigint);
    while (1){
        client_sock = accept(server_sock, (struct sockaddr*)&client_dir, &client_dir_len);
        if (client_sock < 0) {
            perror("Error al hacer el conccet");
            continue;
        }
        // Creo un hilo, que se encargara de ese cliente
        pthread_t thread_id;
        struct thread_args* info = malloc(sizeof(struct thread_args));
        if (info == NULL){
            perror("Error al hacer el malloc para el cliente");
            close(client_sock);
            continue;
        }
        // Relleno thread_args
        info->client_sock = client_sock;
        memcpy(&(info->client_dir), &client_dir, sizeof(client_dir));
        info->client_rpc = clnt;

        if (pthread_create(&thread_id, NULL, handle_client, (void*)info) !=0 ) {
            perror("Error al crear el hilo para el cliente");
            close(client_sock);
            free(info);
            continue;
        }
        pthread_detach(thread_id);

   }

}
