#include <stdio.h>
#include <signal.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	signal(SIGHUP, SIG_IGN);
	
	execvp(argv[1], &argv[1]);
	perror("execvp error");
	return 1;
}

