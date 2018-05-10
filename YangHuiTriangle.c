#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int N = 10;
	
	if(argc == 2) {
		N = abs(atoi(argv[1]));
	}
	
	int i,j;
	int *A = (int*) malloc(sizeof(int)*N);
	int *A2 = (int*) malloc(sizeof(int)*N);
	
	for(i=0; i<N; i++) {
		A[i] = 1;
		A2[i] = 1;
	}
	
	for(i=1; i<=N; i++) {
		for(j=0; j<i; j++) {
			printf("%4d", A[j]);
		}
		printf("\n");
		if(i<2) {
			continue;
		}
		if(i==N) {
			break;
		}
		if(i>2) {
			memcpy(A2+1, A+1, sizeof(int)*(N-2));
		}
		for(j=1; j<i; j++) {
			A[j] = A2[j] + A2[j-1];
		}
	}

	return 0;
}

