#ifndef NTPSTRUCTURE
#define NTPSTRUCTURE


struct head{
  unsigned char topline;
  unsigned char strat;
  unsigned char poll;
  unsigned char precision;
};

struct timestamps{
  unsigned int sec;
  unsigned int frac;
};

struct sntp_packet{
  struct head header;
  unsigned int root_delay;
  unsigned int root_dispersion;
  unsigned int reference_id;
  struct timestamps ref;
  struct timestamps origin;
  struct timestamps receive;
  struct timestamps transmit;
};

union Packetmagic{
struct sntp_packet packet;
unsigned char bytes[48];
};

#endif


