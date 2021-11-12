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

#define printf(...) fprintf(stdout, __VA_ARGS__);fflush(stdout)

static int TRIES = 3;
static unsigned long int nTRIES = 0;
static FILE *outFd = NULL;
static int resume = 0;
static int is_soft = 0;

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
	{'i', 1, "input"},
	{'o', 1, "output"},
	{'d', 0, "debug"},
	{'P', 0, "pasv"},
	{'s', 0, "ssl"},
	{'R', 0, "resume"},
	{'S', 0, "soft"},
	{'?', 0, "help"},

	{'-', 0, NULL} /* end of args */
};

#ifdef HAVE_FTP_SSL
	#define USAGE_FTP_SSL " [-s]"
#else
	#define USAGE_FTP_SSL
#endif

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
		"  -u user, --user user              FTP account(default: anonymous)\n"
		"  -w password, --password password  FTP password(default: anonymous)\n"
		"  -m method, --method method        FTP operation method(get, put, rls, del)\n"
		"  -l local, --local local           Local working directory\n"
		"  -r remote, --remote remote        Remote working directory\n"
		"  -i file, --input file             Input a -o paramter usage file\n"
		"  -o file, --output file            Output a -i paramter usage file\n"
		"  -t try, --try try                 Failure try times(default: 3)\n"
		"  -T timeout, --timeout timeout     Timeout(default: 3 seconds)\n"
		"  -d, --debug                       Print debug info to stderr\n"
		"  -P, --pasv                        switches passive mode off(default: on)\n"
	#ifdef HAVE_FTP_SSL
		"  -s, --ssl                         Use ssl\n"
	#endif
		"  -R, --resume                      Resume\n"
		"  -S, --soft                        Upload soft link target file\n"
		"  -H, --hide-args                   Hidden cmd args\n"
		"  -?                                display this help and exit\n"
		"example:\n"
		"  %s -h host [-p port] [-u user] [-w password] -m get -r remote -l local [-o file]" USAGE_FTP_SSL " [-d]\n"
		"  %s -h host [-p port] [-u user] [-w password] -m put -r remote -l local [-o file]" USAGE_FTP_SSL " [-d]\n"
		"  %s -h host [-p port] [-u user] [-w password] -m del -r remote [-o file]" USAGE_FTP_SSL " [-d]\n"
		"  %s -h host [-p port] [-u user] [-w password] -m rls -r remote [-o file]" USAGE_FTP_SSL " [-d]\n"
		"  %s -h host [-p port] [-u user] [-w password] -i file [-o file]" USAGE_FTP_SSL " [-d]\n"
		, prog, prog, prog, prog, prog, prog);
}

typedef int (*list_func_t)(ftpbuf_t *ftp, const char *local, const char *remote, const char *method, char type, unsigned long int size, const char *name, const char *link);

int ftplist(ftpbuf_t *ftp, const char *local, const char *remote, const char *method, list_func_t func) {
	char *path = NULL;
	char **lines;
	char *line;
	int len;

	char perms[11]="";
	int number, owner, group;
	unsigned long int size;
	char dt1[4]="", dt2[3]="", dt3[6]="", datetime[13]="", name[256]="", link[256]="";
	char *ptr;

	int tries = -1;

	if(*remote) {
		len = asprintf(&path, "-a %s", remote);
	} else {
		len = asprintf(&path, "-a");
	}
	trylst:
	tries++;
	lines = ftp_list(ftp, path, len, 0);
	if(!lines) {
		if(tries < TRIES && ftp_reconnect(ftp)) goto trylst;
		fprintf(stderr, "The directory is not found for \"%s\"\n", remote);
		free(path);
		
		nTRIES += tries;

		if(outFd) fprintf(outFd, "LST\t%s\t%s\t%s\n", local?local:"", remote, method);
		return 1;
	}

	nTRIES += tries;

	int ret = 0, ret2, n = 0;
	while((line=lines[n++]) != NULL) {
		if(line[0] == 'd' && (ptr = strrchr(line, ' ')) && (!strcmp(ptr+1, ".") || !strcmp(ptr+1, ".."))) {
			continue;
		}
		ret2 = sscanf(line, "%[^ ] %d %d %d %lu %s %s %s %n", perms, &number, &owner, &group, &size, dt1, dt2, dt3, &len);
		if(ret2 != 8) {
			if(ret2 == 2) continue;

			ret2 = sscanf(line, "%[^ ] %d %*[^ ] %*[^ ] %lu %s %s %s %n", perms, &number, &size, dt1, dt2, dt3, &len);
			if(ret2 != 6) {
				if(outFd) fprintf(outFd, "LST\t%s\t%s\t%s\n", local?local:"", remote, method);
				ret = 1;
				break;
			}
			owner = 1000;
			group = 1000;
		}
		if(line[0] == 'l' && (ptr = strstr(line+len, " -> "))) {
			strncpy(name, line+len, ptr-line-len);
			name[ptr-line-len] = '\0';
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
		if(ret || ((ret = func(ftp, local, remote, method, perms[0], size, name, link)) && (perms[0] != 'd' || ret == 2))) {
			ret = 1;
			if(outFd) fprintf(outFd, "ITEM\t%s\t%s\t%s\t%c\t%lu\t%s\t%s\n", local?local:"", remote, method, perms[0], size, name, link);
		}
	}

	free(path);
	free(lines);

	return ret;
}

int ftpget_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *method, char type, unsigned long int size, const char *name, const char *link) {
	printf("%c: %s/%s <= %s/%s\n", type, local, name, remote, name);

	char *plocal = NULL, *premote = NULL;
	struct stat st;
	int i, ret = 0;

	asprintf(&plocal, "%s/%s", local, name);
	asprintf(&premote, "%s/%s", remote, name);

	int tries = -1;

	tryget:
	tries++;
	i = lstat(plocal, &st);

	if(type == 'd') {
		if((i==0 && S_ISDIR(st.st_mode)) || (i == -1 && mkdir(plocal, 0755) == 0)) {
			ret = ftplist(ftp, plocal, premote, method, ftpget_func);
		} else if(i==0) {
			fprintf(stderr, "%s is not a directory\n", plocal);
			ret = 2;
		} else {
			fprintf(stderr, "Failed to create directory %s\n", plocal);
			ret = 2;
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
		if((i==0 && S_ISREG(st.st_mode) && (!resume || st.st_size != size || (resume && size>0 && ftp_mdtm(ftp, premote, strlen(premote)) > st.st_mtime))) || i == -1) {
			FILE *fp = fopen(plocal, resume && i==0 && st.st_size < size ? "a" : "w");
			if(fp) {
				ftp_set_total(ftp, size, resume && i==0 && st.st_size < size ? st.st_size : 0, 0);
				if(!ftp_get(ftp, fp, premote, strlen(premote), FTPTYPE_IMAGE, resume && i==0 && st.st_size < size ? st.st_size : 0)) {
					if(tries < TRIES && ftp_reconnect(ftp)) {
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
	long size;
	struct stat st;
	FILE *fp;
	int ret = 0;
	int tries = -1;

	lstat(local, &st);

	if(S_ISDIR(st.st_mode)) {
		trymkdir:
		tries++;
		if(*remote && !ftp_chdir(ftp, remote, strlen(remote)) && !ftp_mkdir(ftp, remote, strlen(remote))) {
			if(tries < TRIES && ftp_reconnect(ftp)) goto trymkdir;
			nTRIES += tries;
			if(outFd) fprintf(outFd, "PUT\t%s\t%s\n", local, remote);
			fprintf(stderr, "Failed to create remote directory %s\n", remote);
			return 1;
		}
		nTRIES += tries;

		if(*local || *remote) printf("d: %s => %s\n", local, remote);

		struct dirent *dir;

		DIR *dh = opendir(local);
		if(!dh) {
			if(outFd) fprintf(outFd, "PUT\t%s\t%s\n", local, remote);
			fprintf(stderr, "Failed to open directory %s\n", local);
			return 1;
		}

		char *plocal, *premote;
		while((dir=readdir(dh))) {
			if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) {
				continue;
			}

			plocal = NULL;
			premote = NULL;

			asprintf(&plocal, "%s/%s", local, dir->d_name);
			asprintf(&premote, "%s/%s", remote, dir->d_name);

			if(ret) {
				if(outFd) fprintf(outFd, "PUT\t%s\t%s\n", plocal, premote);
			} else {
				ret = ftpput(ftp, plocal, premote);
			}

			free(plocal);
			free(premote);
		}

		closedir(dh);

		return ret;
	} else if(S_ISREG(st.st_mode)) {
		printf("-: %s => %s\n", local, remote);

		size = resume ? ftp_size(ftp, remote, strlen(remote)) : -1;
		fp = NULL;
		tryput:
		tries++;
		if(size == 0)
			ftp_delete(ftp, remote, strlen(remote));
		if(st.st_size == size) {
			if(size>0 && ftp_mdtm(ftp, remote, strlen(remote)) < st.st_mtime) {
				size = 0;
				goto tryput;
			}
		} else if((fp=fopen(local, "r"))) {
			ftp_set_total(ftp, st.st_size, 0, st.st_size > size && size > 0 ? size : 0);
			if(!ftp_put(ftp, remote, strlen(remote), fp, FTPTYPE_IMAGE, st.st_size > size && size > 0 ? size : 0)) {
				if(tries < TRIES && ftp_reconnect(ftp)) {
					fclose(fp);
					fp = NULL;
					size = resume ? ftp_size(ftp, remote, strlen(remote)) : 0;
					goto tryput;
				}
				fprintf(stderr, "Upload file %s failed\n", local);
				ret = 1;
			}
			fclose(fp);
			nTRIES += tries;
		} else {
			fprintf(stderr, "fopen %s file failed\n", local);
			ret = 1;
		}
	} else if(is_soft && S_ISLNK(st.st_mode)) {
		char buf[PATH_MAX];
		if(readlink(local, buf, PATH_MAX) > 0) {
			if(ftpput(ftp, buf, remote)) {
				fprintf(stderr, "skip %s -> %s => %s\n", local, buf, remote);
			}
		} else {
			fprintf(stderr, "readlink %s failed\n", local);
			ret = 1;
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
		printf("%c: %s => %s\n", type, local, remote);
		fprintf(stderr, "File %s is an unsupported local file type\n", local);
		ret = 1;
	}

	if(ret && outFd) fprintf(outFd, "PUT\t%s\t%s\n", local, remote);

	return ret;
}

int ftpremove_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *method, char type, unsigned long int size, const char *name, const char *link) {
	printf("%c: %s/%s\n", type, remote, name);

	char *premote = NULL;
	int ret = 0;
	int tries = -1;

	asprintf(&premote, "%s/%s", remote, name);

	if(type == 'd') {
		ret = ftplist(ftp, local, premote, method, ftpremove_func);
		tryrmdir:
		tries++;
		if(!ret && !ftp_rmdir(ftp, premote, strlen(premote))) {
			if(tries < TRIES && ftp_reconnect(ftp)) goto tryrmdir;
			fprintf(stderr, "Deletion of directory %s failed\n", premote);
			ret = 2;
		}
	} else {
		trydel:
		tries++;
		if(!ftp_delete(ftp, premote, strlen(premote))) {
			if(tries < TRIES && ftp_reconnect(ftp)) goto trydel;
			fprintf(stderr, "Deletion of file %s failed\n", premote);
			ret = 1;
		}
	}

	free(premote);

	nTRIES += tries;

	return ret;
}

int ftplist_func(ftpbuf_t *ftp, const char *local, const char *remote, const char *method, char type, unsigned long int size, const char *name, const char *link) {
	char *premote = NULL;
	int ret = 0;

	asprintf(&premote, "%s/%s", remote, name);

	if(type == 'd') {
		printf("%c: %s/%s\n", type, remote, name);
		ret = ftplist(ftp, local, premote, method, ftplist_func);
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
	int pasv = 1;
	char *host = NULL, *user = strdup("anonymous"), *password = strdup("anonymous"), *method = NULL, *local = NULL, *remote = NULL;
	char *inFile = NULL, *outFile = NULL;

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
				free(user);
				user = strdup((const char *)optarg);
				break;
			case 'w':
				free(password);
				password = strdup((const char *)optarg);
				{
					char *p = (char*) optarg;
					while(*p) *p++ = '*';
				}
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
			case 'i':
				inFile = strdup((const char *)optarg);
				break;
			case 'o':
				outFile = strdup((const char *)optarg);
				break;
			case 'd':
				debug = 1;
				break;
			case 'P':
				pasv = 0;
				break;
		#ifdef HAVE_FTP_SSL
			case 's':
				use_ssl = 1;
				break;
		#endif
			case 'R':
				resume = 1;
				break;
			case 'S':
				is_soft = 1;
				break;
			case 'H':
				hide_argv = 1;
				break;
			case '?':
			default:
				usage(argv[0]);
				return 1;
		}
	}

	if(!host || port<=0 || (!inFile && !method) || (method && strcmp(method, "get") && strcmp(method, "put") && strcmp(method, "rls") && strcmp(method, "del")) || (method && !remote) || (method && !local && strcmp(method, "rls") && strcmp(method, "del"))) {
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
			"  inFile  = %s\n"
			"  outFile  = %s\n"
			"  TRIES    = %d\n"
			"  timeout  = %d\n"
			"  debug  = %d\n"
			"  pasv  = %d\n"
		#ifdef HAVE_FTP_SSL
			"  ssl      = %d\n"
		#endif
			"  resume   = %d\n"
			"  is_soft   = %d\n"
			, host?host:"", port, user?user:"", password?password:"", method?method:"", local?local:"", remote?remote:"", inFile?inFile:"", outFile?outFile:"", TRIES, timeout, debug, pasv
		#ifdef HAVE_FTP_SSL
			, use_ssl
		#endif
			, resume, is_soft
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
		exit_status = 1;
		goto optEnd;
	}

	ftp->usepasvaddress = 1;
#ifdef HAVE_FTP_SSL
	ftp->use_ssl = use_ssl;
#endif
	if(!ftp_login(ftp, user, strlen(user), password, strlen(password))) {
		fprintf(stderr, "user %s login failure\n", user);
		exit_status = 1;
		goto ftpQuit;
	}

	if(!ftp_pasv(ftp, pasv)) {
		fprintf(stderr, "switches passive mode %s failure\n", pasv ? "on" : "off");
	}

	if(remote) { // {{{ format remote
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
	} // }}}
	
	if(local) {
		char *ptr = local + strlen(local) - 1;
		if(local != ptr && *ptr == '/') *ptr = '\0';
	}

	if(outFile) {
		outFd = fopen(outFile, "w");
		if(!outFd) {
			fprintf(stderr, "fopen %s file failed\n", outFile);
			exit_status = 1;
			goto ftpQuit;
		}
	}

	if(inFile) {
		FILE *inFd = fopen(inFile, "r");
		if(inFd) {
			char *func, *plocal, *premote, *pmethod;
			char type;
			unsigned long int size;
			char *name, *link;
			char line[16*1024];
			char *l;
			char *fmt = "ssssclss";
			char *f;
			char** args[8] = {&func, &plocal, &premote, &pmethod, (char**)&type, (char**)&size, &name, &link};
			char *p, *endptr = NULL;
			int ret = 0;

			while(!feof(inFd) && fgets(line, sizeof(line), inFd)) {
				l = line;
				for(f=fmt,i=0; *l && *f; f++,i++) {
					p = l;
					while(*l && *l != '\t' && *l != '\n') l++;
					if(*l) *l++ = '\0';
					switch(*f) {
						case 's':
							*args[i] = p;
						break;
						case 'c':
							*((char*)args[i]) = *p;
						break;
						case 'l':
							endptr = NULL;
							*((unsigned long int*)args[i]) = strtol(p, &endptr, 10);
						break;
					}
				}
				for(c=i; *f; f++,c++) {
					switch(*f) {
						case 's':
							*args[c] = NULL;
						break;
						case 'c':
							*((char*)args[c]) = '\0';
						break;
						case 'l':
							*((unsigned long int*)args[c]) = 0;
						break;
					}
				}
				// fprintf(stderr, "i = %d, func = %s, plocal = %s, premote = %s, pmethod = %s, type = %c, size = %lu, name = %s, link = %s\n", i, func, plocal, premote, pmethod, type, size, name, link);
				if(i == 8) {
					if(strcmp(func, "ITEM")) {
						fprintf(stderr, "Input file error, Not usage function is %s in 8 columns\n", func);
						ret = 1;
					} else {
						c = type != 'd';
						if(ret) {
							c = 1;
						} else if(!strcmp(pmethod, "GET")) {
							ret = ftpget_func(ftp, plocal, premote, pmethod, type, size, name, link);
						} else if(!strcmp(pmethod, "DEL")) {
							ret = ftpremove_func(ftp, plocal, premote, pmethod, type, size, name, link);
						} else if(!strcmp(pmethod, "RLS")) {
							ret = ftplist_func(ftp, plocal, premote, pmethod, type, size, name, link);
						} else {
							fprintf(stderr, "Input file error, Not usage method is %s in 8 columns\n", pmethod);
							ret = 1;
							c = 0;
						}
						if(ret && (c || ret == 2) && outFd) {
							ret = 1;
							fprintf(outFd, "%s\t%s\t%s\t%s\t%c\t%lu\t%s\t%s\n", func, plocal, premote, pmethod, type, size, name, link);
						}
					}
				} else if(i == 4) {
					if(strcmp(func, "LST")) {
						fprintf(stderr, "Input file error, Not usage function is %s in 4 columns\n", func);
						ret = 1;
					} else {
						c = 0;
						if(ret) {
							c = 1;
						} else if(!strcmp(pmethod, "GET")) {
							struct stat st;
							i = lstat(plocal, &st);
							if(i != 0 && mkdir(plocal, 0755) != 0) {
								fprintf(stderr, "Failed to create directory %s\n", plocal);
								ret = 2;
							} else {
								ret = ftplist(ftp, plocal, premote, pmethod, ftpget_func);
							}
						} else if(!strcmp(pmethod, "DEL")) {
							ret = ftplist(ftp, plocal, premote, pmethod, ftpremove_func);
						} else if(!strcmp(pmethod, "RLS")) {
							ret = ftplist(ftp, plocal, premote, pmethod, ftplist_func);
						} else {
							fprintf(stderr, "Input file error, Not usage method is %s in 4 columns\n", pmethod);
							ret = 1;
						}
						if((c || ret == 2) && outFd) {
							ret = 1;
							fprintf(outFd, "%s\t%s\t%s\t%s\n", func, plocal, premote, pmethod);
						}
					}
				} else if(i == 3) {
					if(strcmp(func, "PUT")) {
						fprintf(stderr, "Not usage function is %s\n", func);
						ret = 1;
					} else {
						if(ret) {
							if(outFd) fprintf(outFd, "%s\t%s\t%s\n", func, plocal, premote);
						} else {
							ret = ftpput(ftp, plocal, premote);
						}
					}
				} else {
					fprintf(stderr, "parse error: %d columns\n", i);
					ret = 1;
				}
			}

			fclose(inFd);

			exit_status = ret;
		} else {
			fprintf(stderr, "fopen %s file failed\n", inFile);
			exit_status = 1;
		}
	} else if(!strcmp(method, "get")) {
		struct stat st;

		i = lstat(local, &st);
		if(i == 0 && !S_ISDIR(st.st_mode)) {
			fprintf(stderr, "%s is not a directory\n", local);
			exit_status = 1;
		} else {
			if(i != 0 && mkdir(local, 0755) != 0) {
				exit_status = fprintf(stderr, "Failed to create directory %s\n", local);
			} else {
				exit_status = ftplist(ftp, local, remote, "GET", ftpget_func);
			}
		}
	} else if(!strcmp(method, "put")) {
		exit_status = ftpput(ftp, local, remote);
	} else if(!strcmp(method, "rls")) {
		if(*remote) printf("d: %s\n", remote);
		exit_status = ftplist(ftp, NULL, remote, "RLS", ftplist_func);
	} else {
		if(*remote) printf("d: %s\n", remote);
		if(ftplist(ftp, NULL, remote, "DEL", ftpremove_func) || (*remote && !ftp_rmdir(ftp, remote, strlen(remote)))) {
			fprintf(stderr, "Deletion of directory %s failed\n", remote);
			exit_status = 1;
		}
	}

	if(outFd) fclose(outFd);

ftpQuit:
	if(ftp->reconnect) fprintf(stderr, "Reconnected times is %d\n", ftp->reconnect);
	ftp_quit(ftp);
	ftp_close(ftp);

optEnd:
	{
		char *frees[8] = {host, user, password, method, local, remote, inFile, outFile};
		for(i=0; i<sizeof(frees)/sizeof(char*); i++) {
			if(frees[i]) {
				free((void*) frees[i]);
			}
		}
	}

	if(exit_status && errno) fprintf(stderr, "Error: %s\n", strerror(errno));
	fprintf(stderr, "Total try times is %lu\n", nTRIES);

	return exit_status;
}
