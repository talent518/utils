#ifndef _CRYPT_H
#define _CRYPT_H

#include <string.h>
#include <stdbool.h>

unsigned int crypt_code(const char *str, unsigned int len, char **ret, const char *key, bool mode, unsigned int expiry);

static inline char *crypt_encode(const char *str, unsigned int len, const char *key, unsigned int expiry) {
	char *enc = NULL;

	crypt_code(str, len, &enc, key, false, expiry);

	return enc;
}

static inline unsigned int crypt_decode(const char *dec, char **data, const char *key, unsigned int expiry) {
	return crypt_code(dec, strlen(dec), data, key, true, expiry);
}

#endif
