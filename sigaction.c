#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

void sig_catch(int sig) {
	printf("catch signal!! %d, %s\n", sig, strsignal(sig));
	sleep(10);
}

int main(int argc, char *argv[]) {
	struct sigaction act;

	act.sa_handler = sig_catch;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);

	if (argc > 1)
		sigaddset(&act.sa_mask, SIGQUIT);

	sigaction(SIGINT, &act, NULL);

	getchar();
}
