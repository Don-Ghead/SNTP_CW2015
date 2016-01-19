/*************************************
 *ALL WORK HERE REFERS TO ASCULLY24's work 
 *provided by the following GITHUB link
 * https://github.com/AScully24/SNTP-Server-Client
 **************************************/
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

const unsigned long long SECONDS_OFFSET = 2208988800ULL;
const unsigned long long N_SECONDS_OFFSET = 4294967295ULL;

//ASCULLY24
u_int64_t tv_to_ntp(struct timeval tv)
{
  unsigned long long tv_ntp, tv_usecs;

  tv_ntp = tv.tv_sec + SECONDS_OFFSET;
  tv_usecs = (N_SECONDS_OFFSET * tv.tv_usec) / 1000000UL; // (1000*1000) Micro -> Secs

  return(tv_ntp << 32) | tv_usecs;
}

//ASCULLY24
struct timeval ntp_to_tv(unsigned long long ntp)
{
    unsigned long long tv_secs, tv_usecs;
    tv_usecs = ntp & 0xFFFFFFFF;
    tv_secs = (ntp >> 32) & 0xFFFFFFFF;

    struct timeval temp;

    tv_secs = tv_secs - SECONDS_OFFSET;
    //         a        c           b
    tv_usecs = (tv_usecs + 1) * 1000000UL / N_SECONDS_OFFSET;

    temp.tv_sec = (time_t) tv_secs;
    temp.tv_usec = (suseconds_t) tv_usecs;
    return temp;
}

//ASCULLY24
void print_tv(struct timeval tv)
{
    time_t nowtime;
    struct tm *nowtm;
    char tmbuf[64], buf[64];

    nowtime = tv.tv_sec;
    nowtm = localtime(&nowtime);
    strftime(tmbuf, sizeof (tmbuf), "%Y-%m-%d %H:%M:%S", nowtm);

    snprintf(buf, sizeof buf, "%s.%06d", tmbuf, (int) tv.tv_usec);
    printf("%s\n", buf);
}
