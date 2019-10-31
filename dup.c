#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
	char buf[1024];

	if(argc < 2) {
		fprintf(stderr, "usage: %s file\n", argv[0]);
		return 1;
	}
	int fp = open(argv[1], O_RDWR|O_APPEND|O_CREAT, 0755);
	if(fp < 0) {
		perror("open file failure");
		return 2;
	}

	printf("fp = %d\n", fp);

	int ret;

	ret = write(fp, buf, snprintf(buf, sizeof(buf), "%s:%d\n", __FILE__, __LINE__));
	if(ret <= 0) {
		perror("write(fp, buf, len) failure");
	}

	int fp2 = dup(fp);
	if(fp2 < 0) {
		perror("dup(fp) failure");
		close(fp);
		return 3;
	}

	printf("fp2 = %d\n", fp2);

	ret = write(fp, buf, snprintf(buf, sizeof(buf), "%s:%d\n", __FILE__, __LINE__));
	if(ret <= 0) {
		perror("write(fp2, buf, len) failure");
	}

	int fpout = dup(STDOUT_FILENO); // 复制stdout标准输出文件描述符为fpout
	int fp3 = dup2(fp, STDOUT_FILENO); // 标准输出stdout重定向到argv[1]参数指定的文件中
	if(fp3 < 0) {
		perror("dup2(oldfp, newfd) failure");
		close(fp2);
		close(fp);
		return 3;
	}

	printf("fp3 = %d\n", fp3);

	ret = write(fp3, buf, snprintf(buf, sizeof(buf), "%s:%d\n", __FILE__, __LINE__));
	if(ret <= 0) {
		perror("write(fp3, buf, len) failure");
	}

	printf("%s:%d\n", __FILE__, __LINE__);

	int fperr = dup(STDERR_FILENO); // 复制stderr标准错误文件描述符为fperr
	int fp4 = dup2(fp, STDERR_FILENO); // 标准输出stderr重定向到argv[1]参数指定的文件中
	if(fp4 < 0) {
		perror("dup2(oldfp, newfd) failure");
		close(fp3);
		close(fp2);
		close(fp);
		return 3;
	}

	printf("fp4 = %d\n", fp4);

	ret = write(fp4, buf, snprintf(buf, sizeof(buf), "%s:%d\n", __FILE__, __LINE__));
	if(ret <= 0) {
		perror("write(fp3, buf, len) failure");
	}

	fprintf(stderr, "%s:%d\n", __FILE__, __LINE__);

	ret = close(fp4);
	if(ret < 0) perror("close(fp4) failure");
	ret = close(fp3);
	if(ret < 0) perror("close(fp4) failure");

	int ret2;

	ret = dup2(fpout, STDOUT_FILENO); // 把fpout还原为stdout
	ret2 = close(fpout);
	if(ret2 < 0) perror("close(fpout) failure");
	printf("ret = %d, fpout -> stdout\n", ret);

	ret = dup2(fperr, STDERR_FILENO); // 把fperr还原为stderr
	ret2 = close(fperr);
	if(ret2 < 0) perror("close(fperr) failure");
	fprintf(stderr, "ret = %d, fperr -> stderr\n", ret);

	int fp5 = fcntl(fp, F_DUPFD, 0); // 复制fp并生成一个最小可用文件描述符
	printf("fp5 = %d\n", fp5);
	int fp6 = fcntl(fp, F_DUPFD, 3); // 复制fp并生成大于等于3且最小可用的描述
	printf("fp6 = %d\n", fp6);

	ret = close(fp5);
	if(ret < 0) perror("close(fp5) failure");
	ret = close(fp6);
	if(ret < 0) perror("close(fp6) failure");

	ret = close(fp2);
	if(ret < 0) perror("close(fp2) failure");
	ret = close(fp);
	if(ret < 0) perror("close(fp) failure");
}
