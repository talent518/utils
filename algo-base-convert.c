#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static char BASE[36] = {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z'};

#define P(c) ((fbase <= 10 && c >= '0' && c < '0' + fbase) || (fbase > 10 && ((c >= '0' && c <= '9') || (c >= 'A' && c < 'A' + fbase - 10) || (c >= 'a' && c < 'a' + fbase - 10))))

int main(int argc, char *argv[]) {
	register unsigned char *p, *pp, *p0;
	register int n, fbase, tbase;
	unsigned char *ap, *bp, *bin;
	
	if(argc != 4 || (fbase = atoi(argv[2]))<2 || fbase>36 || (tbase = atoi(argv[3]))<2 || tbase>36 || fbase == tbase) {
		goto usage;
	}
	
	p = argv[1];
	if(*p == '0') {
		goto usage;
	}
	while(P(*p)) p++;
	if(*p) goto usage;
	
	ap = strdup(argv[1]);
	n = strlen(ap);
	bp = (char *) malloc(n+1);
	p0 = bp;
	*p0 = '\0';
	
	if(fbase > tbase) n = n * (int) ceil(logf(fbase) / logf(tbase));

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
			n = n * fbase;
		}
		if(*p <= '9') n += *p - '0';
		else if(*p >= 'a') n += *p + 10 - 'a';
		else n += *p + 10 - 'A';
		*p0++ = BASE[n / tbase];
		n = n % tbase;
	}
	*p0 = '\0';
	*pp-- = BASE[n];
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
	
	printf("%d scale number %s of %d scale number is %s\n", fbase, argv[1], tbase, p);
	
	free(ap);
	free(bp);
	free(bin);
	
	return 0;
usage:
	printf("usage: %s <number> <frombase> <tobase>\n", argv[0]);
	return 1;
}
