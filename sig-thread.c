#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

static void signal_handler(int sig) {
	printf("sig: %d - %s, pid: %d, tid: %lu\n", sig, strsignal(sig), getpid(), pthread_self());
}

static void *worker_thread(void *arg) {
	printf("worker pid: %d, tid: %lu\n", getpid(), pthread_self());

	signal(SIGINT, signal_handler);

	while(1);

	pthread_exit(NULL);

	return NULL;
}

int main(int argc, char *argv[]) {
	// signal(SIGINT, signal_handler);

	pthread_t tid;
	pthread_create(&tid, NULL, worker_thread, NULL);
	pthread_detach(tid);

	// signal(SIGINT, signal_handler);

	printf("main pid: %d, tid: %lu\n", getpid(), pthread_self());
	while(1);

	return 0;
}
