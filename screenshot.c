/***************************************************************************

Written by WuXiao 2010.05.23
Compile this code using:
gcc -o output main.c -lX11 -ljpeg

***************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xutil.h>
#include <X11/Xlib.h>

#include <jpeglib.h>



/***************************************************************
Write XImage data to a JPEG file

Must include <jpeglib.h>
Return value:
0 - failed
1 - success
****************************************************************/
int JpegWriteFileFromImage(char *filename, XImage* img) {
	FILE* fp;
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	fp = fopen(filename,"wb");
	if(fp==NULL) {
		fprintf(stderr, "cannot write file %s", filename);
		return 0;
	}

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	jpeg_stdio_dest(&cinfo,fp);
	cinfo.image_width = img->width;
	cinfo.image_height = img->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;

	jpeg_set_defaults(&cinfo);
	jpeg_start_compress(&cinfo,TRUE);

	JSAMPROW row_pointer[1];/* pointer to scanline */
	unsigned char* pBuf = (unsigned char*)malloc(3*img->width);
	row_pointer[0] = pBuf;

	int i=0;
	while(cinfo.next_scanline < cinfo.image_height) {
		int j=0;
		for(i=0;i<img->width;i++) {
			// memcpy(pBuf+j, img->data + cinfo.next_scanline * img->bytes_per_line + i * 4, 3);
			*(pBuf + j) = *(img->data + cinfo.next_scanline * img->bytes_per_line + i * 4 + 2);
			*(pBuf + j + 1) = *(img->data + cinfo.next_scanline * img->bytes_per_line + i * 4 + 1);
			*(pBuf + j + 2) = *(img->data + cinfo.next_scanline * img->bytes_per_line + i * 4);
			j += 3;
		}
		jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	free(pBuf);
	jpeg_finish_compress(&cinfo);
	jpeg_destroy_compress(&cinfo);

	fclose(fp);
	return 1;
}

/*****************************************************************
Capture a local screenshot of the desktop,
saving to the file specified by filename.
write a JPEG file.

Return Value:
0 - fail
1 - success
******************************************************************/
int CaptureDesktop(char* filename) {
	Window desktop;
	Display *dsp;
	XImage *img;

	int screen_width;
	int screen_height;

	dsp = XOpenDisplay(NULL); // Connect to a local display
	if(NULL == dsp) {
		fprintf(stderr, "Cannot connect to local display");
		return 0;
	}
	desktop = RootWindow(dsp, 0);/* Refer to the root window */
	if(0 == desktop) {
		fprintf(stderr, "cannot get root window");
		return 0;
	}

	/* Retrive the width and the height of the screen */
	screen_width = DisplayWidth(dsp, 0);
	screen_height = DisplayHeight(dsp, 0);
	
	fprintf(stderr, "Width: %d, Height: %d\n", screen_width, screen_height);

	/* Get the Image of the root window */
	img = XGetImage(dsp, desktop, 0, 0, screen_width, screen_height, ~0, ZPixmap);

	JpegWriteFileFromImage(filename, img);

	XDestroyImage(img);
	XCloseDisplay(dsp);
	return 1;
}

int main(int argc, char *argv[]) {
	CaptureDesktop("./screenshot.jpg");
	printf("Done.\n");
}

