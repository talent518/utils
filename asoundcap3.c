#define ALSA_PCM_NEW_HW_PARAMS_API

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <alsa/asoundlib.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

snd_pcm_t *gp_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
snd_pcm_hw_params_t *gp_params;  //设置流的硬件参数
snd_pcm_uframes_t g_frames; //snd_pcm_uframes_t其实是unsigned long类型
char *gp_buffer;
unsigned int g_bufsize;

int set_hardware_params(int sample_rate, int channels, int format_size) {
	int rc;
	
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

	return 0;

err1:
	snd_pcm_close(gp_handle);
	return -1;
}

char *nowtime(char *buf, int max) {
	struct timeval tv;
	if(gettimeofday(&tv, NULL)) {
		perror("gettimeofday error");
	} else {
		struct tm tm;
		localtime_r(&tv.tv_sec, &tm);
		int n = strftime(buf, max, "%F %T", &tm);
		if(n > 0 && n < max) n = snprintf(buf + n, max - n, ".%03d", (int)(tv.tv_usec / 1000));
		return buf;
	}
}

volatile bool is_running = true;
void sig_handle(int sig) {
	// fprintf(stderr, "sig: %d\n", sig);
	is_running = false;
	fprintf(stderr, "\r");
}

static sem_t sem;
#define SIZE 128
static char *bufs[SIZE];

typedef struct {
	int sum;
	short db;
} calc_t;

static void *calc_thread(void *arg) {
	int channels = * (int*) arg;
	int ret, pos = -1, i, c;
	short *data;
	char buf[128];
	calc_t *dBs = malloc(channels * sizeof(calc_t));

	fprintf(stderr, "%23s%9s%9s\n", "time", "channel1", "channel2");
	
	sem_wait(&sem);
	while(is_running) {
		pos ++;
		if(pos >= SIZE) pos = 0;
		
		data = (short*) bufs[pos];
		for(i = 0; i < channels; i++) dBs[i].sum = 0;
		for(i = 0; i < g_frames * channels; i += channels) {
			for(c = 0; c < channels; c ++) {
				dBs[c].sum += abs(data[i + c]);
			}
		}
		printf("%s", nowtime(buf, sizeof(buf)));
		for(i = 0; i < channels; i++) {
			dBs[i].db = dBs[i].sum * 500.0 / (g_frames * 32767.0);
			if(dBs[i].db > 100) dBs[i].db = 100;
			printf("%9d", dBs[i].db);
		}
		printf("\n");
		
		sem_wait(&sem);
	}
	
	free(dBs);
	
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s sample_rate channels format_size\n", argv[0]);
		return -1;
	}

	int sample_rate = atoi(argv[1]);
	int channels = atoi(argv[2]);
	int format_size = atoi(argv[3]);
	int ret = set_hardware_params(sample_rate, channels, format_size);
	if (ret < 0) {
		fprintf(stderr, "set_hardware_params error\n");
		return -1;
	}

	fprintf(stderr, "sample_rate: %d, channels: %d, format_size: %d, frames: %lu\n", sample_rate, channels, format_size, g_frames);

	signal(SIGINT, sig_handle);
	
	sem_init(&sem, 0, 0);
	memset(bufs, 0, sizeof(bufs));
	for(int i = 0; i < SIZE; i ++) {
		bufs[i] = (char*) malloc(g_frames * channels * format_size / 8);
		if(!bufs[i]) {
			fprintf(stderr, "malloc failure\n");
			return -1;
		}
	}
	
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if(pthread_create(&thread, &attr, calc_thread, &channels)) {
		perror("pthread_create failure");
		return -1;
	}

	int pos = -1;
	
	snd_pcm_pause(gp_handle, 0);
	snd_pcm_prepare(gp_handle);
	while(is_running) {
		pos ++;
		if(pos >= SIZE) pos = 0;
		
	prepare:
		ret = snd_pcm_readi(gp_handle, bufs[pos], g_frames);
		if (ret == -EPIPE) {
			fprintf(stderr, "read pipe\n");
			snd_pcm_prepare(gp_handle);
			goto prepare;
		} else if(ret == -EINTR) {
			break;
		} else if (ret < 0) {
			fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
			break;
		} else {
			sem_post(&sem);
			if(ret != g_frames) {
				fprintf(stderr, "write ret: %d\n", ret);
			}
		}
	}
	
	sem_post(&sem);
	if(pthread_join(thread, NULL)) {
		perror("pthread_join failure");
	}
	
	sem_destroy(&sem);

	snd_pcm_drop(gp_handle);
	snd_pcm_close(gp_handle);
	free(gp_buffer);
	
	for(int i = 0; i < SIZE; i ++) {
		if(bufs[i]) free(bufs[i]);
	}
	
	fprintf(stderr, "Exited\n");
	return 0;
}
