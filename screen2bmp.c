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
 
 
static unsigned int capframe = 0;
static unsigned char filename[30];
 
unsigned char bmp_head_t[] = {
	0x42, 0x4d, 0x42, 0x58, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x42, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0xf0, 0x00,
	0x00, 0x00, 0x40, 0x01, 0x00, 0x00, 0x01, 0x00, 0x10, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0xf8, 0x00, 0x00, 0xe0, 0x07,
	0x00, 0x00, 0x1f, 0x00, 0x00, 0x00
};

unsigned char bmp_head[54];
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

	printf("%dx%d bpp:%d mmaped %p\n", width, height, fb_bpp, fb_addr);
 
	return fd;
}

void save_to_file(unsigned int size) {
	FILE *fp;

	capframe++;
	sprintf(filename, "screen-%02d.bmp", capframe);

	printf("Saving to %s ...\n", filename);

	fp = fopen(filename, "w+");
	if(fb_bpp == 16)
		fwrite(bmp_head_t, 1, 66, fp);
	else
		fwrite(bmp_head, 1, 54, fp);

	for(int y = 0; y < height; y++)
		fwrite(fb_addr + (height - 1 - y) * xoffset, 1, xsize, fp);

	fclose(fp);
}

int main(int argc, char *argv[]) {
	unsigned int i = 10;
	unsigned int size = 0;
	int fd = fb_init();

	size = width * height * fb_bpp / 8;
	printf("size: %u\n", size);
 
	if(fb_bpp == 16){
	    *((unsigned int*)(bmp_head_t + 18)) = width;
	    *((unsigned int*)(bmp_head_t + 22)) = height;
	    *((unsigned short*)(bmp_head + 28)) = 16;
	}else{
		bmp_head[0] = 'B';
		bmp_head[1] = 'M';
		*((unsigned int*)(bmp_head + 2)) = (width * fb_bpp / 8) * height + 54;
		*((unsigned int*)(bmp_head + 10)) = 54;
		*((unsigned int*)(bmp_head + 14)) = 40;
		*((unsigned int*)(bmp_head + 18)) = width;
		*((unsigned int*)(bmp_head + 22)) = height;
		*((unsigned short*)(bmp_head + 26)) = 1;
		*((unsigned short*)(bmp_head + 28)) = fb_bpp;
		*((unsigned short*)(bmp_head + 34)) = (width * fb_bpp / 8) * height;
	}

	while(i--) save_to_file(size);

	munmap(fb_addr, fb_size);
	close(fd);

	return 0;
}
