#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void _seq(char *str, int b, int n) {
	static int id = 0;
	register char chr;
	register int i;
	
	for(i=b; i<n; i++) {
		if(i>b) {
			chr = str[b];
			str[b] = str[i];
			str[i] = chr;
		}
		
		if(b == n-2) {
			printf("%10d: %s\n", ++id, str);
		} else {
			printf("b: %d\n", b);
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