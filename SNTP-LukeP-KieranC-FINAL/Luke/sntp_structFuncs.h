#ifndef SNTP_STRUCTFUNCS_H
#define SNTP_STRUCTFUNCS_H

#include <sys/types.h>


/*To condense the size and hopefully make it easier to manipulate */
/*Might change to just have individual uints of UINTS*/
struct packed_header
{
  unsigned int LI:2;
  unsigned int VN:3;
  unsigned int mode:3;
  unsigned int stratum:8;
  unsigned int poll:8;
  unsigned int precision:8;
};

/*created a second struct header to see if calculations are any */
/*Easier/clearer by doing it like this (REFER TO CONVERSIONS IN H FILE)*/
/*CONFIRMED: Bit calculation/Manipulation simpler with this method*/
/*Refer to AScully24*/
struct header_v2
{
  u_int8_t flags;
  u_int8_t stratum;
  u_int8_t poll;
  int8_t   precision;
};

/*could be changed later to include Key Identifier */
/*changed packet structure to include uint64_t to 
 work easier with Andrew Scully's time conversion
refer to Externalreferences*/
struct sntp_packet
{
  struct header_v2 head;
  u_int32_t rootDelay; 
  u_int32_t rootDispersion; 
  u_int32_t RI;  
  u_int64_t ts_ref;
  u_int64_t ts_org;
  u_int64_t ts_rcv;
  u_int64_t ts_transmit;
};

/*The members of a union share the same address space and work in tandem with each other, so filling the top 8 bytes of bytes[] will populat the transmit 
  timestamp in pc*/
  union sntp_union
  {
    struct sntp_packet pc;
    unsigned char bytes[48];
  };

/*FUNCTION PROTO DECLARATION */

u_int64_t tv_to_ntp(struct timeval tv);
struct timeval ntp_to_tv(unsigned long long ntp);
void zeroPacket(union sntp_union *un);
void buildReqPacket(union sntp_union *un);
void print_tv(struct timeval tv);
void printRP(union sntp_union *un);
void packetDecode(union sntp_union *un);
int sockethandler(union sntp_union *un, char *argv[]);
void printFormatTS(union sntp_union *un);

#endif 
