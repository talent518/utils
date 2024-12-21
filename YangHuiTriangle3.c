#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>

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
	int L = (int) ceil(log10(A[M]+1)) + 1, l, ln;
	int NM = N*L, nm;
	unsigned int v;
	int j, istty = isatty(1);
	
	printf("max: %d, L: %d, NM: %d\n\n", A[M], L, NM);
	for(a=0,n=1; n<=N; n++) {
		nm = (NM - ((n-1)*L+1)) / 2;
		for(i=0; i<nm; i++) putchar(' ');
		for(i=0; i<n; i++) {
			v = A[a++];
			l = log10f(v) + 1;
			
			ln = (L - l) / 2;
			for(j=0; j<ln; j++) putchar(' ');
			
			if(istty) printf("\033[%dm%u\033[0m", 31 + (i%2), v); else printf("%u", v);
			
			ln = L - l - ln;
			for(j=0; j<ln; j++) putchar(' ');
		}
		printf("\n");
	}
	
	free((void*) A);

	return 0;
}
