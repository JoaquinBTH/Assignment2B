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
int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    printf("Usage: %s <ip>:<port> \n", argv[0]);
    exit(1);
  }
  /*
    Read first input, assumes <ip>:<port> syntax, convert into one string (Desthost) and one integer (port). 
     Atm, works only on dotted notation, i.e. IPv4 and DNS. IPv6 does not work if its using ':'. 
  */
  char delim[] = ":";
  char *Desthost = strtok(argv[1], delim);
  char *Destport = strtok(NULL, delim);
  // *Desthost now points to a sting holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter.

  /* Do magic */
  int port = atoi(Destport);
#ifdef DEBUG
  printf("Host %s, and port %d.\n", Desthost, port);
#endif

  /*
  sockaddr_in hint;
  memset(&hint, 0, sizeof hint);
  hint.sin_family = AF_UNSPEC;
  hint.sin_port = htons(port);
  inet_pton(AF_UNSPEC, Desthost, &hint.sin_addr);
  socklen_t addrlen = sizeof (hint);
  */

  struct addrinfo hint, *servinfo, *p;
  int rv;
  int sock;

  memset(&hint, 0, sizeof hint);
  hint.ai_family = AF_UNSPEC;
  hint.ai_socktype = SOCK_DGRAM;

  if ((rv = getaddrinfo(Desthost, Destport, &hint, &servinfo)) != 0)
  {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
  }
  for (p = servinfo; p != NULL; p = p->ai_next)
  {
    if ((sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      printf("Socket creation failed.\n");
      continue;
    }
    printf("Socket Created.\n");
    break;
  }

  if (p == NULL)
  {
    fprintf(stderr, "Client failed to create an apporpriate socket.\n");
    freeaddrinfo(servinfo);
    exit(1);
  }

  calcMessage *firstMessage = new calcMessage;
  //*firstMessage = {22, 0, 17, 1, 0};
  firstMessage->type = htons(22);
  firstMessage->message = htonl(0);
  firstMessage->protocol = htons(17);
  firstMessage->major_version = htons(1);
  firstMessage->minor_version = htons(0);
  int send;

  for (int i = 0; i < 3; i++)
  {
    send = sendto(sock, firstMessage, sizeof(*firstMessage), 0, p->ai_addr, p->ai_addrlen);
    if (send == -1)
    {
      if(i < 2)
      {
        printf("Send failed, trying again in 2 seconds\n");
        sleep(2);
        printf("Trying again...\n");
      }
      else
      {
        printf("Send failed too many times. Closing...");
        sleep(2);
        return 2;
      }
    }
    else
    {
      break;
    }
  }

  calcProtocol *protocol = new calcProtocol;
  calcMessage *message = new calcMessage;

  int numberOfBytes = recvfrom(sock, protocol, sizeof(*protocol), 0, p->ai_addr, &p->ai_addrlen);
  if (numberOfBytes == -1)
  {
    printf("Recieve failed!");
    return 3;
  }
  else if (numberOfBytes == sizeof(calcProtocol))
  {
    protocol->type = ntohs(protocol->type);
    protocol->major_version = ntohs(protocol->major_version);
    protocol->minor_version = ntohs(protocol->minor_version);
    protocol->id = ntohl(protocol->id);
    protocol->arith = ntohl(protocol->arith);
    protocol->inValue1 = ntohl(protocol->inValue1);
    protocol->inValue2 = ntohl(protocol->inValue2);
    protocol->inResult = ntohl(protocol->inResult);
  }
  else if (numberOfBytes == sizeof(calcMessage))
  {
    message = (calcMessage *)protocol;
    message->type = ntohs(message->type);
    message->message = ntohs(message->message);
    message->major_version = ntohs(message->major_version);
    message->minor_version = ntohs(message->minor_version);
    if ((message->type == 2) && (message->message == 2) && (message->major_version == 1) && (message->minor_version == 0))
    {
      printf("NOT OK");
      return 4;
    }
  }

  if (protocol->arith == 1)
  {
    protocol->inResult = protocol->inValue1 + protocol->inValue2;
  }
  else if (protocol->arith == 2)
  {
    protocol->inResult = abs(protocol->inValue1 - protocol->inValue2);
  }
  else if (protocol->arith == 3)
  {
    protocol->inResult = protocol->inValue1 * protocol->inValue2;
  }
  else if (protocol->arith == 4)
  {
    protocol->inResult = protocol->inValue1 / protocol->inValue2;
  }
  else if (protocol->arith == 5)
  {
    protocol->flResult = protocol->flValue1 + protocol->flValue2;
  }
  else if (protocol->arith == 6)
  {
    protocol->flResult = abs(protocol->flValue1 - protocol->flValue2);
  }
  else if (protocol->arith == 7)
  {
    protocol->flResult = protocol->flValue1 * protocol->flValue2;
  }
  else if (protocol->arith == 8)
  {
    protocol->flResult = protocol->flValue1 / protocol->flValue2;
  }
  else
  {
    printf("Something went wrong!");
    return 5;
  }

  protocol->type = htons(protocol->type);
  protocol->major_version = htons(protocol->major_version);
  protocol->minor_version = htons(protocol->minor_version);
  protocol->id = htonl(protocol->id);
  protocol->arith = htonl(protocol->arith);
  protocol->inValue1 = htonl(protocol->inValue1);
  protocol->inValue2 = htonl(protocol->inValue2);
  protocol->inResult = htonl(protocol->inResult);

  for(int i = 0; i < 3; i++)
  {
    send = sendto(sock, protocol, sizeof(*protocol), 0, p->ai_addr, p->ai_addrlen);
    if (send == -1)
    {
      if(i < 2)
      {
        printf("Send failed. Trying again in 2 seconds\n");
        sleep(2);
        printf("Trying again...\n");
      }
      else
      {
        printf("Send failed too many times. Closing...");
        sleep(2);
        return 6;
      }
    }
    else
    {
      break;
    }
  }

  numberOfBytes = recvfrom(sock, message, sizeof(*message), 0, p->ai_addr, &p->ai_addrlen);
  if (numberOfBytes == -1)
  {
    printf("Recieve Failed!");
    return 7;
  }
  else if (numberOfBytes == sizeof(calcMessage))
  {
    message->type = ntohs(message->type);
    message->message = ntohl(message->message);
    message->protocol = ntohs(message->protocol);
    message->major_version = ntohs(message->major_version);
    message->minor_version = ntohs(message->minor_version);
  }

  printf("Message: %d", message->message);

  close(sock);
  freeaddrinfo(servinfo);
  delete firstMessage;
  delete protocol;
  delete message;
  return 0;
}
