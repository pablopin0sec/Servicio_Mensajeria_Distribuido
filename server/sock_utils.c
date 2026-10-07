#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "sock_utils.h"

int sendMessage(int socket, char *buffer, int len) {
    int r;
    int l = len;
    do {
        r = write(socket, buffer, l);
        l = l - r;
        buffer = buffer + r;
    } while ((l > 0) && (r >= 0));

    if (r < 0) return -1;
    else return 0;
}

int recvMessage(int socket, char *buffer, int len) {
    int r;
    int l = len;
    do {
        r = read(socket, buffer, l);
        l = l - r;
        buffer = buffer + r;
    } while ((l > 0) && (r >= 0));

    if (r < 0) return -1;
    else return 0;
}

int sendString(int socket, char* buffer) {
    return sendMessage(socket, buffer, strlen(buffer) + 1);
}

int recvString(int socket, char* buffer, int max_len) {
    memset(buffer, 0, max_len);
    int contador = 0;
    char caracter_actual = 'x';
    while (caracter_actual != '\0' &&  contador < max_len - 1) {
        int respuesta = recvMessage(socket, &caracter_actual, 1);
        if (respuesta < 0){
            return -1;
        }
        buffer[contador] = caracter_actual;
        contador ++;
    }
    return 0;
}

int conectar_a_cliente(char* ip, int puerto) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    struct sockaddr_in dir;
    dir.sin_family = AF_INET;
    dir.sin_port = htons(puerto);
    inet_pton(AF_INET, ip, &dir.sin_addr);
    if (connect(sock, (struct sockaddr*)&dir, sizeof(dir)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

