#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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

typedef int (*list_func_t)(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link);

/* {{{ return 1 is break or failure, 0 is complete
 */
int ftplist(ftpbuf_t *ftp, const char *local, const char *remote, list_func_t func) {
	char *path = NULL;
	char **lines;
	int n = 0;
	char *line;
	int len = asprintf(&path, "-a %s", remote);
	
	char perms[12]="";
	int number, owner, group;
	unsigned long int size;
	char dt1[4]="", dt2[3]="", dt3[6]="", datetime[13]="", name[256]="", link[256]="";
	char *ptr;
	
	lines = ftp_list(ftp, path, len, 0);
	if(!lines) {
		fprintf(stderr, "The directory is not found for \"%s\"\n", remote);
		free(path);
		return 1;
	}
	
	int ret = 0;
	while(line=lines[n++]) {
		if(line[0] == 'd' && (ptr = strrchr(line, ' ')) && (!strcmp(ptr+1, ".") || !strcmp(ptr+1, ".."))) {
			continue;
		}
		sscanf(line, "%12s %d %d %d %lu %s %s %s %n", perms, &number, &owner, &group, &size, dt1, dt2, dt3, &len);
		if(line[0] == 'l' && (ptr = strstr(line+len, " -> "))) {
			strncpy(name, line+len, ptr-line-len);
			strcpy(link, ptr+4);
		} else {
			strcpy(name, line+len);
			link[0] = '\0';
		}
		strcpy(datetime, dt1);
		strcat(datetime, " ");
		strcat(datetime, dt2);
		strcat(datetime, " ");
		strcat(datetime, dt3);
		if(func(ftp, local, remote, line, perms[0], perms, number, owner, group, size, datetime, name, link)) {
			ret = 1;
			break;
		}
	}
	
	free(path);
	free(lines);
	
	return ret;
}
/* }}} */

#if 0
	#define PRINT_FUNC(isrm, op) \
		printf("%s: %s\n", __func__, line); \
		printf("  type: %c\n", type); \
		printf("  perms: %s\n", perms); \
		printf("  number: %d\n", number); \
		printf("  owner: %d\n", owner); \
		printf("  group: %d\n", group); \
		printf("  size: %lu\n", size); \
		printf("  datetime: %s\n", datetime); \
		printf("  name: %s\n", name); \
		printf("  link: %s\n", link)
#else
	#define PRINT_FUNC(isrm,op) if(isrm) printf("%c: %s/%s\n", type, remote, name); else printf("%c: %s/%s "op" %s/%s\n", type, local, name, remote, name)
#endif

int ftpget_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link) {
	PRINT_FUNC(0,"<=");
	
	char *plocal = NULL, *premote = NULL;
	struct stat st;
	int i, ret = 0;
	
	asprintf(&plocal, "%s/%s", local, name);
	asprintf(&premote, "%s/%s", remote, name);
	
	i = lstat(plocal, &st);
	
	if(type == 'd') {
		if((i==0 && S_ISDIR(st.st_mode)) || (i == -1 && mkdir(plocal, 0755) == 0)) {
			ret = ftplist(ftp, plocal, premote, ftpget_func);
		} else if(i==0) {
			fprintf(stderr, "%s is not a directory\n", plocal);
			ret = 1;
		} else {
			fprintf(stderr, "Failed to create directory %s\n", plocal);
			ret = 1;
		}
	} else if(type == 'l') {
		if((i==0 && S_ISLNK(st.st_mode)) || (i == -1 && symlink(link, plocal) == 0)) {
			ret = 0; // OK
		} else if(i==0) {
			fprintf(stderr, "%s is not a directory\n", plocal);
			ret = 1;
		} else {
			fprintf(stderr, "Failed to create directory %s\n", plocal);
			ret = 1;
		}
	} else {
		if((i==0 && S_ISREG(st.st_mode) && st.st_size != size) || i == -1) {
			FILE *fp = fopen(plocal, "a");
			if(fp) {
				if(!ftp_get(ftp, fp, premote, strlen(premote), FTPTYPE_IMAGE, i==0 ? st.st_size : 0)) {
					fprintf(stderr, "Download file %s failed\n", plocal);
					ret = 1;
				}
				fclose(fp);
			} else {
				fprintf(stderr, "Unable to open file %s\n", plocal);
				ret = 1;
			}
		} else if(i==0 && !S_ISREG(st.st_mode)) {
			fprintf(stderr, "%s is not an ordinary file\n", plocal);
		}
	}
	
	free(plocal);
	free(premote);
	
	return ret;
}

int ftpput_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link) {
	PRINT_FUNC(0,"=>");
	
	return 0;
}

int ftpremove_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link) {
	PRINT_FUNC(1,"");
	
	return 0;
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
	
	struct stat st;
	
	i = strcmp(method, "del") ? lstat(local, &st) : 0;
	
	if(!strcmp(method, "get")) {
		if(i == 0 && !S_ISDIR(st.st_mode)) {
			fprintf(stderr, "%s is not a directory\n");
		} else {
			if(i != 0 && mkdir(local, 0755) != 0) {
				fprintf(stderr, "Failed to create directory %s\n", local);
			} else {
				ftplist(ftp, local, remote, ftpget_func);
			}
		}
	} else if(!strcmp(method, "put")) {
		if(i == 0 && S_ISDIR(st.st_mode)) {
			ftplist(ftp, local, remote, ftpput_func);
		} else {
			fprintf(stderr, "%s is not a directory\n");
		}
	} else {
		if(!ftplist(ftp, NULL, remote, ftpremove_func) && ftp_rmdir(ftp, remote, strlen(remote))) {
			printf("d: %s\n", remote);
		} else {
			fprintf(stderr, "Deletion of directory %s failed\n", remote);
		}
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

