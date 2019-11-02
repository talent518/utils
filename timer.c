#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
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
}

void timer_thread(union sigval v) {
	static int i = 0;
	printf("%s - int: %d, %d\n", gettimeofstr(), v.sival_int, ++i);
}

int main() {
	timer_t timerid;
	struct sigevent evp;
	memset(&evp, 0, sizeof(struct sigevent));       //清零初始化

	evp.sigev_value.sival_int = 111;                //也是标识定时器的，回调函数可以获得
	evp.sigev_notify = SIGEV_THREAD;                //线程通知的方式，派驻新线程
	evp.sigev_notify_function = timer_thread;       //线程函数地址

	if (timer_create(CLOCK_REALTIME, &evp, &timerid) == -1) {
		perror("fail to timer_create");
		exit(-1);
	}

	/* 第一次间隔it.it_value这么长,以后每次都是it.it_interval这么长,就是说it.it_value变0的时候会装载it.it_interval的值 */
	struct itimerspec it;
	it.it_interval.tv_sec = 1;
	it.it_interval.tv_nsec = 0;
	it.it_value.tv_sec = 2;
	it.it_value.tv_nsec = 0;

	if (timer_settime(timerid, 0, &it, NULL) == -1) {
		perror("fail to timer_settime");
		exit(-1);
	}

	signal(SIGINT, signal_handler);
	printf("%s - started\n", gettimeofstr());
	sleep(-1);
	printf("%s - stopped\n", gettimeofstr());

	return 0;
}
/*
 * int timer_gettime(timer_t timerid, struct itimerspec *curr_value);
 * 获取timerid指定的定时器的值，填入curr_value
 */
