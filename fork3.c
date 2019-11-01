#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

/**
 * IFDEFUNCT
 *
 * 为1：表示僵尸进程(子进程先结束且父进程没有使用wait函数的函数时，子进程就会变成僵尸进程)
 * 为0：类似nohup的方式，父进程先结束，子进程继续运行，shell不会等待子进程的结束
 */
#define IFDEFUNCT 1

int main(int argc, char *argv[]) {
	pid_t pid = fork();

	if (pid > 0) {
		printf("pid: %d,%d\n", getpid(), getppid());
#if IFDEFUNCT
		sleep(1000);
#endif
	} else if (pid == 0) {
#if IFDEFUNCT == 0
		sleep(1000);
#endif
		printf("pid: %d,%d\n", getpid(), getppid());
	} else
		perror("fork error");

	return 0;
}
