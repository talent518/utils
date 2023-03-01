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

#ifndef FB_FILE
#define FB_FILE "/dev/fb0"
#endif

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
static unsigned char format[48];
static unsigned char filename[48];
static int is_no_cgi = 1;

static unsigned char bmp_head_66[] = {
	0x42, 0x4d, 0x42, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x42, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0xf0, 0x00,
	0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xe0, 0x07,
	0x00, 0x00, 0x1f, 0x00, 0x00, 0x00
};
static unsigned char bmp_head_54[54];

static char *fb_addr = NULL;
static int fb_width = 0, fb_height = 0;
static int fb_xoffset = 0, fb_xsize = 0;
static int fb_bpp;
static int fb_size;
static struct fb_var_screeninfo fb_vinfo;

static int fb_init(void) {
	int fd;
	
	fd = open(FB_FILE, O_RDWR);
	if(fd < 0) {
		perror(FB_FILE);
		exit(1);
	}

	if(ioctl(fd, FBIOGET_VSCREENINFO, &fb_vinfo)) {
		printf("Error reading variable information.\n");
		close(fd);
		exit(1);
	}

	fb_width = fb_vinfo.xres;
	fb_xoffset = fb_vinfo.xres_virtual * fb_vinfo.bits_per_pixel / 8;
	fb_xsize = fb_vinfo.xres * fb_vinfo.bits_per_pixel / 8;
	fb_height = fb_vinfo.yres;
	fb_bpp = fb_vinfo.bits_per_pixel;

	fb_size = fb_vinfo.xres_virtual * fb_vinfo.yres_virtual * fb_bpp / 8;
	fb_addr = (char*) mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if(fb_addr == MAP_FAILED) {
		perror("mmap()");
		close(fd);
		exit(1);
	}

	printf("[%lf] size: %dx%d, bpp: %d, mmap: %p\n", microtime(), fb_width, fb_height, fb_bpp, fb_addr);
 
	return fd;
}

void save_to_file() {
	FILE *fp;
	char *p;

	printf("[%lf] ", microtime());

	capframe++;
	sprintf(filename, format, capframe);

	printf("Saving to %s ...\n", filename);

	fp = fopen(filename, "w+");
	if(fb_bpp == 16)
		fwrite(bmp_head_66, 1, 66, fp);
	else
		fwrite(bmp_head_54, 1, 54, fp);

	for(int y = 0; y < fb_height; y++)
		fwrite(fb_addr + (fb_height - 1 - y) * fb_xoffset, 1, fb_xsize, fp);

	fclose(fp);
}

void cgi_out(const char *sp) {
	printf("%s OK\r\nConnection: Close\r\nContent-Type: image/bmp\r\nContent-Length: %d\r\n\r\n", sp, (fb_bpp == 16 ? 66 : 54) + fb_height * fb_xsize);

	if(fb_bpp == 16)
		fwrite(bmp_head_66, 1, 66, stdout);
	else
		fwrite(bmp_head_54, 1, 54, stdout);

	for(int y = 0; y < fb_height; y++)
		fwrite(fb_addr + (fb_height - 1 - y) * fb_xoffset, 1, fb_xsize, stdout);
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
	    *((unsigned int*)(bmp_head_66 + 18)) = fb_width;
	    *((unsigned int*)(bmp_head_66 + 22)) = fb_height;
	    *((unsigned short*)(bmp_head_66 + 28)) = 16;
	}else{
		bmp_head_54[0] = 'B';
		bmp_head_54[1] = 'M';
		*((unsigned int*)(bmp_head_54 + 2)) = (fb_width * fb_bpp / 8) * fb_height + 54;
		*((unsigned int*)(bmp_head_54 + 10)) = 54;
		*((unsigned int*)(bmp_head_54 + 14)) = 40;
		*((unsigned int*)(bmp_head_54 + 18)) = fb_width;
		*((unsigned int*)(bmp_head_54 + 22)) = fb_height;
		*((unsigned short*)(bmp_head_54 + 26)) = 1;
		*((unsigned short*)(bmp_head_54 + 28)) = fb_bpp;
		*((unsigned short*)(bmp_head_54 + 34)) = (fb_width * fb_bpp / 8) * fb_height;
	}

	{
		char *sp = getenv("SERVER_PROTOCOL");
		if(sp && !strncmp(sp, "HTTP/", 5)) {
			cgi_out(sp);
			goto end;
		}
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

end:
	munmap(fb_addr, fb_size);
	close(fd);

	return 0;
}
