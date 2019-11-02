#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

#define MICRO_IN_SEC 1000000.00

static double microtime() {
	struct timeval tp = { 0 };

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double) (tp.tv_sec + tp.tv_usec / MICRO_IN_SEC);
}

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

static bool running = true;
static double sleep_before = 0;

void signal_handler(int sig) {
	if (sig == SIGCHLD)
		running = false;

	printf("%s - time: %lf, sig: %d, %s\n", gettimeofstr(), microtime() - sleep_before, sig, strsignal(sig));
}

int main(int argc, char *argv[]) {
	pid_t pid = fork();

	if (pid < 0) {
		perror("fork error");
	} else if (pid > 0) {
		signal(SIGUSR1, signal_handler);
		signal(SIGCHLD, signal_handler); // 无此行时子进程结束时主进程还继续运行，默认系统会忽略SIGCHLD信号
		printf("%s - parent process: %d\n", gettimeofstr(), getpid());
		int i = 0, n;
		char buf[128];
		while (running) {
			sleep_before = microtime();
#if 1
			sleep(5); // 信号产生时会提前终止sleep而去执行信号处理函数
#else
			// 默认自动重启动的系统调用包括：ioctl(),read(),readv(),write(),writev(),wait(),waitpid();其中前5个函数只有在对低速设备进行操作时才会被信号中断。而wait和waitpid在捕捉到信号时总是被中断。
			n = read(STDIN_FILENO, buf, sizeof(buf)); // 信号来临时不会提前终止read，是由系统重启调用该函数了。
			write(STDOUT_FILENO, "buf: ", 5);
			write(STDOUT_FILENO, buf, n);
#endif
			printf("%s - i: %d\n", gettimeofstr(), ++i);
		}
	} else {
		sleep(3);
		printf("%s - child process: %d, %d\n", gettimeofstr(), getpid(), getppid());
		kill(getppid(), SIGUSR1);
		sleep(3);
	}

	return 0;
}
