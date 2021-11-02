#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>

static const char *const formats[] = {
	NULL,
	"%6d",
	"%9d",
	"%9d",
	"%10d",
	"%10d",
	"%10d",
	"%6d",
	"%11d",
	"%9d",
	NULL,
	"%8d"
};

static const char *const states[] = {
	NULL,
	"ESTAB",
	"SYN-SENT",
	"SYN-RECV",
	"FIN-WAIT1",
	"FIN-WAIT2",
	"TIME-WAIT",
	"CLOSE",
	"CLOSE-WAIT",
	"LAST-ACK",
	"LISTEN",
	"CLOSING"
};

typedef struct {
    int port;
    int n[12];
    unsigned long inode;
    int pid;
    const char *prog;
} listen_t;

static listen_t listens[128];
static const int nlisten = sizeof(listens)/sizeof(listens[0]);

void do_stat(const char *file, int is_udp, int is_verb) {
    char line[2048];
    char localaddr[48], remaddr[48];
    int id, localport, remport, state, uid;
    unsigned long rxq, txq, inode;
    int n = 0, i;
    FILE *fp;

    fp = fopen(file, "r");
    if(!fp) return;

    while(fgets(line, sizeof(line), fp)) {
        if(n++ < 1) continue;

        if(sscanf(line, "%d: %[^:]:%X %[^:]:%X %X %lX:%lX %*X:%*X %*X %d %*d %lu", &id, localaddr, &localport, remaddr, &remport, &state, &txq, &rxq, &uid, &inode) != 10) continue;

        if(is_verb) {
            printf("%10d %8d %11s %lu\n", localport, remport, is_udp && state == 7 ? "LISTEN" : states[state], inode);
            continue;
        }

        for(i=0; i<nlisten; i++) {
            if(listens[i].port == 0) {
                if(remport == 0) {
                    listens[i].port = localport;
                    listens[i].inode = inode;
                }
                break;
            } else if(listens[i].port == localport) {
                if(state != 10) listens[i].n[state]++;
                break;
            }
        }
    }
    fclose(fp);
}

void do_prog(const char *path, int pid) {
    char file[PATH_MAX];
    DIR *dir = opendir(path);
    struct dirent *d;
    int id, i, n;
    unsigned long inode;
    FILE *fp;
    char *p, *prog;

    if(!dir) return;

    while((d = readdir(dir)) != NULL) {
        if(!isdigit(d->d_name[0])) continue;

        id = atoi(d->d_name);
        if(pid > 0) {
            snprintf(file, sizeof(file), "%s/%d", path, id);
            if(readlink(file, file, sizeof(file)) > 0) {
                inode = 0;
                if(sscanf(file, "socket:[%lu]", &inode) != 1) continue;

                n = 0;
                for(i=0; i<nlisten && listens[i].port; i++) {
                    if(listens[i].inode == inode) {
                        listens[i].pid = pid;
                        n++;
                    }
                }
                if(n == 0) continue;

                prog = NULL;
                snprintf(file, sizeof(file), "/proc/%d/comm", pid);
                if((fp = fopen(file, "r")) != NULL) {
                    if(fgets(file, sizeof(file), fp)) {
                        p = file + strlen(file) - 1;
                        while(p >= file && isspace(*p)) *p-- = '\0';
                        prog = file;
                    }
                    fclose(fp);
                }
                if(prog == NULL) continue;

                for(i=0; i<nlisten && listens[i].port; i++) {
                    if(listens[i].inode == inode) {
                        listens[i].prog = strdup(prog);
                    }
                }
            }
        } else {
            snprintf(file, sizeof(file), "%s/%d/fd", path, id);
            do_prog(file, id);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    int i, j;
    int c;
    int is_prog = 0, is_udp = 0, is_verb = 0;

    while((c = getopt(argc, argv, "puvh?")) != -1) {
        switch(c) {
            case 'p':
                is_prog = 1;
                break;
            case 'u':
                is_udp = 1;
                break;
            case 'v':
                is_verb = 1;
                break;
            default:
                fprintf(stderr,
                    "Usage: %s options\n"
                    "  -p      Show pid and program\n"
                    "  -u      Show udp listen(default: tcp)\n"
                    "  -v      Show verbose\n"
                    "  -h,-?   This help\n"
                    , argv[0]
                );
                return 0;
        }
    }

    memset(listens, 0, sizeof(listens));

    if(is_verb) printf("%10s %8s %11s %s\n", "LOCAL-PORT", "REM-PORT", "STATE", "INODE");

    if(is_udp) {
        do_stat("/proc/net/udp", is_udp, is_verb);
        do_stat("/proc/net/udp6", is_udp, is_verb);
    } else {
        do_stat("/proc/net/tcp", is_udp, is_verb);
        do_stat("/proc/net/tcp6", is_udp, is_verb);
    }

    if(is_verb) return 0;

    if(is_prog) do_prog("/proc", 0);

    printf("%8s", is_udp ? "UDP-PORT" : "TCP-PORT");
    for(j=1; j<12; j++) {
        if(j!=10) printf(" %s", states[j]);
    }
    if(is_prog) printf("%8s %s\n", "PID", "PROGRAM");
    else printf("\n");

    for(i=0; i<nlisten && listens[i].port; i++) {
        printf("%8d", listens[i].port);
        for(j=1; j<12; j++) {
            if(j!=10) printf(formats[j], listens[i].n[j]);
        }
        if(is_prog) {
            printf("%8d %s\n", listens[i].pid, listens[i].prog ? listens[i].prog : "-");
            if(listens[i].prog) free((void*) listens[i].prog);
        } else printf("\n");
    }

    return 0;
}
