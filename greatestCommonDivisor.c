#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	register long int n, ntmp, nmax = 0;
	register int i;
	
	if(argc <= 2) {
		fprintf(stderr, "usage: %s <integer> <integer> ...\n", argv[0]);
		return 1;
	}
	
	for(i=1; i<argc; i++) {
		n = labs(atol(argv[i]));
		printf("%d: %ld\n", i, n);
		
		if(!n) {
			fprintf(stderr, "warning: Can't be zero\n");
			continue;
		}
		
		if(nmax == 0) {
			nmax = n;
			printf("  %ld %% %ld = 0\n", n, n);
		} else {
			if(nmax>n) {
				ntmp = nmax;
				nmax = n;
				n = ntmp;
			}
			while((ntmp = n % nmax) != 0) {
				printf("  %ld %% %ld = %ld\n", n, nmax, ntmp);
				n = nmax;
				nmax = ntmp;
			}
			printf("  %ld %% %ld = 0\n", n, nmax);
		}
	}
	
	printf("Greatest common divisor is %ld\n", nmax);
	
	return 0;
}

