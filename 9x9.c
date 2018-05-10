#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	int i, j, k, n;
	
	for(k=0; k<3; k++) {
		if(k>0) {
			printf("==============================================================\n");
		}
		for(i=1; i<=9; i++) {
			for(j=1; j<=i; j++) {
				if(j>1) {
					printf(" ");
				}
				switch(k) {
					case 0: {
						n = printf("%d+%d=%d", j, i, j+i);
						break;
					}
					case 1: {
						n = printf("%d-%d=%d", i, j, i-j);
						break;
					}
					default: {
						n = printf("%dx%d=%d", j, i, j*i);
						break;
					}
				}
				if(k!=1 && n<6) {
					printf(" ");
				}
			}
			printf("\n");
		}
	}
	printf("==============================================================\n");
	for(i=1; i<=9; i++) {
		for(j=1; j<=9; j++) {
			if(j>1) {
				printf(" ");
			}
			printf("%2d/%d=%d", i*j, j, i);
		}
		printf("\n");
	}

	return 0;
}
