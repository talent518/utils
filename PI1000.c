#include <stdio.h>

int main(int argc, char *argv[]) {
	const int ARRSIZE=1010, DISPCNT=1000; // 定义数组大小，显示位数
	// const int ARRSIZE=10100, DISPCNT=10000;
	char x[ARRSIZE], z[ARRSIZE]; // x[0] x[1] . x[2] x[3] x[4] .... x[ARRSIZE-1]
	int a=1, b=3, c, d, Run=1, Cnt=0,i;
	
	for (i=0; i<ARRSIZE; i++) {
		x[i]=0;
		z[i]=0;
	}
	// memset(x,0,ARRSIZE);
	// memset(z,0,ARRSIZE);
	x[1] = 2;
	z[1] = 2;
	
	while(Run && (++Cnt<200000000)) {
		// z *= a;
		d = 0;
		for(i=ARRSIZE-1; i>0; i--) {
			c = z[i]*a + d;
			z[i] = c % 10;
			d = c / 10;
		}
		// z /= b;
		d = 0;
		for(i=0; i<ARRSIZE; i++) {
			c = z[i]+d*10;
			z[i] = c / b;
			d = c % b;
		}
		// x += z;
		Run = 0;
		for(i=ARRSIZE-1; i>0; i--) {
			c = x[i] + z[i];
			x[i] = c%10;
			x[i-1] += c/10;
			Run |= z[i];
		}
		a++;
		b += 2;
		// printf("计算了 %d 次\n",Cnt);
	}
	
	printf("计算了 %d 次\r\n",Cnt);
	printf("PI = %d%d.\r\n", x[0],x[1]);
	
	for(i=0; i<DISPCNT; i++) {
		if(i && ((i % 100) == 0))
			printf("\r\n");

		printf("%d",(int)x[i+2]);
	}
	printf("\r\n");
	return 0;
}

