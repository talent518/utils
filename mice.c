#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc,char **argv) {
	int fd, ret;
	char buf[6];
	fd_set readfds;
	struct timeval tv;

	// 打开鼠标设备
	fd = open( "/dev/input/mice", O_RDONLY);
	// 判断是否打开成功
	if(fd < 0) {
		perror("/dev/input/mice");
		return 1;
	}

	while(1) {
		// 设置最长等待时间
		tv.tv_sec = 5;
		tv.tv_usec = 0;

		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);

		ret = select(fd+1, &readfds, NULL, NULL, &tv);
		if(ret > 0 && FD_ISSET(fd, &readfds)) {
			// 读取鼠标设备中的数据
			if(read(fd, buf, 6) <= 0) continue;

			// 打印出从鼠标设备中读取到的数据
			printf("Button type = %d, X = %d, Y = %d, Z = %d\n", (buf[0] & 0x07), buf[1], buf[2],   buf[3]);
		}
	}
	close(fd);
	return 0;
}
