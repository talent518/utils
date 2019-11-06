#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

/**
 * 八皇后问题是一个以国际象棋为背景的问题：
 * 如何能够在 8×8 的国际象棋棋盘上放置八个皇后，使得任何一个皇后都无法直接吃掉其他的皇后？
 * 为了达到此目的，任两个皇后都不能处于同一条横行、纵行或斜线上。
 * 八皇后问题可以推广为更一般的n皇后摆放问题：这时棋盘的大小变为n1×n1，而皇后个数也变成n2。
 * 而且仅当 n2 ≥ 1 或 n1 ≥ 4 时问题有解。
 */

static int N = 8;

// 检验新王后放入后，是否冲突
static bool check(int *A, int row, int col) {
	int i;
	for(i=0; i<row; i++) {
		// 在同一列
		if(col == A[i]) return false;
		// 在同一斜线上
		if(row-i == abs(A[i]-col)) return false;
	}

	return true;
}

// 向数组放置第k个王后
static void queen(int *A, int k) {
	int i;
	if(k == N) {
		for(i=0; i<N; i++)
			printf(" %d", A[i]);
		putchar('\n');
		for(i=0; i<N*2; i++)
			putchar('-');
		putchar('\n');
		int j;
		for(i=0; i<N; i++) {
			for(j=0; j<N; j++) printf("%2d", A[i]==j);
			putchar('\n');
		}
		putchar('\n');
		return;
	}
	for(i=0; i<N; i++) {
		A[k] = i;
		if(check(A, k, i)) queen(A, k+1);
	}
}

int main(int argc, char *argv[]) {
	if (argc == 2) {
		N = abs(atoi(argv[1]));
		if (N < 3)
			N = 3;
	}

	int *A = (int*) malloc(sizeof(int) * N);

	queen(A, 0);
}
