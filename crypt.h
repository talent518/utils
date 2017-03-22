#ifndef _CRYPT_H
#define _CRYPT_H

unsigned int crypt_code(const char *str, unsigned int strlen, char **ret, const char *key, bool mode, unsigned int expiry);

inline char *crypt_encode(const char *str, unsigned int strlen, const char *key, unsigned int expiry) {
	char *enc = NULL;

	crypt_code(str, strlen, &enc, key, false, expiry);
	
	return enc;
}

inline unsigned int crypt_decode(const char *dec, char **data, const char *key, unsigned int expiry) {
	return crypt_code(dec, strlen(dec), data, key, true, expiry);
}

#endif
