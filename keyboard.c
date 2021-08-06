#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/input.h>

void listen_device(const char *dev, int timeout) {
	int retval, fd;
	fd_set readfds;
	struct timeval tv;
	struct input_event event;

	fd = open(dev, O_RDONLY);
	if (fd < 0) {
		perror(dev);
		return;
	}

	while(1) {
		FD_ZERO(&readfds);
		FD_SET(fd, &readfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;
		if((retval = select(fd+1, &readfds, NULL, NULL, &tv)) == 1) {
			if(read(fd, &event, sizeof(event)) == sizeof(event)) {
				if (event.type == EV_KEY) {
					if (event.value == 0 || event.value == 1) {
						printf("key %d %s\n", event.code, event.value ? "Pressed" : "Released");
					}
				} else {
					printf("type=%x %d %d\n", event.type, event.code, event.value);
				}
			}
		} else {
			break;
		}
	}

	close(fd);
}

int main(int argc, char **argv) {
	listen_device("/dev/input/event2", 3600);
	return 0;
}
