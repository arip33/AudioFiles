/** This class is used to send and receive data across the DCCP 
 * transport layer.  It is inherited from the TransProtocol class.
 *
 * @author William McVicker
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "DCCP.h"

#include <sstream>
#include <iostream>
#include <string.h>

DCCP::DCCP(enum PROTO_IO protoIO, char *hostname, uint16_t portNum) {

   socket_num = -1;
   client_socket = -1;
   temp_socket = -1;

   if (protoIO == SERVER_IO) {
      initMaster(portNum);
   } else if (protoIO == CLIENT_IO)
      initSlave(hostname, portNum);
   else {
      cerr << "Invalid protocol I/O: " << protoIO << endl;
      exit(EXIT_FAILURE);
   }
}

DCCP::~DCCP() {

   close(socket_num);
   close(client_socket);
}

/** Opens a DCCP socket
 * 
 * @return the socket number
 */
void DCCP::initMaster(uint16_t portNum) {
   struct sockaddr_in address;

   int reuseport;

   if ((socket_num = socket(AF_INET, SOCK_DCCP, IPPROTO_DCCP)) < 0) {
      perror("socket");
      exit(EXIT_FAILURE);
   }

   reuseport = 0;
   if (setsockopt(socket_num, SOL_DCCP, SO_REUSEADDR,
            (const char *) &reuseport, sizeof(reuseport)) < 0) {
      perror("setsockopt");
      exit(EXIT_FAILURE);
   }

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

#ifdef DEBUG
   getPortNum();
#endif

   if (listen(socket_num, MAX_DCCP_CONNECTION_BACK_LOG) == -1) {
      perror("listen()");
      close(socket_num);
      exit(EXIT_FAILURE);
   }

}

void DCCP::initSlave(char *hostname, uint16_t portNum) {
   struct addrinfo hints;
   struct addrinfo *result, *rp;
   int retval, reuseport;
   stringstream myStream;
   myStream << portNum;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_INET;
   hints.ai_socktype = SOCK_DCCP;
   hints.ai_protocol = IPPROTO_DCCP;

   if ((retval = getaddrinfo(hostname, (myStream.str()).c_str(), &hints, &result)) != 0) {
      cerr << "getaddrinfo: " << gai_strerror(retval) << endl;
      exit(EXIT_FAILURE);
   }

   for (rp = result; rp != NULL; rp = rp->ai_next) {
      socket_num = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (socket_num < 0)
         continue;

      if (connect(socket_num, rp->ai_addr, rp->ai_addrlen) != -1 )
         break;

      close(socket_num);
   }

   if (rp == NULL) {
      cerr << "Could not connect to server\n";
      exit(EXIT_FAILURE);
   }
    
   reuseport = 0;
   if (setsockopt(socket_num, SOL_SOCKET, SO_REUSEADDR,
            (const char *) &reuseport, sizeof(reuseport)) < 0) {
      perror("setsockopt");
      exit(EXIT_FAILURE);
   }
   
   freeaddrinfo(result);
   client_socket = socket_num;
}

/** Sends a packet over the DCCP socket 
 *
 * @param buf the packet to send
 * @param len the size of the packet
 * @param flags flags to set
 * @return the number of characters sent. On error, -1 is returned
 */
int DCCP::sendPacket(const void *buf, size_t len, int flags) {

   return send(client_socket, buf, len, flags);
}

/** Gets a packet from the DCCP socket.
 *
 * @param buf the buffer to store the packet in
 * @param len the length of the packet received
 * @param flags flags that indicate how to receive the packet
 * @return the number of bytes received. On error, -1 is returned
 */
int DCCP::recvPacket(void *buf, size_t len, int flags) {
   
   return recv(client_socket, buf, len, flags);
}

/** Identifies who is calling 
 *
 * @return the uid of the person calling when successful, else 0
 */
uint32_t DCCP::getCallerID() {
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
#ifdef DEBUG
         cerr << "getCallerID: temp_socket shutdown\n";
#endif
      } else if (rec_size != sizeof(packet)) {
         perror("recv()");
      } else {
         return ntohl(initPacket.uid);
      }
   }

   close(temp_socket);
   return 0;
}

void DCCP::ignoreCaller() {

   close(temp_socket);
}

void DCCP::answerCall() {

   client_socket = temp_socket;
}

void DCCP::endCall() {

   close(client_socket);
}
