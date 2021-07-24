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

#include <jpeglib.h>

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

void save_jpeg_to_stream(FILE* fp, const char *sp) {
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	unsigned char *outbuffer = NULL;
    unsigned long outsize = 0;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if(sp) {
		jpeg_mem_dest(&cinfo, &outbuffer, &outsize);
	} else {
		jpeg_stdio_dest(&cinfo, fp);
	}

	cinfo.image_width = width;
	cinfo.image_height = height;
	cinfo.input_components = fb_bpp / 8;
	
	switch(cinfo.input_components) {
		case 1:
			cinfo.in_color_space = JCS_GRAYSCALE; // todo: no check is right
			break;
		case 2:
			cinfo.in_color_space = JCS_RGB565;
			break;
		case 3:
			cinfo.in_color_space = JCS_EXT_BGR;
			break;
		case 4:
			cinfo.in_color_space = JCS_EXT_RGBA;
			break;
		default:
			fprintf(stderr, "color type error\n");
			break;
	}

	jpeg_set_defaults(&cinfo);
	jpeg_start_compress(&cinfo,TRUE);

	JSAMPROW row_pointer[1];/* pointer to scanline */

	for(int y = 0; y < height; y++) {
		row_pointer[0] = fb_addr + y * xoffset;

		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	if(sp) {
		fprintf(fp, "%s OK\r\nConnection: Close\r\nImage-Size: %dx%d\r\nImage-Color: %d\r\nContent-Type: image/jpeg\r\nContent-Length: %lu\r\n\r\n", sp, width, height, fb_bpp, outsize);
		fwrite(outbuffer, 1, outsize, fp);
		free(outbuffer);
	}
}

void save_to_file() {
	FILE *fp;

	printf("[%lf] ", microtime());

	capframe++;
	sprintf(filename, format, capframe);

	printf("Saving to %s ...\n", filename);

	fp = fopen(filename, "w+");
	save_jpeg_to_stream(fp, NULL);
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

	{
		char *sp = getenv("SERVER_PROTOCOL");
		if(sp && !strncmp(sp, "HTTP/", 5)) {
			save_jpeg_to_stream(stdout, sp);
			goto end;
		}
	}

	if(argc < 2 || (maxframe = atoi(argv[1])) < 1) {
		maxframe = 10;
	}

	sprintf(format, "screen-%%0%dd.jpg", (int) floor(log10(maxframe)) + 1);

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
