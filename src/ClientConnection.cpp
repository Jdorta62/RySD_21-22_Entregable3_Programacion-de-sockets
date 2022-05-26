//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//              This class processes an FTP transaction.
// 
//****************************************************************************



#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h> 
#include <iostream>
#include <dirent.h>

#include "common.h"
#include "ClientConnection.h"

int define_socket_TCP(int port);

ClientConnection::ClientConnection(int s) {
  int sock = (int)(s);
  char buffer[MAX_BUFF];
  control_socket = s;
  // Check the Linux man pages to know what fdopen does.
  fd = fdopen(s, "a+");
  if (fd == NULL){
    std::cout << "Connection closed" << std::endl;
    fclose(fd);
    close(control_socket);
    ok = false;
    return ;
  }
  ok = true;
  data_socket = -1;
  parar = false;
};

ClientConnection::~ClientConnection() {
  fclose(fd);
  close(control_socket); 
}

int connect_TCP( uint32_t address,  uint16_t  port) {
  struct sockaddr_in sin;
  int s;
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0) {
    errexit("Error - ClientConnection.cpp(68): No se pudo crear el socket %s\n", strerror(errno));
  }
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = address;
  sin.sin_port = htons(port);
  if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {  /// asigna una dirección al socket s
    errexit("Error - ClientConnection.cpp(76): No puedo hacer el conect con el puerto: %s\n", strerror(errno));
  }

  return s;
}

void ClientConnection::stop() {
  close(data_socket);
  close(control_socket);
  parar = true;
}

#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {
  if (!ok) {
    return;
  }
  fprintf(fd, "220 Service ready\n");

  while(!parar) {
    fscanf(fd, "%s", command);
    if (COMMAND("USER")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "331 User name ok, need password\n");
    } else if (COMMAND("PWD")) {
      // printf("(PWD):SHOW\n");
      // char path[MAX_BUFF]; 
      // if (getcwd(path, sizeof(path)) != NULL)
      //   fprintf(fd, "257 \"%s\" \n", path);
    } else if (COMMAND("PASS")) {
      fscanf(fd, "%s", arg);
      if(strcmp(arg,"1234") == 0) {
        fprintf(fd, "230 User logged in\n");
      } else {
        fprintf(fd, "530 Not logged in.\n");
        parar = true;
      }
    } else if (COMMAND("PORT")) {
      int h0, h1, h2, h3, a1, a2;
      fscanf(fd, "%d, %d, %d, %d, %d, %d", &h0, &h1, &h2, &h3, &a1, &a2);
      int32_t ip_address = h3 << 24 | h2 << 16 | h1 << 8 | h0;
      int16_t port_number = a1 << 8 | a2;
      data_socket = connect_TCP(ip_address, port_number);
      fprintf(fd, "200 OK\n");
      
      // To be implemented by students
    } else if (COMMAND("PASV")) {
      sockaddr_in socket;     /// Socket para poder escuchar en un puerto distinto que el por defecto
      socklen_t lenght = sizeof(socket);
      int s = define_socket_TCP(0);   /// Creamos un socket nuevo y dejamos el SO le asigne un puerto
      getsockname(s, (sockaddr*)&socket, &lenght);
      uint16_t port = socket.sin_port;  /// obtenemos puerto en el que se va escuchar por peticiones.
      int port_1 = port >> 8;
      int port_2 = port & 0xFF;
      fprintf(fd, "227 Entering passive mode (127,0,0,1,%d,%d)\n", port_2, port_1);  /// Mostramos la dirección IP y el puerto por el que está escuchando
      fflush(fd);
      data_socket = accept(s, (sockaddr*)&socket, &lenght);  /// inicializamos el atributo privado de la clase ClientConnection
      // To be implemented by students
    } else if (COMMAND("STOR") ) {
      char buffer[1024]; /// Tamaño del buffer 1024 bytes
      int maxbuffer = 64;  /// Nmero de bytes máximo a extraer por cada llamada a la funcion recv
      fscanf(fd, "%s", arg);  
      FILE* file = fopen(arg, "wb");
      if (file != NULL) {
        fprintf(fd, "150 File status okay; about to open data connection.\n");
        fflush(fd);
        while (1) {
          int recived_bytes = recv(data_socket, buffer, maxbuffer, 0);
          int r = fwrite(buffer, 1, maxbuffer, file);
          if (recived_bytes == 0) {
            break;
          }
        }
        fprintf(fd, "226 Closing data connection. Requested file action successful.\n");
      } else {
        fprintf(fd, "450 Requested file action not taken. File unavaible.\n");
      }
      fclose(file);
      close(data_socket);
      // To be implemented by students
    } else if (COMMAND("RETR")) {
      char buffer[1024];
      int maxbuffer = 64;
      fscanf(fd, "%s", arg);
      printf("(RETR):%s\n", arg);
      FILE* file = fopen(arg, "rb");
      if (file != NULL) {
        fprintf(fd, "150 File status okay; about to open data connection.\n");
        while (1) {
          int r = fread(buffer, 1, maxbuffer, file);
          if (r == 0) {
            break;
          }
          send (data_socket, buffer, r, 0);
        }
        fprintf(fd, "226 Closing data connection. Requested file action successful.\n");
        fprintf(fd, "200  OK\n");
      } else {
        fprintf(fd, "450 Requested file action not taken. File unavaible.\n");
      }
      close(data_socket);
      fclose(file);
      // To be implemented by students
    } else if (COMMAND("LIST")) {
      DIR *dir = opendir(".");
      struct dirent *ent;
      if (dir > 0) {
       fprintf(fd,"125 Data connection already open; transfer starting.\n");
       while ((ent = readdir(dir))) {
         std::string directory_list = ent->d_name;
         directory_list += "\x0D\x0A";
         send(data_socket, directory_list.c_str(), directory_list.size(), 0);
       }
       close(data_socket);
       closedir(dir);
       fprintf(fd, "250 Requested file action okay, completed\n");
      } else {
        perror("450 Requested file action not taken. File unavaible.\n");
      }
      fprintf(fd, "200  OK\n");
    } else if (COMMAND("SYST")) {
      fprintf(fd, "215 UNIX Type: L8.\n");   
    } else if (COMMAND("TYPE")) {
      fscanf(fd, "%s", arg);
      fprintf(fd, "200 OK\n");   
    } else if (COMMAND("QUIT")) {
      fprintf(fd, "221 Service closing control connection. Logged out if appropriate.\n");
      close(data_socket);	
      parar=true;
      break;
    } else {
      fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
      printf("Comando : %s %s\n", command, arg);
      printf("Error - ClientConnection.cpp (128): Error interno del servidor\n");
    }
  }
  fclose(fd);
  return;
};
