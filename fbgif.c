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

#include <gif_lib.h>

void PrintGifError(int ErrorCode) {
	const char *Err = GifErrorString(ErrorCode);

	if (Err != NULL)
		fprintf(stderr, "GIF-LIB error: %s.\n", Err);
	else
		fprintf(stderr, "GIF-LIB undefined error %d.\n", ErrorCode);
}

volatile unsigned int is_running = 1;
volatile unsigned int is_debug = 0; 

// GIF global variable
static GifFileType *gifFile;
static int *delayTimes;
static int curFrame = 0;

// framebuffer global variable
static char *fb_addr = NULL;
int width = 0, height = 0;
int xoffset = 0, xsize = 0;
int fb_bpp;
int fb_size;
int xleft = 0, ytop = 0;

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

	if(is_debug) fprintf(stderr, "size: %dx%d, bpp: %d, mmap: %p\n", width, height, fb_bpp, fb_addr);
 
	return fd;
}

static void display_frame() {
	{
		struct itimerval itv;

		itv.it_interval.tv_sec = 0;
		itv.it_interval.tv_usec = delayTimes[curFrame] * 1000; // 40ms

		itv.it_value.tv_sec = 0;
		itv.it_value.tv_usec = delayTimes[curFrame] * 1000; // 40ms

		setitimer(ITIMER_REAL, &itv, NULL);
	}

	{
		register int x, y;
		SavedImage *image = &gifFile->SavedImages[curFrame];
		register unsigned char *p;
		GifColorType *color;
		GifByteType index;
		
		if(is_debug) fprintf(stderr, "curFrame: %d, DelayTime: %dms, Left: %d, Top: %d, Width: %d, Height: %d, ColorMap: %p\n", curFrame, delayTimes[curFrame], image->ImageDesc.Left, image->ImageDesc.Top, image->ImageDesc.Width, image->ImageDesc.Height, image->ImageDesc.ColorMap);

		for(y = 0; y < image->ImageDesc.Height; y ++) {
			p = fb_addr + xoffset * (y + ytop + image->ImageDesc.Top) + xleft + image->ImageDesc.Left * fb_bpp / 8;
			for(x = 0; x < image->ImageDesc.Width; x ++) {
				index = image->RasterBits[y * image->ImageDesc.Width + x];
				if(image->ImageDesc.ColorMap == NULL) {
					color = &gifFile->SColorMap->Colors[index];
				} else {
					color = &image->ImageDesc.ColorMap->Colors[index];
				}
				if(fb_bpp == 32) {
					*p++ = color->Red;
					*p++ = color->Green;
					*p++ = color->Blue;
					*p++ = 0xff;
				} else {
					*p++ = color->Blue;
					*p++ = color->Green;
					*p++ = color->Red;
				}
			}
		}
	}
	
	curFrame++;
	if(curFrame == gifFile->ImageCount) curFrame = 0;
}

static void signal_handler(int sig) {
	switch(sig) {
		case SIGINT:
			is_running = 0;
			break;
		case SIGALRM:
			if(is_running) display_frame();
			break;
		default:
			printf("SIG: %d\n", sig);
	}
}

int main(int argc, char *argv[]) {
	int fd, i;
	GraphicsControlBlock ecb;
	char *buf, *p, *p2;

	if(argc < 2) {
		printf("usage: %s <gif>\n", argv[0]);
		return 1;
	}
	
	if(access(argv[1], R_OK)) {
		perror("gif file no readable");
		return 1;
	}
	
	if(argc >= 3) is_debug = atoi(argv[2]);
	
	gifFile = DGifOpenFileName(argv[1], &i);
	if(!gifFile) {
		PrintGifError(i);
		return 1;
	}
	
	if(DGifSlurp(gifFile) != GIF_OK) {
		PrintGifError(gifFile->Error);
		goto gif;
	}
	
	if(is_debug) fprintf(stderr, "SWidth: %d, SHeight: %d\n", gifFile->SWidth, gifFile->SHeight);
	
	delayTimes = (int *) malloc(sizeof(int)*gifFile->ImageCount);
	for(i = 0; i < gifFile->ImageCount; i ++) {
		if(DGifSavedExtensionToGCB(gifFile, i, &ecb) != GIF_OK) {
			PrintGifError(gifFile->Error);
			goto gif;
		}
		delayTimes[i] = ecb.DelayTime * 10;
	}

	fd = fb_init();
	
	if(fb_bpp != 32 && fb_bpp != 24) goto end;
	
	if(gifFile->SWidth < width) xleft = (width - gifFile->SWidth) * fb_bpp / 8 / 2;
	if(gifFile->SHeight < height) ytop = (height - gifFile->SHeight) / 2;

	signal(SIGINT, signal_handler);
	
	buf = (char*) malloc(xsize * height);
	p = buf;
	p2 = fb_addr;
	for(i = 0; i < height; i ++) {
		memcpy(p, p2, xsize);
		memset(p2, 0, xsize);
		
		p += xsize;
		p2 += xoffset;
	}

	signal(SIGALRM, signal_handler);
	display_frame();
	while(is_running) usleep(1000);
	
	p = buf;
	p2 = fb_addr;
	for(i = 0; i < height; i ++) {
		memcpy(p2, p, xsize);
		
		p += xsize;
		p2 += xoffset;
	}

end:
	free(buf);
	munmap(fb_addr, fb_size);
	close(fd);

gif:
	if(DGifCloseFile(gifFile, &i) != GIF_OK) PrintGifError(i);

	return 0;
}

