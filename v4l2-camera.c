#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/prctl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>

#include <pthread.h>
#include <semaphore.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#define VBUF_NUM 4

static inline double microtime()
{
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double)(tp.tv_sec + tp.tv_usec / 1000000.00);
}

static inline char *nowtime_r(char *buf, int size)
{
	struct timeval tv = {0, 0};
	struct tm tm;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);

	snprintf(buf, size, "%02d:%02d:%02d.%03ld",
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		tv.tv_usec / 1000
	);

	return buf;
}

typedef struct {
	unsigned flag;
	const char *str;
} flag_def;

static char *flags2s(unsigned val, const flag_def *def)
{
	static char str[512];
	char *p = str;

	while (def->flag) {
		if (val & def->flag) {
			if(p != str) p += snprintf(p, sizeof(str) - (p - str), ", ");
			p += snprintf(p, sizeof(str) - (p - str), "%s", def->str);
			val &= ~def->flag;
		}
		def++;
	}
	if (val) {
		if(p != str) p += snprintf(p, sizeof(str) - (p - str), ", ");
		p += snprintf(p, sizeof(str) - (p - str), "0x%08x", val);
	}

	*p = '\0';

	return str;
}

static void cap2s(unsigned cap)
{
	if(cap & V4L2_CAP_VIDEO_CAPTURE)
		printf("\t\tVideo Capture\n");
	if(cap & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
		printf("\t\tVideo Capture Multiplanar\n");
	if(cap & V4L2_CAP_VIDEO_OUTPUT)
		printf("\t\tVideo Output\n");
	if(cap & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
		printf("\t\tVideo Output Multiplanar\n");
	if(cap & V4L2_CAP_VIDEO_M2M)
		printf("\t\tVideo Memory-to-Memory\n");
	if(cap & V4L2_CAP_VIDEO_M2M_MPLANE)
		printf("\t\tVideo Memory-to-Memory Multiplanar\n");
	if(cap & V4L2_CAP_VIDEO_OVERLAY)
		printf("\t\tVideo Overlay\n");
	if(cap & V4L2_CAP_VIDEO_OUTPUT_OVERLAY)
		printf("\t\tVideo Output Overlay\n");
	if(cap & V4L2_CAP_VBI_CAPTURE)
		printf("\t\tVBI Capture\n");
	if(cap & V4L2_CAP_VBI_OUTPUT)
		printf("\t\tVBI Output\n");
	if(cap & V4L2_CAP_SLICED_VBI_CAPTURE)
		printf("\t\tSliced VBI Capture\n");
	if(cap & V4L2_CAP_SLICED_VBI_OUTPUT)
		printf("\t\tSliced VBI Output\n");
	if(cap & V4L2_CAP_RDS_CAPTURE)
		printf("\t\tRDS Capture\n");
	if(cap & V4L2_CAP_RDS_OUTPUT)
		printf("\t\tRDS Output\n");
	if(cap & V4L2_CAP_SDR_CAPTURE)
		printf("\t\tSDR Capture\n");
	if(cap & V4L2_CAP_SDR_OUTPUT)
		printf("\t\tSDR Output\n");
	if(cap & V4L2_CAP_META_CAPTURE)
		printf("\t\tMetadata Capture\n");
	if(cap & V4L2_CAP_META_OUTPUT)
		printf("\t\tMetadata Output\n");
	if(cap & V4L2_CAP_TUNER)
		printf("\t\tTuner\n");
	if(cap & V4L2_CAP_TOUCH)
		printf("\t\tTouch Device\n");
	if(cap & V4L2_CAP_HW_FREQ_SEEK)
		printf("\t\tHW Frequency Seek\n");
	if(cap & V4L2_CAP_MODULATOR)
		printf("\t\tModulator\n");
	if(cap & V4L2_CAP_AUDIO)
		printf("\t\tAudio\n");
	if(cap & V4L2_CAP_RADIO)
		printf("\t\tRadio\n");
	if(cap & V4L2_CAP_READWRITE)
		printf("\t\tRead/Write\n");
	if(cap & V4L2_CAP_ASYNCIO)
		printf("\t\tAsync I/O\n");
	if(cap & V4L2_CAP_STREAMING)
		printf("\t\tStreaming\n");
	if(cap & V4L2_CAP_EXT_PIX_FORMAT)
		printf("\t\tExtended Pix Format\n");
	if(cap & V4L2_CAP_DEVICE_CAPS)
		printf("\t\tDevice Capabilities\n");
}

static const char *buftype2s(int type)
{
	switch (type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
		return "Video Capture";
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
		return "Video Capture Multiplanar";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		return "Video Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		return "Video Output Multiplanar";
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		return "Video Overlay";
	case V4L2_BUF_TYPE_VBI_CAPTURE:
		return "VBI Capture";
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		return "VBI Output";
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
		return "Sliced VBI Capture";
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		return "Sliced VBI Output";
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
		return "Video Output Overlay";
	case V4L2_BUF_TYPE_SDR_CAPTURE:
		return "SDR Capture";
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		return "SDR Output";
	case V4L2_BUF_TYPE_META_CAPTURE:
		return "Metadata Capture";
	case V4L2_BUF_TYPE_META_OUTPUT:
		return "Metadata Output";
	case V4L2_BUF_TYPE_PRIVATE:
		return "Private";
	default:
		return "Unknown";
	}
}

static const char *fcc2s(__u32 val)
{
	static char str[32];

	sprintf(str, "%c%c%c%c%s",
		val & 0x7f,
		(val >> 8) & 0x7f,
		(val >> 16) & 0x7f,
		(val >> 24) & 0x7f,
		(val & (1U << 31)) ? "-BE" : ""
	);

	return str;
}

static char *printfmtname(int fd, __u32 type, __u32 pixfmt)
{
	struct v4l2_fmtdesc fmt = {};

	fmt.index = 0;
	fmt.type = type;
	while(ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0)
	{
		if(fmt.pixelformat == pixfmt)
		{
			static char str[64];

			sprintf(str, " (%s)", (char*) fmt.description);

			return str;
		}

		fmt.index++;
	}

	return "";
}

static const char *field2s(int val)
{
	switch (val) {
	case V4L2_FIELD_ANY:
		return "Any";
	case V4L2_FIELD_NONE:
		return "None";
	case V4L2_FIELD_TOP:
		return "Top";
	case V4L2_FIELD_BOTTOM:
		return "Bottom";
	case V4L2_FIELD_INTERLACED:
		return "Interlaced";
	case V4L2_FIELD_SEQ_TB:
		return "Sequential Top-Bottom";
	case V4L2_FIELD_SEQ_BT:
		return "Sequential Bottom-Top";
	case V4L2_FIELD_ALTERNATE:
		return "Alternating";
	case V4L2_FIELD_INTERLACED_TB:
		return "Interlaced Top-Bottom";
	case V4L2_FIELD_INTERLACED_BT:
		return "Interlaced Bottom-Top";
	default:
		return "Unknown";
	}
}

static const char *colorspace2s(int val)
{
	switch (val) {
	case V4L2_COLORSPACE_DEFAULT:
		return "Default";
	case V4L2_COLORSPACE_SMPTE170M:
		return "SMPTE 170M";
	case V4L2_COLORSPACE_SMPTE240M:
		return "SMPTE 240M";
	case V4L2_COLORSPACE_REC709:
		return "Rec. 709";
	case V4L2_COLORSPACE_BT878:
		return "Broken Bt878";
	case V4L2_COLORSPACE_470_SYSTEM_M:
		return "470 System M";
	case V4L2_COLORSPACE_470_SYSTEM_BG:
		return "470 System BG";
	case V4L2_COLORSPACE_JPEG:
		return "JPEG";
	case V4L2_COLORSPACE_SRGB:
		return "sRGB";
	case V4L2_COLORSPACE_OPRGB:
		return "opRGB";
	case V4L2_COLORSPACE_DCI_P3:
		return "DCI-P3";
	case V4L2_COLORSPACE_BT2020:
		return "BT.2020";
	case V4L2_COLORSPACE_RAW:
		return "Raw";
	default:
		return "Unknown";
	}
}

static const char *xfer_func2s(int val)
{
	switch (val) {
	case V4L2_XFER_FUNC_DEFAULT:
		return "Default";
	case V4L2_XFER_FUNC_709:
		return "Rec. 709";
	case V4L2_XFER_FUNC_SRGB:
		return "sRGB";
	case V4L2_XFER_FUNC_OPRGB:
		return "opRGB";
	case V4L2_XFER_FUNC_DCI_P3:
		return "DCI-P3";
	case V4L2_XFER_FUNC_SMPTE2084:
		return "SMPTE 2084";
	case V4L2_XFER_FUNC_SMPTE240M:
		return "SMPTE 240M";
	case V4L2_XFER_FUNC_NONE:
		return "None";
	default:
		return "Unknown";
	}
}

static const char *ycbcr_enc2s(int val)
{
	switch (val) {
	case V4L2_YCBCR_ENC_DEFAULT:
		return "Default";
	case V4L2_YCBCR_ENC_601:
		return "ITU-R 601";
	case V4L2_YCBCR_ENC_709:
		return "Rec. 709";
	case V4L2_YCBCR_ENC_XV601:
		return "xvYCC 601";
	case V4L2_YCBCR_ENC_XV709:
		return "xvYCC 709";
	case V4L2_YCBCR_ENC_BT2020:
		return "BT.2020";
	case V4L2_YCBCR_ENC_BT2020_CONST_LUM:
		return "BT.2020 Constant Luminance";
	case V4L2_YCBCR_ENC_SMPTE240M:
		return "SMPTE 240M";
	case V4L2_HSV_ENC_180:
		return "HSV with Hue 0-179";
	case V4L2_HSV_ENC_256:
		return "HSV with Hue 0-255";
	default:
		return "Unknown";
	}
}

static const char *quantization2s(int val)
{
	switch (val) {
	case V4L2_QUANTIZATION_DEFAULT:
		return "Default";
	case V4L2_QUANTIZATION_FULL_RANGE:
		return "Full Range";
	case V4L2_QUANTIZATION_LIM_RANGE:
		return "Limited Range";
	default:
		return "Unknown";
	}
}

static const flag_def pixflags_def[] = {
	{ V4L2_PIX_FMT_FLAG_PREMUL_ALPHA,  "premultiplied-alpha" },
	{ 0, NULL }
};

static char *pixflags2s(unsigned flags)
{
	return flags2s(flags, pixflags_def);
}


static const flag_def service_def[] = {
	{ V4L2_SLICED_TELETEXT_B,  "teletext" },
	{ V4L2_SLICED_VPS,         "vps" },
	{ V4L2_SLICED_CAPTION_525, "cc" },
	{ V4L2_SLICED_WSS_625,     "wss" },
	{ 0, NULL }
};

static char *service2s(unsigned service)
{
	return flags2s(service, service_def);
}

static const flag_def vbi_def[] = {
	{ V4L2_VBI_UNSYNC,     "unsynchronized" },
	{ V4L2_VBI_INTERLACED, "interlaced" },
	{ 0, NULL }
};

static char *vbiflags2s(__u32 flags)
{
	return flags2s(flags, vbi_def);
}

/*
 * Any pixelformat that is not a YUV format is assumed to be
 * RGB or HSV.
 */
static bool is_rgb_or_hsv(__u32 pixelformat)
{
	switch (pixelformat) {
	case V4L2_PIX_FMT_UV8:
	case V4L2_PIX_FMT_YVU410:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_YYUV:
	case V4L2_PIX_FMT_YVYU:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_VYUY:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV411P:
	case V4L2_PIX_FMT_Y41P:
	case V4L2_PIX_FMT_YUV444:
	case V4L2_PIX_FMT_YUV555:
	case V4L2_PIX_FMT_YUV565:
	case V4L2_PIX_FMT_YUV32:
	case V4L2_PIX_FMT_AYUV32:
	case V4L2_PIX_FMT_XYUV32:
	case V4L2_PIX_FMT_VUYA32:
	case V4L2_PIX_FMT_VUYX32:
	case V4L2_PIX_FMT_YUV410:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_HI240:
	case V4L2_PIX_FMT_HM12:
	case V4L2_PIX_FMT_M420:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV24:
	case V4L2_PIX_FMT_NV42:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV12MT:
	case V4L2_PIX_FMT_NV12MT_16X16:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
	case V4L2_PIX_FMT_YUV422M:
	case V4L2_PIX_FMT_YVU422M:
	case V4L2_PIX_FMT_YUV444M:
	case V4L2_PIX_FMT_YVU444M:
	case V4L2_PIX_FMT_SN9C20X_I420:
	case V4L2_PIX_FMT_SPCA501:
	case V4L2_PIX_FMT_SPCA505:
	case V4L2_PIX_FMT_SPCA508:
	case V4L2_PIX_FMT_CIT_YYVYUY:
	case V4L2_PIX_FMT_KONICA420:
		return false;
	default:
		return true;
	}
}

static volatile unsigned int width = 0, height = 0;

static void printfmt(int fd, const struct v4l2_format *vfmt)
{
	__u32 colsp = vfmt->fmt.pix.colorspace;
	__u32 ycbcr_enc = vfmt->fmt.pix.ycbcr_enc;

	printf("Format: %s\n", buftype2s(vfmt->type));

	switch (vfmt->type) {
	case V4L2_BUF_TYPE_VIDEO_CAPTURE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT:
		width = vfmt->fmt.pix.width;
		height = vfmt->fmt.pix.height;
		
		printf("\tWidth/Height      : %u/%u\n", vfmt->fmt.pix.width, vfmt->fmt.pix.height);
		printf("\tPixel Format      : '%s'%s\n", fcc2s(vfmt->fmt.pix.pixelformat), printfmtname(fd, vfmt->type, vfmt->fmt.pix.pixelformat));
		printf("\tField             : %s\n", field2s(vfmt->fmt.pix.field));
		printf("\tBytes per Line    : %u\n", vfmt->fmt.pix.bytesperline);
		printf("\tSize Image        : %u\n", vfmt->fmt.pix.sizeimage);
		printf("\tColorspace        : %s\n", colorspace2s(colsp));
		printf("\tTransfer Function : %s", xfer_func2s(vfmt->fmt.pix.xfer_func));
		if (vfmt->fmt.pix.xfer_func == V4L2_XFER_FUNC_DEFAULT)
			printf(" (maps to %s)", xfer_func2s(V4L2_MAP_XFER_FUNC_DEFAULT(colsp)));
		printf("\n");
		printf("\tYCbCr/HSV Encoding: %s", ycbcr_enc2s(ycbcr_enc));
		if (ycbcr_enc == V4L2_YCBCR_ENC_DEFAULT) {
			ycbcr_enc = V4L2_MAP_YCBCR_ENC_DEFAULT(colsp);
			printf(" (maps to %s)", ycbcr_enc2s(ycbcr_enc));
		}
		printf("\n");
		printf("\tQuantization      : %s", quantization2s(vfmt->fmt.pix.quantization));
		if (vfmt->fmt.pix.quantization == V4L2_QUANTIZATION_DEFAULT)
			printf(" (maps to %s)", quantization2s(V4L2_MAP_QUANTIZATION_DEFAULT(is_rgb_or_hsv(vfmt->fmt.pix.pixelformat), colsp, ycbcr_enc)));
		printf("\n");
		if (vfmt->fmt.pix.priv == V4L2_PIX_FMT_PRIV_MAGIC)
			printf("\tFlags             : %s\n", pixflags2s(vfmt->fmt.pix.flags));
		break;
	case V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE:
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE:
		width = vfmt->fmt.pix_mp.width;
		height = vfmt->fmt.pix_mp.height;
		
		printf("\tWidth/Height      : %u/%u\n", vfmt->fmt.pix_mp.width, vfmt->fmt.pix_mp.height);
		printf("\tPixel Format      : '%s'%s\n", fcc2s(vfmt->fmt.pix_mp.pixelformat), printfmtname(fd, vfmt->type, vfmt->fmt.pix_mp.pixelformat));
		printf("\tField             : %s\n", field2s(vfmt->fmt.pix_mp.field));
		printf("\tNumber of planes  : %u\n", vfmt->fmt.pix_mp.num_planes);
		printf("\tFlags             : %s\n", pixflags2s(vfmt->fmt.pix_mp.flags));
		printf("\tColorspace        : %s\n", colorspace2s(vfmt->fmt.pix_mp.colorspace));
		printf("\tTransfer Function : %s\n", xfer_func2s(vfmt->fmt.pix_mp.xfer_func));
		printf("\tYCbCr/HSV Encoding: %s\n", ycbcr_enc2s(vfmt->fmt.pix_mp.ycbcr_enc));
		printf("\tQuantization      : %s\n", quantization2s(vfmt->fmt.pix_mp.quantization));
		for (int i = 0; i < vfmt->fmt.pix_mp.num_planes && i < VIDEO_MAX_PLANES; i++) {
			printf("\tPlane %d           :\n", i);
			printf("\t   Bytes per Line : %u\n", vfmt->fmt.pix_mp.plane_fmt[i].bytesperline);
			printf("\t   Size Image     : %u\n", vfmt->fmt.pix_mp.plane_fmt[i].sizeimage);
		}
		break;
	case V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY:
	case V4L2_BUF_TYPE_VIDEO_OVERLAY:
		width = vfmt->fmt.win.w.width;
		height = vfmt->fmt.win.w.height;
		
		printf("\tLeft/Top    : %d/%d\n",
				vfmt->fmt.win.w.left, vfmt->fmt.win.w.top);
		printf("\tWidth/Height: %d/%d\n",
				vfmt->fmt.win.w.width, vfmt->fmt.win.w.height);
		printf("\tField       : %s\n", field2s(vfmt->fmt.win.field));
		printf("\tChroma Key  : 0x%08x\n", vfmt->fmt.win.chromakey);
		printf("\tGlobal Alpha: 0x%02x\n", vfmt->fmt.win.global_alpha);
		printf("\tClip Count  : %u\n", vfmt->fmt.win.clipcount);
		if (vfmt->fmt.win.clips)
			for (unsigned i = 0; i < vfmt->fmt.win.clipcount; i++) {
				struct v4l2_rect *r = & vfmt->fmt.win.clips[i].c;

				printf("\t\tClip %2d: %ux%u@%ux%u\n", i,
						r->width, r->height, r->left, r->top);
			}
		printf("\tClip Bitmap : %s", vfmt->fmt.win.bitmap ? "Yes, " : "No\n");
		if (vfmt->fmt.win.bitmap) {
			unsigned char *bitmap = (unsigned char *) vfmt->fmt.win.bitmap;
			unsigned stride = (vfmt->fmt.win.w.width + 7) / 8;
			unsigned cnt = 0;

			for (unsigned y = 0; y < vfmt->fmt.win.w.height; y++)
				for (unsigned x = 0; x < vfmt->fmt.win.w.width; x++)
					if (bitmap[y * stride + x / 8] & (1 << (x & 7)))
						cnt++;
			printf("%u bits of %u are set\n", cnt,
					vfmt->fmt.win.w.width * vfmt->fmt.win.w.height);
		}
		break;
	case V4L2_BUF_TYPE_VBI_CAPTURE:
	case V4L2_BUF_TYPE_VBI_OUTPUT:
		printf("\tSampling Rate   : %u Hz\n", vfmt->fmt.vbi.sampling_rate);
		printf("\tOffset          : %u samples (%g secs after leading edge)\n",
				vfmt->fmt.vbi.offset,
				(double) vfmt->fmt.vbi.offset / (double) vfmt->fmt.vbi.sampling_rate);
		printf("\tSamples per Line: %u\n", vfmt->fmt.vbi.samples_per_line);
		printf("\tSample Format   : '%s'\n", fcc2s(vfmt->fmt.vbi.sample_format));
		printf("\tStart 1st Field : %u\n", vfmt->fmt.vbi.start[0]);
		printf("\tCount 1st Field : %u\n", vfmt->fmt.vbi.count[0]);
		printf("\tStart 2nd Field : %u\n", vfmt->fmt.vbi.start[1]);
		printf("\tCount 2nd Field : %u\n", vfmt->fmt.vbi.count[1]);
		if (vfmt->fmt.vbi.flags)
			printf("\tFlags           : %s\n", vbiflags2s(vfmt->fmt.vbi.flags));
		break;
	case V4L2_BUF_TYPE_SLICED_VBI_CAPTURE:
	case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
		printf("\tService Set    : %s\n",
				service2s(vfmt->fmt.sliced.service_set));
		for (int i = 0; i < 24; i++) {
			printf("\tService Line %2d: %8s / %-8s\n", i, service2s(vfmt->fmt.sliced.service_lines[0][i]), service2s(vfmt->fmt.sliced.service_lines[1][i]));
		}
		printf("\tI/O Size       : %u\n", vfmt->fmt.sliced.io_size);
		break;
	case V4L2_BUF_TYPE_SDR_CAPTURE:
	case V4L2_BUF_TYPE_SDR_OUTPUT:
		printf("\tSample Format   : '%s'%s\n", fcc2s(vfmt->fmt.sdr.pixelformat), printfmtname(fd, vfmt->type, vfmt->fmt.sdr.pixelformat));
		printf("\tBuffer Size     : %u\n", vfmt->fmt.sdr.buffersize);
		break;
	case V4L2_BUF_TYPE_META_CAPTURE:
	case V4L2_BUF_TYPE_META_OUTPUT:
		printf("\tSample Format   : '%s'%s\n", fcc2s(vfmt->fmt.meta.dataformat), printfmtname(fd, vfmt->type, vfmt->fmt.meta.dataformat));
		printf("\tBuffer Size     : %u\n", vfmt->fmt.meta.buffersize);
		break;
	}
}

static const flag_def bufcap_def[] = {
	{ V4L2_BUF_CAP_SUPPORTS_MMAP, "mmap" },
	{ V4L2_BUF_CAP_SUPPORTS_USERPTR, "userptr" },
	{ V4L2_BUF_CAP_SUPPORTS_DMABUF, "dmabuf" },
	{ V4L2_BUF_CAP_SUPPORTS_REQUESTS, "requests" },
	{ V4L2_BUF_CAP_SUPPORTS_ORPHANED_BUFS, "orphaned-bufs" },
	{ V4L2_BUF_CAP_SUPPORTS_M2M_HOLD_CAPTURE_BUF, "m2m-hold-capture-buf" },
	{ V4L2_BUF_CAP_SUPPORTS_MMAP_CACHE_HINTS, "mmap-cache-hints" },
	{ 0, NULL }
};

static char *bufcap2s(__u32 caps)
{
	return flags2s(caps, bufcap_def);
}

// BGR3 convert to RGB888
static void bgr3_to_rgb(unsigned char *bgr3_buffer, unsigned char *rgb_buffer, int iWidth, int iHeight)
{
	int x;
	unsigned char *ptr = rgb_buffer;
	unsigned char *bgr3 = bgr3_buffer;
	
	for (x = 0; x < iWidth*iHeight; x++)
	{
		*(ptr + 2) = *(bgr3 + 0);
		*(ptr + 1) = *(bgr3 + 1);
		*(ptr + 0) = *(bgr3 + 2);
		ptr += 3;
		bgr3 += 3;
	}
}

// YUYV convert to RGB888
static void yuyv_to_rgb(unsigned char *yuyv_buffer, unsigned char *rgb_buffer, int iWidth, int iHeight)
{
	int x;
	int z=0;
	unsigned char *ptr = rgb_buffer;
	unsigned char *yuyv = yuyv_buffer;
	
	for (x = 0; x < iWidth*iHeight; x++)
	{
		int r, g, b;
		int y, u, v;
		if (!z)
			y = yuyv[0] << 8;
		else
			y = yuyv[2] << 8;
		u = yuyv[1] - 128;
		v = yuyv[3] - 128;
		b = (y + (359 * v)) >> 8;
		g = (y - (88 * u) - (183 * v)) >> 8;
		r = (y + (454 * u)) >> 8;
		*(ptr++) = (b > 255) ? 255 : ((b < 0) ? 0 : b);
		*(ptr++) = (g > 255) ? 255 : ((g < 0) ? 0 : g);
		*(ptr++) = (r > 255) ? 255 : ((r < 0) ? 0 : r);
		if(z++)
		{
			z = 0;
			yuyv += 4;
		}
	}
}

typedef struct {
	unsigned int len;
	unsigned char *buf;
} video_buffer_t;

static volatile bool is_running = true;
static void sig_handler(int sig)
{
	is_running = false;
	
	if(isatty(2)) fprintf(stderr, "\r\033[2K");
}

static sem_t sem;
static GtkWidget *window = NULL;
static GtkWidget *drawarea = NULL;
static GdkFont *font = NULL;

static volatile unsigned int vframes = 0;

static int on_delete_event(GtkWidget *widget,GdkEvent *event,gpointer data)
{
	is_running = false;
	
	gtk_widget_hide_all(window);
	
	return TRUE;
}

static int on_expose_event(GtkWidget *widget,GdkEvent *event,gpointer data)
{
	return FALSE;
}

static gboolean on_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer _data) {
	static bool is_fullscreen = false;
	uint16_t code = 0;
	
	// printf("%u %02x\n", event->hardware_keycode, event->hardware_keycode);

	switch(event->keyval) {
		case GDK_KEY_Escape:
			is_running = false;
			break;
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_F11:
			is_fullscreen = !is_fullscreen;
			
			if(is_fullscreen)
				gtk_window_fullscreen(GTK_WINDOW(window));
			else
				gtk_window_unfullscreen(GTK_WINDOW(window));
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static gboolean timeout_func_stat(gpointer data)
{
	char title[128];
	unsigned int fps = vframes;

	vframes = 0;
	printf("[%s] fps: %u\n", nowtime_r(title, sizeof(title)), fps);
	sprintf(title, "V4L2-Camera - fps: %u - width: %u - height: %u", fps, width, height);
	
	gtk_window_set_title(GTK_WINDOW(window), title);
	return TRUE;
}

static void *thread_gtk(void *arg)
{
	prctl(PR_SET_NAME, (unsigned long) "v4l2-gtk");
	
	gdk_threads_enter();
	
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	gtk_window_set_title(GTK_WINDOW(window), "V4L2-Camera");
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_widget_set_size_request(window, 800, 600);
	// gtk_window_set_decorated(GTK_WINDOW(window), FALSE);
	// gtk_window_fullscreen(GTK_WINDOW(window));
	g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(on_key_press_event), NULL);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(on_delete_event),NULL);

	// 初始化gdk的rgb
	gdk_rgb_init();
	gtk_widget_push_visual(gdk_rgb_get_visual());
	gtk_widget_push_colormap(gdk_rgb_get_cmap());

	// 创建绘图区域
	drawarea = gtk_drawing_area_new();
	gtk_widget_pop_visual();
	gtk_widget_pop_colormap();
	
	gtk_container_add(GTK_CONTAINER(window), drawarea);
	g_signal_connect(G_OBJECT(drawarea), "expose_event",G_CALLBACK(on_expose_event),NULL);

	gtk_widget_show_all(window);
	
	sem_post(&sem);
	
	{
		guint timer = gtk_timeout_add(1000, timeout_func_stat, NULL);
		gtk_main();
		gtk_timeout_remove(timer);
	}
	
	gdk_threads_leave();
	
	pthread_exit(NULL);
}

static GdkPixmap *drawPixmap = NULL;

static void draw_pixmap(void)
{
	gdk_draw_drawable(drawarea->window, drawarea->style->black_gc, drawPixmap, 0, 0, 0, 0, width, height);
}

static void draw_nowtime(void)
{
	char buf[32];
	
	nowtime_r(buf, sizeof(buf));
	gdk_draw_string(drawPixmap, font, drawarea->style->black_gc, 48, 48, buf);
	gdk_draw_string(drawPixmap, font, drawarea->style->white_gc, 50, 50, buf);
}

int main(int argc, char *argv[])
{
	const char *device = "/dev/video0";
	struct v4l2_capability vcap;
	struct v4l2_format vfmt;
	struct v4l2_streamparm parm;
	struct v4l2_create_buffers cbuf;
	struct v4l2_requestbuffers rbuf;
	struct v4l2_buffer vbuf;
	struct v4l2_plane *vplanes = NULL;
	video_buffer_t *bufs = NULL;
	unsigned char *rgbbuf = NULL;
	int fps = 15;
	int setw = 0, seth = 0;
	unsigned int pixfmt = 0;
	pthread_t tid;
	pthread_attr_t attr;
	bool istime = false;
	int opt, fd, i, type;
	
	while((opt = getopt(argc, argv, "d:f:w:h:p:t?")) != -1)
	{
		switch(opt)
		{
		case 'd':
			device = optarg;
			break;
		case 'f':
			fps = atoi(optarg);
			break;
		case 'w':
			setw = atoi(optarg);
			if(setw < 0) setw = 0;
			break;
		case 'h':
			seth = atoi(optarg);
			if(seth < 0) seth = 0;
			break;
		case 'p':
		{
			bool be_pixfmt = strlen(optarg) == 7 && !memcmp(optarg + 4, "-BE", 3);
			
			if(be_pixfmt || strlen(optarg) == 4)
			{
				pixfmt = v4l2_fourcc(optarg[0], optarg[1], optarg[2], optarg[3]);
				if(be_pixfmt)
				{
					pixfmt |= (1U << 31);
				}
			}
			break;
		}
		case 't':
			istime = true;
			break;
		case '?':
		default:
			fprintf(stderr, "usage: %s [-d <device-path>] [-f <fps>] [-w <width>] [-h <height>] [-p <pixelformat>] [-t] [-?]\n", argv[0]);
			fprintf(stderr, "    -?                   This help\n");
			fprintf(stderr, "    -d <device-path>     Device path(default: %s)\n", device);
			fprintf(stderr, "    -f <fps>             Set Frame-rate(default: %u)\n", fps);
			fprintf(stderr, "    -w <width>           Set width\n");
			fprintf(stderr, "    -h <height>          Set height\n");
			fprintf(stderr, "    -p <pixelformat>     Set pixel-format\n");
			fprintf(stderr, "    -t                   Show time\n");
			return 1;
		}
	}

	fd = open(device, O_RDWR);
	if(fd < 0)
	{
		perror("open device");
		return 1;
	}

	memset(&vcap, 0, sizeof(vcap));
	if(ioctl(fd, VIDIOC_QUERYCAP, &vcap) < 0)
	{
		perror("VIDIOC_QUERYCAP");
		goto err;
	}

	printf("Device Info: %s\n", device);
	printf("\tDriver name      : %s\n", vcap.driver);
	printf("\tCard type        : %s\n", vcap.card);
	printf("\tBus info         : %s\n", vcap.bus_info);
	printf("\tDriver version   : %d.%d.%d\n", vcap.version >> 16, (vcap.version >> 8) & 0xff, vcap.version & 0xff);
	printf("\tCapabilities     : 0x%08x\n", vcap.capabilities);
	cap2s(vcap.capabilities);
	if(vcap.capabilities & V4L2_CAP_DEVICE_CAPS) {
		printf("\tDevice Caps      : 0x%08x\n", vcap.device_caps);
		cap2s(vcap.device_caps);
	}

	bool priv_magic = (vcap.capabilities & V4L2_CAP_EXT_PIX_FORMAT);
	bool is_multiplanar = (vcap.capabilities & (V4L2_CAP_VIDEO_CAPTURE_MPLANE | V4L2_CAP_VIDEO_M2M_MPLANE | V4L2_CAP_VIDEO_OUTPUT_MPLANE));

	memset(&vfmt, 0, sizeof(vfmt));
	vfmt.fmt.pix.priv = (priv_magic ? V4L2_PIX_FMT_PRIV_MAGIC : 0);
	vfmt.type = (is_multiplanar ? V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE : V4L2_BUF_TYPE_VIDEO_CAPTURE); // 视频捕获格式
	if(ioctl(fd, VIDIOC_G_FMT, &vfmt) < 0)
	{
		perror("VIDIOC_G_FMT");
		goto err;
	}
	else
	{
		printfmt(fd, &vfmt);
		
		if(setw || seth || pixfmt)
		{
			if(is_multiplanar)
			{
				if(setw) vfmt.fmt.pix_mp.width = setw;
				if(seth) vfmt.fmt.pix_mp.height = seth;
				if(pixfmt) vfmt.fmt.pix_mp.pixelformat = pixfmt;
			}
			else
			{
				if(setw) vfmt.fmt.pix.width = setw;
				if(seth) vfmt.fmt.pix.height = seth;
				if(pixfmt) vfmt.fmt.pix.pixelformat = pixfmt;
			}
			
			if(ioctl(fd, VIDIOC_S_FMT, &vfmt) < 0)
			{
				perror("VIDIOC_S_FMT");
				goto err;
			}
			
			if(setw)
			{
				width = (is_multiplanar ? vfmt.fmt.pix_mp.width : vfmt.fmt.pix.width);
				printf("Set Width to %d\n", width);
			}
			if(seth)
			{
				height = (is_multiplanar ? vfmt.fmt.pix_mp.height : vfmt.fmt.pix.height);
				printf("Set Height to %d\n", height);
			}
			if(pixfmt)
			{
				pixfmt = (is_multiplanar ? vfmt.fmt.pix_mp.pixelformat : vfmt.fmt.pix.pixelformat);
				printf("Set Pixcel Format to '%s'%s\n", fcc2s(pixfmt), printfmtname(fd, vfmt.type, pixfmt));
			}
		}
		else
		{
			pixfmt = (is_multiplanar ? vfmt.fmt.pix_mp.pixelformat : vfmt.fmt.pix.pixelformat);
		}
	}

	memset(&parm, 0, sizeof(parm));
	parm.type = vfmt.type;
	if(ioctl(fd, VIDIOC_G_PARM, &parm) < 0)
	{
		perror("VIDIOC_G_PARM");
		// goto err;
	}
	else
	{
		struct v4l2_fract *tf = &parm.parm.capture.timeperframe;

		printf("Streaming Parameters %s:\n", buftype2s(parm.type));
		if(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
			printf("\tCapabilities     : timeperframe\n");
		if(parm.parm.capture.capturemode & V4L2_MODE_HIGHQUALITY)
			printf("\tCapture mode     : high quality\n");
		if(!tf->denominator || !tf->numerator)
			printf("\tFrames per second: invalid (%d/%d)\n", tf->denominator, tf->numerator);
		else
			printf("\tFrames per second: %.3f (%d/%d)\n", (1.0 * tf->denominator) / tf->numerator, tf->denominator, tf->numerator);
		
		printf("\tRead buffers     : %d\n", parm.parm.capture.readbuffers);
		
		if(parm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
		{
			tf->denominator = fps;
			tf->numerator = 1;
			if(ioctl(fd, VIDIOC_S_PARM, &parm) < 0)
			{
				perror("VIDIOC_S_PARM");
				goto err;
			}
			else
				printf("denominator: %d, numerator: %d\n", tf->denominator, tf->numerator);
		}
	}

	memset(&cbuf, 0, sizeof(cbuf));
	cbuf.format.type = vfmt.type;
	cbuf.memory = V4L2_MEMORY_MMAP;
	if(ioctl(fd, VIDIOC_CREATE_BUFS, &cbuf) < 0)
	{
		perror("VIDIOC_CREATE_BUFS");
		goto err;
	}
	else
		printf("Create Buffers: %s\n", bufcap2s(cbuf.capabilities));

	// 申请的缓冲区个数
	memset(&rbuf, 0, sizeof(rbuf));
	rbuf.count = VBUF_NUM; // 申请的缓冲区个数
	rbuf.type = vfmt.type;
	rbuf.memory = V4L2_MEMORY_MMAP;
	if(ioctl(fd, VIDIOC_REQBUFS, &rbuf) < 0)
	{
		perror("VIDIOC_REQBUFS");
		goto err;
	}
	else
		printf("Request Buffers: %s\n", bufcap2s(rbuf.capabilities));

	bufs = (video_buffer_t *) malloc(sizeof(video_buffer_t) * rbuf.count);
	memset(bufs, 0, sizeof(video_buffer_t) * rbuf.count);

	vplanes = malloc(sizeof(struct v4l2_plane) * vfmt.fmt.pix_mp.num_planes);
	memset(vplanes, 0, sizeof(struct v4l2_plane) * vfmt.fmt.pix_mp.num_planes);

	// 将缓冲区映射到进程空间
	for(i = 0; i < rbuf.count; i ++)
	{
		memset(&vbuf, 0, sizeof(vbuf));
		vbuf.type = rbuf.type;
		vbuf.memory = V4L2_MEMORY_MMAP;
		vbuf.index = i;

		if(is_multiplanar)
		{
			vbuf.length = vfmt.fmt.pix_mp.num_planes;
			vbuf.m.planes = vplanes;
		}

		// 申请缓冲区失败
		if(ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0)
		{
			perror("VIDIOC_QUERYBUF");
			goto req;
		}

		if(is_multiplanar)
		{
			bufs[i].len = vplanes->length;
			bufs[i].buf = mmap(NULL, vplanes->length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vplanes->m.mem_offset);
		}
		else
		{
			bufs[i].len = vbuf.length;
			bufs[i].buf = mmap(NULL, vbuf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, vbuf.m.offset);
		}
		if(bufs[i].buf == MAP_FAILED)
		{
			perror("mmap");
			bufs[i].buf = NULL;
			goto req;
		}
		else
			printf("video buf(%d): %p\n", i, bufs[i].buf);
	}

	// 将映射的空间添加到采集队列
	memset(&vbuf, 0, sizeof(vbuf));
	vbuf.type = rbuf.type;
	vbuf.memory = V4L2_MEMORY_MMAP;
	for(i = 0; i < rbuf.count; i ++)
	{
		vbuf.index = i;

		if(is_multiplanar)
		{
			vbuf.length = vfmt.fmt.pix_mp.num_planes;
			vbuf.m.planes = vplanes;
		}

		// 申请缓冲区失败
		if(ioctl(fd, VIDIOC_QBUF, &vbuf) < 0)
		{
			perror("VIDIOC_QBUF");
			goto req;
		}
	}
	
	rgbbuf = (unsigned char *) malloc(width * height * 3);
	if(!rgbbuf)
	{
		perror("malloc");
		goto req;
	}
	
	printf("width: %u, height: %u, Running ...\n", width, height);
	
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	signal(SIGUSR1, sig_handler);
	signal(SIGUSR2, sig_handler);
	
	gdk_threads_init();
	
	gtk_init(&argc, &argv);
	
#ifndef USE_FONT_LOAD
	PangoFontDescription *font_desc = pango_font_description_from_string("Sans Bold 32");
	// pango_font_description_set_weight(font_desc, PANGO_WEIGHT_BOLD);
	// pango_font_description_set_size(font_desc, 32 * PANGO_SCALE);
	// printf("font_desc: %d\n", pango_font_description_get_size(font_desc));
	font = gdk_font_from_description(font_desc);
#else
	font = gdk_font_load("-*-helvetica-medium-r-*-*-32-*-*-*-*-*-*-*");
#endif

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	sem_init(&sem, 0, 0);
	pthread_create(&tid, &attr, thread_gtk, NULL);
	sem_wait(&sem);
	
	gdk_threads_enter();
	if(istime) drawPixmap = gdk_pixmap_new(drawarea->window, width, height, -1);
	if(is_running)
	{
		memset(rgbbuf, 0x00, width * height * 3);
		
		if(istime)
		{
			gdk_draw_rgb_image(drawPixmap, drawarea->style->black_gc, 0, 0, width, height, GDK_RGB_DITHER_NONE, rgbbuf, width * 3);
			draw_nowtime();
			draw_pixmap();
		}
		else
		{
			gdk_draw_rgb_image(drawarea->window, drawarea->style->black_gc, 0, 0, width, height, GDK_RGB_DITHER_NONE, rgbbuf, width * 3);
		}
	}
	gdk_threads_leave();
	
	type = rbuf.type;
	if(ioctl(fd, VIDIOC_STREAMON, &type) < 0)
	{
		perror("VIDIOC_STREAMON");
		is_running = false;
	}

	{
		int flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
	
	while(is_running)
	{
		fd_set rset;
		int r;
		struct timeval tv;
		
		FD_ZERO(&rset);
		FD_SET(fd, &rset);
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		r = select(fd + 1, &rset, NULL, NULL, &tv);
		if(r < 0)
		{
			if(EINTR == errno) continue;
			
			perror("select");
			break;
		}
		else if(r == 0) break;
		
		// 读取摄像头图像数据
		memset(&vbuf, 0, sizeof(vbuf));
		vbuf.type = rbuf.type;
		vbuf.memory = V4L2_MEMORY_MMAP;
		if(is_multiplanar)
		{
			vbuf.length = vfmt.fmt.pix_mp.num_planes;
			vbuf.m.planes = vplanes;
		}
		if(ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0)
		{
			perror("VIDIOC_DQBUF");
			break;
		}
		
		vframes ++;
		
		switch(pixfmt)
		{
		case V4L2_PIX_FMT_BGR24:
			bgr3_to_rgb(bufs[vbuf.index].buf, rgbbuf, width, height);
			
			gdk_threads_enter();
			if(is_running)
			{
				if(istime)
				{
					gdk_draw_rgb_image(drawPixmap, drawarea->style->black_gc, 0, 0, width, height, GDK_RGB_DITHER_NONE, rgbbuf, width * 3);
					draw_nowtime();
					draw_pixmap();
				}
				else
				{
					gdk_draw_rgb_image(drawarea->window, drawarea->style->black_gc, 0, 0, width, height, GDK_RGB_DITHER_NONE, rgbbuf, width * 3);
				}
			}
			gdk_threads_leave();
			break;
		case V4L2_PIX_FMT_YUYV:
			yuyv_to_rgb(bufs[vbuf.index].buf, rgbbuf, width, height);
			
			gdk_threads_enter();
			if(is_running)
			{
				if(istime)
				{
					gdk_draw_rgb_image(drawPixmap, drawarea->style->black_gc, 0, 0, width, height, GDK_RGB_DITHER_NONE, rgbbuf, width * 3);
					draw_nowtime();
					draw_pixmap();
				}
				else
				{
					gdk_draw_rgb_image(drawarea->window, drawarea->style->black_gc, 0, 0, width, height, GDK_RGB_DITHER_NONE, rgbbuf, width * 3);
				}
			}
			gdk_threads_leave();
			break;
		case V4L2_PIX_FMT_MJPEG:
			{
				GdkPixbufLoader *loader;
				GdkPixbuf *pixbuf;
				
				gdk_threads_enter();
				
				loader = gdk_pixbuf_loader_new_with_mime_type("image/jpeg", NULL);
				gdk_pixbuf_loader_write(loader, bufs[vbuf.index].buf, bufs[vbuf.index].len, NULL);
				if(gdk_pixbuf_loader_close(loader, NULL)) {
					pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
					if(pixbuf) {
						if(istime)
						{
							gdk_draw_pixbuf(drawPixmap, drawarea->style->black_gc, pixbuf, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);
							draw_nowtime();
							draw_pixmap();
						}
						else
						{
							gdk_draw_pixbuf(drawarea->window, drawarea->style->black_gc, pixbuf, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);
						}
					}
				}
				g_object_unref(loader);
				
				gdk_threads_leave();
			}
			break;
		default:
			is_running = false;
			printf("pixel format is invalid\n");
			break;
		}
		
		// 将缓冲区添加回采集队列
		if(ioctl(fd, VIDIOC_QBUF, &vbuf) < 0)
		{
			perror("VIDIOC_QBUF");
			break;
		}
	}
	
	is_running = false;
	
	gdk_threads_enter();
	gtk_main_quit();
	gdk_threads_leave();
	
	if(pthread_join(tid, NULL)) {
		perror("pthread_join failure");
	}

	free(rgbbuf);
	pthread_attr_destroy(&attr);
	sem_destroy(&sem);

req:
	if(bufs)
	{
		for(i = 0; i < rbuf.count; i ++)
		{
			if(bufs[i].buf)
			{
				munmap(bufs[i].buf, bufs[i].len);
			}
		}

		free(bufs);
		bufs = NULL;
	}

err:
	close(fd);
	return 0;
}
