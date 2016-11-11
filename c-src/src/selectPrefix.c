#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>


int main(int argc, char **argv)
{
	char buf[INET6_ADDRSTRLEN];
	int mask = 64;
	int number;
	int offset;
	char *s;
	struct in6_addr addr;
	ssize_t sz = 0;

	if (argc == 3 )
	{
		offset = atoi(argv[2]);
		if ( (s=strchr(argv[1],'/')) )
		{
			mask = atoi(s+1);
		 	*s = '\0';
			sz = s-argv[1];
			if ( sz >= INET6_ADDRSTRLEN )
			{
				fprintf(stderr,"Wrong argument %s\n",argv[1]);
				
				return 1;
			}
			strncpy(buf, argv[1], INET6_ADDRSTRLEN);
		}
		number = (64-mask);
		number <<= number-1;
		if ( offset > number )
		{
			fprintf(stderr,"Only allowed offset: up to %d, required %d\n",number,offset);
			return 1;
		}

		if ( !inet_pton(AF_INET6, buf, &addr) )
		{
			fprintf(stderr,"Wrong subnet %s",buf);
		}
		addr.s6_addr16[3] += htons(offset);
		inet_ntop(AF_INET6, &addr, buf, sizeof(buf));
		printf("%s\n",buf);
	}
	else
	{
		s = strrchr(argv[0],'/');
		if( s == NULL )
			s = argv[0];
		else
			s++;
		fprintf(stderr,"Usage: %s delegated prefix-range offset\n",s);
		fprintf(stderr,"\t%s 2a01:db8:cafe::/48 1\n",s);
		return 1;
	}

	return 0;
}
