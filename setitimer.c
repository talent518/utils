#include <stdio.h>
#include <string.h>
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

void signal_handler(int sig) {
	if (sig == SIGINT)
		setitimer(ITIMER_REAL, NULL, NULL); // 取消定时器

	printf("%s - sig: %d, %s\n", gettimeofstr(), sig, strsignal(sig));
}

int main(int argc, char *argv[]) {
	signal(SIGALRM, signal_handler);
	signal(SIGINT, signal_handler);

	struct itimerval itv;

	// Interval for periodic timer
	// 周期计时器间隔，每次SIGALRM信号事件的时间间隔
	itv.it_interval.tv_sec = 2;
	itv.it_interval.tv_usec = 0;

	// Time until next expiration
	// 下次到期前的时间，首次SIGALRM信号事件的时间间隔
	itv.it_value.tv_sec = 1;
	itv.it_value.tv_usec = 0;

	setitimer(ITIMER_REAL, &itv, NULL);

	printf("Press enter key to exit\n");
	printf("%s - started\n", gettimeofstr());
	getchar();
	printf("%s - stopped\n", gettimeofstr());
}
