#include <stdio.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>
sem_t empty;  //控制盘子里可放的水果数
sem_t apple;  //控制苹果数
sem_t orange; //控制橙子数
pthread_mutex_t work_mutex;                    //声明互斥量work_mutex
void *procf(void *arg) //father线程
{
	while (1) {
		sem_wait(&empty);     //占用一个盘子空间，可放水果数减1
		pthread_mutex_lock(&work_mutex);     //加锁
		printf("爸爸放入一个苹果!\n");
		sem_post(&apple);     //释放一个apple信号了，可吃苹果数加1
		pthread_mutex_unlock(&work_mutex);   //解锁
		sleep(3);
	}
}
void *procm(void *arg)  //mother线程
{
	while (1) {
		sem_wait(&empty);
		pthread_mutex_lock(&work_mutex);     //加锁
		printf("妈妈放入一个橙子!\n");
		sem_post(&orange);
		pthread_mutex_unlock(&work_mutex);   //解锁
		sleep(4);
	}
}
void *procs(void *arg)  //son线程
{
	while (1) {
		sem_wait(&apple);       //占用一个苹果信号量，可吃苹果数减1
		pthread_mutex_lock(&work_mutex);     //加锁
		printf("儿子吃了一个苹果!\n");
		sem_post(&empty);       //吃了一个苹果，释放一个盘子空间，可放水果数加1
		pthread_mutex_unlock(&work_mutex);   //解锁
		sleep(1);
	}
}
void *procd(void *arg)  //daughter线程
{
	while (1) {
		sem_wait(&orange);
		pthread_mutex_lock(&work_mutex);     //加锁
		printf("女儿吃了一个橙子!\n");
		sem_post(&empty);
		pthread_mutex_unlock(&work_mutex);   //解锁
		sleep(2);
	}
}

int main(int argc, char *argv) {
	pthread_t father;  //定义线程
	pthread_t mother;
	pthread_t son;
	pthread_t daughter;

	sem_init(&empty, 0, 3);  //信号量初始化
	sem_init(&apple, 0, 0);
	sem_init(&orange, 0, 0);
	pthread_mutex_init(&work_mutex, NULL);   //初始化互斥量

	pthread_create(&father, NULL, procf, NULL);  //创建线程
	pthread_create(&mother, NULL, procm, NULL);
	pthread_create(&daughter, NULL, procd, NULL);
	pthread_create(&son, NULL, procs, NULL);

	sleep(100);

	return 0;
}
