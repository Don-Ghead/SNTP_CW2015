/********************************************************************************
Program Name: SNTP Server
Author: Kieran Cooper
Date Written: 16/11/2015(original) 2/12/2015(final)
Version: 1.0.00
Changelog:
Version 0.01: 16/11/2015 
   Wrote majority of code.

Version 0.02: 18/11/2015
   Added forking.

Version 0.03: 25/11/2015
   Refactored code into current form.

Version 1.00: 2/12/2015
   Added documentation.

Version 1.01: 3/12/2015
   Typos.

Description:
The program runs in a shell and binds to a socket. It then waits for incoming
traffic on the port designated by PORTNO. On receiving a packet, the program
forks and constructs an SNTP response packet before transmitting it to the 
sender of the original packet. The child process is then destroyed. During this 
time the parent process continues listening for further requests and creating 
further children to deal with them.
********************************************************************************/

/********************************************************************************
INCLUDES
********************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <time.h>
#include "structure.h"
#include <signal.h>
#include <sys/wait.h>
#include <sys/time.h>

/********************************************************************************
DEFINITIONS
********************************************************************************/

#define MAXIMUMBUFFER 48 //size of packet
#define EPOCH 2208988800U //Linux epoc (1900-1970)
#define NTPFRACTIONCONSTANT 4294967295.0  //number of fraction states in second
#define PORTNO "9100" //port to listen on

void *get_in_addr(struct sockaddr *sa);
void sigchld_handler( int s);
void signal_handler(void);
void packet_constructor(union Packetmagic *Sent,
			union Packetmagic *Received, unsigned char *buffer);
void local_time_finder(union Packetmagic *Sent, int *state);
void ip_finder(struct sockaddr_storage their_addr, char *address_array);
int sender(int *sockfd, union Packetmagic *Sent,
	   struct sockaddr_storage their_addr,
	   socklen_t addr_len, int *numbytes);

/********************************************************************************
 *GET_IN_ADDR
Detects whether the sending address is IPv6 or IPv4

Arguments: struct sockaddr *sa: address information struct.
Returns: N/A
********************************************************************************/
void *get_in_addr(struct sockaddr *sa){
  if (sa->sa_family == AF_INET){
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }
  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
/********************************************************************************
SIGCHLD_HANDLER
Called by signal_handler to remove zombie processes from the process list

Arguments: N/A
Returns: N/A
********************************************************************************/
void sigchld_handler( int s){
  while( wait( NULL) > 0);
}
/********************************************************************************
SIGNAL_HANDLER
After child processes end, this cleans up the zombie processes

Arguments: N/A
Returns: N/A
********************************************************************************/
void signal_handler(){
  struct sigaction sa;
  sa.sa_handler = sigchld_handler; /* reap all dead processes */
  sigemptyset( &sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if( sigaction( SIGCHLD, &sa, NULL) == -1){
    perror( "Server sigaction");
    exit( 1);
  }
  return;
}
/********************************************************************************
PACKET_CONSTRUCTOR
Fills in various parts of the response packet

Arguments: union Packetmagic *Sent: The response packet architecture
           union Packetmagic *Received: The request packet architecture
           unsigned char *buffer: raw data from socket
Returns: N/A
********************************************************************************/
void packet_constructor(union Packetmagic *Sent,
			union Packetmagic *Received,
			unsigned char *buffer){

  memcpy(Received->bytes, buffer,
	 sizeof(Received->bytes));  //transfer buffer to receive packet.

  memcpy(&Sent->packet.header.topline, &Received->packet.header.topline,
	 sizeof(Received->packet.header.topline)); //transfer topline
  memset(&Sent->packet.header.topline, 0x24, 1); //set mode to 4
  memset(&Sent->packet.header.strat, 0x01, 1); //set stratum to 1

  //transfer to originate timestamp
  memcpy(&Sent->packet.origin.sec,
	 &Received->packet.transmit.sec,
	 sizeof(Received->packet.transmit.sec));
  //transfer to originate timestamp
  memcpy(&Sent->packet.origin.frac,
	 &Received->packet.transmit.frac,
	 sizeof(Received->packet.transmit.frac));
  return;
}

/********************************************************************************
LOCAL_TIME_FINDER
Fills in response packet timestamps. On first call the receive timestamp
 is filled in, on the second call the transmit timestamp is filled in.

Arguments: union Packetmagic *Sent: The response packet architecture
           in *state: Tracks whether first call or second call
Returns: N/A 
********************************************************************************/

void local_time_finder(union Packetmagic *Sent, int *state){
  struct timeval servertime;
  gettimeofday(&servertime, NULL);

  if(*state){
    Sent->packet.receive.sec = (htonl(servertime.tv_sec + EPOCH)); //create
    Sent->packet.receive.frac = (htonl((servertime.tv_usec * 1e-6)
				       * NTPFRACTIONCONSTANT));
    *state = 0;
  } else{
    Sent->packet.transmit.sec = (htonl(servertime.tv_sec + EPOCH)); //create
    Sent->packet.transmit.frac = (htonl((servertime.tv_usec * 1e-6)
					* NTPFRACTIONCONSTANT));
    
    memcpy(&Sent->packet.ref.sec, &Sent->packet.transmit.sec,
	   sizeof(Sent->packet.transmit.sec));
    memcpy(&Sent->packet.ref.frac, &Sent->packet.transmit.frac,
	   sizeof(Sent->packet.transmit.frac));
    *state = 1;
  }
  return;
}

/********************************************************************************
SOCKET_INITIALIZER
Clears and initializes hints, calls getaddrinfo,
 then sets up and binds to a socket

Arguments: struct addrinfo *hints: init data for getaddrinfo
           struct addrinfo *serverinfo: temporary struct
           struct addrinfo *p: holds struct data for binding
           int *sockfd: socket file descriptor
           int *rv: error handler
Returns: error handle
********************************************************************************/
int socket_initializer(struct addrinfo *hints,
		       struct addrinfo *serverinfo,
		       struct addrinfo *p, int *sockfd, int *rv){
  //const char *hostname = HOSTNAME;
  memset(hints, 0, sizeof(&hints)); //clear
  hints->ai_family = AF_UNSPEC;  //ip agnosticism
  hints->ai_socktype = SOCK_DGRAM;  //  UDP
  hints->ai_flags = AI_PASSIVE;  //  allow binding

  if ((*rv = getaddrinfo(NULL, PORTNO, hints, &serverinfo)) != 0){
    return 1;
  } //get address info

  for(p = serverinfo; p != NULL; p = p->ai_next){
    if ((*sockfd = socket(p->ai_family, p->ai_socktype,
			  p->ai_protocol)) == -1){
      perror("listener: socket");
      continue;  //create socket
    }
    if (bind(*sockfd, p->ai_addr, p->ai_addrlen) == -1){
      close(*sockfd);
      perror("listener: bind");
      continue; //bind socket
    }
    break;
  }

  if (p == NULL){
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2;
  } // error check
  freeaddrinfo(serverinfo);
  return 0;
}
/********************************************************************************
IP_FINDER
prints the ip address of the packet sender

Arguments: struct sockaddr_storage their_addr: Holds ip address from packet
           char *address_array: Storage space for ip address
Returns: N/A
********************************************************************************/
void ip_finder(struct sockaddr_storage their_addr, char *address_array){
  printf("listener: got packet from %s\n",
	 inet_ntop(their_addr.ss_family,
		   get_in_addr((struct sockaddr *)&their_addr),
		   address_array, INET6_ADDRSTRLEN)); // print sender ip
}

/********************************************************************************
SENDER
Sends the completed packet

Arguments: int *sockfd: Socket file descriptor
           union Packetmagic *Sent: The response packet architecture
           struct sockaddr_storage their addr: Holds ip address
                                               from request packet
           socklen_t addr_len: Length of address
           int *numbytes: number of bytes sent
Returns: error handle
********************************************************************************/
int sender(int *sockfd, union Packetmagic *Sent,
	   struct sockaddr_storage their_addr,
	   socklen_t addr_len, int *numbytes){
  if((*numbytes = sendto(*sockfd, Sent->bytes, sizeof(Sent->bytes), 0,
			 (struct sockaddr*)&their_addr, addr_len)) == -1){
    perror("Talker: sendto");
    return(1);
  } else{
    printf("Sent response\n");
  }
  return 0;
}
/********************************************************************************
MAIN

creates and initialises variables, call socket initializer to create a
listening port, sets up child handling, and then enters a while loop.
While in the loop, the listener waits to receive a packet and then
spawns a child process. The child process creates a response and provides
some feedback to the shell, sends it and then terminates. The parent will
continue to listen to the port and spawn more children as necessary.
********************************************************************************/
int main(int argc, char *argv[]){

  /*                 variables                     */
  int sockfd;
  struct addrinfo hints, *serverinfo = NULL, *p = NULL;
  int numbytes;
  struct sockaddr_storage their_addr;
  unsigned char buffer[MAXIMUMBUFFER];
  socklen_t addr_len;
  int i = 0;
  char address_array[INET6_ADDRSTRLEN];
  int exitstrat;
  int rv;
  /* 		  end of variables 		   */

  exitstrat = socket_initializer(&hints, serverinfo, p, &sockfd, &rv);
  switch(exitstrat){
  case 1:
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
    return 1;
    break;
  case 2:
    fprintf(stderr, "listener: failed to bind socket\n");
    return 2;
    break;
  default:
    break;
  }
  signal_handler(); // reap dead processes
  printf("listener: listening...\n");
  while (1){ 
    addr_len = sizeof their_addr;

    if ((numbytes = recvfrom(sockfd, buffer, sizeof(buffer) , 0,
			     (struct sockaddr *)&their_addr, &addr_len)) == -1){
      perror("recvfrom");
      exit(1); // receive packet
    }

    if( !fork()){
      //more variables
      int state = 1;
      union Packetmagic Sent;
      union Packetmagic Received;
      //initialise
      memset(&Received.bytes, 0, sizeof(Received.bytes)); //clear
      memset(&Sent.bytes, 0, sizeof(Sent.bytes)); //clear
      //set received timestamp
      local_time_finder(&Sent, &state);
      //finder ip address
      ip_finder(their_addr, address_array);
	  
      printf("listener: packet is %d bytes long\n", numbytes); 
      for(i=0;i<sizeof(buffer); i++){
        printf("%02x", buffer[i]);
        if(((i+1)%4 == 0) & (i != 0)){ 
          printf("\n");
        }
      } 
      packet_constructor(&Sent, &Received, buffer);//fill packet
      local_time_finder(&Sent, &state);//fill in transmit timestamp
      exitstrat = sender(&sockfd, &Sent, their_addr, addr_len, &numbytes);//send
      if (exitstrat ==1)
        exit(1);
      exit( 0); //end child
    }//fork()//
  }//while//
  close(sockfd);
  return 0;
}
