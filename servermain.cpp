#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

/* You will to add includes here */
#include <string.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <ctime>
#include <vector>

#include <iostream>

// Included to get the support library
#include <calcLib.h>

#include "protocol.h"

using namespace std;
/* Needs to be global, to be rechable by callback and main */
int loopCount = 0;
int terminateLoop = 0;

struct clientDetails
{
  char address[256];
  int port;
  struct timeval time;
  int id;
};
vector<clientDetails> clientVector;

/* Call back function, will be called when the SIGALRM is raised when the timer expires. */
void checkJobbList(int signum)
{
  // As anybody can call the handler, its good coding to check the signal number that called it.

  printf("Let me be, I want to sleep.\n");
  for (int i = 0; i < (int)clientVector.size(); i++)
  {
    //Tiden nu - tiden där klienten skickade >= 10. Då har det gått 10 sekunder.
    struct timeval compare;
    gettimeofday(&compare, NULL);
    if ((compare.tv_sec - clientVector.at(i).time.tv_sec) >= 10)
    {
      clientVector.erase(clientVector.begin() + i);
      i--;
    }
  }

  if (loopCount > 20)
  {
    printf("I had enough.\n");
    terminateLoop = 1;
  }

  return;
}

void randomCalculation(calcProtocol &protocol)
{
  protocol.arith = rand() % 8 + 1;
  protocol.inValue1 = rand() % 100 + 1;
  protocol.inValue2 = rand() % 100 + 1;

  protocol.flValue1 = (double)rand() / (RAND_MAX / 100);
  protocol.flValue2 = (double)rand() / (RAND_MAX / 100);
  if (protocol.arith == 1)
  {
    //Add
    protocol.inResult = protocol.inValue1 + protocol.inValue2;
  }
  else if (protocol.arith == 2)
  {
    //Subtraction
    protocol.inResult = protocol.inValue1 - protocol.inValue2;
  }
  else if (protocol.arith == 3)
  {
    //Multiplication
    protocol.inResult = protocol.inValue1 * protocol.inValue2;
  }
  else if (protocol.arith == 4)
  {
    //Division
    protocol.inResult = protocol.inValue1 / protocol.inValue2;
  }
  else if (protocol.arith == 5)
  {
    //fAdd
    protocol.flResult = protocol.flValue1 + protocol.flValue2;
  }
  else if (protocol.arith == 6)
  {
    //fSubtraction
    protocol.flResult = protocol.flValue1 - protocol.flValue2;
  }
  else if (protocol.arith == 7)
  {
    //fMultiplication
    protocol.flResult = protocol.flValue1 * protocol.flValue2;
  }
  else if (protocol.arith == 8)
  {
    //fDivision
    protocol.flResult = protocol.flValue1 / protocol.flValue2;
  }
}

#define DEBUG
int main(int argc, char *argv[])
{

  srand(unsigned(time(NULL)));
  /* Do more magic */
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
  if (Desthost == NULL || Destport == NULL)
  {
    printf("Usage: %s <ip>:<port> \n", argv[0]);
    exit(1);
  }
  // *Desthost now points to a sting holding whatever came before the delimiter, ':'.
  // *Dstport points to whatever string came after the delimiter.

  /* Do magic */
  int port = atoi(Destport);
#ifdef DEBUG
  printf("Host %s, and port %d.\n", Desthost, port);
#endif

  struct addrinfo hint, *servinfo, *p;
  int rv;
  int serverSock;

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
    if ((serverSock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
    {
      printf("Socket creation failed.\n");
      continue;
    }
    printf("Socket Created.\n");
    rv = bind(serverSock, p->ai_addr, p->ai_addrlen);
    if (rv == -1)
    {
      perror("Bind failed!\n");
      close(serverSock);
      continue;
    }
    break;
  }

  freeaddrinfo(servinfo);

  if (p == NULL)
  {
    fprintf(stderr, "Client failed to create an apporpriate socket.\n");
    exit(1);
  }

  calcMessage *message = new calcMessage;
  calcProtocol *protocol = new calcProtocol;
  calcProtocol *calculations = new calcProtocol;
  int send;
  int numberOfBytes;
  int id = 0;
  char temp[256];
  struct clientDetails currentClient;

  /* 
     Prepare to setup a reoccurring event every 10s. If it_interval, or it_value is omitted, it will be a single alarm 10s after it has been set. 
  */
  struct itimerval alarmTime;
  alarmTime.it_interval.tv_sec = 10;
  alarmTime.it_interval.tv_usec = 10;
  alarmTime.it_value.tv_sec = 10;
  alarmTime.it_value.tv_usec = 10;

  /* Regiter a callback function, associated with the SIGALRM signal, which will be raised when the alarm goes of */
  signal(SIGALRM, checkJobbList);
  setitimer(ITIMER_REAL, &alarmTime, NULL); // Start/register the alarm.

  /*
  Ta emot meddelande i form av calcMessage som är 22, 0, 17, 1, 0
  Om detta är rätt, skicka iväg ett calcProtocol med calculationer
  Annars skicka iväg ett calcMessage som är 2, 2, x, 1, 0

  Tar emot ett svar i form av calcProtocol.
  Om detta är rätt, skicka iväg ett calcMessage med 1 som message.
  Annars skicka iväg ett calcMessage som är 2, 2, x, 1, 0
  */
  while (terminateLoop == 0)
  {
    sockaddr_in tempAddr;
    socklen_t tempSize = sizeof(tempAddr);
    int choice = 0;

    printf("This is the main loop, %d time.\n", loopCount);

    numberOfBytes = recvfrom(serverSock, protocol, sizeof(*protocol), 0, (sockaddr *)&tempAddr, &tempSize);
    //Om jag får recv från samma klient så ska jag uppdatera vectorClient.time till gettimeofday(&vectorClient.time, NULL);
    inet_ntop(AF_INET, &tempAddr.sin_addr, temp, 256);
    int found = 0;
    for (int i = 0; i < (int)clientVector.size(); i++)
    {
      if (strcmp(clientVector.at(i).address, temp) == 0)
      {
        found++;
        if(clientVector.at(i).port == ntohs(tempAddr.sin_port) && clientVector.at(i).id == (int)ntohl(protocol->id))
        {
          gettimeofday(&clientVector.at(i).time, NULL);
        }
      }
    }
    if (found == 0 && (int)ntohl(protocol->id) >= id)
    {
      strcpy(currentClient.address, temp);
      currentClient.port = ntohs(tempAddr.sin_port);
      currentClient.id = id++;
      gettimeofday(&currentClient.time, NULL);
      clientVector.push_back(currentClient);
    }
    else if(found == 0 && (int)ntohl(protocol->id) < id)
    {
      choice = 2;
      printf("Error, lost client tried sending a message.\n");
    }

    if (numberOfBytes == -1)
    {
      printf("Recieve failed!\n");
      break;
    }
    else if (numberOfBytes == sizeof(calcProtocol) && choice == 0)
    {
      protocol->type = ntohs(protocol->type);
      protocol->major_version = ntohs(protocol->major_version);
      protocol->minor_version = ntohs(protocol->minor_version);
      protocol->id = ntohl(protocol->id);
      protocol->arith = ntohl(protocol->arith);
      protocol->inValue1 = ntohl(protocol->inValue1);
      protocol->inValue2 = ntohl(protocol->inValue2);
      protocol->inResult = ntohl(protocol->inResult);

      if (protocol->inResult == calculations->inResult && protocol->flResult == calculations->flResult)
      {
        choice = 3;
      }
      else
      {
        choice = 2;
      }
    }
    else if (numberOfBytes == sizeof(calcMessage) && choice == 0)
    {
      message = (calcMessage *)protocol;
      message->type = ntohs(message->type);
      message->message = ntohl(message->message);
      message->protocol = ntohs(message->protocol);
      message->major_version = ntohs(message->major_version);
      message->minor_version = ntohs(message->minor_version);
      if ((message->type == 22) && (message->message == 0) && (message->protocol == 17) && (message->major_version == 1) && (message->minor_version == 0))
      {
        choice = 1;
      }
      else
      {
        choice = 2;
      }
    }

    //Got correct calcMessage. Send calculations.
    if (choice == 1)
    {
      randomCalculation(*calculations);

      protocol->type = htons(calculations->type);
      protocol->major_version = htons(calculations->major_version);
      protocol->minor_version = htons(calculations->minor_version);
      protocol->id = htonl(id - 1);
      protocol->arith = htonl(calculations->arith);
      protocol->inValue1 = htonl(calculations->inValue1);
      protocol->inValue2 = htonl(calculations->inValue2);
      protocol->inResult = htonl(calculations->inResult);
      protocol->flValue1 = calculations->flValue1;
      protocol->flValue2 = calculations->flValue2;
      protocol->flResult = calculations->flResult;

      if (calculations->arith < 5)
      {
        printf("Arith: %d, intOne: %d, intTwo: %d, int Result: %d\n", calculations->arith, calculations->inValue1, calculations->inValue2, calculations->inResult);
      }
      else
      {
        printf("Arith: %d, floatOne: %8.8g, floatTwo: %8.8g, float Result: %8.8g\n", calculations->arith, calculations->flValue1, calculations->flValue2, calculations->flResult);
      }

      send = sendto(serverSock, protocol, sizeof(*protocol), 0, (sockaddr *)&tempAddr, tempSize);
    }
    //Got wrong protocol or answer. Send 2, 2, x, 1, 0.
    else if (choice == 2)
    {
      message->type = htons(2);
      message->message = htonl(2);
      message->major_version = htons(1);
      message->minor_version = htons(0);

      send = sendto(serverSock, message, sizeof(*message), 0, (sockaddr *)&tempAddr, tempSize);
    }
    //Got correct protocol and right answer. Send 2, 1, x, 1, 0
    else if (choice == 3)
    {
      message->type = htons(2);
      message->message = htonl(1);
      message->major_version = htons(1);
      message->minor_version = htons(0);

      send = sendto(serverSock, message, sizeof(*message), 0, (sockaddr *)&tempAddr, tempSize);
    }
    //Error
    else
    {
      printf("Error when choosing what to send");
      break;
    }

    if (send == -1)
    {
      printf("Send failed!\n");
      break;
    }

    sleep(1);
    loopCount++;
  }

  printf("done.\n");
  return (0);
}
