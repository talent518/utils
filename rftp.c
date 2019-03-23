#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "getcmdopt.h"
#include "ftp.h"

static int TRIES = 3;
static unsigned long int nTRIES = 0;

static const opt_struct OPTIONS[] = {
	{'H', 0, "hide-args"},
	{'h', 1, "host"},
	{'p', 1, "port"},
	{'u', 1, "user"},
	{'w', 1, "password"},
	{'m', 1, "method"},
	{'l', 1, "local"},
	{'r', 1, "remote"},
	{'t', 1, "try"},
	{'T', 1, "timeout"},
	{'d', 0, "debug"},
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
		prog = "rftp";
	}

	printf( "FTP recursively uploading, downloading, and deleting operation scripts\n\n"
		"Usage: %s <options>\n"
		"\n"
		"options:\n"
		"  -h host, --host host              FTP host\n"
		"  -p port, --port port              FTP port(default: 21)\n"
		"  -u user, --user user              FTP account\n"
		"  -w password, --password password  FTP password\n"
		"  -m method, --method method        FTP operation method(get, put, rls, del)\n"
		"  -l local, --local local           Local working directory\n"
		"  -r remote, --remote remote        Remote working directory\n"
		"  -t try, --try try                 Failure try times(default: 3)\n"
		"  -T timeout, --timeout timeout     Timeout(default: 3 seconds)\n"
		"  -d, --debug                       Print debug info to stderr\n"
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
	
	char perms[11]="";
	int number, owner, group;
	unsigned long int size;
	char dt1[4]="", dt2[3]="", dt3[6]="", datetime[13]="", name[256]="", link[256]="";
	char *ptr;
	int tries = 0;
	
trylst:
	lines = ftp_list(ftp, path, len, 0);
	if(!lines) {
		if(++tries < TRIES && ftp_reconnect(ftp)) goto trylst;
		fprintf(stderr, "The directory is not found for \"%s\"\n", remote);
		free(path);
		return 1;
	}
	
	nTRIES += tries;
	
	int ret = 0;
	while(line=lines[n++]) {
		if(line[0] == 'd' && (ptr = strrchr(line, ' ')) && (!strcmp(ptr+1, ".") || !strcmp(ptr+1, ".."))) {
			continue;
		}
		ret = sscanf(line, "%[^ ] %d %d %d %lu %s %s %s %n", perms, &number, &owner, &group, &size, dt1, dt2, dt3, &len);
		if(ret == 2) {
			ret = sscanf(line, "%[^ ] %d %*[^ ] %*[^ ] %lu %s %s %s %n", perms, &number, &size, dt1, dt2, dt3, &len);
			owner = 1000;
			group = 1000;
		}
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
		ret = 0;
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

int ftpget_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link) {
	printf("%c: %s/%s <= %s/%s\n", type, local, name, remote, name);
	
	char *plocal = NULL, *premote = NULL;
	struct stat st;
	int i, ret = 0;
	
	asprintf(&plocal, "%s/%s", local, name);
	asprintf(&premote, "%s/%s", remote, name);

	int tries = 0;
	
tryget:
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
		if((i==0 && S_ISREG(st.st_mode) && (st.st_size != size || (size>0 && ftp_mdtm(ftp, premote, strlen(premote)) > st.st_mtime))) || i == -1) {
			FILE *fp = fopen(plocal, st.st_size < size ? "a" : "w");
			if(fp) {
				ftp_set_total(ftp, size, st.st_size < size ? st.st_size : 0, 0);
				if(!ftp_get(ftp, fp, premote, strlen(premote), FTPTYPE_IMAGE, i==0 && st.st_size < size ? st.st_size : 0)) {
					if(++tries < TRIES && ftp_reconnect(ftp)) {
						fclose(fp);
						goto tryget;
					}
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
	
	nTRIES += tries;
	
	return ret;
}

int ftpput(ftpbuf_t *ftp, const char *local, const char *remote) {
	int tries = 0;
trymkdir:
	if(*remote && !ftp_chdir(ftp, remote, strlen(remote)) && !ftp_mkdir(ftp, remote, strlen(remote))) {
		if(++tries < TRIES && ftp_reconnect(ftp)) goto trymkdir;
		nTRIES += tries;
		fprintf(stderr, "Failed to create remote directory %s\n", remote);
		return 1;
	}
	nTRIES += tries;
	
	if(*local || *remote) printf("d: %s => %s\n", local, remote);
	
	DIR *dh = opendir(local);
	if(!dh) {
		fprintf(stderr, "Failed to open directory %s\n", local);
		return 1;
	}
	
	char *plocal, *premote;
	struct dirent *dir;
	long size;
	struct stat st;
	FILE *fp;
	int ret = 0;
	
	while(ret == 0 && (dir=readdir(dh))) {
		if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
			continue;
		}
		
		plocal = NULL;
		premote = NULL;
		
		asprintf(&plocal, "%s/%s", local, dir->d_name);
		asprintf(&premote, "%s/%s", remote, dir->d_name);
		
		lstat(plocal, &st);
		
		if(S_ISDIR(st.st_mode)) {
			ret = ftpput(ftp, plocal, premote);
		} else if(S_ISREG(st.st_mode)) {
			printf("-: %s => %s\n", plocal, premote);
			
			size = ftp_size(ftp, premote, strlen(premote));
			fp = NULL;
		tryput:
			if(st.st_size == size) {
				if(size>0 && ftp_mdtm(ftp, premote, strlen(premote)) < st.st_mtime) {
					size = 0;
					goto tryput;
				}
			} else if((fp=fopen(plocal, "r"))) {
				tries = 0;
				ftp_set_total(ftp, st.st_size, 0, st.st_size > size && size > 0 ? size : 0);
				while(!ftp_put(ftp, premote, strlen(premote), fp, FTPTYPE_IMAGE, st.st_size > size ? size : 0) && ++tries < TRIES && ftp_reconnect(ftp)) size = ftp->sent;
				if(fp) fclose(fp);
				nTRIES += tries;
				if(ftp->resp == 1024 || tries >= TRIES) {
					fprintf(stderr, "Upload file %s failed\n", plocal);
					ret = 1;
				}
			}
		} else {
			char type;
			switch(st.st_mode & S_IFMT) {
				case S_IFBLK:  type = 'b'; break;
				case S_IFCHR:  type = 'c'; break;
				case S_IFDIR:  type = 'd'; break;
				case S_IFIFO:  type = 'f'; break;
				case S_IFLNK:  type = 'l'; break;
				case S_IFREG:  type = '-'; break;
				case S_IFSOCK: type = 's'; break;
				default:       type = 'u';break;
			}
			printf("%c: %s => %s\n", type, plocal, premote);
			fprintf(stderr, "File %s is an unsupported local file type\n", plocal);
			ret = 1;
		}
		free(plocal);
		free(premote);
	}
	
	closedir(dh);
	
	return ret;
}

int ftpremove_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link) {
	printf("%c: %s/%s\n", type, remote, name);
	
	char *premote = NULL;
	int ret = 0;
	int tries = 0;
	
	asprintf(&premote, "%s/%s", remote, name);
	
	if(type == 'd') {
	tryrmdir:
		ret = ftplist(ftp, local, premote, ftpremove_func);
		if(!ret && !ftp_rmdir(ftp, premote, strlen(premote))) {
			if(++tries < TRIES && ftp_reconnect(ftp)) goto tryrmdir;
			fprintf(stderr, "Deletion of directory %s failed\n", premote);
			ret = 1;
		}
	} else {
	trydel:
		if(!ftp_delete(ftp, premote, strlen(premote))) {
			if(++tries < TRIES && ftp_reconnect(ftp)) goto trydel;
			fprintf(stderr, "Deletion of file %s failed\n", premote);
			ret = 1;
		}
	}
	
	free(premote);
	
	nTRIES += tries;
	
	return ret;
}

int ftplist_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *line, char type, const char *perms, int number, int owner, int group, unsigned long int size, const char *datetime, const char *name, const char *link) {
	char *premote = NULL;
	int ret = 0;
	
	asprintf(&premote, "%s/%s", remote, name);
	
	if(type == 'd') {
		printf("%c: %s/%s\n", type, remote, name);
		ret = ftplist(ftp, local, premote, ftplist_func);
	} else {
		printf("%c: %s/%s %lu\n", type, remote, name, size);
	}
	
	free(premote);
	
	return ret;
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
	int timeout = 3;
	int debug = 0;
	char *host = NULL, *user = strdup("anonymous"), *password = strdup("anonymous"), *method = NULL, *local = NULL, *remote = NULL;

	if(argc == 1) {
		usage(argv[0]);
		
		return 1;
	}

	while ((c = getcmdopt(argc, argv, OPTIONS, (char**)&optarg, (int*)&optind, 1, 2))!=-1) {
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
			case 't':
				TRIES = atoi((const char *)optarg);
				break;
			case 'T':
				timeout = atoi((const char *)optarg);
				break;
			case 'd':
				debug = 1;
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
	
	if(!host || !user || !password || !method || (strcmp(method, "get") && strcmp(method, "put") && strcmp(method, "rls") && strcmp(method, "del")) || (!local && strcmp(method, "rls") && strcmp(method, "del")) || !remote) {
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
			"  TRIES    = %d\n"
			"  timeout  = %d\n"
			"  debug  = %d\n"
		#ifdef HAVE_FTP_SSL
			"  ssl      = %d\n"
		#endif
			, host?host:"", port, user?user:"", password?password:"", method?method:"", local?local:"", remote?remote:"", TRIES, timeout, debug
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
	
	ftpbuf_t *ftp = ftp_open(host, port, timeout, debug);
	if(!ftp) {
		fprintf(stderr, "connect %s:%d failure\n", host, port);
		goto optEnd;
	}
	
	ftp->usepasvaddress = 1;
#ifdef HAVE_FTP_SSL
	ftp->use_ssl = use_ssl;
#endif
	if(!ftp_login(ftp, user, strlen(user), password, strlen(password))) {
		fprintf(stderr, "user %s login failure\n", user);
		goto ftpQuit;
	}
	
	if(ftp_pasv(ftp, 1)) {
		fprintf(stderr, "passive mode on\n");
	} else {
		fprintf(stderr, "passive mode off\n");
	}

	// {{{ format remote
	char *pwd, *ptr, *tmp;
	if(*remote && !strcmp(remote, "/")) {
		*remote = '\0';
	}
	if(!(pwd = (char*) ftp_pwd(ftp)) || !strcmp(pwd, "/")) {
		pwd = "";
	}
	if(*remote && *remote != '/') {
		ptr = NULL;
		asprintf(&ptr, "%s/%s", pwd, remote);
		free(remote);
		remote = ptr;
	}
	ptr = remote;
	while(*ptr) {
		if(*ptr == '/' && *(ptr+1) == '/') {
			pwd = ptr+2;
			while(*pwd == '/') {
				pwd++;
			}
			tmp = ptr+1;
			while((*tmp = *pwd)) {
				tmp++;
				pwd++;
			}
		}
		ptr++;
	}
	if(*remote && *(ptr-1) == '/') *(ptr-1) = '\0';
	// }}}
	
	struct stat st;
	
	i = strcmp(method, "del") && strcmp(method, "rls") ? lstat(local, &st) : 0;
	
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
			ftpput(ftp, local, remote);
		} else {
			fprintf(stderr, "%s is not a directory\n");
		}
	} else if(!strcmp(method, "rls")) {
		if(*remote) printf("d: %s\n", remote);
		ftplist(ftp, NULL, remote, ftplist_func);
	} else {
		if(*remote) printf("d: %s\n", remote);
		if(ftplist(ftp, NULL, remote, ftpremove_func) || (*remote && !ftp_rmdir(ftp, remote, strlen(remote)))) {
			fprintf(stderr, "Deletion of directory %s failed\n", remote);
		}
	}
	
	exit_status = -errno;

ftpQuit:
	if(ftp->reconnect) fprintf(stderr, "Reconnected times is %d\n", ftp->reconnect);
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
	
	if(errno) fprintf(stderr, "Error: %s\n", strerror(errno));
	fprintf(stderr, "Total try times is %lu\n", nTRIES);

	return exit_status;
}