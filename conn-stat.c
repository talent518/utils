#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <dirent.h>
#include <signal.h>
#include <sys/time.h>
#include <time.h>

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

static bool is_running = true;
void signal_handle(int sig) {
    switch(sig) {
        case SIGALRM:
            break;
        default:
            is_running = false;
            break;
    }
}

int main(int argc, char *argv[]) {
    char bufs[2][16384], *p, *p2;
    int rows = 0, times = 0, is_tty;

    int i, j;
    int c;
    bool is_prog = 0, is_udp = 0, is_verb = 0, interval = 0;

    while((c = getopt(argc, argv, "puvi:h?")) != -1) {
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
            case 'i':
                interval = atoi(optarg);
                break;
            default:
                fprintf(stderr,
                    "Usage: %s [-p] [-u] [-v] [-i <second>] [-h|-?]\n"
                    "  -p          Show pid and program\n"
                    "  -u          Show udp listen(default: tcp)\n"
                    "  -v          Show verbose\n"
                    "  -i <second> Interval seconds(default: %d)\n"
                    "  -h,-?       This help\n"
                    , argv[0], interval
                );
                return 0;
        }
    }

    is_tty = (interval > 0 && isatty(1));

    if(interval > 0) {
        struct itimerval itv;

        itv.it_interval.tv_sec = itv.it_value.tv_sec = interval;
        itv.it_interval.tv_usec = itv.it_value.tv_usec = 0;
        setitimer(ITIMER_REAL, &itv, NULL);

        signal(SIGALRM, signal_handle);
        signal(SIGINT, signal_handle);
        signal(SIGTERM, signal_handle);
        signal(SIGPIPE, SIG_IGN);
    }

retry:
    memset(listens, 0, sizeof(listens));

    if(is_verb) printf("%10s %8s %11s %s\n", "LOCAL-PORT", "REM-PORT", "STATE", "INODE");

    if(is_udp) {
        do_stat("/proc/net/udp", is_udp, is_verb);
        do_stat("/proc/net/udp6", is_udp, is_verb);
    } else {
        do_stat("/proc/net/tcp", is_udp, is_verb);
        do_stat("/proc/net/tcp6", is_udp, is_verb);
    }

    if(is_verb) {
        if(is_running && interval > 0) {
            sleep(interval);
            goto retry;
        }
        return 0;
    }

    if(is_prog) do_prog("/proc", 0);

    if(is_tty) {
        if(times > 0) fprintf(stdout, "\033[%dF", rows);
    } else {
        if(times > 0) fprintf(stdout, "\n");
    }

    memset(bufs[0], 0, sizeof(bufs[0]));

    p = bufs[0];
    rows = 0;

    if(interval > 0) {
        time_t t = time(NULL);
        struct tm tm;

        localtime_r(&t, &tm);

        rows ++;
        i = sprintf(p, "interval %d seconds", interval);
        p += i;
        for(; i < 88 + (is_prog ? 16 : 0); i ++) *p++ = ' ';
        p += sprintf(p, "%02d:%02d:%02d\n", tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    rows ++;
    p += sprintf(p,"%8s", is_udp ? "UDP-PORT" : "TCP-PORT");
    for(j=1; j<12; j++) {
        if(j!=10) p += sprintf(p, " %s", states[j]);
    }
    if(is_prog) p += sprintf(p,"%8s %s\n", "PID", "PROGRAM");
    else p += sprintf(p,"\n");

    for(i=0; i<nlisten && listens[i].port; i++) {
        p += sprintf(p, "%8d", listens[i].port);
        for(j=1; j<12; j++) {
            if(j!=10) p += sprintf(p, formats[j], listens[i].n[j]);
        }
        if(is_prog) {
            p += sprintf(p, "%8d %s\n", listens[i].pid, listens[i].prog ? listens[i].prog : "-");
            if(listens[i].prog) free((void*) listens[i].prog);
        } else p += sprintf(p, "\n");
        rows ++;
    }

    if(is_tty) {
        if(times > 0) {
            p2 = bufs[1];
            p = bufs[0];
            while(*p) {
                if(*p != *p2) {
                    fprintf(stdout, "\033[47;30m");
                    while(*p != *p2) {
                        fputc(*p, stdout);
                        p++;
                        p2++;
                    }
                    fprintf(stdout, "\033[0m");
                }
                fputc(*p, stdout);
                p++;
                p2++;
            }
        } else {
            fwrite(bufs[0], 1, p - bufs[0], stdout);
        }
        memcpy(bufs[1], bufs[0], sizeof(bufs[0]));
    } else {
        fwrite(bufs[0], 1, p - bufs[0], stdout);
    }
    fflush(stdout);

    times ++;

    if(is_running && interval > 0) {
        sleep(interval);
        goto retry;
    }

    return 0;
}
