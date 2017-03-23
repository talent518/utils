#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <math.h>

#include "base64.h"

/*
* base64格式编码地图
*/
static const unsigned char base64_enc_map[64]={
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
	'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
	'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd',
	'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x',
	'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', '+', '/'
};

/*
* base64格式解码地图
*/
static const unsigned char base64_dec_map[128]={
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 127, 127, 127, 127, 127, 127, 127,
	127, 127, 127, 62, 127, 127, 127, 63, 52, 53,
	54, 55, 56, 57, 58, 59, 60, 61, 127, 127,
	127, 127, 127, 127, 127, 0, 1, 2, 3, 4,
	5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 22, 23, 24,
	25, 127, 127, 127, 127, 127, 127, 26, 27, 28,
	29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
	39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 127, 127, 127, 127, 127
};

/*
* 把字符串src(长度slen)编码为base64格式串并写入dst(长度dlen)
* 返回非0为成功，否则为失败
*/
int base64_encode(const unsigned char *src, int slen, unsigned char **dst, int *dlen){
	int i,n;
	int C1,C2,C3;
	unsigned char *p;

	if(slen==0){
		*dlen=0;
		return(0);
	}
	n=ceil((double) slen / 3.0)*4;
	if((*dst) && (*dlen)<n+1) {
		free(*dst);
		*dst = NULL;
	}
	if(!(*dst)) {
		*dst=(unsigned char*)malloc(n+1);
	}
	*dlen=n;

	n=(slen/3)*3;

	for(i=0,p=*dst;i<n;i+=3){
		C1 = *src++;
		C2 = *src++;
		C3 = *src++;

		*p++ = base64_enc_map[(C1 >> 2) & 0x3F];
		*p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];
		*p++ = base64_enc_map[(((C2 & 15) << 2) + (C3 >> 6)) & 0x3F];
		*p++ = base64_enc_map[C3 & 0x3F];
	}

	if(i<slen){
		C1 = *src++;
		C2 = ((i + 1) < slen) ? *src++ : 0;

		*p++ = base64_enc_map[(C1 >> 2) & 0x3F];
		*p++ = base64_enc_map[(((C1 & 3) << 4) + (C2 >> 4)) & 0x3F];

		if(i+1<slen){
			*p++ = base64_enc_map[((C2 & 15) << 2) & 0x3F];
		}
	}
	
	for(;p-*dst<*dlen;){
		// printf("p:%d\n",p);
		*p++ = '=';
	}

	*p = 0;
	return(1);
}

/*
* 把base64格式串src(长度slen)解码为字符串并写入dst(长度dlen)
* 返回非0为成功，否则为失败
*/
int base64_decode(const unsigned char *src, int slen, unsigned char **dst, int *dlen){
	#define SRC_CHECK *src>127 || base64_dec_map[*src]==127
	int i,n;
	unsigned short x;
	unsigned char *p;
	*dlen=0;
	if(slen==0 || SRC_CHECK){
		return(0);
	}
	n=(slen/4)*3;
	if(*dlen<n+1 && *dst) {
		free(*dst);
		*dst = NULL;
	}
	if(!*dst) {
		*dst=(unsigned char*)malloc(n+1);
	}

	x=n=0;
	p=*dst;
	for(i=0;i<slen;i++){
		if(SRC_CHECK) {
			continue;
		}
		x=(x<<6) | (base64_dec_map[*src] & 0x3F);
		n+=6;
		if(n>=8){
			*p++ = (unsigned char)(x>>(n-8));
			n-=8;
		}
		src++;
	}
	*p = 0;
	*dlen=p-*dst;
	return(1);
}
/*
int main(int argc, char *argv[]) {
	char source[]="Base64编码与解码!!";
	char *encode=NULL,*decode=NULL;
	int source_len=strlen(source),encode_len=0,decode_len=0;

	base64_encode(source,source_len,&encode,&encode_len);
	base64_decode(encode,encode_len,&decode,&decode_len);

	printf("source:%s\nsource_len:%d\n\nencode:%s\nencode_len:%d\n\ndecode:%s\ndecode_len:%d\n", source,source_len,encode,encode_len,decode,decode_len);

	return 0;
}
*/
