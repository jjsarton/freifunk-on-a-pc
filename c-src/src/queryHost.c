#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <syslog.h>

void printAddr(struct addrinfo *rp)
{
   char ad[80];
   struct sockaddr_in  *sa;
   struct sockaddr_in6 *sa6;
   ad[0] = 0;

   if ( rp->ai_addr != NULL )
   {
      sa  = (struct sockaddr_in*)rp->ai_addr;
      sa6 = (struct sockaddr_in6*)rp->ai_addr;
      switch(sa->sin_family)
      {
         case AF_INET:
            inet_ntop(AF_INET, &sa->sin_addr, ad, sizeof(ad));
         break;
         case AF_INET6:
            inet_ntop(AF_INET6, &sa6->sin6_addr.s6_addr, ad, sizeof(ad));
         break;
      }
   }
   printf("%s",ad);
}

void usage()
{
	printf("Usage: queryHost [-4|-6] hostname\n");
	exit(0);
}

int main(int argc, char **argv)
{
   char *node;
   struct addrinfo hints;
   struct addrinfo *res, *rp;
   int i;
   int family = AF_UNSPEC;

   if ( argc < 2 )
      return 1;
   if( argc == 2 && (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"h") == 0))
   	usage();

   if ( argc == 3 )
   {
   	if ( strcmp(argv[1],"-4") == 0 || strcmp(argv[1],"4") == 0)
	   family = AF_INET;
   	if ( strcmp(argv[1],"-6") == 0 || strcmp(argv[1],"6") == 0)
	   family = AF_INET6;
    	if ( strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"h") == 0)
	   usage();
  }

   node = argv[argc-1];

   memset(&hints, 0, sizeof hints);
   hints.ai_family   = family;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = 0;
   hints.ai_flags = AI_PASSIVE;

   if ( (i=getaddrinfo(node, NULL, &hints, &res)) != 0)
   {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(i));
      return 1;
   }

   for (i = 1,rp = res; rp != NULL; rp = rp->ai_next, i++)
   {
      //printf("%3d: ",i);
      //printf("Family inet%1s ",rp->ai_family==AF_INET?"":"6");
      printAddr(rp);
      printf("\n");
   }

   freeaddrinfo(res);
   return 0;
}
