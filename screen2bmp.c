#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <asm/types.h> 
#include <linux/videodev2.h>
#include <linux/fb.h>

#include <math.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>

double microtime() {
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double)(tp.tv_sec + tp.tv_usec / 1000000.00);
}

volatile unsigned int is_running = 1;
static unsigned int capframe = 0;
static unsigned int maxframe = 0;
static unsigned char format[30];
static unsigned char filename[30];
 
unsigned char bmp_head_66[] = {
	0x42, 0x4d, 0x42, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x42, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0xf0, 0x00,
	0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xe0, 0x07,
	0x00, 0x00, 0x1f, 0x00, 0x00, 0x00
};

unsigned char bmp_head_54[54];
static char *fb_addr = NULL;
int width = 0, height = 0;
int xoffset = 0, xsize = 0;
int fb_bpp;
int fb_size;

static int fb_init(void) {
	struct fb_var_screeninfo vinfo;
	int fd;
	
	fd = open("/dev/fb0", O_RDWR);
	if(fd < 0) {
		perror("/dev/fb0");
		exit(1);
	}

	if(ioctl(fd, FBIOGET_VSCREENINFO, &vinfo)) {
		printf("Error reading variable information.\n");
		close(fd);
		exit(1);
	}

	width = vinfo.xres;
	xoffset = vinfo.xres_virtual * vinfo.bits_per_pixel / 8;
	xsize = vinfo.xres * vinfo.bits_per_pixel / 8;
	height = vinfo.yres;
	fb_bpp = vinfo.bits_per_pixel;

	fb_size = vinfo.xres_virtual * vinfo.yres_virtual * fb_bpp / 8;
	fb_addr = (char*) mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(fb_addr == MAP_FAILED) {
		perror("mmap()");
		close(fd);
		exit(1);
	}

	printf("[%lf] size: %dx%d, bpp: %d, mmap: %p\n", microtime(), width, height, fb_bpp, fb_addr);
 
	return fd;
}

void save_to_file() {
	FILE *fp;

	printf("[%lf] ", microtime());

	capframe++;
	sprintf(filename, format, capframe);

	printf("Saving to %s ...\n", filename);

	fp = fopen(filename, "w+");
	if(fb_bpp == 16)
		fwrite(bmp_head_66, 1, 66, fp);
	else
		fwrite(bmp_head_54, 1, 54, fp);

	for(int y = 0; y < height; y++)
		fwrite(fb_addr + (height - 1 - y) * xoffset, 1, xsize, fp);

	fclose(fp);
}

void signal_handler(int sig) {
	switch(sig) {
		case SIGINT:
			is_running = 0;
			break;
		case SIGALRM:
			if(maxframe--) save_to_file();
			else is_running = 0;
			break;
		default:
			printf("SIG: %d\n", sig);
	}
}

int main(int argc, char *argv[]) {
	int fd = fb_init();
 
	if(fb_bpp == 16){
	    *((unsigned int*)(bmp_head_66 + 18)) = width;
	    *((unsigned int*)(bmp_head_66 + 22)) = height;
	    *((unsigned short*)(bmp_head_66 + 28)) = 16;
	}else{
		bmp_head_54[0] = 'B';
		bmp_head_54[1] = 'M';
		*((unsigned int*)(bmp_head_54 + 2)) = (width * fb_bpp / 8) * height + 54;
		*((unsigned int*)(bmp_head_54 + 10)) = 54;
		*((unsigned int*)(bmp_head_54 + 14)) = 40;
		*((unsigned int*)(bmp_head_54 + 18)) = width;
		*((unsigned int*)(bmp_head_54 + 22)) = height;
		*((unsigned short*)(bmp_head_54 + 26)) = 1;
		*((unsigned short*)(bmp_head_54 + 28)) = fb_bpp;
		*((unsigned short*)(bmp_head_54 + 34)) = (width * fb_bpp / 8) * height;
	}

	if(argc < 2 || (maxframe = atoi(argv[1])) < 1) {
		maxframe = 10;
	}

	sprintf(format, "screen-%%0%dd.bmp", (int) floor(log10(maxframe)) + 1);

	struct itimerval itv;

	// Interval for periodic timer
	// 周期计时器间隔，每次SIGALRM信号事件的时间间隔
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 40000; // 40ms

	// Time until next expiration
	// 下次到期前的时间，首次SIGALRM信号事件的时间间隔
	itv.it_value.tv_sec = 0;
	itv.it_value.tv_usec = 40000; // 40ms

	signal(SIGALRM, signal_handler);
	setitimer(ITIMER_REAL, &itv, NULL);
	printf("[%lf] start\n", microtime());
	while(is_running) usleep(1000);
	printf("[%lf] end\n", microtime());

	munmap(fb_addr, fb_size);
	close(fd);

	return 0;
}
