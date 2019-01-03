#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
	register char *p, *pp, *p0;
	register int n;
	char *ap, *bp, *bin;
	
	if(argc != 2) {
		goto usage;
	}
	
	p = argv[1];
	if(*p < '1' || *p > '9') {
		goto usage;
	}
	while(*p >= '0' && *p <= '9') p++;
	if(*p) goto usage;
	
	ap = strdup(argv[1]);
	n = strlen(ap);
	bp = (char *) malloc(n+1);
	p0 = bp;
	*p0 = '\0';
	
	n = n + 2 + n/10;
	bin = (char *) malloc(n+1);
	pp = bin + n;
	*pp = '\0';
	memset(bin, '0', n);
	pp--;
	
loop:
	p = ap;
	n = 0;
	for(; *p; p++) {
		if(n) {
			n = n * 10 + (*p - '0');
		} else {
			n = *p - '0';
		}
		*p0++ = '0' + n / 8;
		n = n % 8;
	}
	*p0 = '\0';
	*pp-- = '0' + n;
	p = bp;
	while(*p == '0') p++;
	if(*p) {
		strcpy(ap, p);
		p0 = bp;
		*p0 = '\0';
		n = 0;
		goto loop;
	}
	
	p = bin;
	while(*p == '0') p++;
	
	printf("%s octal value is %s\n", argv[1], p);
	
	free(ap);
	free(bp);
	free(bin);
	
	return 0;
usage:
	printf("usage: %s <number>\n", argv[0]);
	return 1;
}

