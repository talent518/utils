#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[]) {
	int k, n=0;
	for(k=100; k<1000; k++) {
		if(k == powf(k%10, 3) + powf((k/10)%10, 3) + powf(k/100, 3)) {
			n++;
			printf("%-4d", k);
		}
	}
	printf("\nNarcissistic number: %d\n", n);
	return 0;
}
