#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define N 100

int main(int argc, char *argv[]) {
	int a[N];
	register int i, j, tmp, n = 0, n2 = 0;
	register bool flag;
	
	srand((unsigned int) time(NULL));
	
	printf("sort before:");
	for(i=0; i<N; i++) {
		a[i] = rand() % 100;
		printf(" %d", a[i]);
	}
	printf("\n");
	
	for(j=0; j<N-1; j++) {
		flag = true;
		for(i=1; i<N-j; i++) {
			if(a[i-1] > a[i]) {
				tmp = a[i];
				a[i] = a[i-1];
				a[i-1] = tmp;
				
				flag = false;
				n2++;
			}
			
			n++;
		}
		if(flag) break;
	}
	
	printf("j: %d\nsort after:", j);
	for(i=0; i<N; i++) {
		printf(" %d", a[i]);
	}
	printf("\nswaps: %d\nloops: %d\nmax loops: %d\n", n2, n, N*(N-1)/2);
	
	return 0;
}

