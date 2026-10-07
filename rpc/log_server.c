#include "plantilla.h"

bool_t imprimir_log_1_svc (char * usuario, char* operacion, int *resultado, struct svc_req * rqstp) {
    printf("%s\t%s\n", usuario, operacion);
    (void)rqstp; // Para silenciar el warning
    *resultado = 0; // Para indicarle al cliente que hubo exito
    fflush(stdout);  // Para que imprima inmediatamente
    return TRUE;
}

// Funcion de limpieza
int servidor_log_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result) {
    (void)transp;
    xdr_free(xdr_result, result);
    return 1;
}