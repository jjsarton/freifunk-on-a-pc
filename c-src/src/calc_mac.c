#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* laut Laut  lua/gluon/util.lua

-- Functions and IDs defined so far:
-- (1, 0): WAN (for mesh-on-WAN)
-- (1, 1): LAN (for mesh-on-LAN)
-- (2, n): client interface for the n'th radio
-- (3, n): adhoc interface for n'th radio
-- (4, 0): mesh VPN
-- (5, n): mesh interface for n'th radio (802.11s)

*/

int main(int argc, char**argv)
{
	char *mac;
	int m1,m2,m3;
	int a1=0,a2=0;
	if ( argc != 4 )
	   exit(1);
	mac=argv[1];
	(void)sscanf(argv[2],"%x",&a1);
	(void)sscanf(argv[3],"%x",&a2);
	(void)sscanf(mac,"%02x:%02x:%02x",&m1,&m2,&m3);
	printf("%02x:%02x:%02x:%s\n",(m1&0xfe)|0x02,(m2+a1)&0xff,(m3+a2)&0xff,&mac[9]);
	return(0);
}
