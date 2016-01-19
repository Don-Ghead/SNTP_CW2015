/********************************************************************************
 *client-full.c - V1.0 - Author: Luke "Donghead" Parsons
 *Date: 22/11/2015 
 *Is the primary controller for the SNTP Program
 *
 ********************************************************************************/
#include <stdio.h>
#include "sntp_structFuncs.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>
#include <sys/time.h>
#include <time.h>
#include <netdb.h>


/********************************************************************************
 *       DEFINITIONS
 ********************************************************************************/

#define PORT_TALK "9100"
#define PORT_NTP "123"

/********************************************************************************
 *Clears or initialise packet*
 *Arguments: Pointer to Union containing
 *SNTP packet to be 0'd
 *Returns Void
 ********************************************************************************/
void zeroPacket(union sntp_union *un)
{
  memset(un->bytes, 0, sizeof(un->bytes));
}

/********************************************************************************
 *BUILDS REQUEST PACKET TO SEND TO SERVER
 *arguments: Pointer to Union Containing Request packet
 *sets header to specified values (See below)
 *gets Time of day & sets transmit time just before Sending Packet
 *Returns Void
 ********************************************************************************/
void buildReqPacket(union sntp_union *un)
{
  struct timeval tod;
  gettimeofday(&tod, NULL);
  printf("Current time(TIME SENT)");
  print_tv(tod);
  
  //0010 0011 - Can use either method but simpler to just do this way
  //pc->head->flags = 0x23; //LI(0), VN(4), mode(client=3)
  un->bytes[0] = 0x23;

  //tv_to_ntp provided by A-Scully24 Refer to externalReferences.c
  //temp = tv_to_ntp(tod);
  
  un->pc.ts_transmit = tv_to_ntp(tod);
  un->pc.ts_transmit = htobe64(un->pc.ts_transmit);
  
}

/********************************************************************************
 *PRINTS FORMATTED RAW PACKET DATA
 *Arguments: Pointer to Union Containing Packet to Format
 *Prints the packet Data in Hex in a 
 *4 Byte horizontal x 12 Byte vertical
 *With a line identifier for different parts
 *Returns Void
 ********************************************************************************/
void printRP(union sntp_union *un)
{
  int i =0, k =0;
  int lineNo = 0;
  
  for (i = 0; i < sizeof(un->bytes); i+=4)
    {
      putchar('\t');
      for(k=0; k<4; k++)
	printf("%02x", un->bytes[i+k]);
	      
      switch(lineNo)
	{
	case 0:
	  printf(" :HEADER");
	  break;
	case 1:
	  printf(" :ROOT DELAY");
	  break;
	case 2:
	  printf(" :ROOT DISPERSION");
	  break;
	case 3:
	  printf(" :REFERENCE IDENTIFIER");
	  break;
	case 4:
	  printf(" :REFERENCE TS - SECONDS");
	  break;
	case 6:
	  printf(" :ORIGINATE TS - SECONDS");
	  break;
	case 8:
	  printf(" :RECEIVE TS - SECONDS");
	  break;
	case 10:
	  printf(" :TRANSMIT TS - SECONDS");
	  break;
	default:
	  //DO NOTHING
	  break;
	}
      lineNo++;
      printf("\n");
    }
  putchar('\n');
}

/********************************************************************************
 *PACKETDECODE - CONVERTS ENDIANS 
 *Arguments: Pointer to Union Containing SNTP packet to be decoded
 *Converts Endians in unsigned  
 *Return Void
 ********************************************************************************/
void packetDecode(union sntp_union *un)
{
  un->pc.ts_ref = be64toh(un->pc.ts_ref);
  un->pc.ts_org = be64toh(un->pc.ts_org);
  un->pc.ts_rcv = be64toh(un->pc.ts_rcv);
  un->pc.ts_transmit = be64toh(un->pc.ts_transmit);

}

/********************************************************************************
 *SOCKET HANDLER - Deals with Receiving and sending packet
 *IMPORTANT: for use with ntp.uwe.ac.uk, change PORT_TALK to PORT_NTP  
 *           for use with LISTENER, change PORT_NTP to PORT_TALK
 *Arguments: 1.Pointer to Union containing SNTP packet to send/receive 
 *2: pointer to argv[1] from main. When calling from main: sockethandler(&un, argv)
 *
 *First Sets up reference addrinfo to pass to getaddrinfo() which returns a list 
 *of structs that we cycle through to create the socket we'll talk on. Then sends 
 *the 48 bytes of request packet off to Destination Address passed on program call.
 *recvfrom() is a blocking call by default so it waits until it has received something
 *to continue. Could implement timeout function to return() after certain amount of time
 *but since we know that if the request is sent we'll receive a reply it shouldn't
 *Be a problem.
 ********************************************************************************/
int sockethandler(union sntp_union *un, char *argv[])
{
  int mainSock, numBytes; 
  struct addrinfo ref, *p_result, *p_ts;
  struct sockaddr_storage their_addr;
  int ai; //getaddrinfo() error var
  socklen_t addr_len;
  char* host;

  host = argv[1];
     
  memset(&ref, 0, sizeof(ref));
  ref.ai_family = AF_UNSPEC; //Allows for IPv4/IPv6
  ref.ai_socktype = SOCK_DGRAM;
  
  printf("\n");
  if((ai = getaddrinfo(host, PORT_NTP, &ref, &p_result))!= 0)
    {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ai));
      exit(1);
    }

  /*getaddrinfo returns a list of structs
   *loop through and connect. might implement
   *some kind of errorhandling here or optimization
   *might not be necessary referencing both the man page
   *and Beej's so trying to get the best of both*/
  for(p_ts = p_result; p_ts != NULL; p_ts = p_ts->ai_next)
    {
      if ((mainSock = socket(p_ts->ai_family, p_ts->ai_socktype, p_ts->ai_protocol)) == -1)
	{
	  perror("Talker:Socket");
	  continue;
	}
      else
	printf("Created socket\n\n");
      break;
    }

  if(p_ts == NULL)
    {
      fprintf(stderr, "Socket NULL\n");
      return 2;
    }

  freeaddrinfo(p_result); //No longer needed ->Free up memory
	       
  printf("size of packet %lu\n", sizeof(un->bytes));

  addr_len = sizeof their_addr;
  if((numBytes = sendto(mainSock, un->bytes, sizeof(un->bytes), 0,
			p_ts->ai_addr, p_ts->ai_addrlen)) == -1)
    {
      perror("Talker: sendto");
      exit(1);
    }
  else
    printf("Sent packet\n");
  printf("Waiting for response...\n");


  if((numBytes = recvfrom(mainSock,un->bytes , sizeof(un->bytes), 0,
			  (struct sockaddr*)&their_addr , &addr_len)) == -1)
    {
      perror("Talker: rcv");
      exit(1);
    }
  else
    printf("Received Packet:\n");

  printRP(un); //Print received Raw Packet
  
  packetDecode(un); //Convert endians
  
  return(1);
     
}

/********************************************************************************
 *printFormatTS - prints TimeStamp in human readable format 
 *
 *Arguments: Pointer to union containing sntp packet to be formatted 
 *External Functions: print_tv (ASCULLY24 - externResources.c) 
 *
 ********************************************************************************/
void printFormatTS(union sntp_union *un)
{
    struct timeval temp;
    printf("Format: TIMESTAMP /  DATE&TIME\n");
    printf("--------------------------------\n");

    temp = ntp_to_tv(un->pc.ts_ref);
    printf("Reference TimeStamp / ");
    print_tv(temp);
    putchar('\n');

    temp = ntp_to_tv(un->pc.ts_org);
    printf("Originate TimeStamp / ");
    print_tv(temp);
    putchar('\n');

    temp = ntp_to_tv(un->pc.ts_rcv);
    printf("Receive TimeStamp / ");
    print_tv(temp);
    putchar('\n');

    temp = ntp_to_tv(un->pc.ts_transmit);
    printf("Transmit TimeStamp / ");
    print_tv(temp);
    putchar('\n');

    putchar('\n');
    printf("Resolved Time: / ");
    print_tv(temp);
    putchar('\n');
}

/********************************************************************************
 * Main - 
 ********************************************************************************/
int main(int argc, char* argv[])
{
  union sntp_union unpc; //union packet

  if(argc != 2)
    {
      printf("\nUsage: ./client www.example.com OR 164.11.80.XX\n\n");
      exit(1);
    }
  
  zeroPacket(&unpc);
  printf("0 packet\n");
  printRP(&unpc);

  buildReqPacket(&unpc);

  printf("Request packet to send:\n");
  printRP(&unpc);
  
  sockethandler(&unpc, argv);

  /*Now packet is ready to print out using ntp_to_tv*/
  printFormatTS(&unpc);
  putchar('\n');
  printf("Additional Information:\n");
  system("ntpq -c rl");
  putchar('\n');

  return 1;
}

/*
time_t is an absolute time, represented as the integer number of seconds since the UNIX epoch
(midnight GMT, 1 January 1970). It is useful as an unambiguous, easy-to-work-with representation
 of a point in time.

struct tm is a calendar date and time. It may not represent any real point in time (e.g, you can 
have a struct tm that says it is February 31st, or Dodecember 0st). It does not include a time 
zone, so it is not absolute. It is typically used when converting to or from human-readable 
representations of the date and time. */

/********************************************************************************
 *UNUSED CODE
/***********************************************************
 *Delay is defined as (t4-t1) - (T3-T2) 
 *Offset is defined as ((T2 - T1) + (T3 - T4)) / 2 
 *Refer to RFC Page.19
 *T1 - ts_org (Time request sent by client
 *T2 - ts_rcv (Time request received by Server)
 *T3 - ts_transmit (Time reply sent by server)
 *T4 - Time reply received by Client
 *Because of TimeStamp format (uint64), i'm using redundant vars
 *to clarify the calculations (BITMASK FRACTIONS)
 *NOTE: I TRIED
 ***********************************************************
void calculations(union sntp_union *un)
{
  struct timeval tod;
  u_int32_t t1frac, t2frac, t3frac, t4frac, t1sec, t2sec, t3sec, t4sec;
  u_int64_t t1, t2, t3, t4;
  //const int divisor = 1000000; 
  int bml = 0xFFFF0000;
  int bmh = 0x0000FFFF;
  double  t, d;
  
  t1 = (un->pc.ts_org); 
  t2 = (un->pc.ts_rcv);
  t3 = (un->pc.ts_transmit);
  gettimeofday(&tod, NULL); //To fill out t4
  t4 = (tv_to_ntp(tod)); //turning it from TV->NTP and back again..
  // t4frac = (t4); //..because it works
  printf("\nBITMASK TEST\n");

  // SET VARIABLES //
  t1sec = ntohl(t1 >> 32);
  t2sec = ntohl(t2 >> 32);
  t3sec = ntohl(t3 >> 32);
  t4sec = (t4 >> 32);
  t1frac = ntohl(t1 << 32);
  t2frac = ntohl(t1 << 32);
  t3frac = ntohl(t1 << 32);
  t4frac = ntohl(t1 << 32);

  printf("%02x\n",t1);
  
  printf("%u\n",t2);
  printf("%u\n",t3);
  printf("%u\n",t4);
  printf("%ld\n",t1frac);
  printf("%ld\n",t2frac);
	    

  
  //prints the fractional part of all timestamps
  printf("\nFractional Timestamps 1 through 4 respectively:\n" );
  print_tv(ntp_to_tv(t1frac));
  putchar('\n');
  print_tv(ntp_to_tv(t2frac));
  putchar('\n');
  print_tv(ntp_to_tv(t3frac));
  putchar('\n');
  print_tv(ntp_to_tv(t4frac));
  putchar('\n');*/
  /* t1frac =  (unsigned int)(((double) (t1frac) / 1000000.0 )*
			   (double) 0xFFFFFFFF); //Creates fraction part of timestamp
  t2frac =  (unsigned int)(((double) (t2frac) / 1000000.0 )*
			   (double) 0xFFFFFFFF); //Creates fraction part of timestamp
  t3frac =  (unsigned int)(((double) (t3frac) / 1000000.0 )*
			   (double) 0xFFFFFFFF); //Creates fraction part of timestamp
  t4frac =  (unsigned int)(((double) (t4frac) / 1000000.0 )*
			   (double) 0xFFFFFFFF); //Creates fraction part of timestamp
		*/
  //printf("AFTER DIVISOR: %ud \n", t4frac);
  /* d = (( ((double)t4 + t4frac) - ((double)t1 + t1frac) - //(t4-t1) (-)
	 ((double)t3 + t3frac) - ((double)t2 + t2frac) )); //(t3-t2)
  
  t = (( ((double)t2 + t2frac) - ((double)t1 + t1frac) + //(t2-t1) (+)
	  ((double)t3 + t3frac) - ((double)t4 + t4frac) )); //(t3-t4)
  //new stuff
  d  = ( ( (ntohl(t4frac)) - (ntohl(t1frac)) ) - //(t4-t1) (-)
	 ( (ntohl( t3frac)) - (ntohl(t2frac)) )); //(t3-t2)
 
  t = (( ntohl((double)(t2 + t2frac)) - ntohl((double)(t1 + t1frac))) + //(t2-t1) (+)
       (ntohl((double)(t3 + t3frac)) - ntohl((double)(t4 + t4frac) ))) ; //(t3-t4)
  
 
   printf("Offset (t) = %f\n", t);
}
  */
