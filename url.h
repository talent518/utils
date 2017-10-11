#ifndef URL_H
#define URL_H

typedef struct _url_t {
	char *scheme;
	char *user;
	char *pass;
	char *host;
	unsigned short port;
	char *path;
	char *query;
	char *fragment;
} url_t;

void url_free(url_t *theurl);
url_t *url_parse(char const *str);
url_t *url_parse_ex(char const *str, size_t length);
size_t url_decode(char *str, size_t len); /* return value: length of decoded string */
size_t raw_url_decode(char *str, size_t len); /* return value: length of decoded string */
char *url_encode(char const *s, size_t len);
char *raw_url_encode(char const *s, size_t len);
char *replace_controlchars_ex(char *str, size_t len);

#endif /* URL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
