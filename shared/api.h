#ifndef _API_H
#define _API_H

#if defined(__GNUC__) && __GNUC__ >= 4
#	define APID __attribute__ ((visibility("default")))
#	define APIH __attribute__ ((visibility("hidden")))
//#	warning "API OK"
#else
#	define APID
#	define APIH
//#	warning "API NO"
#endif

#define API APID

API int test123(void);
API void test234(void);

#endif

