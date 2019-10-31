#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define RDI 0
#define WRI 1

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
		fd_set readset, writeset, exceptset;
		int ret, status;
		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 200;
		char buf[1024];
		int maxfd = outfds[RDI] > errfds[RDI] ? (outfds[RDI] > infds[WRI] ? outfds[RDI] : infds[WRI]) : (errfds[RDI] > infds[WRI] ? errfds[RDI] : infds[WRI]);
//		int maxfd = outfds[RDI] > errfds[RDI] ? outfds[RDI] : errfds[RDI];

		printf("maxfd = %d\n", maxfd);

	tryslt:
		FD_ZERO(&readset);
		FD_SET(outfds[RDI], &readset);
		FD_SET(errfds[RDI], &readset);

		FD_ZERO(&writeset);
		FD_SET(infds[WRI], &writeset);

		ret = select(maxfd+1, &readset, &writeset, NULL, &tv);
		if(ret < 0)
			perror("select error");
		else if(ret > 0) {
			if(FD_ISSET(infds[WRI], &writeset)) {
				write(infds[WRI], buf, snprintf(buf, sizeof(buf), "%x\t%d\t%d\t%x\n", rand(), rand()%100, rand()%2, rand()));
			}
			if(FD_ISSET(outfds[RDI], &readset)) {
				ret = read(outfds[RDI], buf, sizeof(buf));
				write(STDOUT_FILENO, "\033[32m", 5);
				write(STDOUT_FILENO, buf, ret);
				write(STDOUT_FILENO, "\033[m", 3);
			}
			if(FD_ISSET(errfds[RDI], &readset)) {
				ret = read(errfds[RDI], buf, sizeof(buf)-1);
				write(STDOUT_FILENO, "\033[31m", 5);
				write(STDOUT_FILENO, buf, ret);
				write(STDOUT_FILENO, "\033[m", 3);
			}

			goto tryslt;
		} else {
			ret = waitpid(pid, &status, WNOHANG);
			if(ret < 0)
				perror("waitpid error");
			else if(ret == 0)
				goto tryslt;
		}
	} else {
		dup2(infds[RDI], STDIN_FILENO);
		dup2(outfds[WRI], STDOUT_FILENO);
		dup2(errfds[WRI], STDERR_FILENO);
		execlp("php", "php", "-r", "echo \"begin\n\";"
			"for($i=0;$i<3;$i++){"
			"	fscanf(STDIN, '%s\t%d\t%d\t%s\n', $name, $age, $sex, $address);"
			"	fprintf(STDERR, \"printing\n\");"
			"	usleep(1);"
			"	var_dump(compact('name','age','sex','address','i','f'));"
			"	usleep(1);"
			"	fprintf(STDERR, \"complated\n\");"
			"}"
			"usleep(1);"
			"echo \"end\n\";", NULL);
	}

	return 0;
}

