// gcc -Wno-unused-result -O3 -o cpu-memory-info cpu-memory-info.c -lm && ./cpu-memory-info

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/time.h>
#include <time.h>

#define NCOMM 128
#define NPROC 1000
#define BUFLEN 1024

#define cpu_mem_head(t, c, m) \
	do { \
		printf(t); \
		if(hasCpu) printf(c); \
		if(hasCpu && hasMem) printf("|"); \
		if(hasMem) printf(m); \
		printf("\n"); \
	} while(0)

typedef struct {
	unsigned long int user;
	unsigned long int nice;
	unsigned long int system;
	unsigned long int idle;
	unsigned long int iowait;
	unsigned long int irq;
	unsigned long int softirq;
	unsigned long int stolen;
	unsigned long int guest;
} cpu_t;

void getcpu(cpu_t *cpu) {
	char buff[BUFLEN];
	char strcpu[16];
	FILE *fp;
	
	memset(cpu, 0, sizeof(cpu_t));

	fp = fopen("/proc/stat", "r");
	if(!fp) return;

	fgets(buff, sizeof(buff), fp);

	fclose(fp);
	
	sscanf(buff, "%s%ld%ld%ld%ld%ld%ld%ld%ld%ld", strcpu, &cpu->user, &cpu->nice, &cpu->system, &cpu->idle, &cpu->iowait, &cpu->irq, &cpu->softirq, &cpu->stolen, &cpu->guest);
}

typedef struct {
	unsigned long int total; // MemTotal
	unsigned long int free; // MemFree
	unsigned long int cached; // Cached
	unsigned long int buffers; // Buffers
	unsigned long int locked; // Mlocked
	unsigned long int swapTotal; // SwapTotal
	unsigned long int swapFree; // SwapFree
	unsigned long int shared; // Shmem
} mem_t;

void getmem(mem_t *mem) {
	char buff[BUFLEN] = "";
	char key[20] = "";
	long int val = 0;
	FILE *fp;
	int i = 0377;
	
	memset(mem, 0, sizeof(mem_t));
	
	fp = fopen("/proc/meminfo", "r");
	if(!fp) return;

	
	while(i > 0 && fgets(buff, sizeof(buff), fp) && sscanf(buff, "%[^:]: %ld", key, &val) == 2) {
		// printf("%s => %ld\n", key, val);

		if((i & 01) && !strcmp(key, "MemTotal")) {
			mem->total = val;
			i ^= 01;
			continue;
		}
		if((i & 02) && !strcmp(key, "MemFree")) {
			mem->free = val;
			i ^= 02;
			continue;
		}
		if((i & 04) && !strcmp(key, "Buffers")) {
			mem->buffers = val;
			i ^= 04;
			continue;
		}
		if((i & 010) && !strcmp(key, "Cached")) {
			mem->cached = val;
			i ^= 010;
			continue;
		}
		if((i & 020) && !strcmp(key, "Mlocked")) {
			mem->locked = val;
			i ^= 020;
			continue;
		}
		if((i & 040) && !strcmp(key, "SwapTotal")) {
			mem->swapTotal = val;
			i ^= 040;
			continue;
		}
		if((i & 0100) && !strcmp(key, "SwapFree")) {
			mem->swapFree = val;
			i ^= 0100;
			continue;
		}
		if((i & 0200) && !strcmp(key, "Shmem")) {
			mem->shared = val;
			i ^= 0200;
			continue;
		}
	}

	fclose(fp);
}

char *fsize(unsigned long int size) {
	static char buf[32];
	char *units = "KMGT";
	unsigned int unit;

	if(!size) {
		return "0K";
	}
	
	unit = (int)(log(size)/log(1024));
	if(unit > 3) {
		unit = 3;
	}

	sprintf(buf, "%.1lf%c", (double)size/pow(1024,unit), units[unit]);

	return buf;
}

typedef struct {
	// process memory
	long int size; // total program size (same as VmSize in /proc/[pid]/status)
	long int resident; // resident set size (same as VmRSS in /proc/[pid]/status)
	long int share; // shared pages (i.e., backed by a file)
	long int text; // text (code)
	long int lib; // library (unused in Linux 2.6)
	long int data; // data + stack
	long int dirty; // dirty KB(unused in Linux 2.6)
	long int rssFile; // resident - dirty KB

	// process cpu
	long int utime; // Amount  of  time that this process has been scheduled in user mode, measured in clock ticks
	long int stime; // Amount of time that this process has been scheduled in kernel mode, measured in clock ticks
	long int cutime; // Amount of time that this process's waited-for children have been scheduled in user mode, measured in clock ticks
	long int cstime; // Amount of time that this process's waited-for children have been scheduled in kernel mode, measured in clock ticks

	unsigned int threads; // Number of threads in this process (since Linux 2.6)
	unsigned int etime; // runned time for seconds
} process_t;

//获取第N项开始的指针
const char* get_items(const char*buffer, unsigned int item) {
	const char *p = buffer;

	int len = strlen(buffer);
	int count = 0, i;

	for(i=0; i<len; i++) {
		if(' ' == *p) {
			count ++;
			if(count == item -1) {
				p++;
				break;
			}
		}
		p++;
	}

	return p;
}

unsigned long int getprocessdirtys(int pid) {
	char buff[BUFLEN] = "";
	char fname[64] = "";
	char key[64] = "";
	long int val = 0;
	FILE *fp;
	unsigned long int dirtys = 0;
	
	snprintf(fname, sizeof(fname), "/proc/%d/smaps", pid);

	fp = fopen(fname, "r");
	if(!fp) return 0;

	while(fgets(buff, sizeof(buff), fp)) {
		if(sscanf(buff, "%[^:]: %ld", key, &val) == 2 && (!strcmp(key, "Private_Dirty") || !strcmp(key, "Shared_Dirty"))) {
			dirtys += val;
		}
	}
	fclose(fp);
	
	return dirtys;
}

int getprocessinfo(int pid, process_t *proc) {
	char buff[BUFLEN] = "";
	char fname[64] = "";
	char key[20] = "";
	long int val = 0;
	FILE *fp;

	snprintf(fname, sizeof(fname), "/proc/%d/statm", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);

	sscanf(buff, "%ld%ld%ld%ld%ld%ld%ld", &proc->size, &proc->resident, &proc->share, &proc->text, &proc->lib, &proc->data, &proc->dirty);

	snprintf(fname, sizeof(fname), "/proc/%d/stat", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);

	const char *q = get_items(strchr(buff, ')'), 13);
	sscanf(q, "%ld%ld%ld%ld", &proc->utime, &proc->stime, &proc->cutime, &proc->cstime);
	q = get_items(q, 7);
	sscanf(q, "%u", &proc->threads);
	q = get_items(q, 3);
	
	unsigned long int etime = 0;
	sscanf(q, "%lu", &etime);
	
	fp = fopen("/proc/uptime", "r");
	fgets(buff, sizeof(buff) - 1, fp);

	fclose(fp);
	
	unsigned long int uptime = 0;
	sscanf(buff, "%lu", &uptime);
	
	long tck = sysconf(_SC_CLK_TCK);
	proc->etime = uptime - etime / tck;

	snprintf(fname, sizeof(fname), "/proc/%d/status", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	proc->dirty = 0;
	proc->rssFile = 0;
	while(fgets(buff, sizeof(buff), fp)) {
		if(sscanf(buff, "%[^:]: %ld", key, &val) == 2 && !strcmp(key, "RssFile")) {
			proc->rssFile = val;
			proc->dirty = proc->resident * 4 - val;
		}
	}
	fclose(fp);
	
	if(!proc->dirty || !proc->rssFile) {
		proc->dirty = getprocessdirtys(pid);
		proc->rssFile  = proc->resident * 4 - proc->dirty;
	}

	return 1;
}

int getcomm(char *pid, char *comm, int size) {
	char fname[64] = "";
	FILE *fp;
	char *ptr;
	int len;

	snprintf(fname, sizeof(fname), "/proc/%s/comm", pid);

	fp = fopen(fname, "r");
	if(!fp) {
		return 0;
	}

	len = fread(comm, 1, size - 1, fp);
	if(len <= 0) return 0;

	fclose(fp);
	
	comm[len] = '\0';

	ptr = comm;
	while(*ptr && !(*ptr == '\r' || *ptr == '\n' || *ptr == ' ')) ptr ++;
	*ptr = '\0';
	
	return 1;
}

char procArgStr[NPROC][BUFLEN];

int procarg(int comm_size, char *comms[], int nproc, int *pid, process_t *proc) {
	char fname[64] = "", comm[BUFLEN];
	int n, i;

	// 清除为0的pid[x]
	i = 0;
	for(n=0; n<nproc; n++) {
		if(pid[n]) {
			if(n != i) {
				pid[i] = pid[n];
				pid[n] = 0;
			}
			
			i++;
		}
	}
	nproc = i;

	if(comm_size) {
		DIR *dp;
		struct dirent *dt;
	
		dp = opendir("/proc");
		while(nproc < NPROC && (dt = readdir(dp)) != NULL) {
			if(dt->d_name[0] >= '0' && dt->d_name[0] <= '9') {
				if(getcomm(dt->d_name, comm, BUFLEN)) {
					for(i = 0; i < comm_size && nproc < NPROC; i ++) {
						if(!strcmp(comm, comms[i])) pid[nproc++] = atoi(dt->d_name);
					}
				}
			}
		}
		closedir(dp);
	}
	
	// 去重
	for(n=0; n<nproc; n++) {
		if(pid[n]) {
			for(i = n+1; i < nproc; i++) {
				if(pid[n] == pid[i]) {
					pid[i] = 0;
				}
			}
		}
	}
	
	for(n=0; n<nproc; n++) {
		int len = snprintf(fname, sizeof(fname), "/proc/%d/cmdline", pid[n]);
		FILE *fp = fopen(fname, "r");
		if(!fp) {
			pid[n] = 0;
			continue;
		}
		len = fread(procArgStr[n], 1, sizeof(procArgStr[0]) - 1, fp);
		fclose(fp);

		if(len > 0) procArgStr[n][len] = '\0';

		for(i=0; i<len; i++) {
			if(procArgStr[n][i] == '\0') {
				procArgStr[n][i] = ' ';
			}
		}

		if(!getprocessinfo(pid[n], &proc[n])) {
			pid[n] = 0;
			continue;
		}
	}
	
	return nproc;
}

char *thread_comm(int pid) {
	char path[128], path2[512], comm[128];
	DIR *dp;
	struct dirent *dt;
	FILE *fp;
	int n;
	char *tasks = NULL;

	sprintf(path, "/proc/%d/task", pid);
	
	dp = opendir(path);
	while((dt = readdir(dp)) != NULL) {
		if(dt->d_name[0] >= '0' && dt->d_name[0] <= '9') {
			sprintf(path2, "/proc/%d/task/%s/comm", pid, dt->d_name);

			fp = fopen(path2, "r");
			if(fp) {
				n = fread(comm, 1, sizeof(comm)-1, fp);
				if(n > 0) {
					comm[n--] = '\0';
					while(comm[n] == '\r' || comm[n] == '\n') comm[n--] = '\0';

					if(tasks) {
						asprintf(&tasks, "%s, [%s] %s", tasks, dt->d_name, comm);
					} else {
						asprintf(&tasks, "[%s] %s", dt->d_name, comm);
					}
				}
				fclose(fp);
				fp = NULL;
			}
		}
	}
	closedir(dp);
	
	return tasks;
}

static int lines = 0;

static void ignore_handler(int sig) {
}

static void signal_handler(int sig) {
	lines = 0;
	printf("\r");
}

int main(int argc, char *argv[]) {
	cpu_t cpu, cpu2;
	mem_t mem;
	int pid[NPROC];
	process_t proc[NPROC], proc2[NPROC];

	unsigned int all = 0, all2 = 0, delay = 1;

	double total;
	long int realUsed;
	char hasCpu = 1, hasMem = 1, isTask = 0, isQuiet = 0;
	int nproc = 0, n, comm_size = 0;
	char *comms[NCOMM];

	int nn, opt;
	struct winsize wsize;
	time_t t;
	struct tm tm;
	int istty = isatty(1);
	
	while((opt = getopt(argc, argv, "cmP:p:Lqd:h?")) != -1) {
		switch(opt) {
			case 'c':
				if(!nproc && !comm_size) {
					hasMem = 0;
					hasCpu = 1;
				}
				break;
			case 'm':
				if(!nproc && !comm_size) {
					hasMem = 1;
					hasCpu = 0;
				}
				break;
			case 'P':
				if(comm_size < NCOMM) {
					n = strlen(optarg);
					if(n > 15) optarg[15] = '\0';
					comms[comm_size++] = optarg;
				}
				
				break;
			case 'p':
				hasCpu = 0;
				hasMem = 0;
				
				if(nproc < NPROC) {
					pid[nproc++] = atoi(optarg);
				}
				break;
			case 'L':
				isTask = 1;
				break;
			case 'q':
				isQuiet = 1;
				break;
			case 'd':
				delay = atoi(optarg);
				if(delay <= 0) {
					delay = 1;
				}
				break;
			default:
				printf("Usage: %s [-c] [-m] [-P <comm>] [-p <pid>] [-L] [-q]  [-d <delay>] [-h] [-?]\n"
						"  -c        Cpu info\n"
						"  -m        Memory info\n"
						"  -P <comm> Process comm\n"
						"  -p <pid>  Process info(Multiple)\n"
						"  -L        List thread of comm\n"
						"  -q        Quiet\n"
						"  -h,-?     This help\n", argv[0]);
				return 0;
		}
	}
	
	if(comm_size || nproc) {
		hasCpu = 0;
		hasMem = 0;
	}

	if(hasCpu) {
		getcpu(&cpu);

		all = cpu.user + cpu.nice + cpu.system + cpu.idle + cpu.iowait + cpu.irq + cpu.softirq + cpu.stolen + cpu.guest;
	}
	
	if(hasCpu) lines = 1000;

	signal(SIGQUIT, signal_handler);
	signal(SIGALRM, ignore_handler);
	
	{
		struct itimerval itv;

		itv.it_interval.tv_sec = itv.it_value.tv_sec = delay;
		itv.it_interval.tv_usec = itv.it_value.tv_usec = 0;
		setitimer(ITIMER_REAL, &itv, NULL);
	}

	while(1) {
		if(lines) sleep(delay);

		if(ioctl(STDIN_FILENO, TIOCGWINSZ, &wsize) || wsize.ws_row < 20) {
			wsize.ws_row = 20;
		}
		if(lines == 0 || lines >= wsize.ws_row) {
			if(hasCpu || hasMem) {
				sleep(delay);
				cpu_mem_head("--------|", "---------------------------------------------------------", "---------------------------------------|---------------|-------------------");
				cpu_mem_head("        |", "                         CPU (%%)                         ", "                      Memory Size      |     Swap      |  Real Memory (%%)  ");
				cpu_mem_head("  Time  |", "---------------------------------------------------------", "---------------------------------------|---------------|-------------------");
				cpu_mem_head("        |", " User  Nice System  Idle IOWait  IRQ SoftIRQ Stolen Guest", "  Total    Free  Cached Buffers  Shared|  Total    Free|Memory Cached  Swap");
				cpu_mem_head("--------|", "---------------------------------------------------------", "---------------------------------------|---------------|-------------------");
				lines = 6;
			}
			
			nproc = procarg(comm_size, comms, nproc, pid, proc);

			if(nproc > 0) {
				nn = nproc;
				for(n=0; n<nproc && isQuiet == 0; n++) {
					if(pid[n]>0) {
						unsigned int mtime = proc[n].etime/60;
						unsigned int htime = mtime/60;
						unsigned int dtime = htime/24;
						if(istty) printf("\033[JK\r");
						printf("%d:\n  Run Time: (%u seconds) ", pid[n], proc[n].etime);
						if(dtime>0) {
							printf("%u-", dtime);
						}
						printf("%02u:%02u:%02u\n", htime%24, mtime%60, proc[n].etime%60);
						printf("   Command: %s\n", procArgStr[n]);
						if(isTask) {
							char *p = thread_comm(pid[n]);
							if(p) {
								printf("   Threads: %s\n", p);
								free(p);
							}
						}
					} else {
						nn --;
					}
				}
				if(nn<=0 && comm_size == 0) {
					return 0;
				}
				sleep(delay);
				printf("--------|--------|-------------------------------------------------------------------------|--------|---------------------\n");
				printf("        |        |                          Memory Size                                    |        |       CPU (%%)      \n");
				printf("  Time  |   PID  |-------------------------------------------------------------------------|nThreads|---------------------\n");
				printf("        |        |    Size      RSS    Share     Text  Library Data+Stack    Dirty     Real|        |   User Kernel  Total\n");
				printf("--------|--------|-------------------------------------------------------------------------|--------|---------------------\n");
				lines = 6;
			}
			
			if(lines == 0) {
				if(comm_size == 0) break;

				sleep(delay);
				continue;
			}

			fflush(stdout);
		}

		t = time(NULL);
		localtime_r(&t, &tm);
		
		if(hasCpu || hasMem) {
			printf("%02d:%02d:%02d|", tm.tm_hour, tm.tm_min, tm.tm_sec);
		}

		if(hasCpu) {
			getcpu(&cpu2);

			all2 = cpu2.user + cpu2.nice + cpu2.system + cpu2.idle + cpu2.iowait + cpu2.irq + cpu2.softirq + cpu2.stolen + cpu2.guest;

			total = (all2 - all) / 100.0;
			if(total == 0) total = 1;
		
			printf("%5.1f", (float)((double)(cpu2.user - cpu.user) / total));
			printf("%6.1f", (float)((double)(cpu2.nice - cpu.nice) / total));
			printf("%7.1f", (float)((double)(cpu2.system - cpu.system) / total));
			printf("%6.1f", (float)((double)(cpu2.idle - cpu.idle) / total));
			printf("%7.1f", (float)((double)(cpu2.iowait - cpu.iowait) / total));
			printf("%5.1f", (float)((double)(cpu2.irq - cpu.irq) / total));
			printf("%8.1f", (float)((double)(cpu2.softirq - cpu.softirq) / total));
			printf("%7.1f", (float)((double)(cpu2.stolen - cpu.stolen) / total));
			printf("%6.1f", (float)((double)(cpu2.guest - cpu.guest) / total));
		
			cpu = cpu2;
			all = all2;
		}
		
		if(hasCpu && hasMem) {
			printf("|");
		}

		if(hasMem) {
			getmem(&mem);
			printf("%7s", fsize(mem.total));
			printf("%8s", fsize(mem.free));
			printf("%8s", fsize(mem.cached));
			printf("%8s", fsize(mem.buffers));
			printf("%8s|", fsize(mem.shared));
			printf("%7s", fsize(mem.swapTotal));
			printf("%8s|", fsize(mem.swapFree));
			//printf("%6.2f", (float)((double)(mem.total - mem.free) * 100.0 / (double)mem.total)); // MemPercent
			printf("%6.2f", (float)((double)(realUsed = mem.total - mem.free - mem.cached - mem.buffers) * 100.0 / (double)mem.total)); // MemRealPercent
			printf("%7.2f", (float)((double)(mem.cached) * 100.0 / (double)mem.total)); // MemCachedPercent
			printf("%6.2f", (float)(mem.swapTotal ? (double)(mem.swapTotal - mem.swapFree) * 100.0 / (double)mem.swapTotal : 0.0)); // SwapPercent
		}

		if(hasCpu || hasMem) {
			printf("\n");
			lines ++;
			if(istty) fprintf(stderr, "Press Ctrl+\\ key for show table head\r");

			fflush(stdout);
			continue;
		}

		nn = nproc;
		for(n=0; n<nproc; n++) {
			if(pid[n] ==0 || !getprocessinfo(pid[n], &proc2[n])) {
				pid[n] = 0;
				nn--;
				continue;
			}
			printf("%02d:%02d:%02d|", tm.tm_hour, tm.tm_min, tm.tm_sec);
			printf("%8d|", pid[n]);
			printf("%8s", fsize(proc2[n].size * 4));
			printf("%9s", fsize(proc2[n].resident * 4));
			printf("%9s", fsize(proc2[n].share * 4));
			printf("%9s", fsize(proc2[n].text * 4));
			printf("%9s", fsize(proc2[n].lib * 4));
			printf("%11s", fsize(proc2[n].data * 4));
			printf("%9s", fsize(proc2[n].dirty));
			printf("%9s|", fsize(proc2[n].rssFile));
			printf("%8d|", proc2[n].threads);

			long int utime = (proc2[n].utime/* + proc2[n].cutime*/ - proc[n].utime/* - proc[n].cutime*/) / delay;
			long int stime = (proc2[n].stime/* + proc2[n].cstime*/ - proc[n].stime/* - proc[n].cstime*/) / delay;
			long int ttime = utime + stime;
			if(ttime > proc2[n].threads * 100 && utime <= proc2[n].threads * 100) {
				ttime = proc2[n].threads * 100;
				stime = ttime - utime;
			}
			printf("%7ld", utime);
			printf("%7ld", stime);
			printf("%7ld\n", ttime);

			lines ++;

			proc[n] = proc2[n];
		}
		if(nproc > 0 && nn <= 0 && comm_size == 0) {
			return 0;
		} else if(nn > 1) {
			lines ++;

			printf("--------------------------------------------------------------------------------------------------------------------------\n");
		} else if(nn == 0) {
			lines = 0;
			continue;
		}

		if(istty) fprintf(stderr, "Press Ctrl+\\ key for show table head\r");
		fflush(stdout);
	}

	return 0;
}
