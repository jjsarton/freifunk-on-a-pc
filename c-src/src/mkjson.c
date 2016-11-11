#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <sys/utsname.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/types.h> 
#include <sys/un.h> 
#include <poll.h>
#include <sys/vfs.h>
#include <net/if.h>

#define RESPONDD_PORT 1001

#ifdef MKJSON
static char *version = "0.1";
#endif
extern char **environ;

static char *readTemplate(char *prg, char* name, size_t *msize)
{
	FILE *fp;
	size_t size;
	char *mem = NULL;
	if ( (fp = fopen(name, "r")) == NULL )
	{
		fprintf(stderr,"%s: %s %s\n",prg,name,strerror(errno));
		return mem;
	}
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	rewind(fp);
	mem = calloc(1, size+1);
	if ( mem == NULL )
	{
		fprintf(stderr,"%s: %s %s\n",prg,name,strerror(errno));
	}
	else
	{
		fread(mem, size, 1, fp);
	}
	fclose(fp);
	*msize = size;
	return mem;
}

typedef struct var_s {
	struct var_s *next;
	char *key;
	char *value;
} var_t;

static var_t *vars = NULL;

static char *estrcat(char *t, char*s)
{
	while(*t)
		t++;
	while (*s)
		*t++ = *s++;
	*t = '\0';
	return t;
}

static char *estrncat(char *t, char *s, size_t len)
{
	while(*t)
		t++;
	while(len && *s)
	{
		len--;
		*t++ = *s++;
	}
	if ( *s )
		*t = '\0';

	return t;
}

static char *skipSpace(char*s)
{
	while ( *s  && isspace(*s) )
		s++;
	return s;
}

static char *getWordEnd(char *s)
{
	while ( *s && (isalnum(*s) || *s == '_' || *s == '-') )
		s++;
	return s;	
}


static char *getStringEnd(char *s, char delim)
{
	// string delimiter ', " or space
	while ( *s )
	{
		if ( *s == delim )
		{
			s++;
			break;
		}
		if ( *s ==  '\\' )
			s++;
		if ( *s )
			s++;
		else
			break;
		
	}
	if ( *s == '\n' || *s == '\r' )
		s--;
	return s;
}

static int parseLine(char *line, char **key, char **value)
{
	char *s = line;
	char *t;
	*key = NULL;
	*value = NULL;

	if ( (t = strchr(s,'\n')) )
	{
		*t = '\0';
	}
	s = skipSpace(s);
	if ( *s && *s == '#' )
		return 0;
	t = s;

	// Variable name or export 
	t = getWordEnd(s);
	if ( strncmp(s, "export", t-s) == 0 )
	{
		s = skipSpace(t);
		t = getWordEnd(s);		
	}

	*key = calloc(t+1-s, 1);
	strncpy(*key, s, t-s);

	s = t;
	s = skipSpace(s);
	
	if (*s == '=' )
	{
		s++;
		s = skipSpace(s);
		if ( *s ==  '\'' || *s == '"' )
		{
			t = getStringEnd(s+1, *s);
			s++;
			if ( t > s )
			{
				t--;
			}
		}
		else
		{
			t = getStringEnd(s,' ');
			if ( *t == ' ' )
				t--;
		}
		*value = calloc(t+1-s, 1);
		if ( *value )
		{
			strncpy(*value, s, t-s);
		}
	}
	
	
	if (*value == NULL && *key != NULL) 
	{
		free(*key);
		*key = NULL;
		return 0;
	}
	return 1;
}

static void getGlobale(char *file)
{
	FILE *fp;
	size_t size;
	char *line = NULL;
	char *key;
	char *value;
	var_t *new_var;
	char *s,*t;
	var_t *act = vars;

	if ( (fp = fopen(file, "r")) == NULL )
	{
		return;
	}
		
	while (getline(&line,&size,fp) > 0 )
	{
		value = NULL;
		key = NULL;
		if ( parseLine(line, &key, &value) )
		{
			if (*value=='"')
			{
				s = t = value;
				s++;
				while (*s && *s != '"')
				{
					*t++ = *s++;
				}
				*t='\0';
			}

			/* replace value if the key match */
			while (act)
			{
				if ( strcmp(act->key,key) == 0 )
				{
					free(act-> value);
					act->value = value;
					free(key);
					return;
				}
				act = act->next;
			}	

			/* insert key and value at the begin of out list */
			new_var = (var_t*)calloc(1,sizeof(var_t));
			if ( new_var )
			{
				new_var->key = key;
				new_var->value = value;
				new_var->next = vars;
				vars = new_var;
			}
			else
			{
				if ( key ) free(key);
				if ( value ) free(value);
			}
		}
	}
	fclose(fp);
	if ( line) free(line);
}

static char *fastdConnection(char *sockPath)
{
	char *ret = calloc(1,1);
	int sock = 0;
	char buf[4096];
	struct sockaddr_un sock_un;
	int len;
	struct pollfd fds;

	if (sockPath == NULL )
	{
		return ret;
	}

	memset(sock_un.sun_path,0, sizeof(sock_un.sun_path));
	if ( sockPath && *sockPath=='/' )
	{
		strncpy(sock_un.sun_path, sockPath, sizeof(sock_un.sun_path)-1);
	}
	else if ( sockPath )
	{
		snprintf(sock_un.sun_path,sizeof(sock_un.sun_path)-1,"/var/run/%s",sockPath);
	}
	else
	{
		return ret;
	}

	sock_un.sun_family = AF_UNIX;

	sock= socket(AF_UNIX,SOCK_STREAM,0);
	if ( sock == -1 )
	{
		return ret;
	}

	if ( connect(sock, (struct sockaddr*)&sock_un, sizeof(sock_un))==-1)
	{
		close(sock);
		return ret;
	}

	fds.fd=sock;
	fds.events=POLLIN;
	fds.revents=0;
	if ( poll(&fds,1,100) < 0 )
	{
		perror("poll");
		close(sock);
		return ret;
	}

	len=recv(sock, buf, sizeof(buf), MSG_DONTWAIT);
	
	if ( len > 0 )
	{
		buf[len] = 0;
	}
	else
	{
		perror("recv");
	}
	close(sock);

#define NAME "name\": \""
#define CONNECTION "connection\": "
#define ESTABLISHED "\"established\":"

	/* build output */
	char *s = buf;
	char *t =  strstr(s, NAME);
	char *preambel = "\"mesh_vpn\":{\"groups\":{\"backbone\":{\"peers\":{\"";
	char preambelPrinted = 0;
	int sz = 1024;
	char *end;

	while((s =  strstr(s, NAME)))
	{
		s += strlen(NAME);
		if ( !preambelPrinted )
		{
			ret = calloc(sz,1);
			end = ret;
			end = estrcat(end, preambel);
			preambelPrinted=1;
		}
		else
		{
			end = estrcat(end,",\"");
		}
		t = getWordEnd(s);
		end = estrncat(end, s, t-s);
		end = estrcat(end, "\":");
		s = t;

		s = strstr(s, CONNECTION);
		s += strlen(CONNECTION);
		if ( strncmp(s, "null",4) == 0 )
		{
			end = estrcat(end,"null");
			s += 4;
		}
		else
		{
			s = strstr(s, ESTABLISHED);
			s += strlen(ESTABLISHED);
			while ( !isdigit(*s) )
			{
				s++;
			}
			end = estrcat(end, "{");
			end = estrcat(end, ESTABLISHED);
			t = getWordEnd(s);
			double time;
			char tbuf[20];
			sscanf(s,"%lf",&time);
			snprintf(tbuf, sizeof(tbuf),"%.3lg",time /= 1000.0);
			end = estrcat(end, tbuf);
			end = estrcat(end, "}");
	
		}
	}
	if ( preambelPrinted )
	{
		estrcat(end,"}}}}");
	}

	return ret;
#undef NAME
#undef CONNECTION
#undef ESTABLISHED

}

static char *var(char *key)
{
	var_t *act = vars;
	while(act)
	{
		if (strcmp(act->key, key) == 0 )
		{
			return strdup(act->value);
		}
		act = act->next;
	}
	return NULL;
}

static char *printVars()
{
	size_t sz = 1024;
	size_t len;
	size_t pos;
	var_t *act = vars;
	char *ret = calloc(1, sz);
	char *end = ret;

	while ( act )
	{
		len = strlen(act->key) + strlen(act->value) + 10;
		pos = end - ret;
		if ( pos + len > sz )
		{
			sz += ( len > 128 ) ? len : 128;
			ret = realloc(ret, sz);
			end = ret + pos;
		}
		if(act != vars)
			end = estrcat(end,",");
		end = estrcat(end,"\"");
		end = estrcat(end,act->key);
		end = estrcat(end,"\":\"");
		end = estrcat(end,act->value);
		end = estrcat(end,"\"");
		act = act->next;
	}
	return ret;	
}

void elimSpace(char *mem)
{
	char *s = mem;
	char *t = mem;
	int instring = 0;
	if ( mem )
	{
		while (*s)
		{
			if ( *s == '"' ) instring = ! instring;
			if ( ! instring && isspace(*s) )
				s++;
			else
				*t++ = *s++;
		}
		*t = '\0';
	}
}

static char *fromFile(char *path)
{
	char *result = NULL; 
	FILE *fp = fopen(path,"r");
	
	if ( fp != NULL ) {
		fscanf(fp,"%ms",&result);
		fclose(fp);
	}
	return result;
}

static char* mac(char*dev)
{
	char path[1024];
	snprintf(path, sizeof(path),"/sys/class/net/%s/address",dev);
	return fromFile(path);
}

static char* nodeid(char*dev)
{
	char *result;
	char path[1024];
	snprintf(path, sizeof(path),"/sys/class/net/%s/address",dev);
	result = fromFile(path);
	/* eliminate the colon */
	if ( result )
	{
		char *s = result;
		char *t = result;
		while(*s)
		{
		   if ( *s != ':' )
			*t++ = *s++;
		   else
		      s++;
		}
		*t = '\0';
	}
	return result; 
}

static char *fromUname(char *which)
{
	struct utsname buf;
	uname(&buf);
	if ( strcmp(which, "sysname") == 0 )
		return strdup(buf.sysname);
	if ( strcmp(which, "nodename") == 0 )
		return strdup(buf.nodename);
	if ( strcmp(which, "release") == 0 )
		return strdup(buf.release);
	if ( strcmp(which, "version") == 0 )
		return strdup(buf.version);
	if ( strcmp(which, "machine") == 0 )
		return strdup(buf.machine);
	return NULL;
}

static char *nproc(char *notused)
{
	long l = sysconf(_SC_NPROCESSORS_CONF);
	char *r = calloc(1,32);
	snprintf(r, 31, "%ld",l);
	return r;
}

static char *allIPv6(char *dev)
{
	struct ifaddrs *ifa = NULL;
	struct ifaddrs *pt;
	int family=0;
	char *result = NULL;
	size_t size=0;
	char ad[INET6_ADDRSTRLEN];
	if ( getifaddrs(&ifa) )
	{
		perror("getifaddrs");
		return NULL;
	}
	pt = ifa;
	while(pt)
	{
		if ( strcmp(dev, pt->ifa_name) == 0 )
		{
			struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)pt->ifa_addr;
			family = sa6? sa6->sin6_family : -1;
			if ( family == AF_INET6 )
			{
				inet_ntop(AF_INET6, &sa6->sin6_addr, ad, sizeof(ad));
				if ( result == NULL )
				{
					size_t sz = strlen(ad) + 3;
					size = sz;
					result=calloc(1, sz);
					snprintf(result, sz, "\"%s\"", ad);
				}
				else
				{
					size_t sz = strlen(ad) + 4;
					result = realloc(result, size + sz);
					snprintf(result+size-1, sz ,",\"%s\"", ad);
					size += sz;
				}
			}
		}
		pt = pt->ifa_next;
	}
	freeifaddrs(ifa);
	return result;
}

static char *macList(char *devices)
{
	char *ret = NULL;
	size_t sz = 0;
	size_t len = 0;
	char *s = devices;
	char *e = NULL;
	char *t = NULL;
	int  hasElem=0;
	char *m = NULL;
	char dev[128];

	while(*s)
	{
		s = skipSpace(s);
		e = getWordEnd(s);

		strncpy(dev,s, e-s);
		dev[e-s] = '\0';
		m = mac(dev);
		if (m==NULL) return ret;
		if ( m )
		{
			len = strlen(m);
			ret=realloc(ret,sz+len+4);
			t = ret+sz;
			if (hasElem )
			{
				*t++ = ',';
			}
			*t++ ='"';
			strcpy(t,m);
			t+=len;
			*t++ ='"';
			*t = '\0';
			hasElem = 1;
			sz=t-ret;
			free(m);
			m = NULL;
		}
		s = e;
	}
	return ret;
}

static char *exec(char *cmd)
{
	char *result = NULL;
	size_t size = 0;
	ssize_t ssize = 0;
	
	if ( !cmd )
	{
		return result;
	}

	FILE *fp;
	if ( cmd )
	{
		fp = popen(cmd, "r");
		if ( fp )
		{
			ssize = getline(&result,&size,fp);
			pclose(fp);
		}
	}
	if ( ssize <= 0 )
	{
		free(result);
		result = NULL;
	}
	return result;
}

static char *lsbRelease(char *arg)
{
	char cmd[128];
	char *val = NULL;
	snprintf(cmd, sizeof(cmd),"lsb_release %s",arg);
	val = exec(cmd);
	char *s, *t;
	if ( val )
	{
		t = val;
		s = strchr(val, ':');
		if ( s )
		{
			s++;
			s = skipSpace(s);
			while(*s)
				*t++ = *s++;
			*t='\0';
		}
	}
	return val;
}

static char *include(char *name)
{
	char *empty = calloc(1,1);
	FILE *fp;
	size_t size;
	char *mem = NULL;
	if ( (fp = fopen(name, "r")) == NULL )
	{
		return empty;
	}
	fseek(fp, 0L, SEEK_END);
	size = ftell(fp);
	rewind(fp);
	mem = calloc(1, size+1);
	if ( mem == NULL )
	{
		return empty;
	}
	fread(mem, size, 1, fp);
	fclose(fp);
	elimSpace(mem);
	return mem;
}

char *stringList(char *elem)
{
	char *s = elem;
	char *t;
	int sz = 1000;
	int hasElem = 0;
	int inString = 0;
	if ( !elem )
	{
		return NULL;
	}
	char *ret = calloc(1,1000);
	t = ret;
	while(*s)
	{
		if ( !isspace(*s) )
		{
			if ( ! inString )
			{
				inString = 1;
				if ( hasElem )
				{
					*t++ = ',';
				}
				*t++ = '"';
				hasElem=1;
			}
			*t++ = *s++;
		}
		else
		{
			if ( inString )
			{
				*t++ = '"';
				inString = 0;
				s++;
			}
			else if ( !isspace(*s) )
			{
				*t++ = *s++;
			}
			else
			{
				s++;
			}
		}
		if ( t - ret < 128 )
		{
			sz+=128;
			ret=realloc(ret,sz);
		}
	}
	if ( inString )
	{
		*t++ = '"';
	}
	*t++ = '\0';
	return ret;
}

static char* addVar(char *args)
{
	char *ret = calloc(1,1);
	char *key = NULL;
	char *value = NULL;
	var_t *new_var;
	var_t *act = vars;

	sscanf(args,"%ms %ms",&key,&value);
	if ( value == NULL )
	{
		if ( key ) free(key);
		return ret;
	}

	while ( act )
	{
		if ( strcmp(act->key, key ) == 0)
		{
			free(act->value);
			act->value = value;
			return ret;
		}
		act = act->next;
	}
	new_var = (var_t*)calloc(1,sizeof(var_t));
	new_var->key = key;
	new_var->value = value;
	new_var->next = vars;
	vars = new_var;
	return ret;
}

#ifdef MKJSON
static void freeVars(void)
{
	var_t *act = vars;
	var_t *next;
	
	while(act)
	{
		if (act->key) free(act->key);
		if (act->value) free(act->value);
		next = act->next;
		free(act);
		act = next;
	}
	vars = NULL;
}
#endif

static char *readConf(char*file)
{
	char *ret = calloc(1,1);
	getGlobale(file);
	return ret;
}

static char *uptime()
{
	char *ret= NULL;
	char *vals = fromFile("/proc/uptime");
	sscanf(vals, "%ms %*s",&ret);
	return ret;
}

static char *idletime()
{
	char *ret= NULL;
	ssize_t rd = 0;
	char buf[512];
	int fd = open("/proc/uptime", O_RDONLY);

	if ( fd > 0 )
	{
		rd = read(fd, buf, sizeof(buf)-1);
		close(fd);
	}
	if ( rd > 0 )
	{
		buf[rd] = '\0';
		sscanf(buf, "%*s %ms",&ret);
	}
	return ret;
}

static char *loadavg()
{
	char *ret= NULL;
	char *vals = fromFile("/proc/loadavg");
	sscanf(vals, "%ms %*s",&ret);
	return ret;
}

static char *processes()
{
	char *ret= calloc(1,1);
	char *loadavg = NULL;
	char *running = 0;
	char *total = 0;
	char *s;
	char vals[1024];
	FILE *fp;
	if ( (fp = fopen("/proc/loadavg", "r")) == NULL )
	{
		return ret;
	}
	fgets(vals, sizeof(vals),fp);
	fclose(fp);

	s = strchr(vals,'/');
	if ( s ) *s = ' ';
	
	sscanf(vals,"%ms %*s %*s %ms %ms",&loadavg,&running,&total);
	if ( loadavg && running && total )
	{
		free(ret);
		ret = calloc(1, 1024);
		snprintf(ret,
			1024,
			"\"total\":%s,\"running\":%s,\"loadavg\":%s",
			total, running, loadavg);
	}
	if ( loadavg ) free(loadavg);
	if ( running ) free(running);
	if ( total ) free(total);
	return ret;
}

static char *memory()
{
	char buf[1024];
	ssize_t rd;
	int  fd;
	char *total = NULL;
	char *freemem = NULL;
	char *buffers = NULL;
	char *cached = NULL;
	char *ret = NULL;
	char *s;

	fd = open("/proc/meminfo", O_RDONLY);
	if ( fd > 0 )
	{
		while ( (rd = read(fd, buf, sizeof(buf)-1)) > 0 )
		{
			buf[rd] = '\0';
			if ( (s = strstr(buf, "MemTotal:")) )
				sscanf(s, "%*s %ms",&total);
			if ( (s = strstr(buf,"MemFree:")) )
				sscanf(s, "%*s %ms",&freemem);
			if ( (s = strstr(buf, "Buffers:")) )
				sscanf(s, "%*s %ms",&buffers);
			if ( (s = strstr(buf, "Cached:")) )
				sscanf(s, "%*s %ms",&cached);
		}
		close(fd);
		ret=calloc(12,1024);
		snprintf(ret,1034,
			"\"total\":%s,\"free\":%s,\"buffers\":%s,\"cached\":%s",
			total?total:"-1",
			freemem?freemem:"-1",
			buffers?buffers:"-1",
			cached?cached:"-1");
		if (total) free(total);
		if (freemem) free(freemem);
		if (buffers) free(buffers);
		if (cached) free(cached);
	}
	return ret;
}

static char *gateway(char *interface)
{
	char buf[1024];
	char *ret = NULL;
	char *s;
	char *batctl = var("USE_BATCTL");
	FILE *fp = NULL;
	int use_batctl = 0;

	snprintf(buf,sizeof(buf),"/sys/kernel/debug/batman_adv/%s/gateways",interface);
	if ( (batctl && strcmp(batctl,"true") == 0) || access(buf, F_OK) != 0 )
	{
		snprintf(buf,sizeof(buf),"batctl -m %s gwl",interface);
		use_batctl = 1;
		fp = popen(buf, "r");
	}
	else
	{
		snprintf(buf,sizeof(buf),"/sys/kernel/debug/batman_adv/%s/gateways",interface);
		/* batadv */
		fp = fopen(buf , "r");
	}
	
	if ( fp )
	{
		while ( fgets(buf, sizeof(buf), fp) )
		{
			if ( (s = strstr(buf,"=> ")) )
			{
				/* up to batman_adv 2016.3 */
				sscanf(s,"%*s %ms",&ret);
				break;
			}
			else if ( (s = strstr(buf,"*")) == buf )
			{
				/* batman_adv 2016.4 */
				sscanf(s,"%*s %ms",&ret);
				break;
			}
			
		}
		if ( use_batctl )
			pclose(fp);
		else
			fclose(fp);
	}

	if ( batctl )
		free(batctl);
	return ret;
}

static char *fsusage()
{
	char *ret = NULL;
	struct statfs s;
	
	if (statfs("/", &s))
		return ret;
	ret = calloc(64,1);
	snprintf(ret,63,"%g",(1 - (double)s.f_bfree / s.f_blocks));
	return ret;	
}

#define STR(x) #x
#define XSTR(x) STR(x)


typedef struct bat_nb_s
{
	struct bat_nb_s *next;
	char mac[18];
	double lastseen;
	int tq;
} bat_nb_t;

typedef struct bat_mac_s
{
	struct bat_mac_s *next;
	char iface[IF_NAMESIZE+1];
	struct bat_nb_s *batnb;
} bat_mac_t;

static int insert_iface(bat_mac_t **root, char *ifname, char *mac, int tq, double lastseen)
{
	bat_mac_t *actp = *root;
	bat_nb_t  *nb;
	bat_nb_t  *actn;
	if ( *root == NULL )
	{
		*root = calloc(1, sizeof(bat_mac_t));
		strcpy((*root)->iface,ifname);
		(*root)->batnb = nb = calloc(sizeof(bat_nb_t),1);
		nb->tq = tq;
		nb->lastseen = lastseen;
		strcpy(nb->mac,mac);
		return 0;
	}
	else
	{
		while(actp)
		{
			if ( strcmp(actp->iface, ifname) == 0 )
			{
				actn = actp->batnb;
				while(actn->next)
				{
					actn = actn->next;
				}
				actn->next = nb = calloc(sizeof(bat_nb_t),1);
				nb->tq = tq;
				nb->lastseen = lastseen;
				strcpy(nb->mac,mac);
				return 0;
			}
			if (actp->next == NULL )
			{
				actp->next = calloc(sizeof(bat_mac_t),1);
				actp->next->batnb = nb = calloc(sizeof(bat_nb_t),1);
				strcpy(actp->next->iface,ifname);
				nb->tq = tq;
				nb->lastseen = lastseen;
				strcpy(nb->mac,mac);
				return 0;
			}
			else
			{
				actp = actp->next;
			}
		}
	}
	return 1;
}

static void free_iface(bat_mac_t **root)
{
	bat_mac_t *actp = *root;
	bat_mac_t *nextp;
	bat_nb_t  *nextn;
	bat_nb_t  *actn;
	while (actp)
	{
		nextp = actp->next;
		
		actn = actp->batnb;
		while(actn)
		{
			nextn = actn->next;
			free(actn);
			actn=nextn;
		}
		free(actp);
		actp = nextp;
	}
	*root = NULL;
}

static char *batadv(char *bat)
{
	char *ret = NULL;
	int ns =0;
	int pos = 0;
	FILE *fp;
	char * nmac = NULL;
	char path[1024];
	bat_mac_t *pmac = NULL;
	bat_mac_t *act = NULL;
	char *batctl = var("USE_BATCTL");
	int use_batctl = 0;

	snprintf(path,sizeof(path),"/sys/kernel/debug/batman_adv/%s/originators",bat);
	if ( (batctl && strcmp(batctl,"true") == 0) || access(path,F_OK) != 0 )
	{
		snprintf(path,sizeof(path),"batctl -m %s o",bat);
		use_batctl = 1;
		fp = popen(path, "r");
	}
	else
	{
		snprintf(path,sizeof(path),"/sys/kernel/debug/batman_adv/%s/originators",bat);
		/* batadv */
		fp = fopen(path , "r");
	}

	if ( batctl )
	{
		free(batctl);
	}

	if (fp == NULL)
	{
		return ret;
	}

	while (!feof(fp)) {
		char mac1[18];
		char mac2[18];
		char ifname[IF_NAMESIZE+1];
		int tq;
		double lastseen;

		int count = fscanf(fp,
			"%17s%*[\t ]%lfs%*[\t (]%d) %17s%*[[ ]%" XSTR(IF_NAMESIZE) "[^]]]",
			mac1, &lastseen, &tq, mac2, ifname);
		if (count != 5)
			continue;
			
		if (strcmp(mac1, mac2) == 0)
		{
			insert_iface(&pmac,ifname, mac1,tq,lastseen);	
		}
	}
	
	if ( use_batctl )
		pclose(fp);
	else
		fclose(fp);
	
	/* assemble informations */
	act = pmac;
	while ( act )
	{
		bat_nb_t *nb = act->batnb;
		nmac=mac(act->iface);
		if(nmac==NULL)
			break;

		ns = ns+pos+200;
		ret = realloc(ret,ns);

		snprintf(ret+pos,ns-pos,"\"%s\":{\"neigbours\":{",nmac);
		pos += strlen(ret+pos);
		while(nb)
		{
			if ( pos < ns-100 )
			{
				ns = pos+120;
				ret=realloc(ret,ns);
			}
			snprintf(ret+pos,ns-pos,"\"%s\":{\"tq\":%d,\"lastseen\":%.3g }%c",
				nb->mac,
				nb->tq,
				nb->lastseen,
				nb->next ? ',':'\0');
			nb = nb->next;
			pos += strlen(ret+pos);
		
		}
		strcat(ret+pos,"}}");
		if ( act->next)
		{
			strcat(ret+pos,",");
		}
		pos += strlen(ret+pos);
		
		act = act->next;
		free(nmac);
		nmac = NULL;
	}
        /* end batadv */
	if ( pmac )
		free_iface(&pmac);
	return ret;
}

typedef struct info_s {
	char mac[100];
	char signal[100];
} info_t;

char *wifi(char *devices)
{
	char *ret = calloc(4,1);
	FILE *iw;
	char command[256];
	char line[256];
	int seen = 0;
	size_t sz=0;
	int pos = 0;
	info_t info;
	char *nmac = NULL;
	char *device = NULL;
	char *b,*e;
	int newdev = 0;
	
	b = e = devices;

	for(;;)
	{
		b=skipSpace(e);
		e=getWordEnd(b);
		if ( e > b)
		{
			device = strndup(b,e-b);
			seen = 0;
		}
		else
		{
			break;
		}
		snprintf(command, sizeof(command),"/usr/sbin/iw dev %s station dump",device);
		if ( (iw = popen(command,"r")) == NULL )
		{
			if ( ret )
			{
				*ret='\0';
			}
			if ( device )
			{
				free(device);
			}
			return ret;
		}
		memset(&info, 0, sizeof(info_t));
		while (fgets(line, sizeof(line),iw)!=NULL)
		{
			if (strstr(line,"Station ") )
			{
				sscanf(line,"Station %s ", info.mac);
			}

			if (strstr(line,"signal:") )
			{
				sscanf(line,"\tsignal: %s ", info.signal);
			}

			if ( info.mac[0] && info.signal[0] ) 
			{
				if ( sz-pos < 120 )
				{
					ret = realloc(ret, sz+120);
					sz += 120;
				}
				if ( newdev )
				{
					ret[pos++] = '}';
					ret[pos++] = ',';
					ret[pos] = '\0';
					newdev=0;
				}
				if ( !seen )
				{
					nmac=mac(device);
					if ( nmac == NULL )
						continue;
					snprintf(ret+pos,sz-pos,
						"%s\"%s\":{\"neighbours\":{",
						newdev ? "," : "",
						nmac);
					pos += strlen(ret+pos);
				}
				

				snprintf(ret+pos,sz-pos,
					"%s\"%s\":{\"signal\":%s,\"noise\":%s}",
					seen ? "," : "",
					info.mac,
					info.signal,
					"-95");
				seen=1;	
				pos += strlen(ret+pos);
				memset(&info, 0, sizeof(info_t));
			}
		}
		if (seen )
		{
			ret[pos++] = '}';
			ret[pos] = '\0';
		}
		if (nmac)
		{
			free(nmac);
			nmac = NULL;
		}
		if ( device )
		{
			free(device);
			device = NULL;
		}
		newdev = 1;
		pclose(iw);
	}

	if ( ret && ! *ret )
	{
		return ret;
	}
	if ( ret )
	{
		ret[pos++] = '}';
		ret[pos]='\0';
	}
	return ret;
}

static char *traffic(char *bat)
{
	char *ret = NULL;
	FILE *fp;
	char command[256];
	char line[256];
	int pos = 0;
	
	int rx_pakets  = 0;
	int rx_packets = 0;
	int rx_bytes   = 0; 
	int tx_packets = 0;
	int tx_bytes   = 0;
	int fw_packets = 0;
	int fw_bytes   = 0;
	int mr_bytes   = 0; 
	int mr_packets = 0;
	int mt_bytes   = 0;
	int mt_packets = 0;

	if ( bat == NULL )
		return ret;
		
	snprintf(command, sizeof(command),"/usr/sbin/ethtool -S %s",bat);
	if ( (fp = popen(command,"r")) == NULL )
	{
		return ret;
	}

	while ( fgets(line, sizeof(line),fp) != NULL )
	{
		if (strstr(line,"rx:") )
		{
			sscanf(line,"%*s %d", &rx_pakets);
		}
		else if (strstr(line,"rx_bytes") )
		{
			sscanf(line,"%*s %d", &rx_bytes);
		}
		else if (strstr(line,"tx_bytes") )
		{
			sscanf(line,"%*s %d", &tx_bytes);
		}
		else if (strstr(line,"tx_packets") )
		{
			sscanf(line,"%*s %d", &tx_packets);
		}
		else if (strstr(line,"forward:") )
		{
			sscanf(line,"%*s %d", &fw_packets);
		}
		else if (strstr(line,"forward_bytes") )
		{
			sscanf(line,"%*s %d", &fw_bytes);
		}
		else if (strstr(line,"forward_bytes") )
		{
			sscanf(line,"%*s %d", &fw_bytes);
		}
		else if (strstr(line,"mgmt_rx") )
		{
			sscanf(line,"%*s %d", &mr_bytes);
		}
		else if (strstr(line,"mgmt_rx_bytes") )
		{
			sscanf(line,"%*s %d", &mr_packets);
		}
		else if (strstr(line, "mgmt_tx") )
		{
			sscanf(line,"%*s %d", &mt_bytes);
		}
		else if (strstr(line, "mgmt_tx_bytes") )
		{
			sscanf(line,"%*s %d", &mt_packets);
		}
	}
		
	ret = calloc(1294,1);

	sprintf(ret, "\"rx\":{");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"packets\":%d,",rx_packets);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"bytes\":%d",rx_bytes);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "},");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"tx\":{");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"packets\":%d,",tx_packets);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"bytes\":%d",tx_bytes);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "},");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"forward\":{");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"packets\":%d,",fw_packets);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"bytes\":%d",fw_bytes);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "},");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"mgmt_rx\":{");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"packets\":%d,",mr_packets);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"bytes\":%d",mr_bytes);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "},");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"mgmt_tx\":{");
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"packets\":%d,",mt_packets);
	pos += strlen(ret+pos);
	sprintf(ret+pos, "\"bytes\":%d}",mt_bytes);

	fclose(fp);
	/* assemble buffer */
	
	return ret;
}

char *clients(char *interface)
{
	char *ret = NULL;
	char buf[1024];
	FILE  *fp;
	int countw = 0;
	int countt = 0;
	char *batctl = var("USE_BATCTL");
	int use_batctl = 0;

	snprintf(buf,sizeof(buf),
	         	"/sys/kernel/debug/batman_adv/%s/transtable_local",
		 	interface);
	
	if ( (batctl && strcmp(batctl,"true") == 0) || access(buf, F_OK) != 0 )
	{
		snprintf(buf,sizeof(buf)-1,
	        	"batctl -m %s tl",
			interface);
		use_batctl = 1;
		fp = popen(buf,"r");
	}
	else
	{
		snprintf(buf,sizeof(buf),
	         	"/sys/kernel/debug/batman_adv/%s/transtable_local",
		 	interface);
		fp = fopen(buf,"r");
	}
	if ( batctl )
	{
		free(batctl);
	}
	if ( fp )
	{
		while (fgets(buf,sizeof(buf),fp))
		{
			if (strchr(buf,'['))
			{
				if ( !strchr(buf,'P') )
				{
					countt++;
				}
				if (strchr(buf,'W'))
				{
					countw++;
				}
			}
			
		}
		ret=calloc(1,100);
		snprintf(ret, 100, "{\"wifi\":%d,\"total\":%d}", countw, countt);
		if ( use_batctl )
		{
			pclose(fp);
		}
		else
		{
			fclose(fp);
		}
	}
	else
	{
		ret = calloc(1,4);
		ret[0]='{';
		ret[1]='}';
	}

	return ret;
}

typedef struct funcPointers_s {
	const char *name;
	int isNum;
	char* (*func)(char *);
	const char *args;
} funcPointers_t;


#define DELIMITED 	0
#define NOTDELIM	1
#define BLOCK		2
#define LIST		3
#define NOOUT		4

static funcPointers_t func[] = {
	{ "kern",            NOTDELIM,  &fromFile,  "file"            },
	{ "dkern",           DELIMITED, &fromFile,  "file"            },
	{ "nproc",           NOTDELIM,  &nproc,     ""                },
	{ "uname",           DELIMITED, &fromUname, "sysname|nodename|release|version|machine" },
	{ "mac",             DELIMITED, &mac,       "interface"       },
	{ "macList",         LIST,      &macList,   "interface interface..." },
	{ "allIPv6",         LIST,      &allIPv6,   "interface"       },
	{ "stringList",      LIST,      &stringList, "word1 word2 ..."},
	{ "nodeid",          DELIMITED, &nodeid,     "interface"      },
	{ "var",             NOTDELIM,  &var,        "Variable_name ..." },
	{ "dvar",            DELIMITED, &var,        "Variable_name ..." },
	{ "addVar" ,         NOOUT,     &addVar,     "variable_name value" },
	{ "readConf",        NOOUT,     &readConf,   "filename"       },
	{ "uptime",          NOTDELIM,  &uptime,     ""               },
	{ "exec",            NOTDELIM,  &exec,       "script_name"    },
	{ "idletime",        NOTDELIM,  &idletime,   ""               },
	{ "loadavg",         NOTDELIM,  &loadavg,    ""               },
	{ "fsusage",         NOTDELIM,  &fsusage,    ""               },
	{ "memory",          BLOCK,     &memory,     ""               },
	{ "processes",       BLOCK,     &processes,  ""               },
	{ "lsbRelease",      DELIMITED, &lsbRelease, "-i|-r|..."      },
	{ "include",         NOTDELIM,  &include,    "filename"       },
	{ "fastdConnection", BLOCK,     &fastdConnection, "socket_name" },
	{ "gateway",         DELIMITED, &gateway,    "interface"      },
	{ "batadv",          BLOCK,     &batadv,     "interface"      },
	{ "wifi",            BLOCK,     &wifi,       "interface interface..." },
	{ "traffic",         BLOCK, 	&traffic,    "interface"      },
	{ "clients",         BLOCK, 	&clients,    "interface"      },
	{ "printVars",       BLOCK,     &printVars,   ""              },
	{ NULL, 0, NULL, NULL }
};

static char *processArgs(char *beg, char*end)
{
	char *s = NULL;
	char *t = NULL;
	char *vname = NULL;
	size_t sz = end+1-beg + 128;
	int pos = 0;
	char *ret = calloc(1, sz);
	char *arg = NULL;
	int len;
	t = ret;

	while ( beg < end)
	{
		if (*beg == '$' )
		{
			beg++;
			s = getWordEnd(beg);
			if ( (vname = strndup(beg, s-beg)) )
			{
			 	arg = var(vname);
				if ( arg )
				{
					/* copy to ret */
					len = strlen(arg);
					if ( ((t-ret) + len) > (sz - 127) )
					{
						pos=t-ret;
						sz+=128;
						ret=realloc(ret,sz);
						t = ret+pos;
					}
					strcpy(t,arg);
					t += len;
					if ( *s != '$' && s < end )
						*t++=*s++;
					free(arg);
					arg=NULL;
				}
				free(vname);
				vname = NULL;
			}
			beg = s;
		}
		else
		{
			*t++ = *beg++;
			pos++;
		}	
	}
	*t = '\0';
	return ret;
}

#ifdef MKJSON
static void printBeautyfied(char *json)
{
	int idend = 0;
	char *s = json;
	int i;
	int instring = 0;
	while ( s && *s )
	{
		if ( *s == '"' ) instring = ! instring;
		switch(*s)
		{
			case '{':
			case '[':
				putchar(*s);
				putchar('\n');
				idend += 3;
				i = idend;
				while ( i > 0 )
				{
					putchar(' ');
					i--;
				}
				s++;
			break;
			case ',':
				putchar(*s);
				putchar('\n');
				i = idend;
				while ( i > 0 )
				{
					putchar(' ');
					i--;
				}
				s++;
			break;
			case ']':
			case '}':
				putchar('\n');
				idend -= (idend >= 3) ?3:0;
				i = idend;
				while ( i > 0 )
				{
					putchar(' ');
					i--;
				}
				putchar(*s);
				s++;

				if ( !strchr("},]", *s))
				{
					putchar('\n');
				}
			break;
			case ':':
				putchar(*s);
				if ( !instring )
					putchar(' ');
				s++;
			break;
			default:
				putchar(*s);
				s++;
			break;
		}
	}
	putchar('\n');
}

static void usage(char *name, FILE *out)
{
	fprintf(out,"Usage: %s -t <json-template> [-c <conf-file>] [-d <path>] [-b]\n\n",name);
	fprintf(out,"\t-t json template\n");
	fprintf(out,"\t-c configuration file (shell script\n");
	fprintf(out,"\t-d path where the templates and scripts are located\n");
	fprintf(out,"\t-b human readable output\n");
}

static void help(char *name,FILE *out)
{
	int i = 0;
	char *type = 0;
	usage(name,out);

	fprintf(out,"\n\tBuildin functions:\n\n");
	fprintf(out,"\t\tOutput    function\n");
	fprintf(out,"\t\t--------- -----------------------------------------------\n");
	
	while(func[i].name)
	{
		switch(func[i].isNum)
		{
			case DELIMITED: type = "delimited"; break;
			case NOTDELIM:  type = "number   "; break;
			case BLOCK:     type = "block    "; break;
			case NOOUT:     type = "no output"; break;
			case LIST:      type = "list     "; break;
		}
		fprintf(out,"\t\t%s %s(%s)\n",
			type,
			func[i].name,
			func[i].args);
		i++;
	}
}
#endif

static char *processTemplate(char *template, int beautify)
{
	size_t size=0;
	int i;
	char *fb;
	char *fe;
	char *pb;
	char *pe;
	char *fname = NULL;
	char *param = NULL;	/* process file */
	if ( size < 512 )
		size = 1024;
	char *out = calloc(1, size<<1);
	char *op = out;
	char *s;
	int comma=0;
	
	if ( out == NULL )
	{
		fprintf(stderr,"alloc: %s\n",strerror(errno));
		return out;
	}
	s = template;

	while ( *s )
	{
		if ( isspace(*s) )
		{
			s++;
		}
		else if ( *s == '@' )
		{
			s++;
			if ( *s == ',' )
			{
				comma = 1;
				s++;
			}

			/* get functions name */
			fb = s;
			fe = s;
			while(*s && *s != '(' )
			{
				fe++;
				s++;
			}
			fname = calloc(fe+1-fb,1);
			strncpy(fname,fb, fe-fb);		
			s++;

			/* get parameter */
			pb = s;
			pe = s;
			while(*s && *s != ')' )
			{
				pe++;
				s++;
			}

			param = processArgs(pb,pe);
			for ( i = 0; func[i].name; i++ )
			{
				if (strcmp(func[i].name,fname) == 0 )
				{
					char *val = func[i].func(param);
					char *p;

					if ( val == NULL )
					{
						*op++ = 'n';
						*op++ = 'u';
						*op++ = 'l';
						*op++ = 'l';
					}
					else
					{
						p = val;
						if ( !func[i].isNum )
							*op++ = '"';
						
						while (*p && *p != '\n')
							*op++ = *p++;
						if ( !func[i].isNum )
							*op++ = '"';
						if ( comma && *val && op[-1] != ',')
							*op++ = ',';
						comma = 0;
						free(val);
	
					}
					break;
				}
			}
			if ( fname ) free(fname);
			if ( param ) free(param);
			fname = NULL;
			param = NULL;
			s++;
		} 
		else
		{
			*op++ = *s++;
		}

		if ( s - out > size - 128 )
		{
			int pos = op - out;
			out = realloc(out, size + 1024);
			op = out + pos;
			size += 1024;
		}

	}

	if ( out )
	{
		;//strcat(out,"\n");
	}
	return out;
}

#ifdef MKJSON
int main(int argc, char **argv)
{
	char *s;
	char c;
	char *jsonfile = NULL;
	char *template = NULL;
	size_t size=0;
	char *progname = argv[0];
	char *newpath = NULL;
	char *path = NULL;
	int beautify = 0;
	char *out = NULL;
	
	if ( (s = strrchr(progname,'/')) )
		progname = s+1;
		
	if ( argc == 1 )
	{
		help(progname,stderr);
		exit(0);
	}
	
	while((c = getopt(argc, argv, "vt:c:hd:b")) > -1 )
	{
		switch (c)
		{
			case 'c':
				getGlobale(optarg);
			break;
			case 't':
				jsonfile=optarg;
			break;
			case 'v':
				printf("%s v%s\n",progname,version);
				exit(0);
			break;
			case 'b':
				beautify=1;
			break;
			case 'h':
				help(progname,stdout);
				exit(0);
			break;
			case 'd':
				path = getenv("PATH");
				newpath = calloc(1, strlen(path?path:"") + strlen(optarg) + 10);
				sprintf(newpath,"PATH=%s:%s\n",optarg, path?path:"");
				putenv(newpath);
				chdir(optarg);
			break;
		}
	}
	
	if ( jsonfile == NULL )
	{
		usage(progname, stderr);
	}

	template = readTemplate(progname, jsonfile, &size);
	if ( template == NULL )
	{
		perror("jsonfile");
		return 1;
	}
	out = processTemplate(template, beautify);
	if ( out )
	{
		if ( ! beautify )
		{
			printf("%s\n",out);
		}
		else
		{
			printBeautyfied(out);
		}
	}
	if ( template) free(template);
	if ( out) free(out);

	if ( newpath) free(newpath);
	freeVars();
	return 0;
}
#else

#ifndef true
#define true 1
#define false 0
#endif

static void join_mcast(const int sock, const struct in6_addr addr, const char *iface) {
	struct ipv6_mreq mreq;

	mreq.ipv6mr_multiaddr = addr;
	mreq.ipv6mr_interface = if_nametoindex(iface);

	if (mreq.ipv6mr_interface == 0)
	{
		goto error;
	}
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
	{
		goto error;
	}

	return;

 error:
	fprintf(stderr, "Could not join multicast group on %s: ", iface);
	perror(NULL);
}

static void usage() {
	puts("Usage:");
	puts("  respondd -h");
	puts("  respondd [-p <port>] [-g <group> -i <if0> [-i <if1> ..]] [-d <dir>]");
	puts("        -p <int>         port number to listen on");
	puts("        -g <ip6>         multicast group, e.g. ff02::2:1001");
	puts("        -i <string>      interface on which the group is joined");
	puts("        -d <string>      data provider directory (default: current directory)");
	puts("        -f               don't daemonize\n");
	puts("        -h               this help\n");
}

static void serve(int sock) {
	char input[256];
	char *output = NULL;
	ssize_t input_bytes, output_bytes;
	struct sockaddr_in6 addr;
	socklen_t addrlen = sizeof(addr);
	char *template = NULL;

	input_bytes = recvfrom(sock, input, sizeof(input)-1, 0, (struct sockaddr *)&addr, &addrlen);

	if (input_bytes < 0) {
		perror("recvfrom failed");
		exit(EXIT_FAILURE);
	}

	input[input_bytes] = 0;
	template = readTemplate("", input, (size_t*)&output_bytes);
	if ( template )
	{
		output = processTemplate(template, 0);
		output_bytes = strlen(output);
	}
	
	if (output) {
		if (sendto(sock, output, output_bytes, 0, (struct sockaddr *)&addr, addrlen) < 0)
			perror("sendto failed");
	}

	if ( template )
	{
		free(template);
		template = NULL;
	}

	if (output)
	{
		free(output);
		output = NULL;
	}
}


int main(int argc, char **argv) {
	const int one = 1;
	char *newpath = NULL;
	char *path = NULL;
	int daemonize = 1;
	char *interface = NULL;
	int sock;
	struct sockaddr_in6 server_addr = {};
	struct in6_addr mgroup_addr;

	sock = socket(PF_INET6, SOCK_DGRAM, 0);

	if (sock < 0) {
		perror("creating socket");
		exit(EXIT_FAILURE);
	}

	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one))) {
		perror("can't set socket to IPv6 only");
		exit(EXIT_FAILURE);
	}

	server_addr.sin6_family = AF_INET6;
	server_addr.sin6_addr = in6addr_any;
	server_addr.sin6_port = htons(RESPONDD_PORT);
	opterr = 0;

	int group_set = 0;

	int c;
	while ((c = getopt(argc, argv, "p:g:i:d:hc:f")) != -1) {
		switch (c) {
		case 'p':
			server_addr.sin6_port = htons(atoi(optarg));
			break;

		case 'g':
			if (!inet_pton(AF_INET6, optarg, &mgroup_addr)) {
				perror("Invalid multicast group. This message will probably confuse you");
				exit(EXIT_FAILURE);
			}

			group_set = 1;
			break;

		case 'i':
			interface = optarg;
			if (!group_set) {
				fprintf(stderr, "Multicast group must be given before interface.\n");
				exit(EXIT_FAILURE);
			}
			join_mcast(sock, mgroup_addr, optarg);
			break;

		case 'd':
				path = getenv("PATH");
				newpath = calloc(1, strlen(path?path:"") + strlen(optarg) + 10);
				sprintf(newpath,"PATH=%s:%s\n",optarg, path?path:"");
				putenv(newpath);
				chdir(optarg);
			break;

		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;
		case 'c':
			getGlobale(optarg);
			break;
		case 'f':
			daemonize = 0;
			break;
		default:
			fprintf(stderr, "Invalid parameter -%c ignored.\n", optopt);
		}
	}
	if ( interface == NULL )
	{
		fprintf(stderr, "interface is required\n");
		exit(EXIT_FAILURE);
	}

	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

	if ( daemonize )
	{
		daemon(1,0);
	}
	while (true)
		serve(sock);

	return EXIT_FAILURE;
}
#endif
