#include <stdio.h>
#include <signal.h>
#include <unistd.h>

void print_sigset(sigset_t *set) {
	int i;
	for (i = 1; i <= 31; i++) {
		putchar(sigismember(set, i) ? '1' : '0');
	}
	putchar('\n');
}

int main(int argc, char *argv[]) {
	sigset_t set;

	sigemptyset(&set);

	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGILL);
	sigaddset(&set, SIGTRAP);
	sigaddset(&set, SIGABRT);
	sigaddset(&set, SIGBUS);
	sigaddset(&set, SIGFPE);
	sigaddset(&set, SIGKILL); // KILL和STOP设置了是没有用的，这个系统保护的
	sigaddset(&set, SIGSTOP);

	print_sigset(&set);

	sigprocmask(SIG_BLOCK, &set, NULL);

	printf("pid: %d\n", getpid());

	while(1) {
		sleep(1);
		sigpending(&set);
		print_sigset(&set);
	}

	return 0;
}
