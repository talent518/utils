/**
 * msg.c
 *
 *  Interprocess message queue demo
 *
 *  Created on: 2019-9-22
 *      Author: abao
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MSQ_TIME(msg_stime) \
	tmp = localtime(&msq.msg_stime); \
	if(tmp && strftime(timebuf, sizeof(timebuf), "%F %T", tmp)) { \
		printf("     "#msg_stime": %ld, %s\n", msq.msg_stime, timebuf); \
	} else { \
		printf("     "#msg_stime": %ld\n", msq.msg_stime); \
	}

typedef struct _msgbuf {
	long type;
	unsigned short len;
	char line[16*1024];
} msgbuf;

int main(int argc, char *argv[]) {
	key_t key = ftok(argv[0], 1);
	printf("key: %08x\n", key);

	int msgid = msgget(key, 0666|IPC_CREAT);
	printf("msgid: %d\n", msgid);
	if(msgid < 0) {
		perror("msgget error");
		return 1;
	}

	struct msqid_ds msq;
	if(msgctl(msgid, IPC_STAT, &msq) < 0) {
		perror("msgctl IPC_STAT error");
		return 2;
	}

	char timebuf[20] = "";
	struct tm *tmp;

	printf("msg_perm.__key: %08x\n", msq.msg_perm.__key);
	printf("  msg_perm.uid: %u\n", msq.msg_perm.uid);
	printf("  msg_perm.gid: %u\n", msq.msg_perm.gid);
	printf(" msg_perm.cuid: %u\n", msq.msg_perm.cuid);
	printf(" msg_perm.cgid: %u\n", msq.msg_perm.cgid);
	printf(" msg_perm.mode: %03o\n", msq.msg_perm.mode);
	printf("msg_perm.__seq: %u\n", msq.msg_perm.__seq);
	MSQ_TIME(msg_stime);
	MSQ_TIME(msg_rtime);
	MSQ_TIME(msg_ctime);
	printf("  __msg_cbytes: %lu\n", msq.__msg_cbytes);
	printf("      msg_qnum: %lu\n", msq.msg_qnum);
	printf("    msg_qbytes: %lu\n", msq.msg_qbytes);
	printf("     msg_lspid: %d\n", msq.msg_lspid);
	printf("     msg_lrpid: %d\n", msq.msg_lrpid);

	pid_t pid = fork();

	if(pid < 0) {
		perror("fork error");
	} else if(pid > 0) {
		msgbuf buf = {1,0,""};
		FILE *fp = fopen(__FILE__, "r");
		if(fp) {
			int i = 0;
			while(!feof(fp)) {
				if(fgets(buf.line, sizeof(buf.line), fp)) {
					buf.len = strlen(buf.line);
					// fprintf(stderr, "%4d,%4d: %s", ++i, buf.len, buf.line);
					// fflush(stderr);
					if(msgsnd(msgid, (void *) &buf, sizeof(buf.len) + buf.len, 0) < 0) {
						perror("msgsnd error");
						break;
					}
				} else if(errno) {
					perror("fgets error");
					break;
				}
			}
			fclose(fp);
		} else {
			perror("fopen error");
		}
		buf.type = 2;
		buf.len = 0;
		if(msgsnd(msgid, (void *) &buf, sizeof(buf.len) + buf.len, IPC_NOWAIT) < 0) {
			perror("msgsnd error");
		}

		int status;
		pid_t w;
		do {
			w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
			if (w == -1) {
				perror("waitpid");
				return 1;
			}

			if (WIFEXITED(status)) {
				printf("exited, status=%d\n", WEXITSTATUS(status));
			} else if (WIFSIGNALED(status)) {
				printf("killed by signal %d\n", WTERMSIG(status));
			} else if (WIFSTOPPED(status)) {
				printf("stopped by signal %d\n", WSTOPSIG(status));
			} else if (WIFCONTINUED(status)) {
				printf("continued\n");
			}
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));

		if(status == 0 && msgctl(msgid, IPC_RMID, NULL) < 0) {
			perror("msgctl IPC_RMID error");
			return 3;
		}

		return 0;
	} else {
		msgbuf buf = {0,0,""};
		ssize_t len;

//		printf("key: %08x\n", key);
//		msgid = msgget(key, 0666|IPC_CREAT);
//		printf("msgid: %d\n", msgid);
//		if(msgid < 0) {
//			perror("msgget error");
//			return 1;
//		}

		printf("====== child process msgrcv ====== \n");
		fflush(stdout);
		int i = 0;
		while((len = msgrcv(msgid, (void *) &buf, sizeof(buf.len) + sizeof(buf.line), -2, 0/*MSG_NOERROR | IPC_NOWAIT*/)) > 0 && buf.type == 1) {
			printf("%4d: ", ++i);
			fwrite(buf.line, 1, buf.len, stdout);
			fflush(stdout);
		}

		if(buf.type != 2) perror("msgrcv error");

		printf("====== buf.type: %d, buf.len: %d, Completed ======\n", buf.type, buf.len);

		return errno;
	}
}
