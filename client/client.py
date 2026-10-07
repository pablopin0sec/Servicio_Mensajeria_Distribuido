from enum import Enum
import argparse
import socket
import threading

class client :

    # ******************** TYPES *********************
    # *
    # * @brief Return codes for the protocol methods
    class RC(Enum) :
        OK = 0
        ERROR = 1
        USER_ERROR = 2

    # ****************** ATTRIBUTES ******************
    _server = None
    _port = -1
    _listen_sock = None # Socket donde el hilo escucha al servidor
    _connected_user = None # Nombre del usuario conectado
    _users_info = {} # Información sobre los usuarios conectados

    # **************** AUXILIARY FUNCTIONS ****************
    @staticmethod
    def _normalizar_mensaje(message):
        try:
            import zeep
            ws = zeep.Client(wsdl="http://localhost:8000/?wsdl")
            return ws.service.normalizar(message)
        except (ImportError, OSError, Exception):
            return message  # si el servicio web no está disponible, devuelve el mensaje sin normalizar
    
    @staticmethod
    def _recv_string(sock):
        result = b""
        while True:
            char = sock.recv(1)      # leer un byte
            if char == b'\0':        # si es el terminador, paramos
                break
            result += char           # si no, lo añadimos al resultado
        return result.decode()     # convertimos bytes a string
    
    @staticmethod
    def _listen_messages():
        while True:
            try:
                # 1. Aceptar conexión entrante
                conn, _ = client._listen_sock.accept() # Como addr no lo usamos, lo recogemos en _

                # 2. Leer la operación
                operacion = client._recv_string(conn)

                # 3. Procesar la operación
                if operacion == "SEND_MESSAGE":
                    # Leer remitente, id y mensaje (Se envían por separado, cada uno acaba en su \0)
                    remitente = client._recv_string(conn)
                    id_msg = client._recv_string(conn)
                    msg = client._recv_string(conn)

                    # Mostrar mensaje
                    print(f"\ns> MESSAGE {id_msg} FROM {remitente}")
                    print(f"   {msg}")
                    print("   END")

                    print("c> ", end="", flush=True)  # ← reimprimir el prompt

                elif operacion == "SEND_MESS_ACK":
                    # Leer id
                    id_msg = client._recv_string(conn)

                    # Mostrar confirmación
                    print(f"SEND MESSAGE {id_msg} OK")
                    print("c> ", end="", flush=True)  # ← reimprimir el prompt

                elif operacion == "SEND_MESSAGE_ATTACH":
                    # Leer remitente, id, mensaje y nombre de fichero
                    remitente = client._recv_string(conn)
                    id_msg = client._recv_string(conn)
                    msg = client._recv_string(conn)
                    file_name = client._recv_string(conn)

                    # Mostrar mensaje
                    print(f"\nc> MESSAGE {id_msg} FROM {remitente}")
                    print(f"   {msg}")
                    print("   END")
                    print(f"   FILE {file_name}")

                    print("c> ", end="", flush=True)  # ← reimprimir el prompt

                elif operacion == "SEND_MESS_ATTACH_ACK":
                    # Leer id y nombre del fichero
                    id_msg = client._recv_string(conn)
                    file_name = client._recv_string(conn)

                    # Mostrar confirmación
                    print(f"SENDATTACH MESSAGE {id_msg} {file_name} OK")
                    print("c> ", end="", flush=True)  # ← reimprimir el prompt

                elif operacion == "GET_FILE":
                    # Leer usuario solicitante y fichero que quiere
                    _ = client._recv_string(conn) # !!! El solicitante no se utiliza
                    file_name = client._recv_string(conn)

                    # Leer contenido del fichero
                    with open(file_name, 'rb') as f:
                        data = f.read()

                    # Enviar tamaño y contenido del fichero
                    conn.sendall((str(len(data)) + '\0').encode())
                    conn.sendall(data)
                
                # 4. Cerrar conexión
                conn.close()

            except FileNotFoundError:
                print("c> GET_FILE ERROR: file not found")
                conn.close()
            except (socket.error, OSError) as e:
                # Socket cerrado o error de red; salir del hilo
                break


    # ******************** METHODS *******************
    # *
    # * @param user - User name to register in the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user is already registered
    # * @return ERROR if another error occurred
    @staticmethod
    def  register(user) :
        try:
            # 1. Conectarse al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 2. Enviar la cadena "REGISTER" indicando la operación
            s.sendall(b"REGISTER\0")

            # 3. Enviar nombre de usuario a registrar
            s.sendall((user + '\0').encode())

            # 4. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 5. Cerrar la conexión
            s.close()

            # 6. Procesar el resultado
            code = result[0]
            if code == 0:               # Usuario registrado correctamente
                print("c> REGISTER OK")
                return client.RC.OK
            elif code == 1:             # Nombre de usuario en uso
                print("c> USERNAME IN USE")
                return client.RC.USER_ERROR
            else:                       # Error al realizar la operación de registro
                print("c> REGISTER FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            # Servidor caído o error de comunicación
            print("c> REGISTER FAIL")
            return client.RC.ERROR

    # *
    # 	 * @param user - User name to unregister from the system
    # 	 * 
    # 	 * @return OK if successful
    # 	 * @return USER_ERROR if the user does not exist
    # 	 * @return ERROR if another error occurred
    @staticmethod
    def  unregister(user) :
        try:
            # 1. Conectar al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 2. Enviar la cadena "UNREGISTER" indicando la operación
            s.sendall(b"UNREGISTER\0")

            # 3. Enviar nombre del usuario a dar de baja
            s.sendall((user + '\0').encode())

            # 4. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 5. Cerrar la conexión
            s.close()
            
            # 6. Procesar el resultado
            code = result[0]
            if code == 0:
                print("c> UNREGISTER OK")
                return client.RC.OK
            elif code == 1:
                print("c> USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c> UNREGISTER FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            print("c> UNREGISTER FAIL")
            return client.RC.ERROR


    # *
    # * @param user - User name to connect to the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  connect(user) :
        try:
            # 1. Buscar puerto libre
            listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listen_sock.bind(('', 0))
            puerto = listen_sock.getsockname()[1]
            listen_sock.listen(10) # Por ejemplo 10, más que suficiente para nosotros
            client._listen_sock = listen_sock

            # 2. Crear hilo encargado de escuchar y atender mensajes de otros usuarios
            threading.Thread(target=client._listen_messages, args=()).start()

            # 3. Conectar al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 4. Enviar la cadena "CONNECT" indicando la operación
            s.sendall(b"CONNECT\0")

            # 5. Enviar nombre de usuario que se conecta
            s.sendall((user + '\0').encode())

            # 6. Enviar puerto de escucha del cliente
            s.sendall((str(puerto) + '\0').encode())

            # 7. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 8. Cerrar la conexión
            s.close()

            # 9. Procesar el resultado
            code = result[0]
            if code == 0:
                client._connected_user = user
                print("c> CONNECT OK")
                return client.RC.OK
            elif code == 1:
                print("c> CONNECT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif code == 2:
                print("c> USER ALREADY CONNECTED")
                return client.RC.USER_ERROR 
            else:
                print("c> CONNECT FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            print("c> CONNECT FAIL")
            return client.RC.ERROR

    # *
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist or if it is already connected
    # * @return ERROR if another error occurred
    @staticmethod
    def  users() :
        try:
            # 1. Conectar al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 2. Enviar la cadena "USERS" indicando la operación
            s.sendall(b"USERS\0")

            # 3. Enviar cadena con el nombre que identifica al usuario (!!! está en rojo, será por lo del atributo?)
            s.sendall((client._connected_user + '\0').encode())

            # 4. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 5. Procesar el resultado
            code = result[0]
            if code == 0:
                # Recibe número de usuarios conectados del servidor
                num_users = int(client._recv_string(sock=s))

                # Recibe tantas cadenas del servidor como clientes conectados
                users_info = {}

                for i in range (0, num_users):
                    info = client._recv_string(sock=s)
                    # Parsear info (ej. "pablo :: 192.168.1.1 :: 8080")
                    partes = info.split(" :: ")
                    nombre = partes[0]
                    ip     = partes[1]
                    puerto = partes[2]
                    # Guardar en el diccionario local
                    users_info[nombre] = (ip, puerto)

                # Guardar en el atributo de clase para usarlo en GETFILE
                client._users_info = users_info

                # Mostrar mensaje
                print(f"c> CONNECTED USERS ({num_users} users connected) OK\n")
                for user_name in users_info:
                    print(f"   {user_name}")

                return client.RC.OK
                
            elif code == 1:
                print("c> CONNECTED USERS FAIL, USER IS NOT CONNECTED")
                return client.RC.USER_ERROR
            else:
                print("c> CONNECTED USERS FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            print("c> CONNECTED USERS FAIL")
            return client.RC.ERROR
        except (ValueError, IndexError):
            print("c> CONNECTED USERS FAIL")
            return client.RC.ERROR

        finally:
            # 6. Cerrar la conexión
            if s:
                s.close()

    # *
    # * @param user - User name to disconnect from the system
    # * 
    # * @return OK if successful
    # * @return USER_ERROR if the user does not exist
    # * @return ERROR if another error occurred
    @staticmethod
    def  disconnect(user) :
        try:
            # 1. Conectar al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 2. Enviar la cadena "DISCONNECT" indicando la operación
            s.sendall(b"DISCONNECT\0")

            # 3. Enviar nombre de usuario que se desconecta
            s.sendall((user +  '\0').encode())

            # 4. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 5. Cerrar la conexión
            s.close()

            # 6. Procesar el resultado
            code = result[0]
            if code == 0:
                client._connected_user = None
                print("c> DISCONNECT OK")
                return client.RC.OK
            elif code == 1:
                print("c> DISCONNECT FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            elif code == 2:
                print("c> DISCONNECT FAIL, USER NOT CONNECTED")
                return client.RC.USER_ERROR
            else:
                print("c> DISCONNECT FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            print("c> DISCONNECT FAIL")
            return client.RC.ERROR
        
        finally: # IMPORTANTE: Cerramos la conexión del hilo aquí, para que se ejecute aunque haya excepción
            if client._listen_sock:
                client._listen_sock.close()
                client._listen_sock = None

    # *
    # * @param user    - Receiver user name
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  send(user,  message) :
        try:
            # 1. Conectar al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 2. Enviar la cadena "SEND" indicando la operación
            s.sendall(b"SEND\0")

            # 3. Enviar nombre de usuario remitente del mensaje
            s.sendall((client._connected_user + '\0').encode())

            # 4. Enviar nombre de usuario destinatario del mensaje
            s.sendall((user + '\0').encode())

            # 5. Enviar mensaje normalizado (máximo 255 caracteres + '\0')

            # 5.1 Normalizar mensaje con el servicio web
            message = client._normalizar_mensaje(message)

            # 5.2 Comprobar tamaño mensaje
            if len(message) > 255:
                message = message[:255] # Truncamos el mensaje (enunciado no especifica qué hacer aquí)

            # 5.3 Enviar mensaje
            s.sendall((message + '\0').encode())

            # 6. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 7. Procesar el resultado
            code = result[0]
            if code == 0:
                msg_id = client._recv_string(s)
                print(f"c> SEND OK - MESSAGE {msg_id}")
                return client.RC.OK
            elif code == 1:
                print(f"c> SEND FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c> SEND FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            print("c> SEND FAIL")
            return client.RC.ERROR
        
        finally:
            # 8. Cerrar la conexión
            if s:
                s.close()

    # *
    # * @param user    - Receiver user name
    # * @param file    - file  to be sent
    # * @param message - Message to be sent
    # * 
    # * @return OK if the server had successfully delivered the message
    # * @return USER_ERROR if the user is not connected (the message is queued for delivery)
    # * @return ERROR the user does not exist or another error occurred
    @staticmethod
    def  sendAttach(user,  file,  message) :
        
        try:
            # 1. Conectar al servidor
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))

            # 2. Enviar la cadena "SEND" indicando la operación
            s.sendall(b"SENDATTACH\0")

            # 3. Enviar nombre de usuario remitente del mensaje
            s.sendall((client._connected_user + '\0').encode())

            # 4. Enviar nombre de usuario destinatario del mensaje
            s.sendall((user + '\0').encode())

            # 5. Enviar mensaje normalizado (máximo 255 caracteres + '\0')

            # 5.1 Normalizar mensaje con el servicio web
            message = client._normalizar_mensaje(message)

            # 5.2 Comprobar tamaño mensaje
            if len(message) > 255:
                message = message[:255] # Truncamos el mensaje (enunciado no especifica qué hacer aquí)

            # 5.3 Enviar mensaje
            s.sendall((message + '\0').encode())
            
            # 6. Enviar nombre del fichero
            s.sendall((file + '\0').encode())

            # 7. Recibir resultado (1 byte) de la operación
            result = s.recv(1)

            # 8. Procesar el resultado
            code = result[0]
            if code == 0:
                msg_id = client._recv_string(s)
                print(f"c> SENDATTACH OK - MESSAGE {msg_id}")
                return client.RC.OK
            elif code == 1:
                print("c> SENDATTACH FAIL, USER DOES NOT EXIST")
                return client.RC.USER_ERROR
            else:
                print("c> SENDATTACH FAIL")
                return client.RC.ERROR

        except (socket.error, ConnectionRefusedError, OSError):
            print("c> SENDATTACH FAIL")
            return client.RC.ERROR
        
        finally:
            # 9. Cerrar la conexión
            if s:
                s.close()


    @staticmethod
    def  getfile(user, file, local_file_name) :
        try:
            # 1. Buscar dirección IP y puerto asociado al usuario
            # 1.1 Si no está guardado el usuario, refrescar la lista con USERS
            if user not in client._users_info:
                client.users()

            # 1.2 Si sigue sin estar, lanza error
            if user not in client._users_info:
                print("c> FILE TRANSFER FAILED, user not connected.")
                return client.RC.ERROR
            
            # 1.3 Si se encuentra la información, se almacena
            addr, port = client._users_info[user]


            # 2. Conectar al cliente (a su thread de escucha)
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((addr, int(port)))

            # 3. Enviar la cadena "GET_FILE" indicando la operación
            s.sendall(b"GET_FILE\0")

            # 4. Enviar nombre de usuario remitente
            s.sendall((user + '\0').encode())

            # 5. Enviar nombre del fichero a transferir
            s.sendall((file + '\0').encode())

            # 6. Recibir contenido de fichero y almacenarlo
            # 6.1 Recibir el tamaño del fichero
            size = int(client._recv_string(s))
            
            # 6.2 Recibir los bytes que ocupa el fichero
            data = b""
            while len(data) < size: # !!! se puede hacer data = s.recv(size), pero para ficheros grandes más seguro así
                chunk = s.recv(size - len(data))
                data += chunk

            # 6.3 Guardar en el fichero local
            with open(local_file_name, 'wb') as f:
                f.write(data)

            # 7. Cerrar la conexión
            s.close()
            return client.RC.OK
            
        except (socket.error, ConnectionRefusedError, OSError):
            return client.RC.ERROR
        except (FileNotFoundError, IOError):
            return client.RC.ERROR
        except ValueError:
            return client.RC.ERROR

    # *
    # **
    # * @brief Command interpreter for the client. It calls the protocol functions.
    @staticmethod
    def shell():

        while (True) :
            try :
                command = input("c> ")
                line = command.split(" ")
                if (len(line) > 0):

                    line[0] = line[0].upper()

                    if (line[0]=="REGISTER") :
                        if (len(line) == 2) :
                            client.register(line[1])
                        else :
                            print("Syntax error. Usage: REGISTER <userName>")

                    elif(line[0]=="UNREGISTER") :
                        if (len(line) == 2) :
                            client.unregister(line[1])
                        else :
                            print("Syntax error. Usage: UNREGISTER <userName>")

                    elif(line[0]=="CONNECT") :
                        if (len(line) == 2) :
                            client.connect(line[1])
                        else :
                            print("Syntax error. Usage: CONNECT <userName>")

                    elif(line[0]=="DISCONNECT") :
                        if (len(line) == 2) :
                            client.disconnect(line[1])
                        else :
                            print("Syntax error. Usage: DISCONNECT <userName>")

                    elif(line[0]=="USERS") :
                        if (len(line) == 1) :
                            client.users()
                        else :
                            print("Syntax error. Usage: CONNECTED_USERS <userName>")

                    elif(line[0]=="SEND") :
                        if (len(line) >= 3) :
                            #  Remove first two words
                            message = ' '.join(line[2:])
                            client.send(line[1], message)
                        else :
                            print("Syntax error. Usage: SEND <userName> <message>")

                    elif(line[0]=="SENDATTACH") :
                        if (len(line) >= 4) :
                            #  Remove first two words
                            message = ' '.join(line[3:])
                            client.sendAttach(line[1], line[2], message)
                        else :
                            print("Syntax error. Usage: SENDATTACH <userName> <filename> <message>")

                    elif(line[0]=="QUIT") :
                        if (len(line) == 1) :
                            break
                        else :
                            print("Syntax error. Use: QUIT")
                    
                    elif(line[0]=="GETFILE") :
                        if (len(line) == 4) :
                            client.getfile(line[1], line[2], line[3])
                        else :
                            print("Syntax error. Usage: GETFILE <username> <filename> <localFileName>")
                    else :
                        print("Error: command " + line[0] + " not valid.")
            except Exception as e:
                print("Exception: " + str(e))

    # *
    # * @brief Prints program usage
    @staticmethod
    def usage() :
        print("Usage: python3 client.py -s <server> -p <port>")


    # *
    # * @brief Parses program execution arguments
    @staticmethod
    def  parseArguments(argv) :
        parser = argparse.ArgumentParser()
        parser.add_argument('-s', type=str, required=True, help='Server IP')
        parser.add_argument('-p', type=int, required=True, help='Server Port')
        args = parser.parse_args()

        if (args.s is None):
            parser.error("Usage: python3 client.py -s <server> -p <port>")
            return False

        if ((args.p < 1024) or (args.p > 65535)):
            parser.error("Error: Port must be in the range 1024 <= port <= 65535");
            return False;
        
        client._server = args.s
        client._port = args.p

        return True


    # ******************** MAIN *********************
    @staticmethod
    def main(argv) :
        if (not client.parseArguments(argv)) :
            client.usage()
            return

        #  Write code here
        client.shell()
        print("+++ FINISHED +++")
    

if __name__=="__main__":
    client.main([])