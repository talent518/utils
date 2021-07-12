#include <stdio.h>

#include "api.h"

namespace api {

API Test123::Test123() {
	test456 = std::make_shared<Test456>();
	::printf("%s: %d\n", __func__, __LINE__);
}

API int Test123::test() {
	test456->test();
	return ::printf("%s: %d\n", __func__, __LINE__);
}

API Test123::~Test123() {
	::printf("%s: %d\n", __func__, __LINE__);
}

};

