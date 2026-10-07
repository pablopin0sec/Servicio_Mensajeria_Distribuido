#ifndef SOCK_UTILS_H
#define SOCK_UTILS_H

int sendMessage(int socket, char *buffer, int len);
int recvMessage(int socket, char *buffer, int len);
int sendString(int socket, char* buffer);
int recvString(int socket, char* buffer, int max_len);
int conectar_a_cliente(char* ip, int puerto);

#endif