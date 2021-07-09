#include <stdio.h>

#include "api.h"

int main(void) {
	printf("%s: %d\n", __func__, __LINE__);
	return test123();
}
