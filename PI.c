#include<stdio.h>
#include<stdlib.h>

int main(int argc, char *argv[]) {
	double x=2, z=2;
	int a=1, b=3;
	while(z>1e-15) {
		z = z*a/b;
		x += z;
		a++;
		b+=2;
	}
	printf("PI = %.13f\n", x);

	return 0;
}

