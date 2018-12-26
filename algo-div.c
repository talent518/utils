/**
 * 使用坚式乘法运算算法实现的无限位整数乘法运算
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *strdiv(char *astr, char *bstr, int scale, char **mod) {
	register char *p; // 指针
	char *p0, *ap, *bp, *ep;
	register int n, i, j;
	unsigned int an, bn, flag = 0;
	
	p = astr;
	n = 1;

valid:
	if(*p == '+' || *p == '-') p++;
	if(*p<'1' || *p>'9') return NULL;
	p0 = p;
	for(; *p>='0' && *p<='9'; p++);
	if(*p || p == p0) return NULL;
	if(n) {
		n = 0;
		p = bstr;
		goto valid;
	}
	
	if(*astr == '+') astr++;
	if(*bstr == '+') bstr++;
	
	if(!strcmp(astr, "0") || !strcmp(astr, "-0") || !strcmp(bstr, "0") || !strcmp(bstr, "-0")) {
		p = strdup("0");
	} else {
		// b操作数
		p = bstr;
		if(*bstr == '-') {
			p++;
			flag++;
		}
		bn = strlen(p);
		bp = (unsigned char *) malloc(bn);
		p0 = bp;
		while(*p) {
			*p0++ = *p++ - '0';
		}

		// a操作数
		p = astr;
		if(*astr == '-') {
			p++;
			flag++;
		}
		an = strlen(p);
		n = (bn>an?bn:an) + 1;
		ap = (unsigned char *) malloc(n);
		p0 = ap;
		while(*p) {
			*p0++ = *p++ - '0';
		}
		for(i=an; i<n; i++) *p0++ = '\0';

		// e结果
		n = an + 1 - bn;
		if(n<1) {
			n = 1;
		}
		if(scale) {
			n += scale+1;
		}
		if(flag == 1) {
			n++;
		}
		ep = (unsigned char *) malloc(n+1);
		for(i=0; i<n; i++) ep[i] = '0';
		ep[n] = '\0';
		p0 = ep;
		if(flag == 1) {
			*p0++ = '-';
		}
		flag = 0;
		i = 0;
		if(an < bn) {
			n = an;
		} else {
			n = bn;
		}
	
	divmod:
		if(n<bn) {
			n++;
			p0++;
		} else {
			while(n>=bn) {
				if(n == bn && memcmp(ap+i, bp, bn)<0) {
					n++;

					if(i+n>an) {
						p0++;
						break;
					}
				}
				
			subbp:
				*p0 += 1;
				p = ap+i+n-1;
				for(j=bn-1; j>=0; j--) {
					if(*p>=bp[j]) {
						*p -= bp[j];
						p--;
						continue;
					}
					*p = 10 + *p - bp[j];
					p--;
					*p -= 1;
				}
				for(p = ap+i; n>0 && i<an && p < ap+i+n && *p == '\0'; n--,i++,p++);
				if(n == bn) {
					if(memcmp(ap+i, bp, bn)>=0) goto subbp;
					n++;
					p0++;
				} else if(n > bn) {
					goto subbp;
				} else {
					do {
						n++;
						p0++;
					} while(n<bn && i+n<=an);
				}
				if(i+n>an) break;
			}
		}
		
		if(scale) { // 小数
			if(i<an) {
				if(!flag) {
					*p0++ = '.';
					flag = 1;
				}
				scale--;
				if(n<=bn+1 && n>an) {
					an++;
					goto divmod;
				}
				while(i+n>an) {
					for(j=i; j<an; j++) ap[j-1] = ap[j];
					ap[an-1] = '\0';
					i--;
				}
				goto divmod;
			} else {
				*p0 = '\0';
			}
		} else if(flag) {
			*p0 = '\0';
		} else { // 余数
			*p0 = '\0';
			
			if(i>=an) i=an-1;
			p = (char *) malloc(an-i+1);
			*mod = p;
			for(; i<an; i++) {
				*p++ = '0' + ap[i];
			}
			*p = '\0';
		}
		
		p = ep;
		free(ap);
		free(bp);
	}

	return p;
}

int main(int argc, char *argv[]) {
	char *ret, *a, *b, *p, *p0, flag, *mod = NULL;

	if(argc >= 3 && (ret = strdiv(argv[1], argv[2], argc == 4 ? abs(atoi(argv[3])) : 0, &mod))) {
		a = argv[1];
		if(*a == '+') a++;
		b = argv[2];
		if(*b == '+') b++;

		p = a;
		flag = 1;
	fmt:
		if(*p == '-') {
			p++;
		}
		p0 = p;
		while(*p == '-'|| *p == '0') p++;
		if(p0 != p) {
			while(*p) *p0++ = *p++;
		#if 1
			while(*p0) *p0++ = '\0';
		#else
			*p0 = '\0';
		#endif
		}
		if(flag) {
			flag = 0;
			p = b;
			goto fmt;
		}
		if(mod) {
			printf("%s / %s = %s mod %s\n", a, b, ret, mod);
			free(mod);
		} else {
			printf("%s / %s = %s\n", a, b, ret);
		}
		free(ret);
		return 0;
	} else {
		printf("usage: %s <numeric> <numeric> [scale]\n", argv[0]);
		return 1;
	}
}
