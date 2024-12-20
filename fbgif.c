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

void PrintGifError(int ErrorCode) {
	const char *Err = GifErrorString(ErrorCode);

	if (Err != NULL)
		fprintf(stderr, "GIF-LIB error: %s.\n", Err);
	else
		fprintf(stderr, "GIF-LIB undefined error %d.\n", ErrorCode);
}

volatile unsigned int is_running = 1;
volatile unsigned int is_debug = 0;
volatile int loop_times = 0x7fffffff;
volatile double start_time;

// GIF global variable
static GifFileType *gifFile;
static int *delayTimes;
static int *TransparentColors;
static int *DisposalModes;
static int curFrame = 0;

// framebuffer global variable
static char *fb_addr = NULL;
int width = 0, height = 0;
int xoffset = 0, xsize = 0;
int fb_bpp;
int fb_size;
int xleft = 0, ytop = 0;
struct fb_var_screeninfo vinfo;

static int fb_init(void) {
	int fd;
	
	fd = open(FB_FILE, O_RDWR);
	if(fd < 0) {
		perror(FB_FILE);
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
		SavedImage *image = &gifFile->SavedImages[curFrame];
		register int x, y, left = xleft + image->ImageDesc.Left, top = ytop + image->ImageDesc.Top;
		register unsigned char *p;
		GifColorType *color, *bgColor;
		GifByteType index;
		ColorMapObject *ColorMap;
		
		if(image->ImageDesc.ColorMap == NULL) {
			if(gifFile->Image.Interlace) {
				ColorMap = gifFile->Image.ColorMap;
				if(!ColorMap) ColorMap = gifFile->SColorMap;
			} else {
				ColorMap = gifFile->SColorMap;
			}
		} else {
			ColorMap = image->ImageDesc.ColorMap;
		}

		if(gifFile->Image.Interlace && gifFile->Image.ColorMap) bgColor = &gifFile->Image.ColorMap->Colors[gifFile->SBackGroundColor];
		else bgColor = &gifFile->SColorMap->Colors[gifFile->SBackGroundColor];
		
		if(is_debug) fprintf(stderr, "curFrame: %d, DelayTime: %dms, Left: %d, Top: %d, Width: %d, Height: %d, ColorMap: %p, ColorCount: %d, BitsPerPixel: %d, SortFlag: %d, ExtensionBlockCount: %d, Interlace: %d\n", curFrame, delayTimes[curFrame], image->ImageDesc.Left, image->ImageDesc.Top, image->ImageDesc.Width, image->ImageDesc.Height, image->ImageDesc.ColorMap, ColorMap->ColorCount, ColorMap->BitsPerPixel, ColorMap->SortFlag, image->ExtensionBlockCount, image->ImageDesc.Interlace);

		for(y = 0; y < image->ImageDesc.Height && top + y < height; y ++) {
			if(top + y < 0) continue;
			
			p = fb_addr + xoffset * (top + y) + left * fb_bpp / 8;
			for(x = 0; x < image->ImageDesc.Width && left + x < width; x ++) {
				if(left + x < 0) {
					p += fb_bpp / 8;
					continue;
				}
				index = image->RasterBits[y * image->ImageDesc.Width + x];
				if(index == TransparentColors[curFrame]) {
					if(DisposalModes[curFrame] == 1) {
						p += fb_bpp / 8;
						continue;
					} else {
						color = bgColor;
					}
				} else {
					color = &ColorMap->Colors[index];
				}

				switch(fb_bpp) {
					case 24: {
						p[vinfo.red.offset / 8] = color->Red;
						p[vinfo.green.offset / 8] = color->Green;
						p[vinfo.blue.offset / 8] = color->Blue;
						p += 3;
						break;
					}
					case 32: {
						p[vinfo.red.offset / 8] = color->Red;
						p[vinfo.green.offset / 8] = color->Green;
						p[vinfo.blue.offset / 8] = color->Blue;
						if(vinfo.transp.length) p[vinfo.transp.offset / 8] = 0xff;
						else p[3] = 0xff;
						p += 4;
						break;
					}
					case 16: {
						* (unsigned short *) p = ((color->Red << vinfo.red.offset) & ((1 << vinfo.red.length) - 1)) | ((color->Green << vinfo.green.offset) & ((1 << vinfo.green.length) - 1)) | ((color->Blue << vinfo.blue.offset) & ((1 << vinfo.blue.length) - 1));
						p += 2;
						break;
					}
					default: {
						*p++ = color->Red * 0.3f + color->Green * 0.59f + color->Blue * 0.11f;
					}
				}
			}
		}
	}
	
	if(++curFrame >= gifFile->ImageCount) curFrame = 0;
}

static void signal_handler(int sig) {
	switch(sig) {
		case SIGINT:
			is_running = 0;
			break;
		case SIGALRM:
			if(!is_running) break;
			if(curFrame == 0 && --loop_times < 0 && (microtime() - start_time) > 3.0f) {
				is_running = 0;
				break;
			}
			display_frame();
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
		printf("usage: %s <gif> <debug> <loop times>\n", argv[0]);
		return 1;
	}
	
	if(access(argv[1], R_OK)) {
		perror("gif file no readable");
		return 1;
	}
	
	if(argc >= 3) is_debug = atoi(argv[2]);
	if(argc >= 4) loop_times = atoi(argv[3]);
	
	gifFile = DGifOpenFileName(argv[1], &i);
	if(!gifFile) {
		PrintGifError(i);
		return 1;
	}
	
	if(DGifSlurp(gifFile) != GIF_OK) {
		PrintGifError(gifFile->Error);
		goto gif;
	}
	
	if(is_debug) fprintf(stderr, "Version: %s, SWidth: %d, SHeight: %d, SBackGroundColor: %d, SColorResolution: %d, AspectByte: %d, UserData: %p, Private: %p, ExtensionBlockCount: %d, Image.Interlace: %d\n", DGifGetGifVersion(gifFile), gifFile->SWidth, gifFile->SHeight, gifFile->SBackGroundColor, gifFile->SColorResolution, gifFile->AspectByte, gifFile->UserData, gifFile->Private, gifFile->ExtensionBlockCount, gifFile->Image.Interlace);
	
	delayTimes = (int *) malloc(sizeof(int)*gifFile->ImageCount);
	TransparentColors = (int *) malloc(sizeof(int)*gifFile->ImageCount);
	DisposalModes = (int *) malloc(sizeof(int)*gifFile->ImageCount);
	for(i = 0; i < gifFile->ImageCount; i ++) {
		if(DGifSavedExtensionToGCB(gifFile, i, &ecb) != GIF_OK) {
			PrintGifError(gifFile->Error);
			goto gif;
		}
		// printf("%d: %d %d\n", i, ecb.DisposalMode, ecb.UserInputFlag);
		DisposalModes[i] = ecb.DisposalMode;
		if(ecb.DelayTime < 4) ecb.DelayTime = 4; // min delay time 0.02 second
		delayTimes[i] = ecb.DelayTime * 10;
		TransparentColors[i] = ecb.TransparentColor;
	}

	fd = fb_init();
	
	if(fb_bpp != 32 && fb_bpp != 24) goto end;
	
	xleft = (width - gifFile->SWidth) / 2;
	ytop = (height - gifFile->SHeight) / 2;

	signal(SIGINT, signal_handler);

	fprintf(stdout, "\033[?25l"); // hide cursor
	fflush(stdout);

	buf = (char*) malloc(xsize * height);
	p = buf;
	p2 = fb_addr;
	for(i = 0; i < height; i ++) {
		memcpy(p, p2, xsize);
		memset(p2, 0, xsize);
		
		p += xsize;
		p2 += xoffset;
	}

	start_time = microtime();

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

	free(buf);

	fprintf(stdout, "\033[?25h"); // show cursor
	fflush(stdout);

end:
	munmap(fb_addr, fb_size);
	close(fd);

gif:
	if(DGifCloseFile(gifFile, &i) != GIF_OK) PrintGifError(i);
	free(delayTimes);
	free(TransparentColors);
	free(DisposalModes);

	return 0;
}

