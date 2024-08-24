#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>

#include <sys/time.h>
#include <time.h>

char *nowtime_r(char *buf, uint16_t len)
{
	struct timeval tv = {0, 0};
	struct tm tm;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);

	snprintf(buf, len, "%04u-%02u-%02u %02d:%02d:%02d.%03ld",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		tv.tv_usec / 1000
	);

	return buf;
}

char *nowtime(void)
{
	static char buf[32];

	return nowtime_r(buf, sizeof(buf));
}

int p_scanf(const char *command, const char *format, ...)
{
	int n = 0;
	va_list ap;
	FILE *fp = popen(command, "r");

	if(fp)
	{
		va_start(ap, format);
		n = vfscanf(fp, format, ap);
		va_end(ap);
		pclose(fp);
	}

	return n;
}

int f_scanf(const char *path, const char *format, ...)
{
	int n = 0;
	va_list ap;
	FILE *fp = fopen(path, "r");

	if(fp)
	{
		va_start(ap, format);
		n = vfscanf(fp, format, ap);
		va_end(ap);
		fclose(fp);
	}

	return n;
}

static volatile bool is_running = true;

static void sig_handler(int sig)
{
	is_running = false;
	if(isatty(2)) fprintf(stderr, "\r\033[2K");
}

int main()
{
	char path[256], split[2048];
	int i, n, sz, ret;
	uint32_t min, max, freq;

	if(p_scanf("nproc", "%d", &sz) != 1 || sz <= 0) return 1;

	memset(split, 0, sizeof(split));
	memset(split, '-', 16);
	memset(split, '-', sz * 6);

	fprintf(stdout, "[%s]%s\n", nowtime(), split);
	fprintf(stdout, "[%s] cpu   min   max\n", nowtime());
	fprintf(stdout, "[%s]%s\n", nowtime(), split);

	for(i = 0; i < sz; i ++)
	{
		min = 0;
		
		sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		ret = f_scanf(path, "%u", &min);
		
		freq = 0;
		sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		if(ret != 1) f_scanf(path, "%u", &min);
		else if(f_scanf(path, "%u", &freq) == 1 && min > freq) min = freq;
		
		max = 0;
		sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		ret = f_scanf(path, "%u", &max);
		
		freq = 0;
		sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		if(ret != 1) f_scanf(path, "%u", &max);
		else if(f_scanf(path, "%u", &freq) == 1 && freq > max) max = freq;
		
		fprintf(stdout, "[%s]%4d%6.3f%6.3f\n", nowtime(), i, min / 1000000.0f, max / 1000000.0f);
	}

	fflush(stdout);

	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);

head:
	n = 0;
	fprintf(stdout, "[%s]%s\n", nowtime(), split);
	fprintf(stdout, "[%s]", nowtime());
	for(i = 0; i < sz; i ++)
	{
		char col[24];
		sprintf(col, "%6d", i);
		fprintf(stdout, "%6s", col);
	}
	fprintf(stdout, "\n");
	fprintf(stdout, "[%s]%s\n", nowtime(), split);
	fflush(stdout);

	while(is_running)
	{
		fprintf(stdout, "[%s]", nowtime());
		for(i = 0; i < sz; i ++)
		{
			max = 0;
			sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", i);
			ret = f_scanf(path, "%u", &max);
			
			freq = 0;
			sprintf(path, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", i);
			if(ret != 1) f_scanf(path, "%u", &max);
			else if(f_scanf(path, "%u", &freq) == 1 && freq > max) max = freq;

			fprintf(stdout, "%6.3f", max / 1000000.0f);
		}
		fprintf(stdout, "\n");
		fflush(stdout);
		sleep(1);
		if(++n >= 20) goto head;
	}

	return 0;
}
