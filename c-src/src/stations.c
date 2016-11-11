#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
/*
Station 62:e8:28:9f:4b:e4 (on mesh0)                <- mac
	inactive time:	10 ms                       <- time
	rx bytes:	128883255
	rx packets:	992696
	tx bytes:	569919
	tx packets:	6934
	tx retries:	0
	tx failed:	0
	signal:  	-43 [-49, -45] dBm          <- first value
	signal avg:	-43 [-48, -45] dBm
	Toffset:	-1331048652 us
	tx bitrate:	130.0 MBit/s MCS 15
	rx bitrate:	43.3 MBit/s MCS 4 short GI
	mesh llid:	43096
	mesh plid:	20105
	mesh plink:	ESTAB
	mesh local PS mode:	ACTIVE
	mesh peer PS mode:	ACTIVE
	mesh non-peer PS mode:	ACTIVE
	authorized:	yes
	authenticated:	yes
	preamble:	long
	WMM/WME:	yes
	MFP:		no
	TDLS peer:	no

data: {"90:f6:52:11:44:f1":{"signal":-27,"noise":-95,"inactive":80},"62:e8:28:9f:4b:e4":{"signal":-44,"noise":-95,"inactive":60}}
	
	
*/
typedef struct info_s {
	char mac[100];
	char inactive[100];
	char signal[100];
} info_t;

static info_t info;
static void getword(char *line, char *target)
{
	char *c = line;
	char *t=target;
	while (*c && *c != ':' ) c++;
	while (*c && (isspace(*c)||*c==':')) c++;
	while(*c && ! isspace(*c)) *t++ =*c++;
	*t='\0';
}
static void parse(char *line)
{
	if (strncmp(line,"Station ",(size_t) 8) == 0 )
		sscanf(line,"Station %s ", info.mac);
	if (strncmp(line,"\tsignal:",(size_t)8) == 0)
		getword(line,info.signal);
	if (strncmp(line,"\tinactive ",10) == 0)
		getword(line,info.inactive);
	
}

int main(int argc, char **argv)
{
	FILE *iw;
	char command[1000];
	char line[1000];
	char *data;
	size_t dl=0;
	int pos = 0;
	int r;
        int count=0;
	int stream = 0;
	if ( argc != 2 )
		exit(1);

	/* if the directory /sys/class/net/<dev>/wireless exist this will work */
	for(;;)
	{
		snprintf(command, sizeof(command),"/usr/sbin/iw dev %s station dump",argv[1]);
		if ( (iw = popen(command,"r")) == NULL )
		{
			exit(1);
		}
		memset(&info, 0, sizeof(info_t));
	
		data = calloc(1000,sizeof(char));
		dl=1000;

		r = sprintf(data+pos,"data: {");
		if (r >0)
			pos += r;
	
		while (fgets(line, (size_t)1000,iw)!=NULL)
		{
			parse(line);
			if ( info.mac[0] && info.inactive[0] && info.signal[0] ) 
			{
				if ( count )
					r = sprintf(data+pos,",");
				if ( r == 1 )
					pos += 1;
				r = sprintf(data+pos,"\"%s\":{\"signal\":%s,\"noise\":-95,\"inactive\":%s}",
					info.mac,info.signal, info.inactive);
				if ( r > 0 )
					pos += r;
				if ( dl - pos < 500 )
				{
					data = realloc(data,dl+500);
					dl+=500;
				}
				count=1;
				memset(&info, 0, sizeof(info_t));
			}
		}

		if ( strcmp("data: {", data) != 0 )
		{

			if ( !stream )
			{
			
				printf(	"Content-type: text/event-stream\n\n");
				stream = 1;
			}

			printf("%s}\n\n",data);
			fflush(stdout);
			free(data);
			data=NULL;
			dl=0;
			pos = 0;
      			count=0;
	     		usleep(150000);
			
		}
		else
		{
			exit(1);
		}
	}

	return(0);
}
