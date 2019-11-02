#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>

static bool running = true;

void sig_alarm(int sig) {
	running = false;
}

int main(int argc, char *argv[]) {
	int i;
	signal(SIGALRM, sig_alarm);
	alarm(1);
	for(i=0;running;i++) {
		printf("%d\n", i);
	}
}
