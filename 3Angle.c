#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 10

int main(int argc, char *argv[]) {
	int i, j;
	
	for(i=0; i<N; i++) {
		for(j=1; j<N-i; j++) {
			printf(" ");
		}
		for(j=0; j<=i*2; j++) {
			printf("*");
		}
		printf("\n");
	}

	return 0;
}
