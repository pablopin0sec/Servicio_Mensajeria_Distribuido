# Practica-SSDD

Sistema de mensajería distribuido compuesto por cuatro procesos: un servidor de mensajería (C), un servidor de logs vía ONC RPC (C), un cliente interactivo (Python) y un servicio web SOAP normalizador de mensajes (Python).

## Dependencias del sistema

```bash
# Librería TI-RPC y portmapper (imprescindibles)
sudo apt install libtirpc-dev rpcbind
sudo service rpcbind restart
```

## Compilación (componentes C)

```bash
make          # compila ambos binarios en build/
make clean    # elimina build/
```

El Makefile genera dos ejecutables a partir de los fuentes en `server/` y `rpc/`:

| Binario               | Fuentes                                                                 | Función                            |
|-----------------------|-------------------------------------------------------------------------|------------------------------------|
| `build/servidor_rpc`  | `rpc/log_server.c`, `rpc/plantilla_svc.c`, `rpc/plantilla_xdr.c`       | Servidor de logs vía ONC RPC       |
| `build/server`        | `server/server.c`, `server/sock_utils.c`, `rpc/plantilla_clnt.c`, `rpc/plantilla_xdr.c` | Servidor principal de mensajería   |

**Nota sobre rpcgen:** los ficheros `plantilla.h`, `plantilla_clnt.c`, `plantilla_svc.c` y `plantilla_xdr.c` están generados a partir de `rpc/plantilla.x` y se versionan en el repositorio. Si modificas `plantilla.x`, debes regenerarlos manualmente:

```bash
cd rpc/
rpcgen plantilla.x    # regenera los stubs
cd ..
make clean && make    # recompila
```


## Orden de despliegue

El orden de arranque **es estricto**. Cada proceso debe ejecutarse en una terminal distinta.

### 1. Servidor RPC de logs

```bash
./build/servidor_rpc
```

Se registra en el portmapper (`rpcbind`) bajo el programa `100001` y espera peticiones RPC por TCP y UDP. Imprime en stdout cada operación registrada con formato `usuario\toperacion`.

### 2. Servidor principal de mensajería

```bash
LOG_RPC_IP=127.0.0.1 ./build/server -p <puerto>
```

- `LOG_RPC_IP`: IP donde corre el servidor RPC de logs (obligatorio, se lee vía `getenv`).
- `-p <puerto>`: puerto TCP donde escucha conexiones de clientes (obligatorio, rango 1024–65535).

El servidor es multihilo (un hilo por cliente, desacoplado con `pthread_detach`). Protege el array compartido `clientes[]` con un único `pthread_mutex_t`.

### 3. Servicio SOAP normalizador (opcional)

```bash
# Con el entorno virtual activado:
python3 ws-conversor.py
```

Arranca un servidor WSGI en `http://127.0.0.1:8000`. Expone un endpoint SOAP que normaliza mensajes (colapsa espacios múltiples y recorta extremos). Si este servicio no está disponible, el cliente simplemente envía los mensajes sin normalizar.

### 4. Cliente Python

```bash
# Con el entorno virtual activado:
python3 client/client.py -s <ip_servidor> -p <puerto_servidor>
```

Ejemplo con todo en localhost:

```bash
python3 client/client.py -s 127.0.0.1 -p 4500
```

El cliente abre un intérprete interactivo con los comandos: `REGISTER`, `UNREGISTER`, `CONNECT`, `DISCONNECT`, `USERS`, `SEND`, `SENDATTACH`, `GETFILE`, `QUIT`.

## Protocolo de red

Protocolo propio sobre TCP con strings delimitados por `\0` (no se usa prefijo de longitud). En C se usan `recvString`/`sendMessage` (definidas en `include/sock_utils.h`, implementadas en `server/sock_utils.c`). En Python se usan `_recv_string`/`_send_string` (definidas en `client/client.py`).

El comando `GETFILE` realiza una transferencia P2P: el cliente origen se conecta directamente al cliente destino usando la IP y puerto que el servidor devuelve en `USERS`.

## Arquitectura

```
┌──────────────────┐     TCP (mensajería)    ┌──────────────────┐
│  Python Client   │ ◄──────────────────────►│  C Server        │
│  (client.py)     │                         │  (server/server) │
│                  │                         │  multihilo       │
│  GETFILE P2P     │                         │  mutex clientes[]│
└────────┬─────────┘                         └────────┬─────────┘
         │                                            │
         │ SOAP (opcional)                            │ ONC RPC (TCP/UDP)
         ▼                                            ▼
┌──────────────────┐                         ┌──────────────────┐
│ SOAP Web Service │                         │ RPC Log Server   │
│ (ws-conversor.py)│                         │ (rpc/log_server) │
│ :8000            │                         │ prog 100001      │
└──────────────────┘                         └──────────────────┘
```

