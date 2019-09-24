#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void *thread_handler(void*arg) {
	sleep(1);
	
	printf("%s: pid: %d\n", __func__, getpid());
	
	pid_t pid = fork();
	if(pid > 0) {
		wait(NULL);
		
		printf("child process exit2\n");
		printf("===================\n");
	} else if(pid < 0)
		perror("fork");
	else {
		printf("====================\n");
		printf("child process start2\n");
	}
	
	sleep(1);
	
	printf("%s: pid: %d, getpid: %d\n", __func__, pid, getpid());
	
	if(pid != 0) pthread_exit(NULL);
	
	return NULL;
}

void mkproc() {
	pthread_t thread;
	pthread_create(&thread, NULL, thread_handler, NULL);

	pid_t pid = fork();
	if(pid > 0) {
		wait(NULL);
		printf("child process exit\n");
		printf("==================\n");
	} else if(pid < 0)
		perror("fork");
	else {
		printf("===================\n");
		printf("child process start\n");
	}

	printf("%s: pid: %d, getpid: %d, getppid: %d\n", __func__, pid, getpid(), getppid());
	printf("join: %d, thread: %lu, self: %lu\n", pthread_join(thread, NULL), thread, pthread_self());
}

void mkproc2() {
	mkproc();
	
	printf("%s: pid: %d\n", __func__, getpid());
}

int main(int argc, char *argv[]) {
	mkproc2();
	
	printf("%s: pid: %d, ppid: %d\n", __func__, getpid(), getppid());

	return 0;
}

