/**
 * 互斥量 实现 多进程 之间的同步
 */

#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <semaphore.h>

struct mt {
	int num;
	sem_t sem;
	sem_t sem2;
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexattr;
};

int main(int argc, char *argv[]) {
	struct mt* mm;
	int i, fd;

	if (access("test.mmap", F_OK) == 0) {
		fd = open("test.mmap", O_RDWR);
		if (fd < 0) {
			perror("open file");
			return 1;
		}
		if (sizeof(*mm) != lseek(fd, 0, SEEK_END)) {
			close(fd);
			fprintf(stderr, "The file test.mmap size is not equal %lu\n", sizeof(*mm));
			return 2;
		}
		mm = mmap(NULL, sizeof(*mm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mm == MAP_FAILED) {
			perror("mmap error");
			close(fd);
			return 3;
		}
		close(fd);

		printf("client started: %p\n", mm);
		sem_post(&mm->sem);
		sem_wait(&mm->sem2);
		for (i = 0; i < 100; i++) {
			pthread_mutex_lock(&mm->mutex);
			(mm->num)++;
			printf("----client----------num++    %d\n", mm->num);
			pthread_mutex_unlock(&mm->mutex);
			usleep(1);
		}
		sem_wait(&mm->sem2);
		sem_post(&mm->sem);
		munmap(mm, sizeof(*mm));
	} else {
		const int N = argc > 1 ? abs(atoi(argv[1])) : 1;
		if (N == 0) {
			fprintf(stderr, "usage: %s n\n", argv[0]);
			return 4;
		}
		fd = open("test.mmap", O_CREAT | O_RDWR, 0755);
		if (fd < 0) {
			perror("open file");
			return 1;
		}
		ftruncate(fd, sizeof(*mm));
		mm = mmap(NULL, sizeof(*mm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mm == MAP_FAILED) {
			perror("mmap error");
			close(fd);
			return 3;
		}
		close(fd);

		memset(mm, 0x00, sizeof(*mm));

		pthread_mutexattr_init(&mm->mutexattr);         // 初始化 mutex 属性
		pthread_mutexattr_setpshared(&mm->mutexattr, PTHREAD_PROCESS_SHARED); // 修改属性为进程间共享
		pthread_mutex_init(&mm->mutex, &mm->mutexattr);      // 初始化一把 mutex 锁

		sem_init(&mm->sem, 1, 0);
		sem_init(&mm->sem2, 1, 0);

		printf("server started: %p\n", mm);

		for (i = 0; i < N; i++)
			sem_wait(&mm->sem);
		for (i = 0; i < N; i++)
			sem_post(&mm->sem2);
		for (i = 0; i < 100; i++) {
			usleep(1);
			pthread_mutex_lock(&mm->mutex);
			mm->num += 2;
			printf("--------server------num+=2   %d\n", mm->num);
			pthread_mutex_unlock(&mm->mutex);
		}
		for (i = 0; i < N; i++)
			sem_post(&mm->sem2);
		for (i = 0; i < N; i++)
			sem_wait(&mm->sem);

		sem_destroy(&mm->sem);
		pthread_mutexattr_destroy(&mm->mutexattr);  // 销毁 mutex 属性对象
		pthread_mutex_destroy(&mm->mutex);          // 销毁 mutex 锁

		munmap(mm, sizeof(*mm));

		system("od -tcx test.mmap");

		unlink("test.mmap");
	}

	return 0;
}
