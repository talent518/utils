#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int N = 10, i, n;
	unsigned long int *A;
	char fmt[10];
	
	if(argc == 2) {
		N = abs(atoi(argv[1]));
	}

	if(N>67) N = 67;

	A = (unsigned long int*) malloc(sizeof(unsigned long int)*N);

	sprintf(fmt, "%%%dlu", (N+2)/3);

	for(n=1; n<=N; n++) {
		A[n-1] = 1;
		for(i=n-2; i>0; i--) {
			if(i == 0 || i == n-1) {
				A[i] = 1;
			} else {
				A[i] += A[i-1];
			}
		}
		for(i=0; i<n; i++) {
			printf(fmt, A[i]);
		}
		printf("\n");
	}

	free(A);

	return 0;
}
