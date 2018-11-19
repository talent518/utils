#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int N = 10;
	int i, j;
	
	if(argc == 2 && (N = atoi(argv[1]))<3) {
		N = 3;
	}
	
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