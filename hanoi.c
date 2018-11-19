#include <stdio.h>
#include <stdlib.h>

static int times;

#define hanoi(n, A, B, C) times=0;_hanoi(n, A, B, C);printf("times = %d, 2^n-1 = %d\n", times, (int)pow(2, n)-1)

void _hanoi(int n, char A, char B, char C) {
	if(n == 1) {
		printf("%5d: Move sheet %d from %c to %c\n", ++times, n, A, C);
	} else {
		_hanoi(n-1, A, C, B);
		printf("%5d: Move sheet %d from %c to %c\n", ++times, n, A, C);
		_hanoi(n-1, B, A, C);
	}
}

int main(int argc, char *argv[]) {
	int n;
	printf("请输入盘数: ");
	scanf("%d", &n);
	hanoi(n, 'A', 'B', 'C');
	return 0;
}