#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(int argc, char *argv[]) {
	int N = 10;
	
	if(argc == 2) {
		N = abs(atoi(argv[1]));
	}
	
	int i, n;
	int NN = N*(N+1)/2;
	unsigned int *A = (unsigned int*) malloc(sizeof(unsigned int)*NN);
	int a;
	
	printf("line: %d\n", N);
	printf("size: %d\n", NN);
	
	for(a=0,n=1; n<=N; n++) {
		for(i=0; i<n; i++) {
			// printf("n=%d, a=%d, a2=%d\n", n, a, a-n+1);
			if(i==0 || i==n-1) {
				A[a] = 1;
			} else {
				// a-n+1 表示 上一行为i的值
				A[a] = A[a-n+1] + A[a-n];
			}
			a++;
		}
	}

	unsigned int M = NN-(int)ceil((float)N/2.0);
	int L = (int) ceil(log10(A[M]));
	char fmt[16], fmt2[16];
	
	sprintf(fmt, "%%%du", L);
	sprintf(fmt2, "%%%du", L+1);
	
	printf(" max: %d\n\n", A[M]);
	for(a=0,n=1; n<=N; n++) {
		printf("%2d(", n);
		printf(fmt, A[a+n/2]);
		printf(")");
		for(i=0; i<n; i++) {
			printf(fmt2, A[a++]);
		}
		printf("\n");
	}
	
	free((void*) A);

	return 0;
}
