#ifndef _API_H
#define _API_H

#if defined(__GNUC__) && __GNUC__ >= 4
#	define API __attribute__ ((visibility("default")))
#else
#	define API
#endif

#include <memory>

namespace api {

class Test456 {
public:
	API Test456();
	API int test();
	API ~Test456();
};

class Test123 {
public:
	API Test123();
	API int test();
	API ~Test123();

private:
	std::shared_ptr<Test456> test456;
};

};

#endif

