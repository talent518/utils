#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "getopt.h"
#include "ftp.h"

static const opt_struct OPTIONS[] = {
	{'H', 0, "hide-args"},
	{'h', 1, "host"},
	{'p', 1, "port"},
	{'u', 1, "user"},
	{'w', 1, "password"},
	{'m', 1, "method"},
	{'l', 1, "local"},
	{'r', 1, "remote"},
	{'s', 0, "ssl"},

	{'-', 0, NULL} /* end of args */
};

/* {{{ usage
 */
static void usage(char *argv0) {
	char *prog;

	prog = strrchr(argv0, '/');
	if (prog) {
		prog++;
	} else {
		prog = "vuforia";
	}

	printf( "FTP recursively uploading, downloading, and deleting operation scripts\n\n"
		"Usage: %s <options>\n"
		"\n"
		"options:\n"
		"  -h host, --host host              FTP host\n"
		"  -p port, --port port              FTP port(default: 21)\n"
		"  -u user, --user user              FTP account\n"
		"  -w password, --password password  FTP password\n"
		"  -m method, --method method        FTP operation method(get, put, del)\n"
		"  -l local, --local local           Local working directory\n"
		"  -r remote, --remote remote        Remote working directory\n"
		"  -s, --ssl                         Use ssl\n"
		"  -H, --hide-args                   Hidden cmd args\n"
		, prog);
}
/* }}} */

void ftpget(ftpbuf_t *ftp, const char *local, const char *remote) {
}

void ftpput(ftpbuf_t *ftp, const char *local, const char *remote) {
}

void ftpremove(ftpbuf_t *ftp, const char *remote) {
}

int main(int argc, char *argv[]) {
	volatile const char *optarg = NULL;
	volatile int optind = 1;
	int exit_status = 0;
	int c, i;
	int hide_argv = 0;
	short port = 21;
#ifdef HAVE_FTP_SSL
	int use_ssl = 0;
#endif
	char *host = NULL, *user = NULL, *password = NULL, *method = NULL, *local = NULL, *remote = NULL;

	if(argc == 1) {
		usage(argv[0]);
		
		return 1;
	}

	while ((c = getopt(argc, argv, OPTIONS, (char**)&optarg, (int*)&optind, 1, 2))!=-1) {
		switch (c) {
			case 'h':
				host = strdup((const char *)optarg);
				break;
			case 'p':
				port = atoi((const char *)optarg);
				break;
			case 'u':
				user = strdup((const char *)optarg);
				break;
			case 'w':
				password = strdup((const char *)optarg);
				break;
			case 'm':
				method = strdup((const char *)optarg);
				break;
			case 'l':
				local = strdup((const char *)optarg);
				break;
			case 'r':
				remote = strdup((const char *)optarg);
				break;
		#ifdef HAVE_FTP_SSL
			case 's':
				use_ssl = 1;
				break;
		#endif
			case 'H':
				hide_argv = 1;
				break;
			default:
				break;
		}
	}
	
	if(!host || !user || !password || !method || (strcmp(method, "get") && strcmp(method, "put") && strcmp(method, "del")) || (!local && strcmp(method, "del")) || !remote) {
		usage(argv[0]);
		fprintf(stderr, "\n"
			"paramters:\n"
			"  host     = \"%s\"\n"
			"  port     = %d\n"
			"  user     = \"%s\"\n"
			"  password = \"%s\"\n"
			"  method   = \"%s\"\n"
			"  local    = \"%s\"\n"
			"  remote   = \"%s\"\n"
		#ifdef HAVE_FTP_SSL
			"  ssl      = %d\n"
		#endif
			, host?host:"", port, user?user:"", password?password:"", method?method:"", local?local:"", remote?remote:""
		#ifdef HAVE_FTP_SSL
			, use_ssl
		#endif
		);
		exit_status = 1;
		goto optEnd;
	}

	if (hide_argv) {
		for (i = 1; i < argc; i++) {
			memset(argv[i], 0, strlen(argv[i]));
		}
	}
	
	ftpbuf_t *ftp = ftp_open(host, port, 90);
	if(!ftp) {
		goto optEnd;
	}
	
	ftp->usepasvaddress = 1;
#ifdef HAVE_FTP_SSL
	ftp->use_ssl = use_ssl;
#endif
	if(!ftp_login(ftp, user, strlen(user), password, strlen(password))) {
		goto ftpQuit;
	}
	
	if(!strcmp(method, "get")) {
		ftpget(ftp, local, remote);
	} else if(!strcmp(method, "put")) {
		ftpput(ftp, local, remote);
	} else {
		ftpremove(ftp, remote);
	}

ftpQuit:
	ftp_quit(ftp);
	ftp_close(ftp);

optEnd:
	{
		char *frees[6] = {host, user, password, method, local, remote};
		for(i=0; i<sizeof(frees)/sizeof(char*); i++) {
			if(frees[i]) {
				free((void*) frees[i]);
			}
		}
	}

	return exit_status;
}

