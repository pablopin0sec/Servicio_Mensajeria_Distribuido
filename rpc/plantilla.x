program SERVIDOR_LOG {
    version VERSION_1 {
        int imprimir_log (string usuario<256>, string operacion<256>) = 1;
    } = 1;
} = 100001;