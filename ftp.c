/*
   +----------------------------------------------------------------------+
   | PHP Version 7                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2018 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Andrew Skalski <askalski@chek.com>                          |
   |          Stefan Esser <sesser@php.net> (resume functions)            |
   +----------------------------------------------------------------------+
 */

/* $Id$ */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/select.h>
#ifdef HAVE_FTP_SSL
#include <openssl/ssl.h>
#endif

#include "ftp.h"

#include <poll.h>
typedef struct pollfd php_pollfd;
#define php_poll2(ufds, nfds, timeout)		poll(ufds, nfds, timeout)
#define PHP_POLLREADABLE	(POLLIN|POLLERR|POLLHUP)

#define warning(fmt, args...) { \
    fprintf(stderr, fmt "\n", ##args); \
    fflush(stderr); \
}

#define dprintf(fmt, args...) { \
    fprintf(stderr, fmt, ##args); \
    fflush(stderr); \
}

#define php_stream_fopen_tmpfile tmpfile
#define php_stream_getc fgetc
#define php_stream_rewind rewind
#define php_stream_eof feof
#define php_stream_putc(fp, c) fputc(c, fp)
#define php_stream_write(fp, s, n) fwrite(s, 1, n, fp)
#define php_stream_close fclose
#define php_gmtime_r gmtime_r
#define php_socket_error_str strerror
#define php_connect_nonb(sock, addr, addrlen, timeout) php_network_connect_socket((sock), (addr), (addrlen), 0, (timeout), NULL, NULL)
#define safe_emalloc(nm, size, offset) malloc(nm * size + offset)
#define emalloc malloc
#define ecalloc calloc
#define estrdup strdup
#define estrndup strndup
#define efree(p) free((void*)(p))
#define closesocket close

#define ZEND_LONG_FMT "%ld"
#define HAVE_GETTIMEOFDAY 1
#define HAVE_GETADDRINFO 1
#define HAVE_IPV6 1
#define HAVE_INET_NTOP 1
#define PHPAPI
#define PHP_GAI_STRERROR(x) (gai_strerror(x))

#define SOCK_ERR -1
#define SOCK_CONN_ERR -1
#define SOCK_RECV_ERR -1

#define STREAM_SOCKOP_NONE                (1 << 0)
#define STREAM_SOCKOP_SO_REUSEPORT        (1 << 1)
#define STREAM_SOCKOP_SO_BROADCAST        (1 << 2)
#define STREAM_SOCKOP_IPV6_V6ONLY         (1 << 3)
#define STREAM_SOCKOP_IPV6_V6ONLY_ENABLED (1 << 4)
#define STREAM_SOCKOP_TCP_NODELAY         (1 << 5)

typedef int zend_bool;

/* sends an ftp command, returns true on success, false on error.
 * it sends the string "cmd args\r\n" if args is non-null, or
 * "cmd\r\n" if args is null
 */
static int		ftp_putcmd(	ftpbuf_t *ftp,
					const char *cmd,
					const size_t cmd_len, 
					const char *args, 
					const size_t args_len);

/* wrapper around send/recv to handle timeouts */
static int		my_send(ftpbuf_t *ftp, php_socket_t s, void *buf, size_t len);
static int		my_recv(ftpbuf_t *ftp, php_socket_t s, void *buf, size_t len);
static int		my_accept(ftpbuf_t *ftp, php_socket_t s, struct sockaddr *addr, socklen_t *addrlen);

/* reads a line the socket , returns true on success, false on error */
static int		ftp_readline(ftpbuf_t *ftp);

/* reads an ftp response, returns true on success, false on error */
static int		ftp_getresp(ftpbuf_t *ftp);

/* sets the ftp transfer type */
static int		ftp_type(ftpbuf_t *ftp, ftptype_t type);

/* opens up a data stream */
static databuf_t*	ftp_getdata(ftpbuf_t *ftp);

/* accepts the data connection, returns updated data buffer */
static databuf_t*	data_accept(databuf_t *data, ftpbuf_t *ftp);

/* closes the data connection, returns NULL */
static databuf_t*	data_close(ftpbuf_t *ftp, databuf_t *data);

/* generic file lister */
static char**		ftp_genlist(ftpbuf_t *ftp, const char *cmd, const size_t cmd_len, const char *path, const size_t path_len);

/* IP and port conversion box */
union ipbox {
	struct in_addr	ia[2];
	unsigned short	s[4];
	unsigned char	c[8];
};

static inline int php_pollfd_for_ms(php_socket_t fd, int events, int timeout)
{
	php_pollfd p;
	int n;

	p.fd = fd;
	p.events = events;
	p.revents = 0;

	n = php_poll2(&p, 1, timeout);

	if (n > 0) {
		return p.revents;
	}

	return n;
}


/* {{{ php_network_getaddresses
 * Returns number of addresses, 0 for none/error
 */
PHPAPI int php_network_getaddresses(const char *host, int socktype, struct sockaddr ***sal, char **error_string)
{
	struct sockaddr **sap;
	int n;
#if HAVE_GETADDRINFO
# if HAVE_IPV6
	static int ipv6_borked = -1; /* the way this is used *is* thread safe */
# endif
	struct addrinfo hints, *res, *sai;
#else
	struct hostent *host_info;
	struct in_addr in;
#endif

	if (host == NULL) {
		return 0;
	}
#if HAVE_GETADDRINFO
	memset(&hints, '\0', sizeof(hints));

	hints.ai_family = AF_INET; /* default to regular inet (see below) */
	hints.ai_socktype = socktype;

# if HAVE_IPV6
	/* probe for a working IPv6 stack; even if detected as having v6 at compile
	 * time, at runtime some stacks are slow to resolve or have other issues
	 * if they are not correctly configured.
	 * static variable use is safe here since simple store or fetch operations
	 * are atomic and because the actual probe process is not in danger of
	 * collisions or race conditions. */
	if (ipv6_borked == -1) {
		int s;

		s = socket(PF_INET6, SOCK_DGRAM, 0);
		if (s == SOCK_ERR) {
			ipv6_borked = 1;
		} else {
			ipv6_borked = 0;
			closesocket(s);
		}
	}
	hints.ai_family = ipv6_borked ? AF_INET : AF_UNSPEC;
# endif

	if ((n = getaddrinfo(host, NULL, &hints, &res))) {
		if (error_string) {
			asprintf(error_string, "php_network_getaddresses: getaddrinfo failed: %s", PHP_GAI_STRERROR(n));
			warning("%s", *error_string);
		} else {
			warning("php_network_getaddresses: getaddrinfo failed: %s", PHP_GAI_STRERROR(n));
		}
		return 0;
	} else if (res == NULL) {
		if (error_string) {
			asprintf(error_string, "php_network_getaddresses: getaddrinfo failed (null result pointer) errno=%d", errno);
			warning("%s", *error_string);
		} else {
			warning("php_network_getaddresses: getaddrinfo failed (null result pointer)");
		}
		return 0;
	}

	sai = res;
	for (n = 1; (sai = sai->ai_next) != NULL; n++)
		;

	*sal = safe_emalloc((n + 1), sizeof(*sal), 0);
	sai = res;
	sap = *sal;

	do {
		*sap = emalloc(sai->ai_addrlen);
		memcpy(*sap, sai->ai_addr, sai->ai_addrlen);
		sap++;
	} while ((sai = sai->ai_next) != NULL);

	freeaddrinfo(res);
#else
	if (!inet_aton(host, &in)) {
		if(strlen(host) > MAXFQDNLEN) {
			host_info = NULL;
			errno = E2BIG;
		} else {
			host_info = php_network_gethostbyname(host);
		}
		if (host_info == NULL) {
			if (error_string) {
				asprintf(error_string, "php_network_getaddresses: gethostbyname failed. errno=%d", errno);
				warning("%s", *error_string);
			} else {
				warning("php_network_getaddresses: gethostbyname failed");
			}
			return 0;
		}
		in = *((struct in_addr *) host_info->h_addr);
	}

	*sal = safe_emalloc(2, sizeof(*sal), 0);
	sap = *sal;
	*sap = emalloc(sizeof(struct sockaddr_in));
	(*sap)->sa_family = AF_INET;
	((struct sockaddr_in *)*sap)->sin_addr = in;
	sap++;
	n = 1;
#endif

	*sap = NULL;
	return n;
}
/* }}} */

/* Connect to a socket using an interruptible connect with optional timeout.
 * Optionally, the connect can be made asynchronously, which will implicitly
 * enable non-blocking mode on the socket.
 * */
/* {{{ php_network_connect_socket */
PHPAPI int php_network_connect_socket(php_socket_t sockfd,
		const struct sockaddr *addr,
		socklen_t addrlen,
		int asynchronous,
		struct timeval *timeout,
		char **error_string,
		int *error_code)
{
	if (asynchronous) {
		asprintf(error_string, "Asynchronous connect() not supported on this platform");
	}
	return (connect(sockfd, addr, addrlen) == 0) ? 0 : -1;
}
/* }}} */

/* {{{ sub_times */
static inline void sub_times(struct timeval a, struct timeval b, struct timeval *result)
{
	result->tv_usec = a.tv_usec - b.tv_usec;
	if (result->tv_usec < 0L) {
		a.tv_sec--;
		result->tv_usec += 1000000L;
	}
	result->tv_sec = a.tv_sec - b.tv_sec;
	if (result->tv_sec < 0L) {
		result->tv_sec++;
		result->tv_usec -= 1000000L;
	}
}
/* }}} */

/* {{{ php_network_freeaddresses
 */
PHPAPI void php_network_freeaddresses(struct sockaddr **sal)
{
	struct sockaddr **sap;

	if (sal == NULL)
		return;
	for (sap = sal; *sap != NULL; sap++)
		efree(*sap);
	efree(sal);
}
/* }}} */

/* Connect to a remote host using an interruptible connect with optional timeout.
 * Optionally, the connect can be made asynchronously, which will implicitly
 * enable non-blocking mode on the socket.
 * Returns the connected (or connecting) socket, or -1 on failure.
 * */

/* {{{ php_network_connect_socket_to_host */
php_socket_t php_network_connect_socket_to_host(const char *host, unsigned short port,
		int socktype, int asynchronous, struct timeval *timeout, char **error_string,
		int *error_code, char *bindto, unsigned short bindport, long sockopts
		)
{
	int num_addrs, n, fatal = 0;
	php_socket_t sock;
	struct sockaddr **sal, **psal, *sa;
	struct timeval working_timeout;
	socklen_t socklen;
#if HAVE_GETTIMEOFDAY
	struct timeval limit_time, time_now;
#endif

	num_addrs = php_network_getaddresses(host, socktype, &psal, error_string);

	if (num_addrs == 0) {
		/* could not resolve address(es) */
		return -1;
	}

	if (timeout) {
		memcpy(&working_timeout, timeout, sizeof(working_timeout));
#if HAVE_GETTIMEOFDAY
		gettimeofday(&limit_time, NULL);
		limit_time.tv_sec += working_timeout.tv_sec;
		limit_time.tv_usec += working_timeout.tv_usec;
		if (limit_time.tv_usec >= 1000000) {
			limit_time.tv_usec -= 1000000;
			limit_time.tv_sec++;
		}
#endif
	}

	for (sal = psal; !fatal && *sal != NULL; sal++) {
		sa = *sal;

		/* create a socket for this address */
		sock = socket(sa->sa_family, socktype, 0);

		if (sock == SOCK_ERR) {
			continue;
		}

		switch (sa->sa_family) {
#if HAVE_GETADDRINFO && HAVE_IPV6
			case AF_INET6:
				if (!bindto || strchr(bindto, ':')) {
					((struct sockaddr_in6 *)sa)->sin6_family = sa->sa_family;
					((struct sockaddr_in6 *)sa)->sin6_port = htons(port);
					socklen = sizeof(struct sockaddr_in6);
				} else {
					socklen = 0;
					sa = NULL;
				}
				break;
#endif
			case AF_INET:
				((struct sockaddr_in *)sa)->sin_family = sa->sa_family;
				((struct sockaddr_in *)sa)->sin_port = htons(port);
				socklen = sizeof(struct sockaddr_in);
				break;
			default:
				/* Unknown family */
				socklen = 0;
				sa = NULL;
		}

		if (sa) {
			/* make a connection attempt */

			if (bindto) {
				struct sockaddr *local_address = NULL;
				int local_address_len = 0;

				if (sa->sa_family == AF_INET) {
					struct sockaddr_in *in4 = emalloc(sizeof(struct sockaddr_in));

					local_address = (struct sockaddr*)in4;
					local_address_len = sizeof(struct sockaddr_in);

					in4->sin_family = sa->sa_family;
					in4->sin_port = htons(bindport);
					if (!inet_aton(bindto, &in4->sin_addr)) {
						warning("Invalid IP Address: %s", bindto);
						goto skip_bind;
					}
					memset(&(in4->sin_zero), 0, sizeof(in4->sin_zero));
				}
#if HAVE_IPV6 && HAVE_INET_PTON
				 else { /* IPV6 */
					struct sockaddr_in6 *in6 = emalloc(sizeof(struct sockaddr_in6));

					local_address = (struct sockaddr*)in6;
					local_address_len = sizeof(struct sockaddr_in6);

					in6->sin6_family = sa->sa_family;
					in6->sin6_port = htons(bindport);
					if (inet_pton(AF_INET6, bindto, &in6->sin6_addr) < 1) {
						warning("Invalid IP Address: %s", bindto);
						goto skip_bind;
					}
				}
#endif

				if (!local_address || bind(sock, local_address, local_address_len)) {
					warning("failed to bind to '%s:%d', system said: %s", bindto, bindport, strerror(errno));
				}
skip_bind:
				if (local_address) {
					efree(local_address);
				}
			}
			/* free error string received during previous iteration (if any) */
			if (error_string && *error_string) {
				efree(*error_string);
				*error_string = NULL;
			}

#ifdef SO_BROADCAST
			{
				int val = 1;
				if (sockopts & STREAM_SOCKOP_SO_BROADCAST) {
					setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&val, sizeof(val));
				}
			}
#endif

#ifdef TCP_NODELAY
			{
				int val = 1;
				if (sockopts & STREAM_SOCKOP_TCP_NODELAY) {
					setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(val));
				}
			}
#endif

			n = php_network_connect_socket(sock, sa, socklen, asynchronous,
					timeout ? &working_timeout : NULL,
					error_string, error_code);

			if (n != -1) {
				goto connected;
			}

			/* adjust timeout for next attempt */
#if HAVE_GETTIMEOFDAY
			if (timeout) {
				gettimeofday(&time_now, NULL);

				if (timercmp(&time_now, &limit_time, >=)) {
					/* time limit expired; don't attempt any further connections */
					fatal = 1;
				} else {
					/* work out remaining time */
					sub_times(limit_time, time_now, &working_timeout);
				}
			}
#else
			if (error_code && *error_code == PHP_TIMEOUT_ERROR_VALUE) {
				/* Don't even bother trying to connect to the next alternative;
				 * we have no way to determine how long we have already taken
				 * and it is quite likely that the next attempt will fail too. */
				fatal = 1;
			} else {
				/* re-use the same initial timeout.
				 * Not the best thing, but in practice it should be good-enough */
				if (timeout) {
					memcpy(&working_timeout, timeout, sizeof(working_timeout));
				}
			}
#endif
		}

		closesocket(sock);
	}
	sock = -1;

connected:

	php_network_freeaddresses(psal);

	return sock;
}
/* }}} */

/* {{{ php_any_addr
 * Fills the any (wildcard) address into php_sockaddr_storage
 */
PHPAPI void php_any_addr(int family, php_sockaddr_storage *addr, unsigned short port)
{
	memset(addr, 0, sizeof(php_sockaddr_storage));
	switch (family) {
#if HAVE_IPV6
	case AF_INET6: {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) addr;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = htons(port);
		sin6->sin6_addr = in6addr_any;
		break;
	}
#endif
	case AF_INET: {
		struct sockaddr_in *sin = (struct sockaddr_in *) addr;
		sin->sin_family = AF_INET;
		sin->sin_port = htons(port);
		sin->sin_addr.s_addr = htonl(INADDR_ANY);
		break;
	}
	}
}
/* }}} */

/* {{{ php_sockaddr_size
 * Returns the size of struct sockaddr_xx for the family
 */
PHPAPI int php_sockaddr_size(php_sockaddr_storage *addr)
{
	switch (((struct sockaddr *)addr)->sa_family) {
	case AF_INET:
		return sizeof(struct sockaddr_in);
#if HAVE_IPV6
	case AF_INET6:
		return sizeof(struct sockaddr_in6);
#endif
#ifdef AF_UNIX
	case AF_UNIX:
		return sizeof(struct sockaddr_un);
#endif
	default:
		return 0;
	}
}
/* }}} */

static
int
_ftp_open(ftpbuf_t *ftp)
{
	socklen_t		 size;
	struct timeval tv;
	
	tv.tv_sec = ftp->timeout_sec;
	tv.tv_usec = 0;

	if (ftp->fd != -1) {
#ifdef HAVE_FTP_SSL
		if (ftp->ssl_active) {
			SSL_shutdown(ftp->ssl_handle);
			SSL_free(ftp->ssl_handle);
		}
#endif
		closesocket(ftp->fd);
	}

	ftp->fd = php_network_connect_socket_to_host(ftp->host,
			(unsigned short) (ftp->port), SOCK_STREAM,
			0, &tv, NULL, NULL, NULL, 0, STREAM_SOCKOP_NONE);
	if (ftp->fd == -1) {
		return 0;
	}
	if(ftp->debug) snprintf(ftp->prompt, FTP_BUFSIZE, "...@%s:%d", ftp->host, ftp->port);

	size = sizeof(ftp->localaddr);
	memset(&ftp->localaddr, 0, size);
	if (getsockname(ftp->fd, (struct sockaddr*) &ftp->localaddr, &size) != 0) {
		warning("getsockname failed: %s (%d)", strerror(errno), errno);
		return 0;
	}

	if (!ftp_getresp(ftp) || ftp->resp != 220) {
		return 0;
	}
	
	ftp->disconnect = 0;
	
	return 1;
}

void ftp_progress(ftpbuf_t *ftp, databuf_t *data, size_t rcvd, size_t sent)
{
	static int dd=-1;
	
	if(ftp->debug && ftp->total) {
		ftp->rcvd += rcvd;
		ftp->sent += sent;
		int d = (rcvd?ftp->rcvd:ftp->sent)*100/ftp->total;
		if(d != dd) dprintf("%s %s %d%%\r", ftp->prompt, rcvd?"< rcvd":"> sent", d);
		dd = d;
	}
}

/* {{{ ftp_open
 */
ftpbuf_t*
ftp_open(const char *host, short port, zend_long timeout_sec, int debug)
{
	ftpbuf_t		*ftp;

	/* alloc the ftp structure */
	ftp = ecalloc(1, sizeof(*ftp));

	ftp->host = (char*)host;
	ftp->port = port ? port : 21;
	ftp->fd = -1;
	ftp->debug = debug;
	ftp->timeout_sec = timeout_sec;
	ftp->nb = 0;
	ftp->disconnect = 0;
	ftp->reconnect = 0;
	ftp->pasv = 0;
	ftp->progress = ftp_progress;
	ftp->total = 0;
	ftp->rcvd = 0;
	ftp->sent =0;
	
	if(!_ftp_open(ftp)) goto bail;

	return ftp;

bail:
	if (ftp->fd != -1) {
		closesocket(ftp->fd);
	}
	efree(ftp);
	return NULL;
}
/* }}} */

/* {{{ ftp_close
 */
ftpbuf_t*
ftp_close(ftpbuf_t *ftp)
{
	if (ftp == NULL) {
		return NULL;
	}
	if (ftp->data) {
		data_close(ftp, ftp->data);
	}
	if (ftp->stream && ftp->closestream) {
			php_stream_close(ftp->stream);
	}
	if (ftp->fd != -1) {
#ifdef HAVE_FTP_SSL
		if (ftp->ssl_active) {
			SSL_shutdown(ftp->ssl_handle);
			SSL_free(ftp->ssl_handle);
		}
#endif
		closesocket(ftp->fd);
	}
	ftp_gc(ftp);
	efree(ftp);
	return NULL;
}
/* }}} */

/* {{{ ftp_gc
 */
void
ftp_gc(ftpbuf_t *ftp)
{
	if (ftp == NULL) {
		return;
	}
	if (ftp->pwd) {
		efree(ftp->pwd);
		ftp->pwd = NULL;
	}
	if (ftp->syst) {
		efree(ftp->syst);
		ftp->syst = NULL;
	}
}
/* }}} */

/* {{{ ftp_quit
 */
int
ftp_quit(ftpbuf_t *ftp)
{
	if (ftp == NULL) {
		return 0;
	}

	if (!ftp_putcmd(ftp, "QUIT", sizeof("QUIT")-1, NULL, (size_t) 0)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 221) {
		return 0;
	}

	if (ftp->pwd) {
		efree(ftp->pwd);
		ftp->pwd = NULL;
	}

	return 1;
}
/* }}} */

/* {{{ ftp_login
 */
int
ftp_login(ftpbuf_t *ftp, const char *user, const size_t user_len, const char *pass, const size_t pass_len)
{
#ifdef HAVE_FTP_SSL
	SSL_CTX	*ctx = NULL;
	long ssl_ctx_options = SSL_OP_ALL;
	int err, res;
	zend_bool retry;
#endif
	if (ftp == NULL) {
		return 0;
	}

#ifdef HAVE_FTP_SSL
	if (ftp->use_ssl && !ftp->ssl_active) {
		if (!ftp_putcmd(ftp, "AUTH", sizeof("AUTH")-1, "TLS", sizeof("TLS")-1)) {
			return 0;
		}
		if (!ftp_getresp(ftp)) {
			return 0;
		}

		if (ftp->resp != 234) {
			if (!ftp_putcmd(ftp, "AUTH", sizeof("AUTH")-1, "SSL", sizeof("SSL")-1)) {
				return 0;
			}
			if (!ftp_getresp(ftp)) {
				return 0;
			}

			if (ftp->resp != 334) {
				return 0;
			} else {
				ftp->old_ssl = 1;
				ftp->use_ssl_for_data = 1;
			}
		}

		ctx = SSL_CTX_new(SSLv23_client_method());
		if (ctx == NULL) {
			warning("failed to create the SSL context");
			return 0;
		}

#if OPENSSL_VERSION_NUMBER >= 0x0090605fL
		ssl_ctx_options &= ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS;
#endif
		SSL_CTX_set_options(ctx, ssl_ctx_options);

		/* allow SSL to re-use sessions */
		SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_BOTH);

		ftp->ssl_handle = SSL_new(ctx);
		if (ftp->ssl_handle == NULL) {
			warning("failed to create the SSL handle");
			SSL_CTX_free(ctx);
			return 0;
		}

		SSL_set_fd(ftp->ssl_handle, ftp->fd);

		do {
			res = SSL_connect(ftp->ssl_handle);
			err = SSL_get_error(ftp->ssl_handle, res);

			/* TODO check if handling other error codes would make sense */
			switch (err) {
				case SSL_ERROR_NONE:
					retry = 0;
					break;

				case SSL_ERROR_ZERO_RETURN:
					retry = 0;
					SSL_shutdown(ftp->ssl_handle);
					break;

				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE: {
						php_pollfd p;
						int i;

						p.fd = ftp->fd;
						p.events = (err == SSL_ERROR_WANT_READ) ? (POLLIN|POLLPRI) : POLLOUT;
						p.revents = 0;

						i = php_poll2(&p, 1, 300);

						retry = i > 0;
					}
					break;

				default:
					warning("SSL/TLS handshake failed");
					SSL_shutdown(ftp->ssl_handle);
					SSL_free(ftp->ssl_handle);
					return 0;
			}
		} while (retry);

		ftp->ssl_active = 1;

		if (!ftp->old_ssl) {

			/* set protection buffersize to zero */
			if (!ftp_putcmd(ftp, "PBSZ", sizeof("PBSZ")-1, "0", sizeof("0")-1)) {
				return 0;
			}
			if (!ftp_getresp(ftp)) {
				return 0;
			}

			/* enable data conn encryption */
			if (!ftp_putcmd(ftp, "PROT", sizeof("PROT")-1, "P", sizeof("P")-1)) {
				return 0;
			}
			if (!ftp_getresp(ftp)) {
				return 0;
			}

			ftp->use_ssl_for_data = (ftp->resp >= 200 && ftp->resp <=299);
		}
	}
#endif

	if (!ftp_putcmd(ftp, "USER", sizeof("USER")-1, user, user_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp)) {
		return 0;
	}
	if (ftp->resp == 230) {
		return 1;
	}
	if (ftp->resp != 331) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "PASS", sizeof("PASS")-1, pass, pass_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp)) {
		return 0;
	}
	ftp->user = (char*)user;
	ftp->user_len = (int)user_len;
	ftp->pass = (char*)pass;
	ftp->pass_len = (int)pass_len;
	if(ftp->debug && ftp->resp == 230) snprintf(ftp->prompt, FTP_BUFSIZE, "%s@%s:%d", ftp->user, ftp->host, ftp->port);
	return (ftp->resp == 230);
}
/* }}} */

/* {{{ ftp_reinit
 */
int
ftp_reinit(ftpbuf_t *ftp)
{
	if (ftp == NULL) {
		return 0;
	}

	ftp_gc(ftp);

	ftp->nb = 0;

	if (!ftp_putcmd(ftp, "REIN", sizeof("REIN")-1, NULL, (size_t) 0)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 220) {
		return 0;
	}

	return 1;
}
/* }}} */

/* {{{ ftp_syst
 */
const char*
ftp_syst(ftpbuf_t *ftp)
{
	char *syst, *end;

	if (ftp == NULL) {
		return NULL;
	}

	/* default to cached value */
	if (ftp->syst) {
		return ftp->syst;
	}
	if (!ftp_putcmd(ftp, "SYST", sizeof("SYST")-1, NULL, (size_t) 0)) {
		return NULL;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 215) {
		return NULL;
	}
	syst = ftp->inbuf;
	while (*syst == ' ') {
		syst++;
	}
	if ((end = strchr(syst, ' '))) {
		*end = 0;
	}
	ftp->syst = estrdup(syst);
	if (end) {
		*end = ' ';
	}
	return ftp->syst;
}
/* }}} */

/* {{{ ftp_pwd
 */
const char*
ftp_pwd(ftpbuf_t *ftp)
{
	char *pwd, *end;

	if (ftp == NULL) {
		return NULL;
	}

	/* default to cached value */
	if (ftp->pwd) {
		return ftp->pwd;
	}
	if (!ftp_putcmd(ftp, "PWD", sizeof("PWD")-1, NULL, (size_t) 0)) {
		return NULL;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 257) {
		return NULL;
	}
	/* copy out the pwd from response */
	if ((pwd = strchr(ftp->inbuf, '"')) == NULL) {
		return NULL;
	}
	if ((end = strrchr(++pwd, '"')) == NULL) {
		return NULL;
	}
	ftp->pwd = estrndup(pwd, end - pwd);
	if(ftp->debug) snprintf(ftp->prompt, FTP_BUFSIZE, "%s@%s:%d%s", ftp->user, ftp->host, ftp->port, ftp->pwd);

	return ftp->pwd;
}
/* }}} */

/* {{{ ftp_exec
 */
int
ftp_exec(ftpbuf_t *ftp, const char *cmd, const size_t cmd_len)
{
	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "SITE EXEC", sizeof("SITE EXEC")-1, cmd, cmd_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 200) {
		return 0;
	}

	return 1;
}
/* }}} */

/* {{{ ftp_raw
 */
unsigned int
ftp_raw(ftpbuf_t *ftp, const char *cmd, const size_t cmd_len, char ***lines)
{
	if (ftp == NULL || cmd == NULL) {
		return 0;
	}
	if (!ftp_putcmd(ftp, cmd, cmd_len, NULL, (size_t) 0)) {
		return 0;
	}
	
	unsigned int n = 0, n2 = 10;
	char **_lines;
	
	*lines = _lines = (char **) malloc(sizeof(char*)*n2);
	memset(_lines, 0, sizeof(char*)*n2);
	
	while (ftp_readline(ftp)) {
		if(n >= n2) {
			n2 += 10;
			_lines = (char **) realloc(_lines, sizeof(char*)*n2);
			
			if(_lines != *lines) {
				free(*lines);
				*lines = _lines;
			}
		}
		
		_lines[n++] = strdup(ftp->inbuf);
		if (isdigit(ftp->inbuf[0]) && isdigit(ftp->inbuf[1]) && isdigit(ftp->inbuf[2]) && ftp->inbuf[3] == ' ') {
			return n;
		}
	}
	
	return n;
}
/* }}} */

/* {{{ ftp_chdir
 */
int
ftp_chdir(ftpbuf_t *ftp, const char *dir, const size_t dir_len)
{
	if (ftp == NULL) {
		return 0;
	}

	if (ftp->pwd) {
		efree(ftp->pwd);
		ftp->pwd = NULL;
	}

	if (!ftp_putcmd(ftp, "CWD", sizeof("CWD")-1, dir, dir_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 250) {
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ ftp_cdup
 */
int
ftp_cdup(ftpbuf_t *ftp)
{
	if (ftp == NULL) {
		return 0;
	}

	if (ftp->pwd) {
		efree(ftp->pwd);
		ftp->pwd = NULL;
	}

	if (!ftp_putcmd(ftp, "CDUP", sizeof("CDUP")-1, NULL, (size_t) 0)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 250) {
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ ftp_mkdir
 */
char*
ftp_mkdir(ftpbuf_t *ftp, const char *dir, const size_t dir_len)
{
	char *mkd, *end;

	if (ftp == NULL) {
		return NULL;
	}
	if (!ftp_putcmd(ftp, "MKD", sizeof("MKD")-1, dir, dir_len)) {
		return NULL;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 257) {
		return NULL;
	}
	/* copy out the dir from response */
	if ((mkd = strchr(ftp->inbuf, '"')) == NULL) {
		return (char*) dir;
	}
	if ((end = strrchr(++mkd, '"')) == NULL) {
		return NULL;
	}
	*end = 0;

	return mkd;
}
/* }}} */

/* {{{ ftp_rmdir
 */
int
ftp_rmdir(ftpbuf_t *ftp, const char *dir, const size_t dir_len)
{
	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "RMD", sizeof("RMD")-1, dir, dir_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 250) {
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ ftp_chmod
 */
int
ftp_chmod(ftpbuf_t *ftp, const int mode, const char *filename, const int filename_len)
{
	char *buffer = NULL;
	int buffer_len;

	if (ftp == NULL || filename_len <= 0) {
		return 0;
	}

	buffer_len = asprintf(&buffer, "CHMOD %o %s", mode, filename);

	if (!buffer) {
		return 0;
	}

	if (!ftp_putcmd(ftp, "SITE", sizeof("SITE")-1, buffer, buffer_len)) {
		efree(buffer);
		return 0;
	}

	efree(buffer);

	if (!ftp_getresp(ftp) || ftp->resp != 200) {
		return 0;
	}

	return 1;
}
/* }}} */

/* {{{ ftp_alloc
 */
int
ftp_alloc(ftpbuf_t *ftp, const zend_long size, char **response)
{
	char buffer[64];
	int buffer_len;

	if (ftp == NULL || size <= 0) {
		return 0;
	}

	buffer_len = snprintf(buffer, sizeof(buffer) - 1, ZEND_LONG_FMT, size);

	if (buffer_len < 0) {
		return 0;
	}

	if (!ftp_putcmd(ftp, "ALLO", sizeof("ALLO")-1, buffer, buffer_len)) {
		return 0;
	}

	if (!ftp_getresp(ftp)) {
		return 0;
	}

	if (response) {
		*response = ftp->inbuf;
	}

	if (ftp->resp < 200 || ftp->resp >= 300) {
		return 0;
	}

	return 1;
}
/* }}} */

/* {{{ ftp_nlist
 */
char**
ftp_nlist(ftpbuf_t *ftp, const char *path, const size_t path_len)
{
	return ftp_genlist(ftp, "NLST", sizeof("NLST")-1, path, path_len);
}
/* }}} */

/* {{{ ftp_list
 */
char**
ftp_list(ftpbuf_t *ftp, const char *path, const size_t path_len, int recursive)
{
	return ftp_genlist(ftp, ((recursive) ? "LIST -R" : "LIST"), ((recursive) ? sizeof("LIST -R")-1 : sizeof("LIST")-1), path, path_len);
}
/* }}} */

/* {{{ ftp_mlsd
 */
char**
ftp_mlsd(ftpbuf_t *ftp, const char *path, const size_t path_len)
{
	return ftp_genlist(ftp, "MLSD", sizeof("MLSD")-1, path, path_len);
}
/* }}} */

/* {{{ ftp_type
 */
int
ftp_type(ftpbuf_t *ftp, ftptype_t type)
{
	const char *typechar;

	if (ftp == NULL) {
		return 0;
	}
	if (type == ftp->type) {
		return 1;
	}
	if (type == FTPTYPE_ASCII) {
		typechar = "A";
	} else if (type == FTPTYPE_IMAGE) {
		typechar = "I";
	} else {
		return 0;
	}
	if (!ftp_putcmd(ftp, "TYPE", sizeof("TYPE")-1, typechar, 1)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 200) {
		return 0;
	}
	ftp->type = type;

	return 1;
}
/* }}} */

/* {{{ ftp_pasv
 */
int
ftp_pasv(ftpbuf_t *ftp, int pasv)
{
	char			*ptr;
	union ipbox		ipbox;
	unsigned long		b[6];
	socklen_t			n;
	struct sockaddr *sa;
	struct sockaddr_in *sin;

	if (ftp == NULL) {
		return 0;
	}
	if (pasv && ftp->pasv == 2) {
		return 1;
	}
	if (!pasv) {
		ftp->pasv = 0;
		return 1;
	}
	n = sizeof(ftp->pasvaddr);
	memset(&ftp->pasvaddr, 0, n);
	sa = (struct sockaddr *) &ftp->pasvaddr;

	if (getpeername(ftp->fd, sa, &n) < 0) {
		return 0;
	}

#if HAVE_IPV6
	if (sa->sa_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) sa;
		char *endptr, delimiter;

		/* try EPSV first */
		if (!ftp_putcmd(ftp, "EPSV", sizeof("EPSV")-1, NULL, (size_t) 0)) {
			return 0;
		}
		if (!ftp_getresp(ftp)) {
			return 0;
		}
		if (ftp->resp == 229) {
			/* parse out the port */
			for (ptr = ftp->inbuf; *ptr && *ptr != '('; ptr++);
			if (!*ptr) {
				return 0;
			}
			delimiter = *++ptr;
			for (n = 0; *ptr && n < 3; ptr++) {
				if (*ptr == delimiter) {
					n++;
				}
			}

			sin6->sin6_port = htons((unsigned short) strtoul(ptr, &endptr, 10));
			if (ptr == endptr || *endptr != delimiter) {
				return 0;
			}
			ftp->pasv = 2;
			return 1;
		}
	}

	/* fall back to PASV */
#endif

	if (!ftp_putcmd(ftp, "PASV",  sizeof("PASV")-1, NULL, (size_t) 0)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 227) {
		return 0;
	}
	/* parse out the IP and port */
	for (ptr = ftp->inbuf; *ptr && !isdigit(*ptr); ptr++);
	n = sscanf(ptr, "%lu,%lu,%lu,%lu,%lu,%lu", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]);
	if (n != 6) {
		return 0;
	}
	for (n = 0; n < 6; n++) {
		ipbox.c[n] = (unsigned char) b[n];
	}
	sin = (struct sockaddr_in *) sa;
	if (ftp->usepasvaddress) {
		sin->sin_addr = ipbox.ia[0];
	}
	sin->sin_port = ipbox.s[2];

	ftp->pasv = 2;

	return 1;
}
/* }}} */

/* {{{ ftp_get
 */
int
ftp_get(ftpbuf_t *ftp, php_stream *outstream, const char *path, const size_t path_len, ftptype_t type, zend_long resumepos)
{
	databuf_t		*data = NULL;
	size_t			rcvd;
	char			arg[11];

	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_type(ftp, type)) {
		goto bail;
	}

	if ((data = ftp_getdata(ftp)) == NULL) {
		goto bail;
	}

	ftp->data = data;

	if (resumepos > 0) {
		int arg_len = snprintf(arg, sizeof(arg), ZEND_LONG_FMT, resumepos);

		if (arg_len < 0) {
			goto bail;
		}
		if (!ftp_putcmd(ftp, "REST", sizeof("REST")-1, arg, arg_len)) {
			goto bail;
		}
		if (!ftp_getresp(ftp) || (ftp->resp != 350)) {
			goto bail;
		}
	}

	if (!ftp_putcmd(ftp, "RETR", sizeof("RETR")-1, path, path_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || (ftp->resp != 150 && ftp->resp != 125)) {
		goto bail;
	}

	if ((data = data_accept(data, ftp)) == NULL) {
		goto bail;
	}

	while ((rcvd = my_recv(ftp, data->fd, data->buf, FTP_BUFSIZE))) {
		if (rcvd == (size_t)-1) {
			goto bail;
		}

		ftp->progress(ftp, data, rcvd, 0);

		if (type == FTPTYPE_ASCII) {
			char *s;
			char *ptr = data->buf;
			char *e = ptr + rcvd;
			/* logic depends on the OS EOL
			 * Win32 -> \r\n
			 * Everything Else \n
			 */
			while (e > ptr && (s = memchr(ptr, '\r', (e - ptr)))) {
				php_stream_write(outstream, ptr, (s - ptr));
				if (*(s + 1) == '\n') {
					s++;
					php_stream_putc(outstream, '\n');
				}
				ptr = s + 1;
			}
			if (ptr < e) {
				php_stream_write(outstream, ptr, (e - ptr));
			}
		} else if (rcvd != php_stream_write(outstream, data->buf, rcvd)) {
			goto bail;
		}
	}

	ftp->data = data = data_close(ftp, data);

	if (!ftp_getresp(ftp) || (ftp->resp != 226 && ftp->resp != 250)) {
		goto bail;
	}

	return 1;
bail:
	ftp->data = data_close(ftp, data);
	return 0;
}
/* }}} */

/* {{{ ftp_put
 */
int
ftp_put(ftpbuf_t *ftp, const char *path, const size_t path_len, php_stream *instream, ftptype_t type, zend_long startpos)
{
	databuf_t		*data = NULL;
	zend_long			size;
	char			*ptr;
	int			ch;
	char			arg[11];

	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_type(ftp, type)) {
		goto bail;
	}
	if ((data = ftp_getdata(ftp)) == NULL) {
		goto bail;
	}
	ftp->data = data;

	if (startpos > 0) {
		int arg_len = snprintf(arg, sizeof(arg), ZEND_LONG_FMT, startpos);

		if (arg_len < 0) {
			goto bail;
		}
		if (!ftp_putcmd(ftp, "REST", sizeof("REST")-1, arg, arg_len)) {
			goto bail;
		}
		if (!ftp_getresp(ftp) || (ftp->resp != 350)) {
			goto bail;
		}
	} else {
		startpos = 0;
	}

	if (!ftp_putcmd(ftp, "STOR", sizeof("STOR")-1, path, path_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || (ftp->resp != 150 && ftp->resp != 125)) {
		goto bail;
	}
	if ((data = data_accept(data, ftp)) == NULL) {
		goto bail;
	}

	if (fseek(instream, startpos, SEEK_SET)) {
		goto bail;
	}

	size = 0;
	ptr = data->buf;
	while (!php_stream_eof(instream) && (ch = php_stream_getc(instream))!=EOF) {
		/* flush if necessary */
		if (FTP_BUFSIZE - size < 2) {
			if (my_send(ftp, data->fd, data->buf, size) != size) {
				goto bail;
			}
			ftp->progress(ftp, data, 0, size);
			ptr = data->buf;
			size = 0;
		}

		if (ch == '\n' && type == FTPTYPE_ASCII) {
			*ptr++ = '\r';
			size++;
		}

		*ptr++ = ch;
		size++;
	}

	if (size && my_send(ftp, data->fd, data->buf, size) != size) {
		goto bail;
	}
	if(size) ftp->progress(ftp, data, 0, size);
	ftp->data = data = data_close(ftp, data);

	if (!ftp_getresp(ftp) || (ftp->resp != 226 && ftp->resp != 250 && ftp->resp != 200)) {
		goto bail;
	}
	return 1;
bail:
	ftp->data = data_close(ftp, data);
	return 0;
}
/* }}} */


/* {{{ ftp_append
 */
int
ftp_append(ftpbuf_t *ftp, const char *path, const size_t path_len, php_stream *instream, ftptype_t type)
{
	databuf_t		*data = NULL;
	zend_long			size;
	char			*ptr;
	int			ch;

	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_type(ftp, type)) {
		goto bail;
	}
	if ((data = ftp_getdata(ftp)) == NULL) {
		goto bail;
	}
	ftp->data = data;

	if (!ftp_putcmd(ftp, "APPE", sizeof("APPE")-1, path, path_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || (ftp->resp != 150 && ftp->resp != 125)) {
		goto bail;
	}
	if ((data = data_accept(data, ftp)) == NULL) {
		goto bail;
	}

	size = 0;
	ptr = data->buf;
	while (!php_stream_eof(instream) && (ch = php_stream_getc(instream))!=EOF) {
		/* flush if necessary */
		if (FTP_BUFSIZE - size < 2) {
			if (my_send(ftp, data->fd, data->buf, size) != size) {
				goto bail;
			}
			ftp->progress(ftp, data, 0, size);
			ptr = data->buf;
			size = 0;
		}

		if (ch == '\n' && type == FTPTYPE_ASCII) {
			*ptr++ = '\r';
			size++;
		}

		*ptr++ = ch;
		size++;
	}

	if (size && my_send(ftp, data->fd, data->buf, size) != size) {
		goto bail;
	}
	if(size) ftp->progress(ftp, data, 0, size);
	ftp->data = data = data_close(ftp, data);

	if (!ftp_getresp(ftp) || (ftp->resp != 226 && ftp->resp != 250 && ftp->resp != 200)) {
		goto bail;
	}
	return 1;
bail:
	ftp->data = data_close(ftp, data);
	return 0;
}
/* }}} */

/* {{{ ftp_size
 */
zend_long
ftp_size(ftpbuf_t *ftp, const char *path, const size_t path_len)
{
	if (ftp == NULL) {
		return -1;
	}
	if (!ftp_type(ftp, FTPTYPE_IMAGE)) {
		return -1;
	}
	if (!ftp_putcmd(ftp, "SIZE", sizeof("SIZE")-1, path, path_len)) {
		return -1;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 213) {
		return -1;
	}
	return atol(ftp->inbuf);
}
/* }}} */

/* {{{ ftp_mdtm
 */
time_t
ftp_mdtm(ftpbuf_t *ftp, const char *path, const size_t path_len)
{
	time_t		stamp;
	struct tm	*gmt, tmbuf;
	struct tm	tm;
	char		*ptr;
	int		n;

	if (ftp == NULL) {
		return -1;
	}
	if (!ftp_putcmd(ftp, "MDTM", sizeof("MDTM")-1, path, path_len)) {
		return -1;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 213) {
		return -1;
	}
	/* parse out the timestamp */
	for (ptr = ftp->inbuf; *ptr && !isdigit(*ptr); ptr++);
	n = sscanf(ptr, "%4u%2u%2u%2u%2u%2u", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
	if (n != 6) {
		return -1;
	}
	tm.tm_year -= 1900;
	tm.tm_mon--;
	tm.tm_isdst = -1;

	/* figure out the GMT offset */
	stamp = time(NULL);
	gmt = php_gmtime_r(&stamp, &tmbuf);
	if (!gmt) {
		return -1;
	}
	gmt->tm_isdst = -1;

	/* apply the GMT offset */
	tm.tm_sec += stamp - mktime(gmt);
	tm.tm_isdst = gmt->tm_isdst;

	stamp = mktime(&tm);

	return stamp;
}
/* }}} */

/* {{{ ftp_delete
 */
int
ftp_delete(ftpbuf_t *ftp, const char *path, const size_t path_len)
{
	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "DELE", sizeof("DELE")-1, path, path_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 250) {
		return 0;
	}

	return 1;
}
/* }}} */

/* {{{ ftp_rename
 */
int
ftp_rename(ftpbuf_t *ftp, const char *src, const size_t src_len, const char *dest, const size_t dest_len)
{
	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "RNFR", sizeof("RNFR")-1, src, src_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 350) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "RNTO", sizeof("RNTO")-1, dest, dest_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 250) {
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ ftp_site
 */
int
ftp_site(ftpbuf_t *ftp, const char *cmd, const size_t cmd_len)
{
	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_putcmd(ftp, "SITE", sizeof("SITE")-1, cmd, cmd_len)) {
		return 0;
	}
	if (!ftp_getresp(ftp) || ftp->resp < 200 || ftp->resp >= 300) {
		return 0;
	}

	return 1;
}
/* }}} */

/* static functions */

/* {{{ ftp_putcmd
 */
int
ftp_putcmd(ftpbuf_t *ftp, const char *cmd, const size_t cmd_len, const char *args, const size_t args_len)
{
	int		size;
	char		*data;

	if (strpbrk(cmd, "\r\n")) {
		return 0;
	}
	/* build the output buffer */
	if (args && args[0]) {
		/* "cmd args\r\n\0" */
		if (cmd_len + args_len + 4 > FTP_BUFSIZE) {
			return 0;
		}
		if (strpbrk(args, "\r\n")) {
			return 0;
		}
		size = snprintf(ftp->outbuf, sizeof(ftp->outbuf), "%s %s\r\n", cmd, args);
	} else {
		/* "cmd\r\n\0" */
		if (cmd_len + 3 > FTP_BUFSIZE) {
			return 0;
		}
		size = snprintf(ftp->outbuf, sizeof(ftp->outbuf), "%s\r\n", cmd);
	}

	data = ftp->outbuf;

	/* Clear the extra-lines buffer */
	ftp->extra = NULL;

	if(ftp->debug) dprintf("%s > %s", ftp->prompt, data);
	if (my_send(ftp, ftp->fd, data, size) != size) {
		return 0;
	}
	return 1;
}
/* }}} */

/* {{{ ftp_readline
 */
int
ftp_readline(ftpbuf_t *ftp)
{
	long		size, rcvd;
	char		*data, *eol;

	/* shift the extra to the front */
	size = FTP_BUFSIZE;
	rcvd = 0;
	if (ftp->extra) {
		memmove(ftp->inbuf, ftp->extra, ftp->extralen);
		rcvd = ftp->extralen;
	}

	data = ftp->inbuf;

	do {
		size -= rcvd;
		for (eol = data; rcvd; rcvd--, eol++) {
			if (*eol == '\r') {
				*eol = 0;
				ftp->extra = eol + 1;
				if (rcvd > 1 && *(eol + 1) == '\n') {
					ftp->extra++;
					rcvd--;
				}
				if ((ftp->extralen = --rcvd) == 0) {
					ftp->extra = NULL;
				}
				if(ftp->debug) dprintf("%s < %s\n", ftp->prompt, data);
				return 1;
			} else if (*eol == '\n') {
				*eol = 0;
				ftp->extra = eol + 1;
				if ((ftp->extralen = --rcvd) == 0) {
					ftp->extra = NULL;
				}
				if(ftp->debug) dprintf("%s < %s\n", ftp->prompt, data);
				return 1;
			}
		}

		data = eol;
		if ((rcvd = my_recv(ftp, ftp->fd, data, size)) < 1) {
			return 0;
		}
	} while (size);

	return 0;
}
/* }}} */

/* {{{ ftp_getresp
 */
int
ftp_getresp(ftpbuf_t *ftp)
{
	if (ftp == NULL) {
		return 0;
	}
	ftp->resp = 0;

	while (1) {

		if (!ftp_readline(ftp)) {
			return 0;
		}

		/* Break out when the end-tag is found */
		if (isdigit(ftp->inbuf[0]) && isdigit(ftp->inbuf[1]) && isdigit(ftp->inbuf[2]) && ftp->inbuf[3] == ' ') {
			break;
		}
	}

	/* translate the tag */
	if (!isdigit(ftp->inbuf[0]) || !isdigit(ftp->inbuf[1]) || !isdigit(ftp->inbuf[2])) {
		return 0;
	}

	ftp->resp = 100 * (ftp->inbuf[0] - '0') + 10 * (ftp->inbuf[1] - '0') + (ftp->inbuf[2] - '0');

	memmove(ftp->inbuf, ftp->inbuf + 4, FTP_BUFSIZE - 4);

	if (ftp->extra) {
		ftp->extra -= 4;
	}
	return 1;
}
/* }}} */

int
ftp_reconnect(ftpbuf_t *ftp)
{
	if(!ftp->disconnect) return 1;
	
	errno = 0;
	
	ftp->disconnect = 0;
	ftp->reconnect++;
	ftp->data = data_close(ftp, ftp->data);
	ftp->type = 0;
	
	return _ftp_open(ftp) && ftp_login(ftp, ftp->user, ftp->user_len, ftp->pass, ftp->pass_len);
}

/* {{{ my_send
 */
int
my_send(ftpbuf_t *ftp, php_socket_t s, void *buf, size_t len)
{
	zend_long		size, sent;
    int         n;
#ifdef HAVE_FTP_SSL
	int err;
	zend_bool retry = 0;
	SSL *handle = NULL;
	php_socket_t fd;
#endif


	size = len;
	while (size) {
		n = php_pollfd_for_ms(s, POLLOUT, ftp->timeout_sec * 1000);

		if (n < 1) {
			if (n == 0) {
				errno = ETIMEDOUT;
			}
			if(ftp->fd == s) ftp->disconnect = 1;
			return -1;
		}

#ifdef HAVE_FTP_SSL
		if (ftp->use_ssl && ftp->fd == s && ftp->ssl_active) {
			handle = ftp->ssl_handle;
			fd = ftp->fd;
		} else if (ftp->use_ssl && ftp->fd != s && ftp->use_ssl_for_data && ftp->data->ssl_active) {
			handle = ftp->data->ssl_handle;
			fd = ftp->data->fd;
		}

		if (handle) {
			do {
				sent = SSL_write(handle, buf, size);
				err = SSL_get_error(handle, sent);

				switch (err) {
					case SSL_ERROR_NONE:
						retry = 0;
						break;

					case SSL_ERROR_ZERO_RETURN:
						retry = 0;
						SSL_shutdown(handle);
						break;

					case SSL_ERROR_WANT_READ:
					case SSL_ERROR_WANT_CONNECT: {
							php_pollfd p;
							int i;

							p.fd = fd;
							p.events = POLLOUT;
							p.revents = 0;

							i = php_poll2(&p, 1, 300);

							retry = i > 0;
						}
						break;

					default:
						warning("SSL write failed");
						return -1;
				}
			} while (retry);
		} else {
#endif
			sent = send(s, buf, size, 0);
#ifdef HAVE_FTP_SSL
		}
#endif
		if (sent == -1) {
			if(ftp->fd == s) ftp->disconnect = 1;
			return -1;
		}

		buf = (char*) buf + sent;
		size -= sent;
	}

	return len;
}
/* }}} */

/* {{{ my_recv
 */
int
my_recv(ftpbuf_t *ftp, php_socket_t s, void *buf, size_t len)
{
	int		n, nr_bytes;
#ifdef HAVE_FTP_SSL
	int err;
	zend_bool retry = 0;
	SSL *handle = NULL;
	php_socket_t fd;
#endif

	n = php_pollfd_for_ms(s, PHP_POLLREADABLE, ftp->timeout_sec * 1000);
	if (n < 1) {
		if (n == 0) {
			errno = ETIMEDOUT;
		}
		if(ftp->fd == s) ftp->disconnect = 1;
		return -1;
	}

#ifdef HAVE_FTP_SSL
	if (ftp->use_ssl && ftp->fd == s && ftp->ssl_active) {
		handle = ftp->ssl_handle;
		fd = ftp->fd;
	} else if (ftp->use_ssl && ftp->fd != s && ftp->use_ssl_for_data && ftp->data->ssl_active) {
		handle = ftp->data->ssl_handle;
		fd = ftp->data->fd;
	}

	if (handle) {
		do {
			nr_bytes = SSL_read(handle, buf, len);
			err = SSL_get_error(handle, nr_bytes);

			switch (err) {
				case SSL_ERROR_NONE:
					retry = 0;
					break;

				case SSL_ERROR_ZERO_RETURN:
					retry = 0;
					SSL_shutdown(handle);
					break;

				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_CONNECT: {
						php_pollfd p;
						int i;

						p.fd = fd;
						p.events = POLLIN|POLLPRI;
						p.revents = 0;

						i = php_poll2(&p, 1, 300);

						retry = i > 0;
					}
					break;

				default:
					warning("SSL read failed");
					return -1;
			}
		} while (retry);
	} else {
#endif
		nr_bytes = recv(s, buf, len, 0);
#ifdef HAVE_FTP_SSL
	}
#endif
	if(nr_bytes == -1 && ftp->fd == s) ftp->disconnect = 1;
	return (nr_bytes);
}
/* }}} */

/* {{{ data_available
 */
int
data_available(ftpbuf_t *ftp, php_socket_t s)
{
	int		n;

	n = php_pollfd_for_ms(s, PHP_POLLREADABLE, 1000);
	if (n < 1) {
		if (n == 0) {
			errno = ETIMEDOUT;
		}
		if(ftp->fd == s) ftp->disconnect = 1;
		return 0;
	}

	return 1;
}
/* }}} */
/* {{{ data_writeable
 */
int
data_writeable(ftpbuf_t *ftp, php_socket_t s)
{
	int		n;

	n = php_pollfd_for_ms(s, POLLOUT, 1000);
	if (n < 1) {
		if (n == 0) {
			errno = ETIMEDOUT;
		}
		if(ftp->fd == s) ftp->disconnect = 1;
		return 0;
	}

	return 1;
}
/* }}} */

/* {{{ my_accept
 */
int
my_accept(ftpbuf_t *ftp, php_socket_t s, struct sockaddr *addr, socklen_t *addrlen)
{
	int		n;

	n = php_pollfd_for_ms(s, PHP_POLLREADABLE, ftp->timeout_sec * 1000);
	if (n < 1) {
		if (n == 0) {
			errno = ETIMEDOUT;
		}
		if(ftp->fd == s) ftp->disconnect = 1;
		return -1;
	}

	return accept(s, addr, addrlen);
}
/* }}} */

/* {{{ ftp_getdata
 */
databuf_t*
ftp_getdata(ftpbuf_t *ftp)
{
	int				fd = -1;
	databuf_t		*data;
	php_sockaddr_storage addr;
	struct sockaddr *sa;
	socklen_t		size;
	union ipbox		ipbox;
	char			arg[sizeof("255, 255, 255, 255, 255, 255")];
	struct timeval	tv;
	int				arg_len;


	/* ask for a passive connection if we need one */
	if (ftp->pasv && !ftp_pasv(ftp, 1)) {
		return NULL;
	}
	/* alloc the data structure */
	data = ecalloc(1, sizeof(*data));
	data->listener = -1;
	data->fd = -1;
	data->type = ftp->type;

	sa = (struct sockaddr *) &ftp->localaddr;
	/* bind/listen */
	if ((fd = socket(sa->sa_family, SOCK_STREAM, 0)) == SOCK_ERR) {
		warning("socket() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	/* passive connection handler */
	if (ftp->pasv) {
		/* clear the ready status */
		ftp->pasv = 1;

		/* connect */
		/* Win 95/98 seems not to like size > sizeof(sockaddr_in) */
		size = php_sockaddr_size(&ftp->pasvaddr);
		tv.tv_sec = ftp->timeout_sec;
		tv.tv_usec = 0;
		if (php_connect_nonb(fd, (struct sockaddr*) &ftp->pasvaddr, size, &tv) == -1) {
			warning("php_connect_nonb() failed: %s (%d)", strerror(errno), errno);
			goto bail;
		}

		data->fd = fd;

		ftp->data = data;
		return data;
	}


	/* active (normal) connection */

	/* bind to a local address */
	php_any_addr(sa->sa_family, &addr, 0);
	size = php_sockaddr_size(&addr);

	if (bind(fd, (struct sockaddr*) &addr, size) != 0) {
		warning("bind() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	if (getsockname(fd, (struct sockaddr*) &addr, &size) != 0) {
		warning("getsockname() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	if (listen(fd, 5) != 0) {
		warning("listen() failed: %s (%d)", strerror(errno), errno);
		goto bail;
	}

	data->listener = fd;

#if HAVE_IPV6 && HAVE_INET_NTOP
	if (sa->sa_family == AF_INET6) {
		/* need to use EPRT */
		char eprtarg[INET6_ADDRSTRLEN + sizeof("|x||xxxxx|")];
		char out[INET6_ADDRSTRLEN];
		int eprtarg_len;
		inet_ntop(AF_INET6, &((struct sockaddr_in6*) sa)->sin6_addr, out, sizeof(out));
		eprtarg_len = snprintf(eprtarg, sizeof(eprtarg), "|2|%s|%hu|", out, ntohs(((struct sockaddr_in6 *) &addr)->sin6_port));

		if (eprtarg_len < 0) {
			goto bail;
		}

		if (!ftp_putcmd(ftp, "EPRT", sizeof("EPRT")-1, eprtarg, eprtarg_len)) {
			goto bail;
		}

		if (!ftp_getresp(ftp) || ftp->resp != 200) {
			goto bail;
		}

		ftp->data = data;
		return data;
	}
#endif

	/* send the PORT */
	ipbox.ia[0] = ((struct sockaddr_in*) sa)->sin_addr;
	ipbox.s[2] = ((struct sockaddr_in*) &addr)->sin_port;
	arg_len = snprintf(arg, sizeof(arg), "%u,%u,%u,%u,%u,%u", ipbox.c[0], ipbox.c[1], ipbox.c[2], ipbox.c[3], ipbox.c[4], ipbox.c[5]);

	if (arg_len < 0) {
		goto bail;
	}
	if (!ftp_putcmd(ftp, "PORT", sizeof("PORT")-1, arg, arg_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || ftp->resp != 200) {
		goto bail;
	}

	ftp->data = data;
	return data;

bail:
	if (fd != -1) {
		closesocket(fd);
	}
	efree(data);
	return NULL;
}
/* }}} */

/* {{{ data_accept
 */
databuf_t*
data_accept(databuf_t *data, ftpbuf_t *ftp)
{
	struct sockaddr addr;
	socklen_t			size;

#ifdef HAVE_FTP_SSL
	SSL_CTX		*ctx;
	SSL_SESSION *session;
	int err, res;
	zend_bool retry;
#endif

	if (data->fd != -1) {
		goto data_accepted;
	}
	size = sizeof(addr);
	data->fd = my_accept(ftp, data->listener, (struct sockaddr*) &addr, &size);
	closesocket(data->listener);
	data->listener = -1;

	if (data->fd == -1) {
		efree(data);
		return NULL;
	}

data_accepted:
#ifdef HAVE_FTP_SSL

	/* now enable ssl if we need to */
	if (ftp->use_ssl && ftp->use_ssl_for_data) {
		ctx = SSL_get_SSL_CTX(ftp->ssl_handle);
		if (ctx == NULL) {
			warning("data_accept: failed to retreive the existing SSL context");
			return 0;
		}

		data->ssl_handle = SSL_new(ctx);
		if (data->ssl_handle == NULL) {
			warning("data_accept: failed to create the SSL handle");
			return 0;
		}

		SSL_set_fd(data->ssl_handle, data->fd);

		if (ftp->old_ssl) {
			SSL_copy_session_id(data->ssl_handle, ftp->ssl_handle);
		}

		/* get the session from the control connection so we can re-use it */
		session = SSL_get_session(ftp->ssl_handle);
		if (session == NULL) {
			warning("data_accept: failed to retreive the existing SSL session");
			SSL_free(data->ssl_handle);
			return 0;
		}

		/* and set it on the data connection */
		res = SSL_set_session(data->ssl_handle, session);
		if (res == 0) {
			warning("data_accept: failed to set the existing SSL session");
			SSL_free(data->ssl_handle);
			return 0;
		}

		do {
			res = SSL_connect(data->ssl_handle);
			err = SSL_get_error(data->ssl_handle, res);

			switch (err) {
				case SSL_ERROR_NONE:
					retry = 0;
					break;

				case SSL_ERROR_ZERO_RETURN:
					retry = 0;
					SSL_shutdown(data->ssl_handle);
					break;

				case SSL_ERROR_WANT_READ:
				case SSL_ERROR_WANT_WRITE: {
						php_pollfd p;
						int i;

						p.fd = ftp->fd;
						p.events = (err == SSL_ERROR_WANT_READ) ? (POLLIN|POLLPRI) : POLLOUT;
						p.revents = 0;

						i = php_poll2(&p, 1, 300);

						retry = i > 0;
					}
					break;

				default:
					warning("data_accept: SSL/TLS handshake failed");
					SSL_shutdown(data->ssl_handle);
					SSL_free(data->ssl_handle);
					return 0;
			}
		} while (retry);

		data->ssl_active = 1;
	}

#endif

	return data;
}
/* }}} */

/* {{{ data_close
 */
databuf_t*
data_close(ftpbuf_t *ftp, databuf_t *data)
{
	if (data == NULL) {
		return NULL;
	}
	if (data->listener != -1) {
#ifdef HAVE_FTP_SSL
		if (data->ssl_active) {
			/* don't free the data context, it's the same as the control */
			SSL_shutdown(data->ssl_handle);
			SSL_free(data->ssl_handle);
			data->ssl_active = 0;
		}
#endif
		closesocket(data->listener);
	}
	if (data->fd != -1) {
#ifdef HAVE_FTP_SSL
		if (data->ssl_active) {
			/* don't free the data context, it's the same as the control */
			SSL_shutdown(data->ssl_handle);
			SSL_free(data->ssl_handle);
			data->ssl_active = 0;
		}
#endif
		closesocket(data->fd);
	}
	if (ftp) {
		ftp->data = NULL;
	}
	efree(data);
	return NULL;
}
/* }}} */

/* {{{ ftp_genlist
 */
char**
ftp_genlist(ftpbuf_t *ftp, const char *cmd, const size_t cmd_len, const char *path, const size_t path_len)
{
	php_stream	*tmpstream = NULL;
	databuf_t	*data = NULL;
	char		*ptr;
	int		ch, lastch;
	size_t		size, rcvd;
	size_t		lines;
	char		**ret = NULL;
	char		**entry;
	char		*text;


	if ((tmpstream = php_stream_fopen_tmpfile()) == NULL) {
		warning("Unable to create temporary file.  Check permissions in temporary files directory.");
		return NULL;
	}

	if (!ftp_type(ftp, FTPTYPE_ASCII)) {
		goto bail;
	}

	if ((data = ftp_getdata(ftp)) == NULL) {
		goto bail;
	}
	ftp->data = data;

	if (!ftp_putcmd(ftp, cmd, cmd_len, path, path_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || (ftp->resp != 150 && ftp->resp != 125 && ftp->resp != 226)) {
		goto bail;
	}

	/* some servers don't open a ftp-data connection if the directory is empty */
	if (ftp->resp == 226) {
		ftp->data = data_close(ftp, data);
		php_stream_close(tmpstream);
		return ecalloc(1, sizeof(char*));
	}

	/* pull data buffer into tmpfile */
	if ((data = data_accept(data, ftp)) == NULL) {
		goto bail;
	}
	size = 0;
	lines = 0;
	lastch = 0;
	while ((rcvd = my_recv(ftp, data->fd, data->buf, FTP_BUFSIZE))) {
		if (rcvd == (size_t)-1 || rcvd > ((size_t)(-1))-size) {
			goto bail;
		}

		php_stream_write(tmpstream, data->buf, rcvd);

		size += rcvd;
		for (ptr = data->buf; rcvd; rcvd--, ptr++) {
			if (*ptr == '\n' && lastch == '\r') {
				lines++;
			}
			lastch = *ptr;
		}
	}

	ftp->data = data_close(ftp, data);

	php_stream_rewind(tmpstream);

	ret = safe_emalloc((lines + 1), sizeof(char*), size);

	entry = ret;
	text = (char*) (ret + lines + 1);
	*entry = text;
	lastch = 0;
	while ((ch = php_stream_getc(tmpstream)) != EOF) {
		if (ch == '\n' && lastch == '\r') {
			*(text - 1) = 0;
			*++entry = text;
		} else {
			*text++ = ch;
		}
		lastch = ch;
	}
	*entry = NULL;

	php_stream_close(tmpstream);

	if (!ftp_getresp(ftp) || (ftp->resp != 226 && ftp->resp != 250)) {
		efree(ret);
		return NULL;
	}

	return ret;
bail:
	ftp->data = data_close(ftp, data);
	php_stream_close(tmpstream);
	if (ret)
		efree(ret);
	return NULL;
}
/* }}} */

/* {{{ ftp_nb_get
 */
int
ftp_nb_get(ftpbuf_t *ftp, php_stream *outstream, const char *path, const size_t path_len, ftptype_t type, zend_long resumepos)
{
	databuf_t		*data = NULL;
	char			arg[11];

	if (ftp == NULL) {
		return PHP_FTP_FAILED;
	}

	if (!ftp_type(ftp, type)) {
		goto bail;
	}

	if ((data = ftp_getdata(ftp)) == NULL) {
		goto bail;
	}

	if (resumepos>0) {
		int arg_len = snprintf(arg, sizeof(arg), ZEND_LONG_FMT, resumepos);

		if (arg_len < 0) {
			goto bail;
		}
		if (!ftp_putcmd(ftp, "REST", sizeof("REST")-1, arg, arg_len)) {
			goto bail;
		}
		if (!ftp_getresp(ftp) || (ftp->resp != 350)) {
			goto bail;
		}
	}

	if (!ftp_putcmd(ftp, "RETR", sizeof("RETR")-1, path, path_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || (ftp->resp != 150 && ftp->resp != 125)) {
		goto bail;
	}

	if ((data = data_accept(data, ftp)) == NULL) {
		goto bail;
	}

	ftp->data = data;
	ftp->stream = outstream;
	ftp->lastch = 0;
	ftp->nb = 1;

	return (ftp_nb_continue_read(ftp));

bail:
	ftp->data = data_close(ftp, data);
	return PHP_FTP_FAILED;
}
/* }}} */

/* {{{ ftp_nb_continue_read
 */
int
ftp_nb_continue_read(ftpbuf_t *ftp)
{
	databuf_t	*data = NULL;
	char		*ptr;
	int		lastch;
	size_t		rcvd;
	ftptype_t	type;

	data = ftp->data;

	/* check if there is already more data */
	if (!data_available(ftp, data->fd)) {
		return PHP_FTP_MOREDATA;
	}

	type = ftp->type;

	lastch = ftp->lastch;
	if ((rcvd = my_recv(ftp, data->fd, data->buf, FTP_BUFSIZE))) {
		if (rcvd == (size_t)-1) {
			goto bail;
		}

		ftp->progress(ftp, data, rcvd, 0);

		if (type == FTPTYPE_ASCII) {
			for (ptr = data->buf; rcvd; rcvd--, ptr++) {
				if (lastch == '\r' && *ptr != '\n') {
					php_stream_putc(ftp->stream, '\r');
				}
				if (*ptr != '\r') {
					php_stream_putc(ftp->stream, *ptr);
				}
				lastch = *ptr;
			}
		} else if (rcvd != php_stream_write(ftp->stream, data->buf, rcvd)) {
			goto bail;
		}

		ftp->lastch = lastch;
		return PHP_FTP_MOREDATA;
	}

	if (type == FTPTYPE_ASCII && lastch == '\r') {
		php_stream_putc(ftp->stream, '\r');
	}

	ftp->data = data = data_close(ftp, data);

	if (!ftp_getresp(ftp) || (ftp->resp != 226 && ftp->resp != 250)) {
		goto bail;
	}

	ftp->nb = 0;
	return PHP_FTP_FINISHED;
bail:
	ftp->nb = 0;
	ftp->data = data_close(ftp, data);
	return PHP_FTP_FAILED;
}
/* }}} */

/* {{{ ftp_nb_put
 */
int
ftp_nb_put(ftpbuf_t *ftp, const char *path, const size_t path_len, php_stream *instream, ftptype_t type, zend_long startpos)
{
	databuf_t		*data = NULL;
	char			arg[11];

	if (ftp == NULL) {
		return 0;
	}
	if (!ftp_type(ftp, type)) {
		goto bail;
	}
	if ((data = ftp_getdata(ftp)) == NULL) {
		goto bail;
	}
	if (startpos > 0) {
		int arg_len = snprintf(arg, sizeof(arg), ZEND_LONG_FMT, startpos);

		if (arg_len < 0) {
			goto bail;
		}
		if (!ftp_putcmd(ftp, "REST", sizeof("REST")-1, arg, arg_len)) {
			goto bail;
		}
		if (!ftp_getresp(ftp) || (ftp->resp != 350)) {
			goto bail;
		}
	}

	if (!ftp_putcmd(ftp, "STOR", sizeof("STOR")-1, path, path_len)) {
		goto bail;
	}
	if (!ftp_getresp(ftp) || (ftp->resp != 150 && ftp->resp != 125)) {
		goto bail;
	}
	if ((data = data_accept(data, ftp)) == NULL) {
		goto bail;
	}
	ftp->data = data;
	ftp->stream = instream;
	ftp->lastch = 0;
	ftp->nb = 1;

	return (ftp_nb_continue_write(ftp));

bail:
	ftp->data = data_close(ftp, data);
	return PHP_FTP_FAILED;
}
/* }}} */


/* {{{ ftp_nb_continue_write
 */
int
ftp_nb_continue_write(ftpbuf_t *ftp)
{
	long			size;
	char			*ptr;
	int 			ch;

	/* check if we can write more data */
	if (!data_writeable(ftp, ftp->data->fd)) {
		return PHP_FTP_MOREDATA;
	}

	size = 0;
	ptr = ftp->data->buf;
	while (!php_stream_eof(ftp->stream) && (ch = php_stream_getc(ftp->stream)) != EOF) {

		if (ch == '\n' && ftp->type == FTPTYPE_ASCII) {
			*ptr++ = '\r';
			size++;
		}

		*ptr++ = ch;
		size++;

		/* flush if necessary */
		if (FTP_BUFSIZE - size < 2) {
			if (my_send(ftp, ftp->data->fd, ftp->data->buf, size) != size) {
				goto bail;
			}
			ftp->progress(ftp, ftp->data, 0, size);
			return PHP_FTP_MOREDATA;
		}
	}

	if (size && my_send(ftp, ftp->data->fd, ftp->data->buf, size) != size) {
		goto bail;
	}
	if(size) ftp->progress(ftp, ftp->data, 0, size);
	ftp->data = data_close(ftp, ftp->data);

	if (!ftp_getresp(ftp) || (ftp->resp != 226 && ftp->resp != 250)) {
		goto bail;
	}
	ftp->nb = 0;
	return PHP_FTP_FINISHED;
bail:
	ftp->data = data_close(ftp, ftp->data);
	ftp->nb = 0;
	return PHP_FTP_FAILED;
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
