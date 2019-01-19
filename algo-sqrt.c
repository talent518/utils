#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// tstr = (vstr*20+i)*i
// return tstr > nstr
int str_val(char *tstr, char *vstr, char i, char *nstr) {
	int tn, nn, n;
	char *tp, *vp;
	
	if(*vstr == '\0') {
		sprintf(tstr, "%d", i*i);
	} else if(i == 0) {
		strcpy(tstr, "0");
	} else {
		nn = strlen(vstr);
		tn = nn+3;
		memset(tstr, '0', tn);
		tp = tstr + tn;
		*tp-- = '0' + i;
		vp = vstr + nn -1;
		n = 0;
		while(vp >= vstr) {
			n += (*vp-- - '0') * 2;
			*tp-- = (n%10) + '0';
			n /= 10;
		}
		if(n) *tp = '0' + n;
		
		if(i>1) {
			n = 0;
			tp = tstr + tn;
			while(tp >= tstr) {
				n += (*tp - '0') * i;
				*tp-- = (n%10) + '0';
				n /= 10;
			}
			if(n) *tp = '0' + n;
		}
		
		tp = tstr;
		while(*tp == '0' && *(tp+1)) {
			tp++;
		}
		vp = tstr;
		if(tp > vp) {
			while(*tp) {
				*vp++ = *tp++;
			}
			*vp = '\0';
		}
	}
	
	tn = strlen(tstr);
	nn = strlen(nstr);
	if(tn == nn) {
		return memcmp(tstr, nstr, nn) > 0;
	} else if(tn > nn) {
		return 1;
	} else {
		return 0;
	}
}

void str_minus(char *nstr, char *tstr) {
	int nn = strlen(nstr), tn = strlen(tstr), i;
	char *np, *tp;
	
	if(tn == 1 && *tstr == '0') return;
	if(tn > nn) return;
	if(tn == nn && (i = memcmp(tstr, nstr, nn)) >= 0) {
		if(i == 0) {
			strcpy(nstr, "0");
		}
		return;
	}
	
	np = nstr + nn - 1;
	tp = tstr + tn -1;
	
	i = 0;
	while(tp >= tstr) {
		i += (*np - '0');
		nn = (*tp-- - '0');
		if(i >= nn) {
			i -= nn;
			*np-- = '0' + i;
			i = 0;
		} else {
			i += 10;
			i -= nn;
			*np-- = '0' + i;
			i = -1;
		}
	}
	while(np >= nstr) {
		*np-- = '0';
	}
	
	tp = nstr;
	while(*tp == '0') {
		tp++;
	}
	np = nstr;
	if(tp > np) {
		while(*tp) {
			*np++ = *tp++;
		}
		*np = '\0';
	}
}

char *str_sqrt(const char *str, int scale) {
	register char *p, *pv;
	register char i = 0;
	char *nstr, *vstr, *tstr;
	int an = 0, bn = 0, sign = 0, n = 0;
	const int N = 4;
	char *val = NULL;
	
	if(scale <= 0) {
		return NULL;
	}
	
	p = (char*) str;
	
	if(*p == '-') {
		p++;
		sign = 1;
	}
	
	while(*p >= '0' && *p <= '9') {
		p++;
		an++;
	}
	
	if(*p == '.') {
		p++;

		while(*p >= '0' && *p <= '9') {
			p++;
			bn++;
		}
		
		if(bn == 0) {
			return NULL;
		} else if(*(p-1) == '0') {
			return NULL;
		}
	} else if(an == 0) {
		return NULL;
	} else if(an > 1 && *str == '0') {
		return NULL;
	}
	
	if(*p) return NULL;
	
	if(an == 1 && bn == 0) {
		if((sign == 0 && *str == '0' || sign && *(str+1) == '0')) {
			return strdup("0");
		} else if(sign == 0 && *str == '1') {
			return strdup("1");
		} else if(sign == 1 && *(str+1) == '1') {
			return strdup("-1");
		}
	}
	
	if(sign) n++;
	
	if(an) {
		n += (an+1)/2;
	} else {
		n ++;
	}
	
	if(scale) {
		n += scale + 1;
	}
	
	nstr = (char *) malloc(n+N);
	memset(nstr, '\0', n+N);
	
	vstr = (char *) malloc(n+N);
	memset(vstr, '\0', n+N);
	
	tstr = (char *) malloc(n+N);
	memset(tstr, '\0', n+N);
	
	val = (char *) malloc(n+1);
	memset(val, '\0', n+1);
	
	pv = val;
	p = (char*) str;
	
	if(sign) {
		*pv = '-';
		pv++;
		p++;
	}
	
	sign = 1;
	if(an == 1 && *p == '0') {
		an--;
		p += 2;
		*pv++ = '0';
		*pv++ = '.';
		sign = 0;
	}
	
loop:
	if(!strcmp(nstr, "0")) *nstr = '\0';
	if(an) {
		if(an % 2) {
			strncpy(nstr, p, 1);
			p++;
			an--;
		} else {
			strncat(nstr, p, 2);
			an-=2;
			p+=2;
		}
		if(*p == '.') p++;
	} else if(bn) {
		if(scale) {
			scale--;
		} else {
			free(nstr);
			free(vstr);
			free(tstr);
			return val;
		}
		if(sign) {
			*pv++ = '.';
			sign = 0;
		}
		if(bn > 1) {
			strncat(nstr, p, 2);
			p+=2;
			bn-=2;
		} else {
			strncat(nstr, p, 1);
			strncat(nstr, "0", 1);
			p++;
			bn = 0;
		}
	} else if(scale == 0) {
		free(nstr);
		free(vstr);
		free(tstr);
		return val;
	} else {
		if(sign) {
			*pv++ = '.';
			sign = 0;
		}
		strncat(nstr, "00", 2);
		scale--;
	}
	
	i = 9;
	while(i>0 && str_val(tstr, vstr, i, nstr)) i--;
	*pv++ = '0' + i;
	// printf("tstr: %s, nstr: %s, vstr: %s, i: %d\n", tstr, nstr, vstr, i);
	str_minus(nstr, tstr);
	vstr[strlen(vstr)] = i + '0';
	// printf("vstr: %s, tstr: %s, nstr: %s\n", vstr, tstr, nstr);
	
	if(an || bn || strcmp(nstr, "0")) goto loop;
	
	free(nstr);
	free(vstr);
	free(tstr);
	return val;
}

int main(int argc, char *argv[]) {
	char *s;
	if(argc >= 2 && (s = str_sqrt(argv[1], argc>2 ? atoi(argv[2]) : 3)) != NULL) {
		printf("The square root of %s is %s\n", argv[1], s);
		free(s);
	} else {
		fprintf(stderr, "usage: %s <number> [scale]\n", argv[0]);
	}

	return 0;
}

