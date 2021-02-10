#include <stdio.h>
#include <stdlib.h>
/* You will to add includes here */
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
// Included to get the support library
#include <calcLib.h>


#include "protocol.h"


#define DEBUG
int main(int argc, char *argv[]){
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port). 
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'. 
  */
  char delim[]=":";
  char *Desthost=strtok(argv[1],delim);
  char *Destport=strtok(NULL,delim);
  // *Desthost now points to a sting holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter. 

  /* Do magic */
  int port=atoi(Destport);
#ifdef DEBUG 
  printf("Host %s, and port %d.\n",Desthost,port);
#endif

  sockaddr_in hint;
  memset(&hint, 0, sizeof hint);
  hint.sin_family = AF_UNSPEC;
  hint.sin_port = htons(port);
  inet_pton(AF_UNSPEC, Desthost, &hint.sin_addr);
  socklen_t addrlen = sizeof (hint);

  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock == -1)
  {
    printf("Socket failed!");
    return 1;
  }

  calcMessage* firstMessage = new calcMessage; 
  *firstMessage = {22, 0, 17, 1, 0};
  firstMessage->type = htons(firstMessage->type);
  firstMessage->message = htonl(firstMessage->message);
  firstMessage->protocol = htons(firstMessage->protocol);
  firstMessage->major_version = htons(firstMessage->major_version);
  firstMessage->minor_version = htons(firstMessage->minor_version);

  int send = sendto(sock, firstMessage, sizeof (*firstMessage), 0, (sockaddr*)&hint, addrlen);
  if(send == -1)
  {
    perror("Send failed!");
    return 2;
  }
  /*
  ntohl;
  ntohs;
  htons;
  htonl;
  */
  calcProtocol* protocol = new calcProtocol;
  calcMessage* message = new calcMessage;


  int numberOfBytes = recvfrom(sock, protocol, sizeof (protocol), 0, (sockaddr*)&hint, &addrlen);
  if(numberOfBytes == sizeof (calcProtocol))
  {
    protocol->type = ntohs(protocol->type);
    protocol->major_version = ntohs(protocol->major_version);
    protocol->minor_version = ntohs(protocol->minor_version);
    protocol->id = ntohl(protocol->id);
    protocol->arith = ntohl(protocol->arith);
    protocol->inValue1 = ntohl(protocol->inValue1);
    protocol->inValue2 = ntohl(protocol->inValue2);
    protocol->inResult = ntohl(protocol->inResult);
    protocol->flValue1 = ntohl(protocol->flValue1);
    protocol->flValue2 = ntohl(protocol->flValue2);
    protocol->flResult = ntohl(protocol->flResult);
  }
  if(numberOfBytes == sizeof (calcMessage))
  {
    message = (calcMessage*)protocol;
    if((message->type == 2) && (message->message == 2) && (message->major_version == 1) && (message->minor_version == 0))
    {
      printf("NOT OK");
      return 3;
    }
  }

  close(sock);
  delete firstMessage;
  delete protocol;
  delete message;
  return 0;
}
