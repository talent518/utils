#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pty.h>
#include <signal.h>

#define PASSWORD "123456\r"
#define CMD "service sshd restart\n"
#define EXIT "exit\n"

int main(int argc, char *argv[]) {
	int master = 0, pid, len, passwd = 0, cmd = 0, status = 0;
	char buf[2048], name[128];
	
	signal(SIGCHLD, SIG_IGN);
	pid = forkpty(&master, name, NULL, NULL);
	
	printf("name: %s, pid: %d\n", name, pid);
	
	if(pid < 0) {
		perror("fork error");
		return 1;
	} else if(pid == 0) {
		char *envp[] = {"LANG=en",NULL};
		execle("/bin/su", "su", "-", "root", NULL, envp);
		perror("execle");
	} else {
		for(;;) {
			fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(master, &readfds);
            
            if(select(master + 1, &readfds, NULL, NULL, NULL) == -1) {
            	perror("select");
            	break;
            }
            
            if(FD_ISSET(master, &readfds)) {
            	len = read(master, buf, sizeof(buf));
                if(len < 1) break;
                write(STDOUT_FILENO, buf, len);
				if(!passwd && strstr(buf, "assword:")) {
					passwd = 1;
					write(master, PASSWORD, sizeof(PASSWORD)-1);
				} else if(cmd < 2 && strstr(buf, ":~# ")) {
					if(cmd == 0) {
						cmd=1;
						write(master, CMD, sizeof(CMD)-1);
					} else {
						cmd = 2;
						write(master, EXIT, sizeof(EXIT)-1);
					}
				}
            }
		}
		
		close(master);
		
		waitpid(pid, &status, 0);
	}
}
