#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void _seq(char *str, int b, int n) {
	static int id;
	static char fmt[12];
	register char chr;
	register int i;
	if(b == 0) {
		id = 1;
		for(i=n;i>1;i--) {
			id *= i;
		}
		sprintf(fmt, "%%%dd: %%s\n", (int) ceil(log10(id+1)));
		id = 0;
	}
	
	for(i=b; i<n; i++) {
		if(i>b) {
			chr = str[b];
			str[b] = str[i];
			str[i] = chr;
		}
		
		if(b == n-2) {
			printf(fmt, ++id, str);
		} else {
			// printf("b: %d\n", b);
			memcpy(str+n+1, str, n);
			_seq(str+n+1, b+1, n);
		}
	}
}

void seq(const char *str) {
	unsigned n = strlen(str);
	char *buf = (char*) malloc(n*n);
	
	memset(buf, 0, n*n);
	memcpy(buf, str, n);
	
	_seq(buf, 0, n);
	
	free(buf);
}

int main(int argc, char *argv[]) {
	seq(argc>1?argv[1]:"123");

	return 0;
}
