// gcc -O3 -lm -o crypt base64.c md5.c crypt.c && ./crypt

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/time.h>

#include "md5.h"
#include "base64.h"
#include "crypt.h"

#if 0
	#define nprint(result, retlen) printf("%s(%d): \"", __func__, __LINE__);fwrite(result, retlen, 1, stdout);printf("\"\n")
#else
	#define nprint(result, retlen)
#endif

#define MICRO_IN_SEC 1000000.00

inline double microtime()
{
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double)(tp.tv_sec + tp.tv_usec / MICRO_IN_SEC);
}

unsigned int crypt_code(const char *str, unsigned int len, char **ret, const char *key, bool mode, unsigned int expiry) {
	MD5_CTX md5;
	unsigned char digest[16];
	unsigned char _key[32], keya[32], keyb[32], keyc[32], cryptkey[64];
	unsigned char buffer[64], *ptr = NULL;
	unsigned int _len = 0;
	unsigned int ckey_length = 20;	// 随机密钥长度 取值 0-32;
								// 加入随机密钥，可以令密文无任何规律，即便是原文和密钥完全相同，加密结果也会每次不同，增大破解难度。
								// 取值越大，密文变动规律越大，密文变化 = 16 的 $ckey_length 次方
								// 当此值为 0 时，则不产生随机密钥
	
	if(!len || (mode && len<=ckey_length)) {
		return 0;
	}
	
	MD5DigestHex(&md5, key, strlen(key), digest, _key);nprint(_key, 32);
	MD5DigestHex(&md5, _key, 16, digest, keya);nprint(keya, 32);
	MD5DigestHex(&md5, _key+16, 16, digest, keyb);nprint(keyb, 32);
	
	memset(keyc, 0, sizeof(keyc));
	if(ckey_length) {
		if(mode) {
			memcpy(keyc, str, ckey_length);
		} else {
			snprintf(buffer, sizeof(buffer), "%lf", microtime());
			MD5Digest(&md5, buffer, strlen(buffer), digest);

			ptr = keyc;
			_len = sizeof(keyc);
			base64_encode(digest, 16, (unsigned char**)&ptr, &_len);
		}
		nprint(keyc, ckey_length);
	}
	
	memcpy(cryptkey, keya, 32);
	memcpy(buffer, keya, 32);
	if(ckey_length) {
		memcpy(buffer+32, keyc, ckey_length);
	}
	MD5DigestHex(&md5, buffer, 32+ckey_length, digest, cryptkey+32);nprint(cryptkey, 64);
	
	unsigned char *data = NULL;
	unsigned int data_len = 0;
	if(mode) {
		base64_decode(str+ckey_length, len-ckey_length, &data, &data_len);
		if(!data || !data_len) {
			return 0;
		}
	} else {
		data_len = len+26;
		data = (char*) malloc(data_len);
		
		sprintf(data, "%010d", expiry ? expiry+(int)time(NULL) : 0);
		
		MD5Init(&md5);
		MD5Update(&md5, (unsigned char *)(str), (unsigned int)(len));
		MD5Update(&md5, keyb, 32);
		MD5Final(&md5, digest);
		str2hex(digest, 16, buffer);
		
		memcpy(data+10, buffer, 16);
		memcpy(data+26, str, len);
		
		nprint(data, data_len);
	}
	
	if(*ret) {
		free((void*)(*ret));
	}
	
	unsigned char *result = (char *) malloc(data_len);
	unsigned int retlen = data_len;
	unsigned char box[256], rndkey[256], tmp;
	register unsigned long int i, j, a;
	
	for(i=0; i<256; i++) {
		box[i] = i;
		rndkey[i] = cryptkey[i%64];
	}
	
	for ( j = i = 0; i < 256; i ++  ) {
		j = ( j + box[i] + rndkey[i] ) % 256;
		tmp = box[i];
		box[i] = box[j];
		box[j] = tmp;
	}
	
	for ( a = j = i = 0; i < data_len; i ++  ) {
		a = ( a + 1 ) % 256;
		j = ( j + box[a] ) % 256;
		tmp = box[a];
		box[a] = box[j];
		box[j] = tmp;
		result[i] = data[i] ^ (box[(box[a] + box[j]) % 256]);
	}

	free(data);
	data = NULL;
	
	if(mode) {
		i = strtol(result, (char**) &data, 10);nprint(result, retlen);
	
		if(i == 0 || i-time(NULL) > 0) {
			MD5Init(&md5);
			MD5Update(&md5, (unsigned char *)(result+26), (unsigned int)(retlen-26));
			MD5Update(&md5, keyb, 32);
			MD5Final(&md5, digest);
			str2hex(digest, 16, buffer);nprint(buffer, 16);nprint(result+10, 16);

			if(!memcmp(result+10, buffer, 16)) {
				retlen -= 26;
				*ret = strndup(result+26, retlen);
				free(result);
				return retlen;
			}
		}
		
		free(result);
		return 0;
	} else if(ckey_length) {
		data_len = retlen;
		retlen = ceil((double) (data_len) / 3.0) * 4 + ckey_length;
		*ret = (char*) malloc(retlen+1);
		memcpy(*ret, keyc, ckey_length);
		ptr = *ret + ckey_length;
		_len = retlen - ckey_length + 1;
		base64_encode(result, data_len, (unsigned char**)&ptr, &_len);
		memset(*ret + retlen, 0, 1);
		
		free(result);
		
		return retlen;
	} else {
		*ret = NULL;
		retlen = 0;
		base64_encode(result, retlen, (unsigned char**)ret, &retlen);
		return retlen;
	}
}

int main(int argc, char *argv[]) {
	char *str = "Crypt加密解密测试!";
	
	char *enc, *dec = NULL;
	enc = crypt_encode(str, strlen(str), "123456", 0);
	
	printf("encode(%d): %s\n", strlen(enc), enc);

	int len = crypt_decode(enc, &dec, "123456", 0);

	printf("decode(%d): %s\n", len, dec);

	return 0;
}

