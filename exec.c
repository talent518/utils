#include <stdio.h>
#include <unistd.h>

#define EXEC 5

int main(int argc, char *argv[]) {
	printf("before\n");

#if EXEC == 0
	execl("/bin/ls", "ls", "-CF", NULL);
	perror("execl error");
#elif EXEC == 1
	execlp("ls", "ls", "-CF", NULL);
	perror("execlp error");
#elif EXEC == 2
	char *envp[] = {"set=1","SET=2",NULL};
	execle("/usr/bin/env", "env", NULL, envp);
	perror("execle error");
#elif EXEC == 3
	char *args[] = {"ls", "-CF", NULL};
	execv("/bin/ls", args);
	perror("execv error");
#elif EXEC == 4
	char *args[] = {"ls", "-CF", NULL};
	execvp("ls", args);
	perror("execvp error");
#else
	char *args[] = {"env", NULL};
	char *envp[] = {"set=1","SET=2",NULL};
	execvpe("env", args, envp);
	perror("execvpe error");
#endif

	printf("after\n");
	return 0;
}
