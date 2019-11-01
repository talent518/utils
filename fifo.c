#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

static bool running = true;

static void exit_signal(int sig) {
	printf("sig: %d, %s\n", sig, strsignal(sig));
	running = false;
}

int main(int argc, char *argv[]) {
	if (argc < 3 || (strcmp(argv[2], "r") && strcmp(argv[2], "w"))) {
		fprintf(stderr, "usage: %s file.fifo {r|w}\n", argv[0]);
		return 1;
	}

	int i, fd;
	char buf[128];
	struct stat st;

	if(lstat(argv[1], &st) < 0) {
		fprintf(stderr, "The file \"%s\" is not found\n", argv[1]);
		return 2;
	} else if(!S_ISFIFO(st.st_mode)) {
		fprintf(stderr, "The file \"%s\" is not fifo file\n", argv[1]);
		return 3;
	}

	signal(SIGPIPE, exit_signal);
	signal(SIGTERM, exit_signal);
	signal(SIGINT, exit_signal);

	if (argv[2][0] == 'r') {
		fd = open(argv[1], O_RDONLY);
		while (running) {
			i = read(fd, buf, sizeof(buf));
			if (i <= 0)
				break;
			write(STDOUT_FILENO, buf, i);
		}
	} else {
		fd = open(argv[1], O_WRONLY);
		int n = 0, pid = getpid();
		printf("pid: %d\n", pid);
		while (running) {
			i = snprintf(buf, sizeof(buf), "Writting a integer is %d, %d\n", pid, n++);
			i = write(fd, buf, i);
			if (i <= 0)
				break;
		}
		printf("n: %d\n", n);
	}
	close(fd);

	return 0;
}
