#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HAVE_STRMUL 1
#include "algo-mul.c"

// tstr = vstr*vstr*300*i+vstr*30*i*i+i*i*i
// return tstr > nstr
int str_val(char *tstr, char *vstr, char i, char *nstr) {
	int tn, nn, n;
	char *tp1, *tp2, *tp3, *vp1, *vp2, *vp3, *p;
	char istr[2] = {i+'0', '\0'};
	
	if(*vstr == '\0') {
		sprintf(tstr, "%d", i*i*i);
	} else if(i == 0) {
		strcpy(tstr, "0");
	} else {
		// vstr*vstr*300*i
		tp1 = tp2 = strmul(vstr, vstr);
		tp1 = strmul(tp1, "300");
		free(tp2);
		vp1 = strmul(tp1, istr);
		free(tp1);
		
		// vstr*30*i*i
		tp1 = tp2 = strmul(vstr, "30");
		tp1 = strmul(tp1, istr);
		free(tp2);
		vp2 = strmul(tp1, istr);
		free(tp1);
		
		// i*i*i
		tp1 = strmul(istr, istr);
		vp3 = strmul(tp1, istr);
		free(tp1);
		
		nn = 0;
		
	#define NN(i) do { \
		n = strlen(vp##i); \
		tp##i = vp##i+n-1; \
		if(n>nn) nn = n; \
	} while(0)
		NN(1);NN(2);NN(3);
	#undef NN
		
		p = tstr + nn + 1;
		memset(tstr, '0', nn);
		*p-- = '\0';
		n = 0;
		do {
			if(tp1>=vp1) {
				n+=*tp1---'0';
			}
			if(tp2>=vp2) {
				n+=*tp2---'0';
			}
			if(tp3>=vp3) {
				n+=*tp3---'0';
			}
			*p-- = (n%10) + '0';
			n/=10;
		} while(tp1>=vp1 || tp2>=vp2 || tp3>=vp3);
		while(n) {
			*p-- = (n%10) + '0';
			n/=10;
		}
		p++;
		if(p>tstr) {
			tp1 = tstr;
			while(*p) *tp1++ = *p++;
			*tp1 = '\0';
		}
		
		// printf("vstr: %s, i: %s, vstr*vstr*300*i+vstr*30*i*i+i*i*i = %s+%s+%s = %s\n", vstr, istr, vp1, vp2, vp3, tstr);
		
		free(vp1);
		free(vp2);
		free(vp3);
	}
	
	if(i == 9) {
		tp1 = nstr;
		while(*tp1 == '0') tp1++;
		if(*tp1 && tp1>nstr) {
			tp2 = nstr;
			while(*tp1) *tp2++=*tp1++;
			*tp2 = '\0';
		} else if(!*tp1) {
			*nstr = '\0';
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

char *str_cbrt(const char *str, int scale) {
	register char *p, *pv;
	register char i = 0;
	char *nstr, *vstr, *tstr;
	int an = 0, bn = 0, n = 0;
	const int N = 3;
	char *val = NULL, isdot = 0;
	
	if(scale <= 0) {
		return NULL;
	}
	
	p = (char*) str;
	
	if(*p == '-') {
		p++;
		isdot = 1;
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
		if((isdot == 0 && *str == '0' || isdot && *(str+1) == '0')) {
			return strdup("0");
		} else if(isdot == 0 && *str == '1') {
			return strdup("1");
		} else if(isdot == 1 && *(str+1) == '1') {
			return strdup("-1");
		}
	}
	
	if(isdot) n++;
	
	if(an) {
		n += (an+2)/3;
	} else {
		n ++;
	}
	
	if(scale) {
		n += scale + 1;
	}
	
	nstr = (char *) malloc(n*N);
	memset(nstr, '\0', n*N);
	
	vstr = (char *) malloc(n*N);
	memset(vstr, '\0', n*N);
	
	tstr = (char *) malloc(n*N);
	memset(tstr, '\0', n*N);
	
	val = (char *) malloc(n+1);
	memset(val, '\0', n+1);
	
	pv = val;
	p = (char*) str;
	
	if(isdot) {
		*pv = '-';
		pv++;
		p++;
	}
	
	isdot = 1;
	if(an == 1 && *p == '0') {
		an--;
		p += 2;
		*pv++ = '0';
		*pv++ = '.';
		isdot = 0;
	}
	
loop:
	if(!strcmp(nstr, "0")) *nstr = '\0';
	if(!strcmp(vstr, "0")) *vstr = '\0';
	if(an) {
		if(an % 3) {
			strncpy(nstr, p, an % 3);
			p+=an % 3;
			an-=an % 3;
		} else {
			strncat(nstr, p, 3);
			an-=3;
			p+=3;
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
		if(isdot) {
			*pv++ = '.';
			isdot = 0;
		}
		if(bn > 2) {
			strncat(nstr, p, 3);
			p+=3;
			bn-=3;
		} else {
			strncat(nstr, p, bn);
			strncat(nstr, "00", 3-bn);
			p+=bn;
			bn = 0;
		}
	} else if(scale == 0) {
		free(nstr);
		free(vstr);
		free(tstr);
		return val;
	} else {
		if(isdot) {
			*pv++ = '.';
			isdot = 0;
		}
		strncat(nstr, "000", 3);
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
	if(argc >= 2 && (s = str_cbrt(argv[1], argc>2 ? atoi(argv[2]) : 3)) != NULL) {
		printf("The cube root of %s is %s\n", argv[1], s);
		free(s);
	} else {
		fprintf(stderr, "usage: %s <number> [scale]\n", argv[0]);
	}

	return 0;
}

