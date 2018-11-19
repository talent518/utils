#include <stdio.h>
#include <stdlib.h>
#include <math.h>

void hanoi(int n, char A, char B, char C) {
	if(n == 1) {
		printf("%2d: %c => %c\n", n, A, C);
	} else {
		hanoi(n-1, A, C, B);
		printf("%2d: %c => %c\n", n, A, C);
		hanoi(n-1, B, A, C);
	}
}

int main(int argc, char *argv[]) {
	int n = 4;
	if(argc == 2 && (n=atoi(argv[1]))<2) {
		n = 2;
	}
	if(n>64) n = 64;
	printf("n = %d, times = 2^n-1 = %lu\n", n, (unsigned long int)pow(2, n)-1);
	hanoi(n, 'A', 'B', 'C');
	return 0;
}
