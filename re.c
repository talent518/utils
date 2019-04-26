#include <stdio.h>
#include <sys/types.h>
#include <regex.h>

int main(int argc, char *argv[]) {
	int status = 0, i = 0;
	int flag = REG_EXTENDED;
	regmatch_t pmatch[1];
	const size_t nmatch = 1;
	regex_t reg;
	const char *pattern = "^\\w+([-+.]\\w+)*@\\w+([-.]\\w+)*\\.\\w+([-.]\\w+)*$";

	if(argc<2) {
		printf("usage: %s <email>...\n", argv[0]);
		return 1;
	}

	regcomp(&reg, pattern, flag);

	for(i=1; i<argc; i++) {
		printf("%s: %s\n", argv[i], regexec(&reg, argv[i], nmatch, pmatch, 0) != REG_NOMATCH ? "true" : "false");
	}

	regfree(&reg);

	return 0;
}

