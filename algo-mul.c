#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
	register char *p, *ap, *bp, *ep, *pp; // 指针
	char *ab[2], *eq; // 操作数: ab[0] x ab[1] = eq
	register unsigned int i, a = 0, b = 0, c, n, over;
	
	if(argc == 3) {
		for(i = 1; i < argc; i++) {
			for(p = argv[i]; *p>='0' && *p<='9'; p++);
			if(*p || p == argv[i]) goto usage;
		}
	} else {
		goto usage;
	}
	
	for(i = 1; i < argc; i++) {
		p = argv[i];
		while(*p == '0') {
			p++;
		}
		if(*p) {
			ab[i-1] = strdup(p);
		} else {
			ab[i-1] = strdup("0");
		}
	}
	
	if(!strcmp(ab[0], "0") || !strcmp(ab[1], "0")) {
		eq = strdup("0");
		pp = eq;
	} else {
		a = strlen(ab[0]);
		b = strlen(ab[1]);
		if(b > a) {
			n = a;
			a = b;
			b = n;
			p = ab[0];
			ab[0] = ab[1];
			ab[1] = p;
		}
		
		ap = ab[0] + a - 1;
		bp = ab[1] + b - 1;
		
		c = a + b;
		eq = (char*) malloc(c + 1);
		for(i = 0; i < c; i++) eq[i] = '0';
		eq[c] = '\0';
		
		c--;
		for(bp = ab[1] + b - 1; bp>=ab[1]; bp--) {
			if(*bp == '0') {
				c--;
				continue;
			}
			ep = eq + c;
			over = 0;
			for(ap = ab[0] + a - 1; ap>=ab[0]; ap--) {
				n = (*ep - '0') + (*ap - '0') * (*bp - '0') + over;
				*ep-- = '0' + n%10;
				over = n/10;
			}
			for(; over>0;) {
				n = (*ep - '0') + over;
				*ep-- = '0' + n%10;
				over = n/10;
			}
			c--;
		}
	
		for(pp = eq; *pp == '0'; pp++);
	}
	
	printf("%s x %s = %s\n", ab[0], ab[1], pp);
	
	free(ab[0]);
	free(ab[1]);
	free(eq);

	return 0;
usage:
	printf("usage: %s <numeric> <numeric>\n", argv[0]);
	return 1;
}

