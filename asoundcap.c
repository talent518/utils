#define ALSA_PCM_NEW_HW_PARAMS_API

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <signal.h>

#define u32 unsigned int
#define u8   unsigned char
#define u16 unsigned short

snd_pcm_t *gp_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
snd_pcm_hw_params_t *gp_params;  //设置流的硬件参数
snd_pcm_uframes_t g_frames; //snd_pcm_uframes_t其实是unsigned long类型
char *gp_buffer;
u32 g_bufsize;

int set_hardware_params(int sample_rate, int channels, int format_size) {
	int rc;
	
	fprintf(stderr, "sample_rate: %d, channels: %d, format_size: %d\n", sample_rate, channels, format_size);
	
	/* Open PCM device for playback */
	rc = snd_pcm_open(&gp_handle, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device\n");
		return -1;
	}


	/* Allocate a hardware parameters object */
	snd_pcm_hw_params_alloca(&gp_params);

	/* Fill it in with default values. */
	rc = snd_pcm_hw_params_any(gp_handle, gp_params);
	if (rc < 0) {
		fprintf(stderr, "unable to Fill it in with default values.\n");
		goto err1;
	}

	/* Interleaved mode */
	rc = snd_pcm_hw_params_set_access(gp_handle, gp_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0) {
		fprintf(stderr, "unable to Interleaved mode.\n");
		goto err1;
	}

	snd_pcm_format_t format;

	if (8 == format_size) {
		format = SND_PCM_FORMAT_U8;
	} else if (16 == format_size) {
		format = SND_PCM_FORMAT_S16_LE;
	} else if (24 == format_size) {
		format = SND_PCM_FORMAT_U24_LE;
	} else if (32 == format_size) {
		format = SND_PCM_FORMAT_U32_LE;
	} else {
		fprintf(stderr, "SND_PCM_FORMAT_UNKNOWN.\n");
		format = SND_PCM_FORMAT_UNKNOWN;
		goto err1;
	}

	/* set format */
	rc = snd_pcm_hw_params_set_format(gp_handle, gp_params, format);
	if (rc < 0) {
		fprintf(stderr, "unable to set format.\n");
		goto err1;
	}

	/* set channels (stero) */
	rc = snd_pcm_hw_params_set_channels(gp_handle, gp_params, channels);
	if (rc < 0) {
		fprintf(stderr, "unable to set channels (stero).\n");
		goto err1;
	}

	/* set sampling rate */
	int rate = sample_rate;
	rc = snd_pcm_hw_params_set_rate_near(gp_handle, gp_params, &rate, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to set sampling rate.\n");
		goto err1;
	}
	if(rate != sample_rate) {
		fprintf(stderr, "set sample rate %d is not support, should set is %d\n", sample_rate, rate);
		goto err1;
	}

	g_frames = sample_rate / 20;
	rc = snd_pcm_hw_params_set_period_size_near(gp_handle, gp_params, &g_frames, 0);
	if(rc < 0) {
		fprintf(stderr, "unable to set sampling rate.\n");
		goto err1;
	}

	/* Write the parameters to the dirver */
	rc = snd_pcm_hw_params(gp_handle, gp_params);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		goto err1;
	}

	snd_pcm_hw_params_get_period_size(gp_params, &g_frames, 0);
	g_bufsize = g_frames * channels * format_size / 8;
	gp_buffer = (u8 *)malloc(g_bufsize);
	if (gp_buffer == NULL) {
		fprintf(stderr, "malloc failed\n");
		goto err1;
	}

	return 0;

err1:
	snd_pcm_close(gp_handle);
	return -1;
}

volatile bool is_running = true;
void sig_handle(int sig) {
	fprintf(stderr, "sig: %d\n", sig);
	is_running = false;
}

int main(int argc, char *argv[]) {
	if (argc < 3) {
		fprintf(stderr, "usage: %s file.pcm sample_rate channels format_size\n", argv[0]);
		return -1;
	}

	FILE * fp = (strcmp(argv[1], "-") ? fopen(argv[1], "w") : fdopen(1, "w"));
	if (fp == NULL) {
		fprintf(stderr, "can't open wav file\n");
		return -1;
	}
	int sample_rate = atoi(argv[2]);
	int channels = atoi(argv[3]);
	int format_size = atoi(argv[4]);
	int ret = set_hardware_params(sample_rate, channels, format_size);
	if (ret < 0) {
		fprintf(stderr, "set_hardware_params error\n");
		return -1;
	}
	
	if(snd_pcm_prepare(gp_handle) < 0) {
		fprintf(stderr, "snd_pcm_prepare error\n");
		return -1;
	}

	signal(SIGINT, sig_handle);

	size_t rc;
	while(is_running) {
		ret = snd_pcm_readi(gp_handle, gp_buffer, g_frames);
		if (ret == -EPIPE) {
			continue;
		} else if(ret == -EINTR) {
			break;
		} else if (ret < 0) {
			fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
			break;
		} else {
			rc = fwrite(gp_buffer, 1, g_bufsize, fp);
			if(rc != g_bufsize) {
				break;
			}
		}
	}

	snd_pcm_drop(gp_handle);
	snd_pcm_close(gp_handle);
	free(gp_buffer);
	fclose(fp);
	return 0;
}
