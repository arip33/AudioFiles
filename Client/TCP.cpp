/** This class is used to send and receive data across the TCP 
 * transport layer.  It is inherited from the TransProtocol class.
 *
 * @author William McVicker
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "TCP.h"

#include <iostream>
#include <string.h>

TCP::TCP(enum PROTO_IO protoIO, string hostname, uint16_t portNum) {

   socket_num = -1;
   client_socket = -1;
   temp_socket = -1;

   openSocket();
   if (protoIO == SERVER_IO)
      initMaster(portNum);
   else if (protoIO == CLIENT_IO)
      initSlave(hostname, portNum);
   else {
      cerr << "Invalid protocol I/O: " << protoIO << endl;
      exit(EXIT_FAILURE);
   }
}

TCP::~TCP() {

   close(socket_num);
   close(client_socket);
}

/** Opens a TCP socket
 * 
 * @return the socket number
 */
int TCP::openSocket() {
   int reuseport;

   if ((socket_num = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
      perror("socket");
      return -1;
   }

   reuseport = 0;
   if (setsockopt(socket_num, SOL_SOCKET, SO_REUSEADDR,
            (const char *) &reuseport, sizeof(reuseport)) < 0) {
      perror("setsockopt");
      return -1;
   }

   return socket_num;
}

void TCP::initMaster(uint16_t portNum) {
   struct sockaddr_in address;

   /* name the socket */
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(portNum);

   /* bind the socket to a port */
   if (bind(socket_num, (struct sockaddr *) &address, sizeof(address)) < 0) {
      perror("bind");
      close(socket_num);
      exit(EXIT_FAILURE);
   }

   // XXX Print the port number chosen
   struct sockaddr mySocket;
   socklen_t mySocket_len = sizeof(struct sockaddr);
   getsockname(socket_num, &mySocket, &mySocket_len);
   cerr << "Client is using port ";
   cerr << ntohs(((struct sockaddr_in *)&mySocket)->sin_port) << endl;
   // XXX end of temp code

   if (listen(socket_num, SOMAXCONN) == -1) {
      perror("listen()");
      close(socket_num);
      exit(EXIT_FAILURE);
   }

}

void TCP::initSlave(string hostname, uint16_t portNum) {
   struct hostent *theHost;
   struct sockaddr_in address;
   char *hstName;

   hstName = new char[hostname.length()+1];
   strcpy(hstName, hostname.c_str());

   if (NULL == (theHost = gethostbyname(hstName))) {
      cerr << "gethostbyname(): " << strerror(h_errno) << endl;
      return;
   }
   memcpy(&address.sin_addr, theHost->h_addr, theHost->h_length);
   address.sin_family = AF_INET;
   address.sin_port = htons(portNum);

   if (connect(socket_num, (struct sockaddr *) &address, sizeof(address)) < 0) {
      perror("connect()");
      exit(EXIT_FAILURE);
   }
   
   client_socket = socket_num;

   delete[] hstName; // cleanup after yourself
}

/** Sends a packet over the TCP socket 
 *
 * @param buf the packet to send
 * @param len the size of the packet
 * @param flags flags to set
 * @return the number of characters sent. On error, -1 is returned
 */
int TCP::sendPacket(const void *buf, size_t len, int flags) {

   return send(client_socket, buf, len, flags);
}

/** Gets a packet from the TCP socket.
 *
 * @param buf the buffer to store the packet in
 * @param len the length of the packet received
 * @param flags flags that indicate how to receive the packet
 * @return the number of bytes received. On error, -1 is returned
 */
int TCP::recvPacket(void *buf, size_t len, int flags) {
   int retVal = recv(client_socket, buf, len, flags);

#ifdef DEBUG
   if (retVal)
      PRINT_PACKET(*((packet *) buf));
#endif

   return retVal;
}

/** Identifies who is calling 
 *
 * @return the uid of the person calling when successful, else 0
 */
uint32_t TCP::getCallerID() {
   int status, rec_size;
   packet initPacket;

   temp_socket = accept(socket_num, NULL, NULL);
   if (temp_socket < 0) {
      perror("accept()");
      return 0;
   }

   status = select_call(&temp_socket, 1, INIT_TIMEOUT, 0);
   if (status) {
      rec_size = recv(temp_socket, (void *) &initPacket, sizeof(packet), 0);
      if (rec_size == 0) {
         // session was shutdown
         cout << "Call ended\n";
      } else if (rec_size != sizeof(packet)) {
         perror("recv()");
      } else {
         return initPacket.uid;
      }
   }

   close(temp_socket);
   return 0;
}

void TCP::ignoreCaller() {

   close(temp_socket);
}

void TCP::answerCall() {

   client_socket = temp_socket;
}

void TCP::endCall() {

   close(client_socket);
}
