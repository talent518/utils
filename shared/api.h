#ifndef _API_H
#define _API_H

#if defined(__GNUC__) && __GNUC__ >= 4
#	define API __attribute__ ((visibility("default")))
#else
#	define API
#endif

API int test123(void);
API void test234(void);

#endif

