#include <stdio.h>
#include <sys/wait.h>
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
		pid = fork();
		if (pid < 0) {
			perror("fork error");
			return 4;
		} else if (pid == 0) {
			close(wfd);
			dup2(rfd, STDIN_FILENO);
			execlp("wc", "wc", "-l", NULL);
			perror("execlp wc error");
			return 5;
		} else {
			// 这里的两close方法必须存在，不然不能保证pipe的单向流操作（正确：1读和1写，错误：多读多写）。
			close(rfd);
			close(wfd);

			wait(NULL);
			wait(NULL);
		}
	}
}
