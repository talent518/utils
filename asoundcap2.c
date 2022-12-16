#define ALSA_PCM_NEW_HW_PARAMS_API

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
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

snd_pcm_t *gp_handle, *gp_handle_cap;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
snd_pcm_hw_params_t *gp_params, *gp_params_cap;  //设置流的硬件参数
snd_pcm_uframes_t g_frames, g_frames_cap; //snd_pcm_uframes_t其实是unsigned long类型
char *gp_buffer, *gp_buffer_cap;
unsigned int g_bufsize, g_bufsize_cap;

int set_hardware_params(int sample_rate, int channels, int format_size) {
	int rc;
	
	/* Open PCM device for playback */
	rc = snd_pcm_open(&gp_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
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

int setcap_hardware_params(int sample_rate, int channels, int format_size) {
	int rc;
	
	/* Open PCM device for playback */
	rc = snd_pcm_open(&gp_handle_cap, "default", SND_PCM_STREAM_CAPTURE, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to open pcm device\n");
		return -1;
	}


	/* Allocate a hardware parameters object */
	snd_pcm_hw_params_alloca(&gp_params_cap);

	/* Fill it in with default values. */
	rc = snd_pcm_hw_params_any(gp_handle_cap, gp_params_cap);
	if (rc < 0) {
		fprintf(stderr, "unable to Fill it in with default values.\n");
		goto err1;
	}

	/* Interleaved mode */
	rc = snd_pcm_hw_params_set_access(gp_handle_cap, gp_params_cap, SND_PCM_ACCESS_RW_INTERLEAVED);
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
	rc = snd_pcm_hw_params_set_format(gp_handle_cap, gp_params_cap, format);
	if (rc < 0) {
		fprintf(stderr, "unable to set format.\n");
		goto err1;
	}

	/* set channels (stero) */
	rc = snd_pcm_hw_params_set_channels(gp_handle_cap, gp_params_cap, channels);
	if (rc < 0) {
		fprintf(stderr, "unable to set channels (stero).\n");
		goto err1;
	}

	/* set sampling rate */
	int rate = sample_rate;
	rc = snd_pcm_hw_params_set_rate_near(gp_handle_cap, gp_params_cap, &rate, 0);
	if (rc < 0) {
		fprintf(stderr, "unable to set sampling rate.\n");
		goto err1;
	}
	if(rate != sample_rate) {
		fprintf(stderr, "set sample rate %d is not support, should set is %d\n", sample_rate, rate);
		goto err1;
	}

	g_frames_cap = sample_rate / 20;
	rc = snd_pcm_hw_params_set_period_size_near(gp_handle_cap, gp_params_cap, &g_frames_cap, 0);
	if(rc < 0) {
		fprintf(stderr, "unable to set sampling rate.\n");
		goto err1;
	}

	/* Write the parameters to the dirver */
	rc = snd_pcm_hw_params(gp_handle_cap, gp_params_cap);
	if (rc < 0) {
		fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
		goto err1;
	}

	snd_pcm_hw_params_get_period_size(gp_params_cap, &g_frames_cap, 0);

	return 0;

err1:
	snd_pcm_close(gp_handle_cap);
	return -1;
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

static void *play_thread(void *arg) {
	int ret, pos = -1;
	// snd_output_t *log;
	// snd_output_stdio_attach(&log, stderr, 0);
	
	snd_pcm_pause(gp_handle, 0);
	snd_pcm_prepare(gp_handle);
	
	sem_wait(&sem);
	usleep(250000);
	
	while(is_running) {
		pos ++;
		if(pos >= SIZE) pos = 0;

		char *data = bufs[pos];
		snd_pcm_uframes_t count = g_frames;
		
	prepare:
		ret = snd_pcm_writei(gp_handle, data, count);
		if (ret == -EPIPE) {
			// fprintf(stderr, "write pipe\n");
			snd_pcm_status_t *status;
			snd_pcm_status_alloca(&status);
			if((ret = snd_pcm_status(gp_handle, status)) < 0) {
				fprintf(stderr, "snd_pcm_status failure: %s\n", snd_strerror(ret));
				break;
			}
			// snd_pcm_status_dump(status, log);
			if(snd_pcm_status_get_state(status) == SND_PCM_STATE_XRUN) {
				if ((ret = snd_pcm_prepare(gp_handle)) < 0) {
					fprintf(stderr, "snd_pcm_prepare failure: %s\n", snd_strerror(ret));
					break;
				}
				goto prepare;
			}
		} else if (ret < 0) {
			fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
			break;
		} else if(ret != g_frames) {
			fprintf(stderr, "write ret: %d\n", ret);
		}
		
		sem_wait(&sem);
	}
	
	snd_pcm_drain(gp_handle);
	snd_pcm_close(gp_handle);
	
	// snd_output_close(log);
	
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
	ret = setcap_hardware_params(sample_rate, channels, format_size);
	if (ret < 0) {
		fprintf(stderr, "setcap_hardware_params error\n");
		return -1;
	}
	if(g_frames != g_frames_cap) {
		fprintf(stderr, "frames is not equals: play %lu, capture %lu\n", g_frames, g_frames_cap);
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
	if(pthread_create(&thread, &attr, play_thread, NULL)) {
		perror("pthread_create failure");
		return -1;
	}

	size_t rc;
	int pos = -1;
	
	snd_pcm_pause(gp_handle_cap, 0);
	snd_pcm_prepare(gp_handle_cap);
	while(is_running) {
		pos ++;
		if(pos >= SIZE) pos = 0;
		
	prepare:
		ret = snd_pcm_readi(gp_handle_cap, bufs[pos], g_frames_cap);
		if (ret == -EPIPE) {
			fprintf(stderr, "read pipe\n");
			snd_pcm_prepare(gp_handle_cap);
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
	
	is_running = false;
	sem_post(&sem);
	if(pthread_join(thread, NULL)) {
		perror("pthread_join failure");
	}
	
	sem_destroy(&sem);

	snd_pcm_drop(gp_handle_cap);
	snd_pcm_close(gp_handle_cap);
	free(gp_buffer);
	
	for(int i = 0; i < SIZE; i ++) {
		if(bufs[i]) free(bufs[i]);
	}
	
	fprintf(stderr, "Exited\n");
	return 0;
}
