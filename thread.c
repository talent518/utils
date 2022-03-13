#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

static void *print_handle(void *arg) {
	printf("J: %d\n", *(int*)arg);
	pthread_exit(NULL);
}

static sem_t sem;
static void *print_handle_sem(void *arg) {
	printf("D: %d\n", *(int*)arg);
	sem_post(&sem);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	pthread_t thread;
	pthread_attr_t attr;
	int i = 0, c, detachstate = PTHREAD_CREATE_JOINABLE;
	void *(*handle)(void*) = print_handle;

	while((c = getopt(argc, argv, "dh?")) != -1) {
		switch(c) {
			case 'd': {
				detachstate = PTHREAD_CREATE_DETACHED;
				handle = print_handle_sem;
				sem_init(&sem, 0, 0);
				break;
			}
			case 'h':
			case '?':
			default:
				fprintf(stderr, "Usage: %s [-d] [{-h | -?}]\n    -d    detached thread\n", argv[0]);
				return 0;
		}
	}

	for(;;) {
		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, detachstate);
		if(pthread_create(&thread, &attr, handle, &i)) {
			perror("pthread_create failure");
			break;
		}
		pthread_attr_destroy(&attr);

		i ++;

		if(detachstate == PTHREAD_CREATE_JOINABLE) {
			if(pthread_join(thread, NULL)) perror("pthread_join failure");
		} else {
			sem_wait(&sem);
			usleep(1000);
		}
	}

	return 0;
}
