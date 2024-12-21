/**
 * 小偷偷窃算法
 * -------------------------
 * 你是一个专业的小偷，计划偷窥沿街的房屋。每间房内都藏有一定的现金，影响你偷窥的唯一制约因素就是相邻的房屋装有相互连通的防盗系统，如果两间相邻的房屋在同一晚上被小偷闯入，系统会自动报警。
 * 给定一个代表每个房屋存放金额的非负整数数组，计算你在不触动警报装置的情况下，能够偷窃到的最高金额。
 * 示例1:
 *     输入: [1,2,3,1]
 *     输出: 4
 *     解释: 偷窃1号房屋(金额为1)，然后偷窃3号房屋(金额为3)，偷窃到的最高金额 = 1 + 3 = 4
 * 示例2:
 *     输入: [2,7,9,3,1]
 *     输出: 12
 *     解释: 偷窃1号房屋(金额为2)，然后偷窃3号房屋(金额为9)，接着偷窃5号房屋(金额为1)，偷窃到的最高金额 = 2 + 9 + 1 = 12
 * 示例3:
 *     输入: [6,3,10,8,2,10,3,5,10,5,3]
 *     输出: 39
 *     解释: 6 + 10 + 10 + 10 + 3 = 39
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#if 1
	#define IFMT
	#define VFMT(i)
#elif 0
	#define IFMT "{%d}"
	#define VFMT(i) , i
#else
	#define IFMT "\033[37m{%d}\033[0m"
	#define VFMT(i) , i
#endif

#if 0
	#define BFMT " \033[34m+\033[0m "
	#define NFMT " \033[31m+\033[0m "
	#define AFMT " \033[32m+\033[0m "
#else
	#define BFMT " + "
	#define NFMT " + "
	#define AFMT " + "
#endif

int main(int argc, char *argv[]) {
	int N = 10, *a, i, max = 0, n, n2, n3, m, plus = 0, t = 0;

#if 0
	int M = 10;
	if(argc>=2) N = atoi(argv[1]);
	else if(argc == 3) M = atoi(argv[2]);

	if(N < 3) {
		fprintf(stderr, "N must be greater than or equal to 3\n");
		return 2;
	}
	if(M < 2) {
		fprintf(stderr, "Max must be greater than or equal to 2\n");
		return 3;
	}

	srandom(time(NULL));

	a = (int*) malloc(sizeof(int)*N);

	printf("N: %d\nM: %d\nlist:", N, M);
	for(i=0; i<N; i++) {
		a[i] = random() % M + 1;
		printf(" %d"IFMT, a[i], i);
	}
	printf("\n\n");

#else
	N = argc - 1;
	if(N<3) {
		fprintf(stderr, "usage: %s <n1> <n2> <n3>...\n", argv[0]);
		return 1;
	}
	a = (int*) malloc(sizeof(int)*N);
	for(i=1; i<argc; i++) {
		a[i-1] = atoi(argv[i]);
	}
#endif

#if 0
	for(n=0; n<2; n++) {
		m = 0;
		printf("%3d,2,%d: ", ++t, n);
		plus = 0;
		for(i=n; i<N; i+=2) {
			m += a[i];
			if(plus++) printf(" + ");
			printf("%2d"IFMT, a[i], i);
		}
		printf(" = %2d"IFMT"\n", m, plus);
		if(m>max) max = m;
	}

	n3 = 3;
	for(n=0; n<N-n3; n++) {
#else
	for(n3 = 2; n3<=3; n3++) for(n=0; n<=N-n3; n++) {
#endif
		for(n2=0; n2<4; n2++) {
			m = 0;
			printf("%3d,%d,%d,%d,%d: ", ++t, n3, n, n2>>1, n2&1);
			plus = 0;
			for(i=n2&1; i<n-1; i+=2) {
				m += a[i];
				if(plus++) printf(BFMT);
				printf("%2d"IFMT, a[i] VFMT(i));
			}

			m += a[n];
			if(plus++) printf(BFMT);
			printf("%2d"IFMT, a[n] VFMT(n));

			if(n+n3 < N) {
				m += a[n+n3];
				if(plus++) printf(NFMT);
				printf("%2d"IFMT, a[n+n3] VFMT(n+n3));

				for(i=(n2>>1)+n+n3+2; i<N; i+=2) {
					m += a[i];
					if(plus++) printf(AFMT);
					printf("%2d"IFMT, a[i] VFMT(i));
				}
			}

			printf(" = %d"IFMT"\n", m VFMT(plus));
			if(m>max) max = m;
		}
	}

	printf("\nmax: %d\n", max);

	free(a);

	return 0;
}
