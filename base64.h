#ifndef _BASE64_H
#define _BASE64_H

int base64_encode(const unsigned char *src, int slen, unsigned char **dst, int *dlen);
int base64_decode(const unsigned char *src, int slen, unsigned char **dst, int *dlen);

#endif
