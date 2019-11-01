#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	int fds[2];

	if (pipe(fds) < 0) {
		perror("pipe error");
		return 1;
	}

	const int rfd = fds[0];
	const int wfd = fds[1];

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork error");
		return 2;
	} else if (pid == 0) {
		close(rfd);
		dup2(wfd, STDOUT_FILENO);
		execlp("ls", "ls", NULL);
		perror("execlp ls error");
		return 3;
	} else {
		close(wfd);
		dup2(rfd, STDIN_FILENO);
		execlp("wc", "wc", "-l", NULL);
		perror("execlp wc error");
		return 4;
	}
}
