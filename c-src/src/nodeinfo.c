#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

char *jsonFmt = "data: {"
    "\"software\": {"
        "\"batman-adv\": {"
            "\"version\": @fromFile/sys(/module/batman_adv/version)"
            "\"compat\": 15"
        "},"
        "\"fastd\": {"
           " \"version\": \"%s\","
            "\"enabled\": true"
       " },"
        "\"firmware\": {"
            "\"base\": \"%s\","
            "\"release\": \"%s\""
        "},"
        "\"status-page\": {"
            "\"api\": 1"
       " }"
   " },"
   " \"network\": {"
        "\"addresses\": ["
            "\"%s\","
            "\"%s\""
        "],"
        "\"mesh\": {"
            "\"bat0\": {"
                "\"interfaces\": {"
                    "\"wireless\": ["
                        "\"%s\""
                    "],"
                    "\"tunnel\": ["
                        "\"%s\""
                    "]"
                "}"
            "}"
        "},"
        "\"mac\": \"%s\""
    "},"
    "%s\"owner\": {"
        "\"contact\": \"%s\""
    "},"
    "\"system\": {"
        "\"site_code\": \"%s\""
    "},"
    "\"node_id\": \"%s\","
    "\"hostname\": \"%s\","
    "\"hardware\": {"
        "\"model\": \"%s\","
        "\"nproc\": %s"
    "}"
"}";

char *location =
    "\"location\": {"
        "\"latitude\": %s,"
        "\"longitude\": %s"
    "},";

/* Parameter jsonFmt
   1:  batman_sdv version
   2:  fastd version
   3:  Os; 
   4:  OS release
   5:  IPv6 br_client glob addr
   6:  IPv6 br_client link local  addr
   7:  bat if: mesh0 mac
   8:  bat if: mesh-vpn mac
   9:  Location oder ""
   10: Site Code (ffld)
   11: nodeid
   12: Hostname
   13: Hardware plattform
   14: Anzahl der Prozessoren

   Parameter location
   1; Latitude  49.160042
   2: longitude 8.03847
*/


typedef struct config_s {
	struct config_s *next;
	char *param;
	char *value;
} config_t;

static config_t *config = NULL;

static int readParamValue(char *buf, char **param, char **value)
{
	char *s1 = buf;
	char *e1 = buf;
	int r = 0;
	
	while(*s1 && isspace(*s1) ) s1++;
	e1 = s1;
	while(*e1 && !(isspace(*e1)||*e1=='=') ) e1++;
	*param = calloc(1, (e1+1)-s1);
	strncpy(*param, s1, e1-s1);
	r++;
	if ( *e1 ) e1++;
	s1 = e1;
	if ( *s1 == '=') s1++;
	while(*s1 && isspace(*s1) ) s1++;
	e1 = s1;
	while(*e1) e1++;
	*value = calloc(1, (e1+1)-s1);
	strncpy(*value, s1, e1-s1);
	r++;
	return r;
}

static int readConfig(char *file, config_t **config)
{
	FILE *fp = fopen(file, "r");
	char * param = NULL;
	char *value = NULL;
	char *buf = NULL;
	size_t n = 0;
	ssize_t r;
	config_t *act = NULL;
	if ( fp == NULL )
	{
		fprintf(stderr,"File %s: %s\n",file,strerror(errno));
		return 1;
	}
	
	while ((r = getline(&buf, &n, fp)) > -1 )
	{
		if ( r > 0 )
		{
			buf[r-1] = '\0';
		}
		if ( *buf == '\n' || *buf == '#' )
		{
			continue;

		}
		r = readParamValue(buf, &param, &value);
		if ( r == 2 )
		{
			if (*config == NULL)
			{
				*config = (config_t*)calloc(1, sizeof(config_t));
				act = *config;
			}
			else
			{
				act->next = (config_t*)calloc(1, sizeof(config_t));
				act = act->next;
			}
			act->param = strdup(param);
			act->value = strdup(value);
		}
		if ( param ) free(param);
		if ( value ) free(value);
		
		buf = NULL;
		n = 0;
	}
	if ( buf ) free(buf);
	fclose(fp);
	return 0;	
}

static char *fromFile(char *path)
{
	char *result = NULL; 
	FILE *fp = fopen(path,"r");
	if ( fp != NULL ) {
		fscanf(fp,"%ms",&result);
		fclose(fp);
	}
	if ( result == NULL )
		return strdup("-");
	return result;
}

static char nil[1] = "";

static char *fromConfig(char *key)
{
	static char nil[1] = "";
	config_t *act = config;
	while ( act )
	{
		if ( strcmp(act->param, key) == 0 )
			return act->value;
		act=act->next;
	}
	return nil;
}

static char *fromNetlob(char *interface)
{
    return nil;
}

static char *fromNetLL(char *interface)
{
    return nil;
}

static fromEnv(vhar *)
{
	return nil;
}

static fromExewc(char *)
{
	return nil;
}

typedef struct funcPointers_s {
	char *name;
	char* (*func)(char *);
} funcPointers_t;

static funcPointers_t func[] = {
	{ "fromConfig", &fromConfig },
	{ "fromPath", &fromPFile },
	{ "fromIPv6LL", &fromNetFlob },
	{ "fromIPv6Glob", &IfromNetLL },
	{ "from env", &fronEnv },
	{ NULL, NULL }
};


int main(int argc, char **argv)
{

	int i=0;
	char *function(char*);
	while ( func[i].name )
	{
		printf("$s() %p\n", func[i].name, func[i].func);
		i++;
	}

	char *batman_adv_version = fromFile("/sys/module/batman_adv/version");
	char *fastd = fromConfig("fastd");
	if ( readConfig("nodeinfo.conf", &config)) exit(1);
	printf(jsonFmt,
		batman_adv_version, // /sys/modules/batman_adv/version
		fromConfig("fastd"),
		fromConfig("base"),
		fromConfig("release"),
		getIPv6Glob(fromConfig("IPv6If")),
		getIPv6LL(fromConfig("IPv6If")),
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"",
		"");

	return 0;
}
		
