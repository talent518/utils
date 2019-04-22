// gcc -w -o url url.c && ./url 'https://github.com/talent518/calc?as=23&ew=23#fdsdasdf'

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "url.h"

/* {{{ free_url
 */
void url_free(url_t *theurl)
{
	if (theurl->scheme)
		free(theurl->scheme);
	if (theurl->user)
		free(theurl->user);
	if (theurl->pass)
		free(theurl->pass);
	if (theurl->host)
		free(theurl->host);
	if (theurl->path)
		free(theurl->path);
	if (theurl->query)
		free(theurl->query);
	if (theurl->fragment)
		free(theurl->fragment);
	free(theurl);
}
/* }}} */

/* {{{ replace_controlchars
 */
char *replace_controlchars_ex(char *str, size_t len)
{
	unsigned char *s = (unsigned char *)str;
	unsigned char *e = (unsigned char *)str + len;

	if (!str) {
		return (NULL);
	}

	while (s < e) {

		if (iscntrl(*s)) {
			*s='_';
		}
		s++;
	}

	return (str);
}
/* }}} */

char *replace_controlchars(char *str)
{
	return replace_controlchars_ex(str, strlen(str));
}

url_t *url_parse(char const *str)
{
	return url_parse_ex(str, strlen(str));
}

/* {{{ url_parse
 */
url_t *url_parse_ex(char const *str, size_t length)
{
	char port_buf[6];
	url_t *ret = (url_t *)calloc(sizeof(url_t), 1);
	char const *s, *e, *p, *pp, *ue;

	s = str;
	ue = s + length;

	/* parse scheme */
	if ((e = memchr(s, ':', length)) && e != s) {
		/* validate scheme */
		p = s;
		while (p < e) {
			/* scheme = 1*[ lowalpha | digit | "+" | "-" | "." ] */
			if (!isalpha(*p) && !isdigit(*p) && *p != '+' && *p != '.' && *p != '-') {
				if (e + 1 < ue && e < s + strcspn(s, "?#")) {
					goto parse_port;
				} else {
					goto just_path;
				}
			}
			p++;
		}

		if (e + 1 == ue) { /* only scheme is available */
			ret->scheme = strndup(s, (e - s));
			replace_controlchars_ex(ret->scheme, (e - s));
			return ret;
		}

		/*
		 * certain schemas like mailto: and zlib: may not have any / after them
		 * this check ensures we support those.
		 */
		if (*(e+1) != '/') {
			/* check if the data we get is a port this allows us to
			 * correctly parse things like a.com:80
			 */
			p = e + 1;
			while (p < ue && isdigit(*p)) {
				p++;
			}

			if ((p == ue || *p == '/') && (p - e) < 7) {
				goto parse_port;
			}

			ret->scheme = strndup(s, (e-s));
			replace_controlchars_ex(ret->scheme, (e - s));

			s = e + 1;
			goto just_path;
		} else {
			ret->scheme = strndup(s, (e-s));
			replace_controlchars_ex(ret->scheme, (e - s));

			if (e + 2 < ue && *(e + 2) == '/') {
				s = e + 3;
				if (!strncasecmp("file", ret->scheme, sizeof("file"))) {
					if (e + 3 < ue && *(e + 3) == '/') {
						/* support windows drive letters as in:
						   file:///c:/somedir/file.txt
						*/
						if (e + 5 < ue && *(e + 5) == ':') {
							s = e + 4;
						}
						goto just_path;
					}
				}
			} else {
				s = e + 1;
				goto just_path;
			}
		}
	} else if (e) { /* no scheme; starts with colon: look for port */
		parse_port:
		p = e + 1;
		pp = p;

		while (pp < ue && pp - p < 6 && isdigit(*pp)) {
			pp++;
		}

		if (pp - p > 0 && pp - p < 6 && (pp == ue || *pp == '/')) {
			unsigned short port;
			memcpy(port_buf, p, (pp - p));
			port_buf[pp - p] = '\0';
			port = atoi(port_buf);
			if (port > 0 && port <= 65535) {
				ret->port = (unsigned short) port;
				if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
				    s += 2;
				}
			} else {
				if (ret->scheme) free(ret->scheme);
				free(ret);
				return NULL;
			}
		} else if (p == pp && pp == ue) {
			if (ret->scheme) free(ret->scheme);
			free(ret);
			return NULL;
		} else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
			s += 2;
		} else {
			goto just_path;
		}
	} else if (s + 1 < ue && *s == '/' && *(s + 1) == '/') { /* relative-scheme URL */
		s += 2;
	} else {
		goto just_path;
	}

	/* Binary-safe strcspn(s, "/?#") */
	e = ue;
	if ((p = memchr(s, '/', e - s))) {
		e = p;
	}
	if ((p = memchr(s, '?', e - s))) {
		e = p;
	}
	if ((p = memchr(s, '#', e - s))) {
		e = p;
	}

	/* check for login and password */
	if ((p = memrchr(s, '@', (e-s)))) {
		if ((pp = memchr(s, ':', (p-s)))) {
			ret->user = strndup(s, (pp-s));
			replace_controlchars_ex(ret->user, (pp - s));

			pp++;
			ret->pass = strndup(pp, (p-pp));
			replace_controlchars_ex(ret->pass, (p-pp));
		} else {
			ret->user = strndup(s, (p-s));
			replace_controlchars_ex(ret->user, (p-s));
		}

		s = p + 1;
	}

	/* check for port */
	if (s < ue && *s == '[' && *(e-1) == ']') {
		/* Short circuit portscan,
		   we're dealing with an
		   IPv6 embedded address */
		p = NULL;
	} else {
		p = memrchr(s, ':', (e-s));
	}

	if (p) {
		if (!ret->port) {
			p++;
			if (e-p > 5) { /* port cannot be longer then 5 characters */
				if (ret->scheme) free(ret->scheme);
				if (ret->user) free(ret->user);
				if (ret->pass) free(ret->pass);
				free(ret);
				return NULL;
			} else if (e - p > 0) {
				unsigned short port;
				memcpy(port_buf, p, (e - p));
				port_buf[e - p] = '\0';
				port = atoi(port_buf);
				if (port > 0 && port <= 65535) {
					ret->port = (unsigned short)port;
				} else {
					if (ret->scheme) free(ret->scheme);
					if (ret->user) free(ret->user);
					if (ret->pass) free(ret->pass);
					free(ret);
					return NULL;
				}
			}
			p--;
		}
	} else {
		p = e;
	}

	/* check if we have a valid host, if we don't reject the string as url */
	if ((p-s) < 1) {
		if (ret->scheme) free(ret->scheme);
		if (ret->user) free(ret->user);
		if (ret->pass) free(ret->pass);
		free(ret);
		return NULL;
	}

	ret->host = strndup(s, (p-s));
	replace_controlchars_ex(ret->host, (p - s));

	if (e == ue) {
		return ret;
	}

	s = e;

	just_path:

	e = ue;
	p = memchr(s, '#', (e - s));
	if (p) {
		p++;
		if (p < e) {
			ret->fragment = strndup(p, (e - p));
			replace_controlchars_ex(ret->fragment, (e - p));
		}
		e = p-1;
	}

	p = memchr(s, '?', (e - s));
	if (p) {
		p++;
		if (p < e) {
			ret->query = strndup(p, (e - p));
			replace_controlchars_ex(ret->query, (e - p));
		}
		e = p-1;
	}

	if (s < e || s == ue) {
		ret->path = strndup(s, (e - s));
		replace_controlchars_ex(ret->path, (e - s));
	}

	return ret;
}
/* }}} */

/* {{{ htoi
 */
static int htoi(char *s)
{
	int value;
	int c;

	c = ((unsigned char *)s)[0];
	if (isupper(c))
		c = tolower(c);
	value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

	c = ((unsigned char *)s)[1];
	if (isupper(c))
		c = tolower(c);
	value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

	return (value);
}
/* }}} */

/* rfc1738:

   ...The characters ";",
   "/", "?", ":", "@", "=" and "&" are the characters which may be
   reserved for special meaning within a scheme...

   ...Thus, only alphanumerics, the special characters "$-_.+!*'(),", and
   reserved characters used for their reserved purposes may be used
   unencoded within a URL...

   For added safety, we only leave -_. unencoded.
 */

static unsigned char hexchars[] = "0123456789ABCDEF";

/* {{{ url_encode
 */
char *url_encode(char const *s, size_t len)
{
	register unsigned char c;
	unsigned char *to;
	unsigned char const *from, *end;

	from = (unsigned char *)s;
	end = (unsigned char *)s + len;
	to = (unsigned char*)malloc(3*len);

	while (from < end) {
		c = *from++;

		if (c == ' ') {
			*to++ = '+';
		} else if ((c < '0' && c != '-' && c != '.') ||
				   (c < 'A' && c > '9') ||
				   (c > 'Z' && c < 'a' && c != '_') ||
				   (c > 'z')) {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
		} else {
			*to++ = c;
		}
	}
	*to = '\0';

	return (char *)to;
}
/* }}} */

/* {{{ url_decode
 */
size_t url_decode(char *str, size_t len)
{
	char *dest = str;
	char *data = str;

	while (len--) {
		if (*data == '+') {
			*dest = ' ';
		}
		else if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1))
				 && isxdigit((int) *(data + 2))) {
			*dest = (char) htoi(data + 1);
			data += 2;
			len -= 2;
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	*dest = '\0';
	return dest - str;
}
/* }}} */

/* {{{ raw_url_encode
 */
char *raw_url_encode(char const *s, size_t len)
{
	register size_t x, y;
	unsigned char *str = (unsigned char *)malloc(3*len);

	for (x = 0, y = 0; len--; x++, y++) {
		str[y] = (unsigned char) s[x];
		if ((str[y] < '0' && str[y] != '-' && str[y] != '.') ||
			(str[y] < 'A' && str[y] > '9') ||
			(str[y] > 'Z' && str[y] < 'a' && str[y] != '_') ||
			(str[y] > 'z' && str[y] != '~')) {
			str[y++] = '%';
			str[y++] = hexchars[(unsigned char) s[x] >> 4];
			str[y] = hexchars[(unsigned char) s[x] & 15];
		}
	}
	str[y] = '\0';

	return (char*)str;
}
/* }}} */

int main(int argc, char **argv) {
	url_t *url;
	int i;
	for(i=1;i<argc;i++) {
		url = url_parse(argv[i]);

		printf("URL: %s\n", argv[i]);
		printf("  scheme: %s\n", url->scheme);
		printf("  user: %s\n", url->user);
		printf("  pass: %s\n", url->pass);
		printf("  host: %s\n", url->host);
		printf("  port: %d\n", url->port ? url->port : (!strcmp(url->scheme, "http") ? 80 : (!strcmp(url->scheme, "https") ? 443 : (!strcmp(url->scheme, "ftp") ? 21 : 0))));
		printf("  path: %s\n", url->path);
		printf("  query: %s\n", url->query);
		printf("  fragment: %s\n", url->fragment);

		url_free(url);
	}
}
