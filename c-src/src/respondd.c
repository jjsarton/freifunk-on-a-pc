/* respondd */
//#include "respondd.h"

//#include "miniz.c"

#include <json-c/json.h>

#include <alloca.h>
#include <dirent.h>
#include <dlfcn.h>
#include <inttypes.h>
#include <search.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

static void join_mcast(const int sock, const struct in6_addr addr, const char *iface) {
	struct ipv6_mreq mreq;

	mreq.ipv6mr_multiaddr = addr;
	mreq.ipv6mr_interface = if_nametoindex(iface);

	if (mreq.ipv6mr_interface == 0)
		goto error;

	if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) == -1)
		goto error;

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
	puts("        -h               this help\n");
}

static void serve(int sock) {
	char input[256];
	const char *output = NULL;
	ssize_t input_bytes, output_bytes;
	struct sockaddr_in6 addr;
	socklen_t addrlen = sizeof(addr);
/*	bool compress;*/

	input_bytes = recvfrom(sock, input, sizeof(input)-1, 0, (struct sockaddr *)&addr, &addrlen);

	if (input_bytes < 0) {
		perror("recvfrom failed");
		exit(EXIT_FAILURE);
	}

	input[input_bytes] = 0;

#if 0
	struct json_object *result = handle_request(input, &compress);
	const char *str = json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN);
	if (!result)
		return;
#else
	const char *str ="";
#endif
#if 0
	if (compress) {
		size_t str_bytes = strlen(str);

		mz_ulong compressed_bytes = mz_compressBound(str_bytes);
		unsigned char *compressed = alloca(compressed_bytes);

		if (!mz_compress(compressed, &compressed_bytes, (const unsigned char *)str, str_bytes)) {
			output = (const char*)compressed;
			output_bytes = compressed_bytes;
		}
	}
	else {
#endif
		output = str;
		output_bytes = strlen(str);
#if 0
	}
#endif
	if (output) {
		if (sendto(sock, output, output_bytes, 0, (struct sockaddr *)&addr, addrlen) < 0)
			perror("sendto failed");
	}
#if 0
	json_object_put(result);
#endif
}


int main(int argc, char **argv) {
	const int one = 1;

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

	opterr = 0;

	int group_set = 0;

	int c;
	while ((c = getopt(argc, argv, "p:g:i:d:h")) != -1) {
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
			if (!group_set) {
				fprintf(stderr, "Multicast group must be given before interface.\n");
				exit(EXIT_FAILURE);
			}
			join_mcast(sock, mgroup_addr, optarg);
			break;

		case 'd':
			if (chdir(optarg)) {
				perror("Unable to change to given directory");
				exit(EXIT_FAILURE);
			}
			break;

		case 'h':
			usage();
			exit(EXIT_SUCCESS);
			break;

		default:
			fprintf(stderr, "Invalid parameter -%c ignored.\n", optopt);
		}
	}

	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

//	load_providers();

	while (true)
		serve(sock);

	return EXIT_FAILURE;
}
