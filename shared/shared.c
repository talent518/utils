#include <stdio.h>

#include "api.h"

API int test123(void) {
	test234();
	return printf("%s: %d\n", __func__, __LINE__);
}
