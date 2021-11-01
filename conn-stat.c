#include <stdio.h>
#include <string.h>

static const char *const states[] = {
	"",
	"ESTABLISHED",
	"SYN_SENT",
	"SYN_RECV",
	"FIN_WAIT1",
	"FIN_WAIT2",
	"TIME_WAIT",
	"CLOSE",
	"CLOSE_WAIT",
	"LAST_ACK",
	"LISTEN",
	"CLOSING"
};

typedef struct {
    int port;
    int n[12];
} listen_t;

static listen_t listens[128];
static const int nlisten = sizeof(listens)/sizeof(listens[0]);

void do_stat(const char *file) {
    char line[2048];
    char localaddr[48], remaddr[48];
    int id, localport, remport, state;
    int n = 0, i;
    FILE *fp;

    fp = fopen(file, "r");
    if(!fp) return;

    while(fgets(line, sizeof(line), fp)) {
        if(n++ < 1) continue;

        if(sscanf(line, "%d: %[^:]:%X %[^:]:%X %X", &id, localaddr, &localport, remaddr, &remport, &state) != 6) continue;

        // printf("%d:%d %s\n", localport, remport, states[state]);

        for(i=0; i<nlisten; i++) {
            if(listens[i].port == 0) {
                if(remport == 0 && state == 10) {
                    listens[i].port = localport;
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

void main(int argc, char *argv[]) {
    int i, j;
    memset(listens, 0, sizeof(listens));

    do_stat("/proc/net/tcp");
    do_stat("/proc/net/tcp6");

    printf("%12s", "LISTEN_PORT");
    for(j=1; j<12; j++) {
        if(j!=10) printf("%12s", states[j]);
    }
    printf("\n");

    for(i=0; i<nlisten && listens[i].port; i++) {
        printf("%12d", listens[i].port);
        for(j=1; j<12; j++) {
            if(j!=10) printf("%12d", listens[i].n[j]);
        }
        printf("\n");
    }
}
