#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void _seq(char *str, int b, int n) {
	static long int id;
	static char fmt[32];
	register char chr;
	register int i;
	if(b == 0) {
		id = 1;
		for(i=n;i>1;i--) {
			id *= i;
		}
		fprintf(stderr, "total: %ld\n", id);
		sprintf(fmt, "%%%ldd: %%s\n", (long int) ceil(log10(id)));
		id = 0;
	}
	
	for(i=b; i<n; i++) {
		if(i>b) {
			chr = str[b];
			str[b] = str[i];
			str[i] = chr;
		}
		
		if(b == n-2) {
			id++;
			fprintf(stderr, "%ld\r", id);
			printf(fmt, id, str);
		} else {
			// printf("b: %d\n", b);
			memcpy(str+n+1, str, n);
			_seq(str+n+1, b+1, n);
		}
	}
}

void seq(const char *str) {
	unsigned n = strlen(str);
	char *buf;

	if(n > 20) {
		fprintf(stderr, "long int overflow\n");
		return;
	}

	buf = (char*) malloc(n*(n+1));
	
	memset(buf, 0, n*(n+1));
	memcpy(buf, str, n);
	
	_seq(buf, 0, n);
	
	free(buf);
}

int main(int argc, char *argv[]) {
	seq(argc>1?argv[1]:"123");

	return 0;
}
