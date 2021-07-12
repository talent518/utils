#include <stdio.h>
#include <math.h>

#include "api.h"

namespace api {

API Test456::Test456() {
	::printf("%s: %d\n", __func__, __LINE__);
}

API int Test456::test() {
	::printf("%s: %d\n", __func__, __LINE__);
	return ::random();
}

API Test456::~Test456() {
	::printf("%s: %d\n", __func__, __LINE__);
}

};

