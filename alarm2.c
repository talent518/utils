#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

static const char *gettimeofstr() {
	static char outstr[16];
	time_t t;
	struct tm *tmp;

	t = time(NULL);
	tmp = localtime(&t);
	if (tmp == NULL) {
		perror("localtime");
		return "";
	}

	if (strftime(outstr, sizeof(outstr), "%T", tmp) == 0) {
		fprintf(stderr, "strftime returned 0");
		return "";
	}

	return outstr;
}

void sig_alarm(int sig) {
	alarm(1);
	printf("%s - sig: %d, %s\n", gettimeofstr(), sig, strsignal(sig));
}

int main(int argc, char *argv[]) {
	signal(SIGALRM, sig_alarm);
	signal(SIGINT, sig_alarm);
	alarm(1);

	printf("Press enter key to exit\n");
	getchar();
}
