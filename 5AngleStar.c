#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define PI 3.14159265F
#define N 60
#define R 30

int main(int argc, char *argv[]) {
	char A[N][N];
	int i, x, y, x1, y1, x2, y2, nX, nY, nMax;
	double angle;
	
	memset(A, ' ', sizeof(A));

	for(i=0; i<720; i+=144) {
		angle = (90+i) * PI / 180.0;
		x1 = R+R*cos(angle);
		y1 = R-R*sin(angle);

		angle = (90+i+144) * PI / 180.0;
		x2 = R+R*cos(angle);
		y2 = R-R*sin(angle);
		
		nX = abs(x1-x2);
		nY = abs(y1-y2);
		if(nX>nY) {
			x = (x1>x2?x2:x1);
			nMax = x+nX;
			for(;x<=nMax;x++) {
				y = (y2-y1)*(x-x1)/(x2-x1)+y1;
				A[y][x] = '*';
			}
		} else {
			y = (y1>y2?y2:y1);
			nMax = y+nY;
			for(;y<=nMax;y++) {
				x = (x2-x1)*(y-y1)/(y2-y1)+x1;
				A[y][x] = '*';
			}
		}
	}
	
	for(i=0; i<360; i++) {
		angle = i * PI / 180.0;
		x = R+R*cos(angle);
		y = R-R*sin(angle);
		A[y][x] = '*';
	}

	for(y=0; y<N; y++) {
		fwrite(A[y], N, 1, stdout);
		printf("\n");
	}

	return 0;
}

