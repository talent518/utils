#include <stdio.h>

#include "api.h"

using namespace api;

int main(void) {
	std::shared_ptr<Test123> test123 = std::make_shared<Test123>();
	return test123->test();
}
