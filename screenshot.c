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
#include <X11/extensions/Xrandr.h>

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
	jpeg_set_quality(&cinfo, 100, TRUE);
	jpeg_start_compress(&cinfo, TRUE);

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

int main(int argc, char *argv[]) {
	Display *dpy;
	Window win;
	XRRScreenResources *res;
	XRRCrtcInfo *crtc_info;
	XRROutputInfo *output_info;
	XRROutputInfo **output_infos;
	RROutput output;
	RRCrtc crtc;
	XImage *img;

	int width;
	int height;
	int n, i, i2, i3, output_info_len;
	char filename[64];

	dpy = XOpenDisplay(NULL);
	if(NULL == dpy) {
		fprintf(stderr, "Cannot connect to local display");
		return 1;
	}
	
	n = XScreenCount(dpy);
	
	for(i=0; i<n; i++) {
		width = XDisplayWidth(dpy, i);
		height = XDisplayHeight(dpy, i);

		fprintf(stderr, "Screen %d: Width: %d, Height: %d\n", i, width, height);

		win = RootWindow(dpy, DefaultScreen(dpy));
		if(!win) {
			fprintf(stderr, "cannot get root window for %d screen\n", i);
			continue;
		}
		
		res = XRRGetScreenResources(dpy, win);
		output = XRRGetOutputPrimary(dpy, win);
		output_infos = (XRROutputInfo**) malloc(sizeof(XRROutputInfo*) * res->ncrtc);

		output_info_len = 0;
		crtc = 0;
		for (i2 = 0; i2 < res->noutput; i2++) {
			output_info = XRRGetOutputInfo (dpy, res, res->outputs[i2]);
			if(output == res->outputs[i2]) crtc = output_info->crtc;
			if(output_info->crtc) output_infos[output_info_len++] = output_info;
			else XRRFreeOutputInfo(output_info);
		}

		for(i2=0; i2<res->ncrtc; i2++) {
			crtc_info = XRRGetCrtcInfo (dpy, res, res->crtcs[i2]);
			
			output_info = NULL;
			for(i3=0; i3<output_info_len; i3++) {
				if(output_infos[i3]->crtc == res->crtcs[i2]) output_info = output_infos[i3];
			}

			snprintf(filename, sizeof(filename), "screen-%02d-%02d.jpg", i, i2);
			
			fprintf(stderr, "    Display: %d, Name: %6s %7s, x: %4d, y: %4d, width: %4d, height: %4d", i2, output_info ? output_info->name : "Virt", res->crtcs[i2] == crtc ? "Primary" : "", crtc_info->x, crtc_info->y, crtc_info->width, crtc_info->height);
			
			if(crtc_info->width && crtc_info->height) {
				fprintf(stderr, ", filename: %s\n", filename);
				img = XGetImage(dpy, win, crtc_info->x, crtc_info->y, crtc_info->width > 0 ? crtc_info->width : 1, crtc_info->height > 0 ? crtc_info->height : 1, ~0, ZPixmap);
				JpegWriteFileFromImage(filename, img);
				XDestroyImage(img);
			} else {
				fprintf(stderr, "\n");
			}

			XRRFreeCrtcInfo(crtc_info);
		}
		
		for (i2 = 0; i2 < output_info_len; i2++) {
			XRRFreeOutputInfo(output_infos[i2]);
		}
		
		free(output_infos);
		
		XRRFreeScreenResources(res);
	}

	XCloseDisplay(dpy);
	return 0;
}

