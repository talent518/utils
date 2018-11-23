/**
 * 使用坚式乘法运算算法实现的无限位整数乘法运算
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *strmul(char *astr, char *bstr) {
	register char *p, *ap, *bp; // 指针
	char *ap0, *bp0, *eq; // 操作数: astr x bstr = eq
	register unsigned int i, a = 0, b = 0, c, n, over;
	
	p = astr;
	n = 1;

valid:
	if(*p == '+' || *p == '-') p++;
	ap0 = p;
	for(; *p>='0' && *p<='9'; p++);
	if(*p || p == ap0) return NULL;
	if(n) {
		n = 0;
		p = bstr;
		goto valid;
	}
	
	if(*astr == '+') astr++;
	if(*bstr == '+') bstr++;
	
	if(!strcmp(astr, "0") || !strcmp(bstr, "0")) {
		p = strdup("0");
	} else {
		a = strlen(astr);
		ap = astr + a - 1; // 操作数a的个位数

		b = strlen(bstr);
		bp = bstr + b - 1; // 操作数b的个位数

		c = a + b;
		eq = (char*) malloc(c + 1); // 运算结果的内存分配
		memset(eq, '0', c);
		*(eq+c) = '\0';

		ap0 = astr; // 操作数a的最高数
		if(*ap0 == '-') {
			ap0++;
		}
		bp0 = bstr; // 操作数b的最高数
		if(*bp0 == '-') {
			bp0++;
		}
		
		c--;
		for(bp = bstr + b - 1; bp>=bp0; bp--) {
			if(*bp == '0') {
				c--;
				continue;
			}
			p = eq + c; // 运算结果的未位，根据b操作数进行缩进
			over = 0; // 进位
			for(ap = astr + a - 1; ap>=ap0; ap--) {
				n = (*p - '0') + (*ap - '0') * (*bp - '0') + over;
				*p-- = '0' + n%10;
				over = n/10;
			}
			for(; over>0;) {
				n = (*p - '0') + over;
				*p-- = '0' + n%10;
				over = n/10;
			}
			c--;
		}
	
		// 最后整理结果
		for(p = eq; *p == '0'; p++);
		if(*astr == '-') {
			if(*bstr != '-') {
				*--p = '-';
			}
		} else if(*bstr == '-') {
			*--p = '-';
		}

		p = strdup(p);
		free(eq);
	}

	return p;
}

int main(int argc, char *argv[]) {
	char *ret, *a, *b, *p, *p0, flag;

	if(argc == 3 && (ret = strmul(argv[1], argv[2]))) {
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
		printf("%s x %s = %s\n", a, b, ret);
		free(ret);
		return 0;
	} else {
		printf("usage: %s <numeric> <numeric>\n", argv[0]);
		return 1;
	}
}
