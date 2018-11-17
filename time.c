#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

int main(int argc, char *argv[]) {
	struct timeval tv;
	struct tm tm;
	
	if(gettimeofday(&tv, NULL) || !localtime_r(&tv.tv_sec, &tm)) {
		printf("error\n");
	} else {
		printf("date time: %04d-%02d-%02d %02d:%02d:%02d.%03ld\n", tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec/1000);
	}
	
	tzset();
	printf("tzset(): tzname = %s/%s, timezone = %ld, daylight = %d\n", tzname[0], tzname[1], timezone, daylight);
	
	return 0;
}

