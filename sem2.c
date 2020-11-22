#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

static sem_t sem1, sem2;
static volatile unsigned int count = 0;
static volatile unsigned int isRun = 1;
static volatile double t;

#define MICRO_IN_SEC 1000000.00

double microtime() {
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double)(tp.tv_sec + tp.tv_usec / MICRO_IN_SEC) * 1000000;
}

void *thread1(void *arg) {
	while(isRun) {
		sem_wait(&sem1);
		//printf("%.3fus\n", microtime() - t);
		//t = microtime();
		count ++;
		sem_post(&sem2);
	}
}

void *thread2(void *arg) {
	while(isRun) {
		sem_wait(&sem2);
		//printf("%.3fus\n", microtime() - t);
		//t = microtime();
		count ++;
		sem_post(&sem1);
	}
}

int main(int argc, char *argv) {
	pthread_t tid1, tid2;
	int s1 = 0, s2 = 0;

	sem_init(&sem1, 0, 0);
	sem_init(&sem2, 0, 0);

	pthread_create(&tid1, NULL, thread1, NULL);
	pthread_create(&tid2, NULL, thread2, NULL);
	
	t = microtime();
	sem_post(&sem1);

	usleep(5000000);
	
	printf("count = %u\n", count);
	printf("runtime = %.3fus\n", microtime() - t);
	
	isRun = 0;
	
	pthread_join(tid1, NULL);
	pthread_join(tid2, NULL);
	
	printf("count = %u\n", count);
	printf("runtime = %.3fus\n", microtime() - t);
	
	sem_getvalue(&sem1, &s1);
	sem_getvalue(&sem2, &s2);
	
	printf("s1 = %d, s2 = %d\n", s1, s2);

	return 0;
}
