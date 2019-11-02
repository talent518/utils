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

struct mt {
	int num;
	pthread_mutex_t mutex;
	pthread_mutexattr_t mutexattr;
};

int main(int argc, char *argv[]) {
	int i;
	struct mt* mm;
	pid_t pid;

	// 建立映射区
	mm = mmap(NULL, sizeof(*mm), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

	memset(mm, 0x00, sizeof(*mm));

	pthread_mutexattr_init(&mm->mutexattr);         // 初始化 mutex 属性
	pthread_mutexattr_setpshared(&mm->mutexattr, PTHREAD_PROCESS_SHARED); // 修改属性为进程间共享
	pthread_mutex_init(&mm->mutex, &mm->mutexattr);      // 初始化一把 mutex 锁

	pid = fork();
	if (pid == 0) {         // 子进程
		for (i = 0; i < 10; i++) {
			pthread_mutex_lock(&mm->mutex);
			(mm->num)++;
			printf("-child--------------num++    %d\n", mm->num);
			pthread_mutex_unlock(&mm->mutex);
			usleep(10);
		}
	} else {
		for (i = 0; i < 10; i++) {
			usleep(10);
			pthread_mutex_lock(&mm->mutex);
			mm->num += 2;
			printf("--------parent------num+=2   %d\n", mm->num);
			pthread_mutex_unlock(&mm->mutex);
		}
	}
	pthread_mutexattr_destroy(&mm->mutexattr);  // 销毁 mutex 属性对象
	pthread_mutex_destroy(&mm->mutex);          // 销毁 mutex 锁
	if(pid > 0) {
		wait(NULL);
		munmap(mm, sizeof(*mm));
	}

	return 0;
}
