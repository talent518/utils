#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define RDI 0
#define WRI 1
#define WAIT_READ 1

void send2stdin(int infd, char *buf, int len) {
	int ret = snprintf(buf, len, "%x\t%d\t%d\t%x\n", rand(), rand() % 100, rand() % 2, rand());
	write(infd, buf, ret);
	write(STDOUT_FILENO, "\033[33m", 5);
	write(STDOUT_FILENO, buf, ret);
	write(STDOUT_FILENO, "\033[m", 3);
}

int main(int argc, char *argv[]) {
	int infds[2], outfds[2], errfds[2];

#if 0 // 经测试：这两种方式都可以
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, infds) < 0)
	perror("socketpair infds error");
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, outfds) < 0)
	perror("socketpair outfds error");
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, errfds) < 0)
	perror("socketpair errfds error");
#else
	if (pipe(infds) < 0)
		perror("pipe infds error");
	if (pipe(outfds) < 0)
		perror("pipe outfds error");
	if (pipe(errfds) < 0)
		perror("pipe errfds error");
#endif

	pid_t pid = fork();
	if (pid < 0)
		perror("fork error");
	else if (pid > 0) {
		int ret, status;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200;
		char buf[1024];
		const int outfd = outfds[RDI];
		const int errfd = errfds[RDI];
		const int infd = infds[WRI];

		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(outfd, &readset);
		FD_SET(errfd, &readset);

#if WAIT_READ == 0
		const int maxfd = outfd > errfd ? (outfd > infd ? outfd : infd) : (errfd > infd ? errfd : infd);
		fd_set writeset;
		FD_ZERO(&writeset);
		FD_SET(infd, &writeset);

	tryslt:
		ret = select(maxfd+1, &readset, &writeset, NULL, &tv);
#else
		const int maxfd = outfd > errfd ? outfd : errfd;
	tryslt:
		ret = select(maxfd + 1, &readset, NULL, NULL, &tv);
#endif
		if (ret < 0)
			perror("select error");
		else if (ret > 0) {
#if WAIT_READ == 0
			if(FD_ISSET(infd, &writeset)) {
				send2stdin(infd, buf, sizeof(buf));
			} else {
				FD_SET(infd, &writeset);
			}
#endif
			if (FD_ISSET(outfd, &readset)) {
				ret = read(outfd, buf, sizeof(buf));
				write(STDOUT_FILENO, "\033[32m", 5);
				write(STDOUT_FILENO, buf, ret);
				write(STDOUT_FILENO, "\033[m", 3);
#if WAIT_READ
				if (strstr(buf, "Input user detail:"))
					send2stdin(infd, buf, sizeof(buf));
#endif
			} else {
				FD_SET(outfd, &readset);
			}

			if (FD_ISSET(errfd, &readset)) {
				ret = read(errfd, buf, sizeof(buf) - 1);
				write(STDOUT_FILENO, "\033[31m", 5);
				write(STDOUT_FILENO, buf, ret);
				write(STDOUT_FILENO, "\033[m", 3);
			} else {
				FD_SET(errfd, &readset);
			}

			goto tryslt;
		} else {
			ret = waitpid(pid, &status, WNOHANG);
			if (ret < 0)
				perror("waitpid error");
			else if (ret == 0) {
				FD_SET(outfd, &readset);
				FD_SET(errfd, &readset);
				goto tryslt;
			}
		}
	} else {
		dup2(infds[RDI], STDIN_FILENO);
		dup2(outfds[WRI], STDOUT_FILENO);
		dup2(errfds[WRI], STDERR_FILENO);

		execlp("php", "php", "-r", "echo \"begin\n\";"
			"for($i=0;$i<3;$i++){"
#if WAIT_READ
			"	echo 'Input user detail: ';"
#endif
			"	fscanf(STDIN, '%s\t%d\t%d\t%s\n', $name, $age, $sex, $address);"
//			"	fprintf(STDERR, \"printing\n\");"
//			"	usleep(1);"
			"	var_dump(compact('name','age','sex','address','i','f'));"
//			"	usleep(1);"
//			"	fprintf(STDERR, \"complated\n\");"
			"}"
			"usleep(1);"
			"echo \"end\n\";", NULL);
	}

	return 0;
}

