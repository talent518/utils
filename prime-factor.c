#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[]) {
	int i, n=0;
	printf("Please input a number: ");
	scanf("%d", &n);
	printf("prime factor: %d = ", n);
	for(i=2; i<n; i++) {
		while(n!=i && n%i == 0) {
			printf("%d * ", i);
			n /= i;
		}
	}
	printf("%d\n", n);
	return 0;
}
