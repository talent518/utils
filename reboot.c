#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <signal.h>

int main(int argc, char *argv[]) {
	int opt;
	int cmd = RB_AUTOBOOT;

	while((opt = getopt(argc, argv, "rhped?")) != -1) {
		switch(opt) {
			case 'r':
				cmd = RB_AUTOBOOT;
				printf("reboot\n");
				break;
			case 'h':
				cmd = RB_HALT_SYSTEM;
				printf("halt system\n");
				break;
			case 'p':
				cmd = RB_POWER_OFF;
				printf("power off\n");
				break;
			case 'e':
				cmd = RB_ENABLE_CAD;
				printf("CAD is enabled\n");
				break;
			case 'd':
				cmd = RB_DISABLE_CAD;
				printf("CAD is disabled\n");
				break;
			case '?':
			default:
				fprintf(stderr, 
					"usage: %s [-?|-r|-h|-p|-e|-d]\n"
                    "    -?     this help\n"
					"    -r     reboot\n"
					"    -h     halt\n"
					"    -p     power off\n"
					"    -e     CAD is enabled\n"
					"    -d     CAD is disabled\n"
					, argv[0]);
				exit(1);
				break;
		}
	}

	if(kill(-1, SIGINT)) perror("kill init error");

	return reboot(cmd);
}

