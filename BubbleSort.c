#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

int main(int argc, char *argv[]) {
	unsigned short N = 10, RND = 100;
	int *a;
	register int i, j, tmp, n = 0, n2 = 0;
	register bool flag, swap2flag = true;

	if(argc>2 && !strcmp(argv[1], "-n")) {
		i = atoi(argv[2]);
		if(i<3) {
			N = 10;
		} else {
			N = i;
		}
	} else if(argc>3) {
		N = argc-1;
		a = (int*) malloc(sizeof(int)*N);
		swap2flag = false;
		printf("----- swap 1 time -----\nbefore:");
		for(i=0; i<N; i++) {
			a[i] = atoi(argv[i+1]);
			printf(" %4d", a[i]);
		}
		printf("\n");
		goto sort;
	}

	a = (int*) malloc(sizeof(int)*N);
	RND = N*10;
	
	srand((unsigned int) time(NULL));
	
	printf("----- swap 2 time -----\nbefore:");
	for(i=0; i<N; i++) {
		a[i] = rand() % RND;
		printf(" %4d", a[i]);
	}
	printf("\n");

sort:
	for(j=0; j<N-1; j++) {
		flag = true;
		for(i=1; i<N-j; i++) {
			if(a[i-1] > a[i]) {
				tmp = a[i];
				a[i] = a[i-1];
				a[i-1] = tmp;
				
				flag = false;
				n2++;

				if(swap2flag && i>1 && a[i-2] > a[i-1]) {
					tmp = a[i-2];
					a[i-2] = a[i-1];
					a[i-1] = tmp;
					n2++;
				}
			}
			
			n++;
		}
		printf("%6d:", j);
		for(i=0; i<N; i++) {
			printf(" %4d", a[i]);
		}
		printf("\n");
		if(flag) break;
	}
	
	printf(" after:");
	for(i=0; i<N; i++) {
		printf(" %4d", a[i]);
	}
	printf("\n----- report -----\n swaps: %d\nloops: %d, max(%d)\n", n2, n, N*(N-1)/2);

	free(a);
	
	return 0;
}

