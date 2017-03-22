#ifndef MD5_H
#define MD5_H

#include <stdio.h>
#include <string.h>

#ifndef bool
typedef enum {
	false,
	true
} bool;
#endif

typedef struct {
	unsigned int count[2];
	unsigned int state[4];
	unsigned char buffer[64];
} MD5_CTX;

void MD5Init(MD5_CTX *context);
void MD5Update(MD5_CTX *context, unsigned char *input, unsigned int inputlen);
void MD5Final(MD5_CTX *context, unsigned char digest[16]);

#define str2hex(str, len, hex) do { \
		register char *__ptr = hex; \
		register int __i; \
		for(__i=0; __i<len; __i++) { \
			sprintf(__ptr, "%02x", str[__i]); \
			__ptr += 2; \
		} \
	} while(0)

#define MD5Digest(md5, str, len, digest) do { \
		MD5Init(md5); \
		MD5Update(md5, (unsigned char *)(str), (unsigned int)(len)); \
		MD5Final(md5, digest); \
	} while(0)

#define MD5DigestHex(md5, str, len, digest, hex) do { \
		MD5Digest(md5, str, len, digest); \
		str2hex(digest, 16, hex); \
	} while(0)

#define MD5Hex(md5, str, len, hex) do { \
		unsigned char digest[16]; \
		MD5DigestHex(md5, str, len, digest, hex); \
	} while(0)

#endif

