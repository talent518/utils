// weditor: python 模块 https://github.com/talent518/weditor.git
// uiautomator2: python 模块 http://gitee.com/talent518/uiautomator2.git

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
#include <math.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <cJSON.h>

#define VIDEO_IMAGE

static snd_pcm_t *gp_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
static snd_pcm_hw_params_t *gp_params;  //设置流的硬件参数
static snd_pcm_uframes_t g_frames; //snd_pcm_uframes_t其实是unsigned long类型

static int sample_rate = 44100, channels = 2, format_size = 16;

static int set_hardware_params() {
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
		int val = 0, val2 = 0;
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

#define MICRO_IN_SEC 1000000.00

static inline double microtime()
{
	struct timeval tp = {0};

	if (gettimeofday(&tp, NULL)) {
		return 0;
	}

	return (double)(tp.tv_sec + tp.tv_usec / MICRO_IN_SEC);
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

static char *get_title();

volatile bool is_running = true;

static sem_t play_sem;
#define BUFSIZE 64
static short *bufs[BUFSIZE];
static int bufsize;
static int bufpos = 0;

typedef struct {
	int sum;
	short db;
} calc_t;

static GtkWidget *window = NULL;
static volatile bool is_fullscreen = false;
static GdkPixmap *pixmapVolume = NULL, *pixmapWave = NULL, *pixmapFFT = NULL;
static GtkObject *sigVolume = NULL, *sigWave = NULL, *sigFFT = NULL;

static int playpos = -1;
static void *play_sound_thread(void *arg) {
	const int MIN_QUE = 4;
	const int MAX_QUE = 20; // 20 frames per second
	int ret;
	char buf[128];
	
	snd_pcm_pause(gp_handle, 0);
	sem_wait(&play_sem);
	while(is_running) {
		if(!sem_getvalue(&play_sem, &ret) && ret > MAX_QUE) {
			fprintf(stderr, "[%s] PLAY QUE: %d, %d, %d\n", nowtime(buf, sizeof(buf)), ret, MIN_QUE, MAX_QUE);

			snd_pcm_reset(gp_handle);

			for(; ret > MIN_QUE; ret--) {
				playpos ++;
				if(playpos >= BUFSIZE) playpos = 0;

				sem_wait(&play_sem);
			}
		}

		playpos ++;
		if(playpos >= BUFSIZE) playpos = 0;
		
	prepare:
		ret = snd_pcm_writei(gp_handle, bufs[playpos], g_frames);
		if (ret == -EPIPE) {
			snd_pcm_prepare(gp_handle);
			goto prepare;
		} else if(ret == -EINTR) {
			break;
		} else if (ret < 0) {
			fprintf(stderr, "error from writei: %s\n", snd_strerror(ret));
			break;
		} else if(ret != g_frames) {
			fprintf(stderr, "write ret: %d\n", ret);
			break;
		}

		while(sem_trywait(&play_sem) && is_running) usleep(10000); // sleep 10ms
	}
	pthread_exit(NULL);
}

#define WS_SELECT_USEC 20000 // 20ms

#define WS_CONN_TIMEOUT 5
#define WS_SEND_TIMEOUT 5
#define WS_RECV_TIMEOUT 5
#define WS_SEND_MASK 1

#define WS_CTL_TXT 0x81
#define WS_CTL_BIN 0x82
#define WS_CTL_PING 0x89
#define WS_CTL_PONG 0x8a

static void ws_setopt(int s, int send_timeout, int recv_timeout, int send_buffer, int recv_buffer) {
	struct timeval tv;
 
    tv.tv_usec = 0;
    tv.tv_sec = send_timeout;
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)); // 发送超时

    tv.tv_usec = 0;
    tv.tv_sec = recv_timeout;
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)); // 接收超时

	setsockopt(s, SOL_SOCKET, SO_SNDBUF, &send_buffer, sizeof(int));//发送缓冲区大小
	setsockopt(s, SOL_SOCKET, SO_RCVBUF, &recv_buffer, sizeof(int));//接收缓冲区大小

	typedef struct {
		u_short l_onoff;
		u_short l_linger;
	} linger;
	linger m_sLinger;
	m_sLinger.l_onoff = 0;//(在closesocket()调用,但是还有数据没发送完毕的时候容许逗留)
	// 如果m_sLinger.l_onoff=0;则功能和2.)作用相同;
	m_sLinger.l_linger = 0;//(容许逗留的时间为5秒)
	setsockopt(s, SOL_SOCKET, SO_LINGER, &m_sLinger, sizeof(linger));
}

static char *servhost = "127.0.0.1";
static int servport = 17310;
static char *servpath = "";
static struct sockaddr_in servaddr;

static int sock_conn(const char *func, int timeout) {
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int ret;
	int flags = fcntl(fd, F_GETFL);
	
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	
	if(connect(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		char buf[32];
		
		if(EINPROGRESS == errno) {
			struct timeval tv;
			fd_set set;
			
			double t = microtime() + timeout;
			
			do {
				tv.tv_sec = 0;
				tv.tv_usec = WS_SELECT_USEC;
				
				FD_ZERO(&set);
				FD_SET(fd, &set);
				
				ret = select(fd+1, NULL, &set, NULL, &tv);
			} while(ret == 0 && microtime() < t && is_running);
			
			if(ret > 0) {
				socklen_t len = sizeof(ret);
				if(getsockopt(fd, SOL_SOCKET, SO_ERROR, &ret, &len) || ret) {
					fprintf(stderr, "[%s] connect %s failure: get connect status error\n", nowtime(buf, sizeof(buf)), func);
				} else {
					goto connok;
				}
			} else {
				fprintf(stderr, "[%s] connect %s failure: timeout\n", nowtime(buf, sizeof(buf)), func);
			}
		} else {
			char *err = strerror(errno);
			fprintf(stderr, "[%s] connect %s failure: %s\n", nowtime(buf, sizeof(buf)), func, err);
		}
		close(fd);
		return 0;
	} else {
	connok:
		flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		return fd;
	}
}

static volatile bool is_connect = false;

static int _ws_conn(const char *func, const char *path, int timeout) {
	char buf[2048];
	int size = 0, ret, sz = 0;
	int fd;
	
	if(!is_connect) return 0;

	fprintf(stderr, "[%s] websocket %s connecting ...\n", nowtime(buf, sizeof(buf)), func);
	
	fd = sock_conn(func, timeout);
	
	if(fd == 0) return 0;
	
	size += snprintf(buf + size, sizeof(buf), "GET %s%s HTTP/1.1\r\n", servpath, path);
	size += snprintf(buf + size, sizeof(buf), "Host: %s:%d\r\n", servhost, servport);
	size += snprintf(buf + size, sizeof(buf), "Connection: Upgrade\r\n");
	size += snprintf(buf + size, sizeof(buf), "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n");
	size += snprintf(buf + size, sizeof(buf), "Sec-WebSocket-Key: uQQ1BAbtsumAi1y7Mu4spQ==\r\n");
	size += snprintf(buf + size, sizeof(buf), "Sec-WebSocket-Version: 13\r\n");
	size += snprintf(buf + size, sizeof(buf), "Upgrade: websocket\r\n");
	size += snprintf(buf + size, sizeof(buf), "User-Agent: weditor for c language client\r\n");
	size += snprintf(buf + size, sizeof(buf), "\r\n");
	
loopsend:
	ret = send(fd, buf + sz, size - sz, 0);
	if(ret > 0) {
		sz += ret;
		
		if(!is_running) goto err;
		if(sz < size) goto loopsend;
	} else {
		goto err;
	}
	
	sz = 0;
	memset(buf, 0, sizeof(buf));
	
looprecv:
	ret = recv(fd, buf + sz, sizeof(buf) - sz, 0);
	if(ret > 0) {
		// fwrite(buf + sz, 1, ret, stdout);
		sz += ret;
		
		char *ptr = strstr(buf, "\r\n\r\n");
		if(!ptr) {
			if(!is_running) goto err;
			
			if(sz < sizeof(buf)) goto looprecv;
			else {
				fprintf(stderr, "[%s] buffer full\n", nowtime(buf, sizeof(buf)));
				goto err;
			}
		}
		
		if(strncmp(buf, "HTTP/1.1 101 ", sizeof("HTTP/1.1 101 ") - 1)) goto err;
		
		if(!strstr(buf, "\r\nUpgrade: websocket\r\n")) goto err;
		if(!strstr(buf, "\r\nConnection: Upgrade\r\n")) goto err;
		if(!(strstr(buf, "\r\nSec-Websocket-Accept: HZw0xDMnzz6PpJGmqKAkwUfw+CU=\r\n") || strstr(buf, "\r\nSec-WebSocket-Accept: HZw0xDMnzz6PpJGmqKAkwUfw+CU=\r\n"))) goto err;
		
		fprintf(stderr, "[%s] websocket %s connected success\n", nowtime(buf, sizeof(buf)), func);
	} else {
	err:
		fprintf(stderr, "[%s] websocket %s connected failure\n", nowtime(buf, sizeof(buf)), func);
		close(fd);
		fd = 0;
	}
	if(fd) {
		ws_setopt(fd, 300, 300, 128 * 1024, 128 * 1024);
		
		int flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		
		return fd;
	} else {
		return 0;
	}
}

static int ws_conn(const char *func, const char *path, int timeout) {
	int fd = 0, i;
	
	while(is_running && !(fd = _ws_conn(func, path, timeout))) {
		is_connect = false;
		for(i = 0; is_running && !is_connect && i < 20 * 3; i ++) usleep(50000);
	}
	
	return fd;
}

#define WS_SEND(fd, ctl, is_mask, str, timeout) ws_send(fd, ctl, is_mask, str, sizeof(str) - 1, timeout, __func__)

static int ws_send(int fd, int ctl, int is_mask, char *data, int size, int timeout, const char *func) {
	char *ptr, mask[4], buf[128];
	int i = 0, j, sz, ret;
	struct timeval tv;
	fd_set set;
	double t;
	
	if(is_mask) is_mask = 0x80;
	
	ptr = (char*) malloc(size + 14);
	ptr[i++] = ctl;
	if(size <= 125) {
		ptr[i++] = size | is_mask;
	} else if(size > 125 && size < 65536) {
		ptr[i++] = 126 | is_mask;
		ptr[i++] = size >> 8;
		ptr[i++] = size;
	} else {
		ptr[i++] = 127 | is_mask;
		ptr[i++] = 0;
		ptr[i++] = 0;
		ptr[i++] = 0;
		ptr[i++] = 0;
		for(j = 24; j >= 0; j -= 8) {
			ptr[i++] = size >> j;
		}
	}
	
	if(is_mask) {
		srand(time(NULL));
		for(j = 0; j < 4; j ++) {
			mask[j] = rand() & 0xff;
		}
		memcpy(ptr + i, mask, 4);
		i += 4;
		
		for(j = 0; j < size; j ++) {
			ptr[i++] = data[j] ^ mask[j % 4];
		}
	} else {
		for(j = 0; j < size; j ++) {
			ptr[i++] = data[j];
		}
	}
	
	sz = 0;
	t = microtime() + timeout;

loopsend:
	do {
		tv.tv_sec = 0;
		tv.tv_usec = WS_SELECT_USEC;
		
		FD_ZERO(&set);
		FD_SET(fd, &set);
		
		ret = select(fd+1, NULL, &set, NULL, &tv);
	} while(ret == 0 && microtime() < t && is_running);
	
	if(ret <= 0 || !is_running) goto err;
	
	ret = send(fd, ptr + sz, i - sz, 0);
	if(ret > 0) {
		sz += ret;
		
		if(sz < i) goto loopsend;
		
		free(ptr);
		return 1;
	} else {
		if(ret == 0) fprintf(stderr, "[%s] websocket %s disconnected\n", nowtime(buf, sizeof(buf)), func);
	err:
		free(ptr);
		return 0;
	}
}

static char *ws_recv(int fd, int *size, char **prev_ptr, int *prev_n, int *is_bin, int timeout, const char *func) {
	char *ptr = NULL;
	int ret, i, n, sz, ctl;
	unsigned char buf[1024] = {0};
	char mask[8];
	struct timeval tv;
	fd_set set;
	double t;
	
begin:
	n = 0;
	if(*prev_ptr) {
		if(*prev_n > 0) {
			n = *prev_n;
			memcpy(buf, *prev_ptr, n);
		}
		
		free(*prev_ptr);
		*prev_n = 0;
		*prev_ptr = NULL;
	} else {
		*prev_n = 0;
	}

	t = microtime() + timeout;
	
	if(n > 0) goto protocol;
slt:
	do {
		tv.tv_sec = 0;
		tv.tv_usec = WS_SELECT_USEC;
		
		FD_ZERO(&set);
		FD_SET(fd, &set);
		
		ret = select(fd+1, &set, NULL, NULL, &tv);
	} while(ret == 0 && microtime() < t && is_running);
	
	if(ret <= 0 || !is_running) goto err;
	
head:
	ret = recv(fd, buf + n, sizeof(buf) - n, 0);
	if(ret > 0) {
		n += ret;
		
	protocol:
		if(n < 2) goto head;

		ctl = buf[0] & 0xf;
		sz = buf[1] & 0x7f;

		if(!(buf[0] & 0x80) || (buf[0] & 0x70) || (ctl != 0x1 && ctl != 0x2 && ctl != 0x8 && ctl != 0x9 && ctl != 0xa)) { // !FIN || (RSV1 || RSV2 || RSV3) || (invalid ctl)
			fprintf(stderr, "[%s] websocket %s protocol type error\n", nowtime(buf, sizeof(buf)), func);
			goto err;
		}

		if(buf[1] & 0x80) {
			if(sz == 126) {
				if(n < 8) goto head;
				
				sz = (buf[2] << 8) | buf[3];
				memcpy(mask, buf + 4, 4);
				*size = sz;
				i = 8;
			} else if(sz == 127) {
				if(n < 14) goto head;
				
				sz = 0;
				for(int i = 0; i < 8; i++) {
					sz |= buf[2+i];
					sz <<= 8;
				}
				
				memcpy(mask, buf + 10, 4);
				*size = sz;
				i = 14;
			} else {
				if(n < 6) goto head;
				
				memcpy(mask, buf + 2, 4);
				*size = sz;
				i = 6;
			}
		} else {
			if(sz == 126) {
				if(n < 4) goto head;
				
				sz = (buf[2] << 8) | buf[3];
				*size = sz;
				i = 4;
			} else if(sz == 127) {
				if(n < 10) goto head;
				
				sz = 0;
				for(int i = 0; i < 8; i++) {
					sz <<= 8;
					sz |= buf[2+i];
				}
				
				*size = sz;
				i = 10;
			} else {
				*size = sz;
				i = 2;
			}
		}
		
		ptr = (char*) malloc(sz + 1);
		ptr[sz] = 0;
		n -= i;
		if(n >= sz) {
			memcpy(ptr, buf + i, sz);
			
			n -= sz;
			
			if(n > 0) {
				*prev_n = n;
				*prev_ptr = (char*) malloc(n);
				memcpy(*prev_ptr, buf + i + sz, n);
			}
		} else {
			if(n > 0) memcpy(ptr, buf + i, n);
			
			while(n < sz) {
				do {
					tv.tv_sec = 0;
					tv.tv_usec = WS_SELECT_USEC;
					
					FD_ZERO(&set);
					FD_SET(fd, &set);
					
					ret = select(fd+1, &set, NULL, NULL, &tv);
				} while(ret == 0 && microtime() < t && is_running);
				
				if(ret <= 0 || !is_running) goto err;

				ret = recv(fd, ptr + n, sz - n, 0);
				if(ret > 0) {
					n += ret;
				} else {
					char *err = strerror(errno);
					fprintf(stderr, "[%s] websocket %s protocol data error: %s\n", nowtime(buf, sizeof(buf)), func, err);
					goto err;
				}
			}
		}
		
		n = 0;
		
		if((buf[1] & 0x80)) { // Mask
			for(i = 0; i < sz; i++) {
				ptr[i] ^= mask[i % 4];
			}
		}
		
		switch(ctl) {
			case 0x1: // text
				if(is_bin) *is_bin = 0;
				break;
			case 0x2: // binary
				if(is_bin) *is_bin = 1;
				break;
			case 0x8: // close
				if(sz > 2) {
					fprintf(stderr, "[%s] websocket %s disconnected, Errno: %d, Error: %s\n", nowtime(buf, sizeof(buf)), func, ((ptr[0] << 8) | ptr[1]), ptr + 2);
				} else {
					fprintf(stderr, "[%s] websocket %s disconnected, Errno: 0, Error: OK\n", nowtime(buf, sizeof(buf)), func);
				}
				goto err;
				break;
			case 0x9: // ping
				fprintf(stderr, "[%s] websocket %s ping: %s\n", nowtime(buf, sizeof(buf)), func, ptr);
				if(!ws_send(fd, WS_CTL_PONG, WS_SEND_MASK, ptr, sz, WS_SEND_TIMEOUT, __func__)) goto err;
				goto gobegin;
				break;
			case 0xA: // pong
				fprintf(stderr, "[%s] websocket %s pong: %s\n", nowtime(buf, sizeof(buf)), func, ptr);
				
				gobegin:
				if(ptr) {
					free(ptr);
					ptr = NULL;
				}
				goto begin;
				break;
			default:
				fprintf(stderr, "unknown control code: %d\n", ctl);
				goto err;
				break;
		}
		
		return ptr;
	} else if(ret) {
		char *err = strerror(errno);
		fprintf(stderr, "[%s] websocket %s protocol head error: %s\n", nowtime(buf, sizeof(buf)), func, err);
	} else {
		fprintf(stderr, "[%s] websocket %s disconnected\n", nowtime(buf, sizeof(buf)), func);
	}

err:
	if(ptr) {
		free(ptr);
		ptr = NULL;
	}
	if(*prev_ptr) {
		free(*prev_ptr);
		*prev_n = 0;
		*prev_ptr = NULL;
	}
	
	return NULL;
}

static int sound_loading = 0;

static void *conn_sound_thread(void *arg) {
	const int DELAY = 4;
	int fd = 0, sz = 0, n = 0, is_bin = 0, delay = 0, i;
	char *ptr, *old = NULL;
	char buf[128];
	
	while(is_running) {
		if(fd) {
			ptr = ws_recv(fd, &sz, &old, &n, &is_bin, WS_RECV_TIMEOUT, __func__);
			if(ptr) {
				if(is_bin) {
					if(sz != bufsize) {
						fprintf(stderr, "sound buffer size %d is not equals %d\n", sz, bufsize);
					} else {
						bufpos ++;
						if(bufpos >= BUFSIZE) bufpos = 0;
						
						memcpy(bufs[bufpos], ptr, sz);

						if(delay > DELAY) {
							sem_post(&play_sem);
						} else if(delay < DELAY) {
							delay ++;
						} else {
							for(i = 0; i <= delay; i ++) {
								sem_post(&play_sem);
							}
							delay ++;
						}

						if(is_running) {
							gdk_threads_enter();
							if(sigVolume) gtk_signal_emit_by_name(sigVolume, "da_event");
							if(sigWave) gtk_signal_emit_by_name(sigWave, "da_event");
							if(sigFFT) gtk_signal_emit_by_name(sigFFT, "da_event");
							gdk_threads_leave();
						}
					}
				} else {
					printf("[%s] sound data: %s\n", nowtime(buf, sizeof(buf)), ptr);
				}
				
				free(ptr);
				ptr = NULL;
			} else {
				close(fd);
				fd = 0;
				if(delay < DELAY) {
					for(i = 0; i < delay; i ++) {
						sem_post(&play_sem);
					}
				}
				delay = 0;
			}
		} else {
			n = 0;
			if(old) {
				free(old);
				old = NULL;
			}
			
			sound_loading = 1;
			fd = ws_conn(__func__, "/ws/v1/minisound", WS_CONN_TIMEOUT);
			sound_loading = 0;
		}
	}
	
	if(fd) close(fd);
	if(old) free(old);

	pthread_exit(NULL);
}

static GtkWidget *videoFrame = NULL, *sizeFrame = NULL, *audioFrame = NULL;
#ifdef VIDEO_IMAGE
static GtkWidget *videoImage = NULL;
#endif
static int video_fps = 0, video_frames = 0, video_loading = 0, video_rotation = 0, video_width = 0, video_height = 0;
static double stats[6] = {0, 0, 0, 0, 0, 0};
#define STAT_ATX_CPU 0
#define STAT_ATX_MEM 1
#define STAT_ATX_DISK 2
#define STAT_WDR_CPU 3
#define STAT_WDR_MEM 4
#define STAT_WDR_DISK 5

static double cjson_get_value_double(cJSON *json, char *key) {
	cJSON *item = cJSON_GetObjectItem(json, key);
	if(item) return cJSON_GetNumberValue(item);
	else return 0.0;
}

static void *conn_video_thread(void *arg) {
	int fd = 0, sz = 0, oldsz = 0, n = 0, is_bin = 0, fwidth = 0, fheight = 0;
	char *ptr, *old = NULL, *oldptr = NULL;
	GdkPixbuf *pixbuf, *dst = NULL;
	GdkPixbufLoader *loader;
	bool isfull = false;
	char buf[128];
	double t;
	
	while(is_running) {
		if(fd) {
			ptr = ws_recv(fd, &sz, &old, &n, &is_bin, WS_RECV_TIMEOUT, __func__);
			if(ptr) {
				if(is_running) {
					if(is_bin) {
					redraw:
						isfull = is_fullscreen;
						gdk_threads_enter();
						video_frames ++;
						loader = gdk_pixbuf_loader_new_with_mime_type("image/jpeg", NULL);
						gdk_pixbuf_loader_write(loader, ptr, sz, NULL);
						if(gdk_pixbuf_loader_close(loader, NULL)) {
							pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
							if(pixbuf) {
								GdkGC *gc = videoFrame->style->fg_gc[GTK_WIDGET_STATE(videoFrame)];
								video_width = gdk_pixbuf_get_width(pixbuf);
								video_height = gdk_pixbuf_get_height(pixbuf);
								
								if(is_fullscreen) {
									int width = 0, height = 0;
									gtk_window_get_size(GTK_WINDOW(window), &width, &height);
									
									gtk_widget_set_size_request(sizeFrame, width, height);
									gtk_widget_set_size_request(videoFrame, width, height);

									double scale = (double) width / (double) video_width;
									int w = width, h = (double) video_height * scale;
									if(h > height) {
										scale = (double) height / (double) video_height;
										h = height;
										w = (double) video_width * scale;
									}
									
									int x = (width - w) / 2, y = (height - h) / 2;
									// printf("x = %d, y = %d, w = %d, h = %d, width = %d, height = %d, video_width = %d, video_height = %d\n", x, y, w, h, width, height, video_width, video_height);
									
									video_width = w;
									video_height = h;

									if(!dst || fwidth != width || fheight != height) {
										fwidth = width;
										fheight = height;
										if(dst) g_object_unref(dst);
										dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
									}
									
									gdk_pixbuf_fill(dst, 0x000000);
									gdk_pixbuf_scale(pixbuf, dst, x, y, w, h, x, y, scale, scale, GDK_INTERP_BILINEAR);
								#ifdef VIDEO_IMAGE
									gtk_image_set_from_pixbuf(GTK_IMAGE(videoImage), dst);
								#else
									gdk_draw_pixbuf(videoFrame->window, gc, dst, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);
								#endif
								} else if(video_width > 800 || video_height > 800) {
									int width, height;
									if(video_width > video_height) {
										width = 800;
										height = 800 * video_height / video_width;
									} else {
										width = 800 * video_width / video_height;
										height = 800;
									}
									
									double scale = (double) width / (double) video_width;
									
									video_width = width;
									video_height = height;
									
									if(!dst || fwidth != width || fheight != height) {
										fwidth = width;
										fheight = height;
										if(dst) g_object_unref(dst);
										dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, width, height);
									}
									
									gdk_pixbuf_fill(dst, 0x000000);
									gdk_pixbuf_scale(pixbuf, dst, 0, 0, width, height, 0, 0, scale, scale, GDK_INTERP_BILINEAR);

									gtk_widget_set_size_request(audioFrame, 400, video_height);
									gtk_widget_set_size_request(videoFrame, video_width, video_height);
									gtk_widget_set_size_request(sizeFrame, video_width + 400 + 20, video_height);
									gtk_widget_set_size_request(window, video_width + 400 + 30, video_height + 20);
									gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
									
								#ifdef VIDEO_IMAGE
									gtk_image_set_from_pixbuf(GTK_IMAGE(videoImage), dst);
								#else
									gdk_draw_pixbuf(videoFrame->window, gc, dst, 0, 0, 0, 0, width, height, GDK_RGB_DITHER_NORMAL, 0, 0);
								#endif
								} else {
									gtk_widget_set_size_request(audioFrame, 400, video_height);
									gtk_widget_set_size_request(videoFrame, video_width, video_height);
									gtk_widget_set_size_request(sizeFrame, video_width + 400 + 20, video_height);
									gtk_widget_set_size_request(window, video_width + 400 + 30, video_height + 20);
									gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
									
								#ifdef VIDEO_IMAGE
									gtk_image_set_from_pixbuf(GTK_IMAGE(videoImage), pixbuf);
								#else
									gdk_draw_pixbuf(videoFrame->window, gc, pixbuf, 0, 0, 0, 0, video_width, video_height, GDK_RGB_DITHER_NORMAL, 0, 0);
								#endif
								}
							}
						}
						g_object_unref(loader);
						gdk_threads_leave();

						if(oldptr) free(oldptr);
						oldptr = ptr;
						oldsz = sz;
						ptr = NULL;
						sz = 0;
					} else if(!strncmp(ptr, "@SysInfo", 8) || !strncmp(ptr, "@HostInfo", 9)) {
						bool atx = (ptr[1] == 'S');
						cJSON *json = cJSON_Parse(ptr + (atx ? 9 : 10));
						int n = (atx ? 0 : 3);
						printf("[%s] %s %lg %04.1lf%% %.3lfGB %04.1lf%% %.3lfGB %.3lfGB %04.1lf%%\n",
							nowtime(buf, sizeof(buf)),
							atx ? "ATX" : "WDR",
							cjson_get_value_double(json, "cpuCount"),
							stats[STAT_ATX_CPU + n] = cjson_get_value_double(json, "cpuPercent"),
							cjson_get_value_double(json, "memTotal") / 1024.0 / 1024.0 / 1024.0,
							stats[STAT_ATX_MEM + n] = cjson_get_value_double(json, "memPercent"),
							cjson_get_value_double(json, "diskTotal") / 1024.0 / 1024.0 / 1024.0,
							cjson_get_value_double(json, "diskUsed") / 1024.0 / 1024.0 / 1024.0,
							stats[STAT_ATX_DISK + n] = cjson_get_value_double(json, "diskPercent")
						);
						cJSON_free(json);
					} else if(!strcmp(ptr, "==equalFrame==")) {
						video_frames ++;

						if(oldptr && isfull != is_fullscreen) {
							printf("[%s] redraw\n", nowtime(buf, sizeof(buf)));
							free(ptr);
							ptr = oldptr;
							sz = oldsz;
							oldptr = NULL;
							oldsz = 0;
							goto redraw;
						}
					} else {
						printf("[%s] video data: %s\n", nowtime(buf, sizeof(buf)), ptr);
						
						if(!strncmp(ptr, "rotation ", sizeof("rotation ") - 1)) {
							video_rotation = atoi(ptr + sizeof("rotation ") - 1);
						}
					}
				}
				
				if(ptr) {
					free(ptr);
					ptr = NULL;
				}
			} else {
				close(fd);
				fd = 0;
				
				is_connect = false;
				for(int i = 0; is_running && i < 20*5; i++) usleep(50000);
			}
		} else {
			n = 0;
			if(old) {
				free(old);
				old = NULL;
			}
			
			video_loading = 1;
			fd = ws_conn(__func__, "/ws/v1/minicap?deviceId=android:", WS_CONN_TIMEOUT);
			t = microtime();
			video_loading = 0;
		}
	}
	
	if(fd) close(fd);
	if(old) free(old);
	if(ptr) free(ptr);
	if(oldptr) free(oldptr);
	pthread_exit(NULL);
}

typedef struct {
	int e;
	float x;
	float y;
} touch_event_t;

#define TOUCH_EVENT_SIZE 512
static touch_event_t touch_events[TOUCH_EVENT_SIZE];
volatile int touch_event_pos = 0, touch_event_size = 0, touch_event_loading = 0;
static pthread_mutex_t touch_lock;

static void touch_event_push(int event, int x, int y) {
	touch_event_t *ptr;
	int pos, x2, y2, w, h;
	
	if(touch_event_loading) return;

	if(is_fullscreen) {
		w = videoFrame->allocation.width;
		h = videoFrame->allocation.height;
		
		x2 = (w - video_width) / 2;
		y2 = (h - video_height) / 2;
		if(x < x2 || x > x2 + video_width) return;
		if(y < y2 || y > y2 + video_height) return;
		
		x -= x2;
		y -= y2;
	}

	switch(video_rotation) {
        case 0:
        default:
        	w = video_width;
        	h = video_height;
        	x2 = x;
        	y2 = y;
            break;
        case 90:
        	w = video_height;
        	h = video_width;
            x2 = video_height - y;
            y2 = x;
            break;
        case 180:
        	w = video_width;
        	h = video_height;
            x2 = video_width - x;
            y2 = video_height - y;
            break;
        case 270:
        	w = video_height;
        	h = video_width;
            x2 = y;
            y2 = video_width - x;
            break;
    }
	
	pthread_mutex_lock(&touch_lock);
	if(touch_event_size < TOUCH_EVENT_SIZE) {
		pos = touch_event_pos + (touch_event_size++);
		if(pos >= TOUCH_EVENT_SIZE) pos = 0;
		ptr = &touch_events[pos];
		
		ptr->e = event;
		ptr->x = (float) x2 / (float) w;
		ptr->y = (float) y2 / (float) h;
	}
	pthread_mutex_unlock(&touch_lock);
}

static int touch_event_send(int fd) {
	touch_event_t *ptr;
	int sz, e;
	float x, y;
	char buf[256];
	
	while(1) {
		pthread_mutex_lock(&touch_lock);
		if(touch_event_size) {
			touch_event_size --;
			
			ptr = &touch_events[touch_event_pos];

			e = ptr->e;
			x = ptr->x;
			y = ptr->y;
			
			touch_event_pos ++;
			if(touch_event_pos >= TOUCH_EVENT_SIZE) touch_event_pos = 0;
			
			sz = 1;
		} else {
			sz = 0;
		}
		pthread_mutex_unlock(&touch_lock);
		
		if(sz) {
			sz = snprintf(buf, sizeof(buf), "{\"operation\":\"%c\",\"index\":0,\"pressure\":0.5,\"xP\":%g,\"yP\":%g}", e, x, y);
			if(sz >= sizeof(buf)) {
				fprintf(stderr, "[%s] snprintf buffer size error\n", nowtime(buf, sizeof(buf)));
				return 0;
			}
			if(!ws_send(fd, WS_CTL_TXT, WS_SEND_MASK, buf, sz, WS_SEND_TIMEOUT, __func__)) {
				return 0;
			}
			if(!WS_SEND(fd, WS_CTL_TXT, WS_SEND_MASK, "{\"operation\":\"c\"}", WS_SEND_TIMEOUT)) {
				return 0;
			}
		} else {
			break;
		}
	}
	return 1;
}

static void *touch_event_thread(void *arg) {
	int fd = 0, sz = 0, n = 0, is_bin = 0, pos, ret;
	char *ptr, *old = NULL;
	struct timeval tv;
	fd_set set;
	touch_event_t evt;
	char buf[128];
	double t = 0;
	
	while(is_running) {
		if(fd) {
			// ======== recv touch message ========
			
			tv.tv_sec = 0;
			tv.tv_usec = WS_SELECT_USEC;
			
			FD_ZERO(&set);
			FD_SET(fd, &set);
			
			ret = 0;
			if((ret = select(fd+1, &set, NULL, NULL, &tv)) > 0) {
			recv:
				ptr = ws_recv(fd, &sz, &old, &n, &is_bin, WS_RECV_TIMEOUT, __func__);
				if(ptr) {
					if(is_running) {
						if(is_bin) printf("[%s] touch data is binary\n", nowtime(buf, sizeof(buf)));
						else printf("[%s] touch data: %s\n", nowtime(buf, sizeof(buf)), ptr);
					}
					
					free(ptr);
					ptr = NULL;
					if(old && n) goto recv;
				} else {
					goto end;
				}
			} else if(ret < 0) {
				goto end;
			}
			
			// ======== send touch event ========
			
			tv.tv_sec = 0;
			tv.tv_usec = WS_SELECT_USEC;
			
			FD_ZERO(&set);
			FD_SET(fd, &set);
			
			ret = 0;
			if(((ret = select(fd+1, NULL, &set, NULL, &tv)) > 0 && !touch_event_send(fd)) || ret < 0) {
				goto end;
			}
		} else {
			n = 0;
			if(old) {
				free(old);
				old = NULL;
			}
			
			touch_event_loading = 1;
			fd = ws_conn(__func__, "/ws/v1/minitouch?deviceId=android:", WS_CONN_TIMEOUT);
			t = microtime();
			touch_event_loading = 0;
			if(fd && !WS_SEND(fd, WS_CTL_TXT, WS_SEND_MASK, "{\"operation\":\"r\"}", WS_SEND_TIMEOUT)) {
			end:
				close(fd);
				fd = 0;
				
				is_connect = false;
				for(int i = 0; is_running && i < 20*5; i++) usleep(50000);
			}
		}
	}
	
	if(fd) close(fd);
	if(old) free(old);
	pthread_exit(NULL);
}

static gboolean scribble_configure_event_volume(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	if(pixmapVolume) g_object_unref(G_OBJECT(pixmapVolume));

	pixmapVolume = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);

	gdk_draw_rectangle(pixmapVolume, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	return TRUE;
}

static gboolean scribble_expose_event_volume(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmapVolume, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

	return FALSE;
}

static void scribble_da_event_volume(GtkWidget *widget, GdkEventButton *event, gpointer _data) {
	int i, c;
	short *data;
	calc_t *dBs;
	char buf[128];
	unsigned short maxs[] = {0, 0}, m;

	GdkRectangle update_rect;

	if(pixmapVolume == NULL) return;

	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = widget->allocation.width;
	update_rect.height = widget->allocation.height;

	dBs = (calc_t*) malloc(channels * sizeof(calc_t));

	data = bufs[bufpos];
	for(i = 0; i < channels; i++) dBs[i].sum = 0;
	for(i = 0; i < g_frames * channels; i += channels) {
		for(c = 0; c < channels; c ++) {
			m = abs(data[i + c]);
			dBs[c].sum += m;
			if(m > maxs[c]) {
				maxs[c] = m;
			}
		}
	}

	for(i = 0; i < channels; i++) {
		dBs[i].db = dBs[i].sum * 100.0 / (g_frames * 32767.0);
		if(dBs[i].db > 100) dBs[i].db = 100;
	}

	gdk_draw_rectangle(pixmapVolume, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	gint h = widget->allocation.height / channels;
	GdkGC *gcs[] = {widget->style->dark_gc[3], widget->style->light_gc[3]};
	for(i = 0; i < channels; i++) {
		gdk_draw_rectangle(pixmapVolume, gcs[i], TRUE, 0, i * h, widget->allocation.width * dBs[i].db / 100, h);
		gdk_draw_rectangle(pixmapVolume, widget->style->base_gc[5 - i], TRUE, (widget->allocation.width * maxs[i] / 32767) - 2, i * h, 4, h);
	}

	gdk_window_invalidate_rect(widget->window, &update_rect, FALSE);

	free(dBs);
}

static gboolean scribble_configure_event_wave(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	if(pixmapWave) g_object_unref(G_OBJECT(pixmapWave));

	pixmapWave = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);

	return TRUE;
}

static gboolean scribble_expose_event_wave(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmapWave, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

	return FALSE;
}

static GdkPoint *points[] = {NULL, NULL};

static void scribble_da_event_wave(GtkWidget *widget, GdkEventButton *event, gpointer _data) {
	GdkRectangle update_rect;
	int i, c;
	short *data;

	if(pixmapWave == NULL) return;

	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = widget->allocation.width;
	update_rect.height = widget->allocation.height;

	gint h = widget->allocation.height / channels, h2 = h / 2;
	double w = (double) widget->allocation.width / (double) g_frames;
	GdkGC *gcs[] = {widget->style->dark_gc[3], widget->style->light_gc[3]};
	for(i = 0; i < channels; i ++) {
		if(!points[i]) points[i] = (GdkPoint*) malloc(sizeof(GdkPoint) * g_frames);
	}
	data = bufs[bufpos];
	GdkPoint *p;
	for(i = 0; i < g_frames * channels; i += channels) {
		for(c = 0; c < channels; c ++) {
			p = &points[c][i/channels];
			p->x = i * w;
			p->y = c * h + h2 - data[i + c] * h2 / 32767;
		}
	}

	gdk_draw_rectangle(pixmapWave, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	for(i = 0; i < channels; i ++) {
		if(i % 2) gdk_draw_line(pixmapWave, widget->style->black_gc, 0, i * h, widget->allocation.width, h);
		gdk_draw_lines(pixmapWave, gcs[i], points[i], g_frames);
	}

	gdk_window_invalidate_rect(widget->window, &update_rect, FALSE);
}

static gboolean scribble_configure_event_fft(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	if(pixmapFFT) g_object_unref(G_OBJECT(pixmapFFT));

	pixmapFFT = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);

	return TRUE;
}

static gboolean scribble_expose_event_fft(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	gdk_draw_drawable(widget->window, widget->style->fg_gc[GTK_WIDGET_STATE(widget)], pixmapFFT, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

	return FALSE;
}

typedef struct {
	float real;
	float imag;
} complex_t;

#ifndef PI
# define PI 3.14159265358979323846264338327950288
#endif

/**
   fft(v,N):
   [0] If N==1 then return.
   [1] For k = 0 to N/2-1, let ve[k] = v[2*k]
   [2] Compute fft(ve, N/2);
   [3] For k = 0 to N/2-1, let vo[k] = v[2*k+1]
   [4] Compute fft(vo, N/2);
   [5] For m = 0 to N/2-1, do [6] through [9]
   [6]   Let w.real = cosf(2*PI*m/N)
   [7]   Let w.imag = -sinf(2*PI*m/N)
   [8]   Let v[m] = ve[m] + w*vo[m]
   [9]   Let v[m+N/2] = ve[m] - w*vo[m]
 */
void fft(complex_t *v, int n, complex_t *tmp) {
	int k,m;
	complex_t z, w, *vo, *ve;

	/* otherwise, do nothing and return */
	if(n <= 1) return;

	ve = tmp; vo = tmp+n/2;
	for(k=0; k<n/2; k++)
	{
		ve[k] = v[2*k];
		vo[k] = v[2*k+1];
	}
	/* FFT on even-indexed elements of v[] */
	fft( ve, n/2, v );
	/* FFT on odd-indexed elements of v[] */
	fft( vo, n/2, v );
	for(m = 0; m < n / 2; m ++) {
		w.real = cosf(2*PI*m/(double)n);
		w.imag = -sinf(2*PI*m/(double)n);
		/* real(w*vo[m]) */
		z.real = w.real*vo[m].real - w.imag*vo[m].imag;
		/* imag(w*vo[m]) */
		z.imag = w.real*vo[m].imag + w.imag*vo[m].real;
		v[  m  ].real = ve[m].real + z.real;
		v[  m  ].imag = ve[m].imag + z.imag;
		v[m+n/2].real = ve[m].real - z.real;
		v[m+n/2].imag = ve[m].imag - z.imag;
	}
}

static complex_t *fft_val[] = {NULL, NULL}, *fft_res[] = {NULL, NULL};
static GdkPoint *fft_pts[] = {NULL, NULL};
static float *fft_mags[] = {NULL, NULL};
static int fft_num = 0;
static int fft_mode = 0;
#define FFT_MAG (20.0f)

static void scribble_clicked_event_fft(GtkWidget *widget, GdkEventButton *event, gpointer data) {
	fft_mode ++;
	if(fft_mode > 2) fft_mode = 0;

	char *title = get_title();
	gtk_window_set_title(GTK_WINDOW(window), title);
	free(title);
}

static void scribble_da_event_fft(GtkWidget *widget, GdkEventButton *event, gpointer _data) {
	GdkRectangle update_rect;
	int i, c;
	short *data;
	complex_t *v;
	GdkPoint *p;

	if(pixmapFFT == NULL) return;

	update_rect.x = 0;
	update_rect.y = 0;
	update_rect.width = widget->allocation.width;
	update_rect.height = widget->allocation.height;

	for(c = 0; c < channels; c ++) {
		if(!fft_val[c]) fft_val[c] = (complex_t*) malloc(sizeof(complex_t) * fft_num);
		if(!fft_res[c]) fft_res[c] = (complex_t*) malloc(sizeof(complex_t) * fft_num);
	}

	data = bufs[bufpos];
	for(i = 0; i < fft_num * channels; i += channels) {
		for(c = 0; c < channels; c ++) {
			v = &fft_val[c][i/channels];
			v->real = (float) data[i + c] / 32767.0f;
			v->imag = 0;
		}
	}

	int h = widget->allocation.height / channels, h2 = h / 2;
	GdkGC *gcs[] = {widget->style->dark_gc[3], widget->style->light_gc[3]};
	int Ns[] = {0, 0};
	int N = ((fft_num / 2) * 5 / 6), j, n;
	const int NN = (fft_mode == 0 ? 50 : (fft_mode == 1 ? 200 : 300));
	for(c = 0; c < channels; c ++) {
		fft(fft_val[c], fft_num, fft_res[c]);
		if(fft_mode && !fft_pts[c]) {
			fft_pts[c] = (GdkPoint*) malloc(sizeof(GdkPoint) * (N + 2));
		}
		if(!fft_mags[c]) fft_mags[c] = (float*) malloc(sizeof(float) * N);

		float max = -10000, min = 10000, f;
		for(i = 0; i < N; i ++) {
			v = &fft_val[c][i];
			f = sqrtf(v->real * v->real + v->imag * v->imag);
			if(f > max) max = f;
			if(f < min) min = f;
			fft_mags[c][i] = f;
		}
		float F = max / 5.0;
		int nn = 0;
		for(i = 0; i < N - 1; i ++) {
			if(fft_mags[c][i] < F) {
				fft_mags[c][nn++] = fft_mags[c][i];
			}
		}
		const int n2 = nn / NN;
		f = 0;
		n = 0;
		j = 0;
		for(i = 0; i < nn; i ++) {
			f += fft_mags[c][i];
			n ++;
			if(n == n2) {
				n = 0;
				f /= (float) n2;
				if(f > FFT_MAG) f = FFT_MAG;
				
				fft_mags[c][j] = f / FFT_MAG;
				
				j ++;
				f = 0;
				if(j >= NN) break;
			}
		}
		Ns[c] = j;
		if(fft_mode) {
			double w = (double) widget->allocation.width / (double) j;
			for(i = 0; i < j; i ++) {
				p = &fft_pts[c][i];
				p->x = i * w;
				p->y = c * h + h - fft_mags[c][i] * h;
			}
			p = &fft_pts[c][j];
			p->x = widget->allocation.width;
			p->y = (c + 1) * h - 1;
			p ++;
			p->x = 0;
			p->y = (c + 1) * h;
		}
#ifdef FFT_DEBUG
		printf("[channel:%d] max: %f, min: %f, count: %d\n", c, max, min, j);
#endif
	}

	gdk_draw_rectangle(pixmapFFT, widget->style->white_gc, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	for(c = 0; c < channels; c ++) {
		if(fft_mode == 0) {
			float x = 0, stepX = (float) widget->allocation.width / (float) Ns[c];
			for(i = 0; i < Ns[c]; i ++) {
				n = h - fft_mags[c][i] * h;
				gdk_draw_rectangle(pixmapFFT, gcs[c], TRUE, x + stepX / 4, c * h + n, stepX / 2, h - n);
				x += stepX;
			}
		} else if(fft_mode == 1) {
			gdk_draw_polygon(pixmapFFT, gcs[c], TRUE, fft_pts[c], Ns[c] + 2);
		} else {
			gdk_draw_lines(pixmapFFT, gcs[c], fft_pts[c], Ns[c]);
		}
	}

	gdk_window_invalidate_rect(widget->window, &update_rect, FALSE);
}

static void scribble_delete_event(GtkWidget *widget, GdkEventButton *event, gpointer _data) {
	sigVolume = sigWave = sigFFT = NULL;
	gtk_main_quit();
}

static int video_is_press = 0;
static void scribble_button_press_event_video(GtkWidget *widget, GdkEventButton *event, gpointer _data) {
	touch_event_push('d', event->x, event->y);
	video_is_press = 1;
	// printf("mouse press: button = %d, type = %d, x = %f, y = %f\n", event->button, event->type, event->x, event->y);
}

static void scribble_motion_notify_event_video(GtkWidget *widget, GdkEventButton *event, gpointer _data) {

	if(video_is_press) {
		touch_event_push('m', event->x, event->y);
		
		// printf("motion x = %f, motion y = %f\n", event->x, event->y);
	}
}

static void scribble_button_release_event_video(GtkWidget *widget, GdkEventButton *event, gpointer _data) {
	if(video_is_press) {
		video_is_press = 0;
		touch_event_push('u', event->x, event->y);
		
		// printf("mouse release: button = %d, type = %d, x = %f, y = %f\n", event->button, event->type, event->x, event->y);
	}
}

typedef struct {
	volatile bool type;
	volatile uint16_t code;
} keycode_t;
#define KEY_SIZE 128
static keycode_t keys[KEY_SIZE];
static volatile uint32_t key_idx = 0;
static volatile uint32_t key_size = 0;

static int http_post(const char *func, const char *path, char *post, const char *result) {
	int fd, size, sz, ret = 0;
	char buf[2048];
	struct timeval tv;
	fd_set set;
	
	fd = sock_conn(func, 100);

	if(fd == 0) {
		memset(buf, 0, sizeof(buf));
		
		goto end;
	}
	
	size = 0;
	size += snprintf(buf + size, sizeof(buf), "POST %s%s HTTP/1.1\r\n", servpath, path);
	size += snprintf(buf + size, sizeof(buf), "Host: %s:%d\r\n", servhost, servport);
	size += snprintf(buf + size, sizeof(buf), "Connection: Close\r\n");
	size += snprintf(buf + size, sizeof(buf), "Accept: */*\r\n");
	size += snprintf(buf + size, sizeof(buf), "User-Agent: weditor for c language client\r\n");
	size += snprintf(buf + size, sizeof(buf), "Content-Type: application/x-www-form-urlencoded; charset=UTF-8\r\n");
	size += snprintf(buf + size, sizeof(buf), "Content-Length: %ld\r\n", strlen(post));
	size += snprintf(buf + size, sizeof(buf), "\r\n%s", post);
	
	// fprintf(stderr, "%s\n", buf);
	
	sz = 0;
loopsend:
	ret = send(fd, buf + sz, size - sz, 0);
	if(ret > 0) {
		sz += ret;
		
		if(!is_running) {
			ret = 0;
			goto err;
		}
		
		if(sz < size) goto loopsend;
	} else {
		ret = 0;
		goto err;
	}
	
	sz = 0;
	memset(buf, 0, sizeof(buf));
	
	{
		int flags = fcntl(fd, F_GETFL);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}
	
looprecv:
	do {
		tv.tv_sec = 0;
		tv.tv_usec = WS_SELECT_USEC;
		
		FD_ZERO(&set);
		FD_SET(fd, &set);
		
		ret = select(fd+1, &set, NULL, NULL, &tv);
	} while(ret == 0 && is_running);
	
	ret = recv(fd, buf + sz, sizeof(buf) - sz, 0);
	if(ret > 0) {
		// fwrite(buf + sz, 1, ret, stdout);
		sz += ret;
		
		char *ptr = strstr(buf, "\r\n\r\n");
		if(ptr) {
			return strstr(buf, result) ? 1 : -1;
		} else {
			if(!is_running) {
				ret = 0;
				goto err;
			}
			
			if(sz < sizeof(buf)) goto looprecv;
			else {
				fprintf(stderr, "[%s] buffer full\n", nowtime(buf, sizeof(buf)));
				ret = 0;
				goto err;
			}
		}
	}
err:
	//fwrite(buf, 1, sz, stderr);
	//fprintf(stderr, "\n");
	//fflush(stderr);
	close(fd);
end:
	return ret;
}

static bool is_ping = false;

static void *key_event_thread(void *arg) {
	keycode_t *key;
	int ret;
	char buf[32];
	double t = microtime() + 10.0, t2 = 0.0;
	bool is_conn = false;
	
	while(is_running) {
		usleep(10000);
		
		if(key_size) {
			key = &keys[key_idx];
			key_idx ++;
			key_size --;
			if(key_idx >= KEY_SIZE) key_idx = 0;

			if(key->type) printf("[%s] KEYCODE: %d %02x ...\n", nowtime(buf, sizeof(buf)), key->code, key->code);
			else printf("[%s] TEXT: %c ...\n", nowtime(buf, sizeof(buf)), key->code);
			
			if(key->type) {
				snprintf(buf, sizeof(buf), "serial=android:&key=%d", key->code);
				ret = http_post(__func__, "/api/v1/press", buf, "{\"ret\": true}");
			} else {
				snprintf(buf, sizeof(buf), "serial=android:&text=%%%02x", key->code);
				ret = http_post(__func__, "/api/v1/text", buf, "{\"ret\": true}");
			}
			if(ret) {
				if(key->type) printf("[%s] KEYCODE: %d %02x %s\n", nowtime(buf, sizeof(buf)), key->code, key->code, ret > 0 ? "OK" : "ERR");
				else printf("[%s] TEXT: %c %s\n", nowtime(buf, sizeof(buf)), key->code, ret > 0 ? "OK" : "ERR");
			}
			
			if(is_ping) t = microtime() + 10.0;
		} else if(is_ping && microtime() > t) {
			printf("[%s] PING ...\n", nowtime(buf, sizeof(buf)));
			ret = http_post(__func__, "/api/v1/ping", "serial=android:", "{\"ret\": \"pong\"}");
			if(ret) {
				printf("[%s] PING %s\n", nowtime(buf, sizeof(buf)), ret > 0 ? "OK" : "ERR");
			}
			t = microtime() + 10.0;
		} else if(microtime() > t2) {
			ret = http_post(__func__, "/api/v1/connect", "platform=Android&deviceUrl=", "\"success\": true");
			printf("[%s] Connect device %s\n", nowtime(buf, sizeof(buf)), ret > 0 ? "OK" : "ERR");
			is_connect = is_conn = ret > 0;
			if(ret > 0) t2 = microtime() + 30.0;
			else t2 = microtime() + 1.0;
		} else if(is_conn && !is_connect) t2 = 0;
	}
	pthread_exit(NULL);
}

static guint fullscreen_timer;
static gboolean fullscreen_func(gpointer data) {
	GtkWindow *win = GTK_WINDOW(window);

	gtk_timeout_remove(fullscreen_timer);

	gtk_window_fullscreen(win);
	gtk_container_set_border_width(GTK_CONTAINER(window), 0);
	gtk_widget_hide(audioFrame);
}

static gboolean scribble_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer _data) {
	bool type = true;
	uint16_t code = 0;
	
	// printf("%u %02x\n", event->hardware_keycode, event->hardware_keycode);

	switch(event->keyval) {
		case GDK_KEY_Escape:
			code = 4; // BACK
			break;
		case GDK_KEY_Home:
		case GDK_KEY_KP_Home:
			code = 3; // HOME
			break;
		case GDK_KEY_End:
		case GDK_KEY_KP_End:
			code = 26; // POWER
			break;
		case GDK_KEY_F1:
		case GDK_KEY_KP_F1:
			code = 209; // MUSIC
			break;
		case GDK_KEY_F2:
		case GDK_KEY_KP_F2:
			code = 25; // VOLUME_DOWN
			break;
		case GDK_KEY_F3:
		case GDK_KEY_KP_F3:
			code = 24; // VOLUME_UP
			break;
		case GDK_KEY_F4:
		case GDK_KEY_KP_F4:
			code = 164; // VOLUME_MUTE MUTE: 91
			break;
		case GDK_KEY_F5:
			code = 88; // MEDIA_PREVIOUS
			break;
		case GDK_KEY_F6:
			code = 87; // MEDIA_NEXT
			break;
		case GDK_KEY_F7:
			code = 126; // MEDIA_PLAY
			break;
		case GDK_KEY_F8:
			code = 86; // MEDIA_STOP
			break;
		case GDK_KEY_F11: {
			char buf[32];
			GtkWindow *win = GTK_WINDOW(window);
			printf("[%s] FullScreent %s\n", nowtime(buf, sizeof(buf)), is_fullscreen ? "OFF" : "ON");
			if(is_fullscreen) {
				is_fullscreen = false;
				gtk_window_unfullscreen(win);
				gtk_container_set_border_width(GTK_CONTAINER(window), 10);
				gtk_widget_show(audioFrame);
				gtk_window_set_resizable(win, FALSE);
			} else {
				is_fullscreen = true;
				gtk_window_set_resizable(win, TRUE);
				fullscreen_timer = gtk_timeout_add(10, fullscreen_func, NULL);
			}
			return FALSE;
		}
		case GDK_KEY_KP_Add: // NumKey -
			type = false;
			code = '+';
			break;
		case GDK_KEY_KP_Subtract: // NumKey +
			type = false;
			code = '-';
			break;
		case GDK_KEY_KP_0:
		case GDK_KEY_KP_1:
		case GDK_KEY_KP_2:
		case GDK_KEY_KP_3:
		case GDK_KEY_KP_4:
		case GDK_KEY_KP_5:
		case GDK_KEY_KP_6:
		case GDK_KEY_KP_7:
		case GDK_KEY_KP_8:
		case GDK_KEY_KP_9:
			type = false;
			code = '0' + (event->keyval - GDK_KEY_KP_0);
			break;
		case GDK_KEY_Delete:
			code = 112;
			break;
		case GDK_KEY_BackSpace:
			code = 67;
			break;
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			code = 66;
			break;
		case GDK_KEY_Up:
		case GDK_KEY_KP_Up:
			code = 19;
			break;
		case GDK_KEY_Down:
		case GDK_KEY_KP_Down:
			code = 20;
			break;
		case GDK_KEY_Left:
		case GDK_KEY_KP_Left:
			code = 21;
			break;
		case GDK_KEY_Right:
		case GDK_KEY_KP_Right:
			code = 22;
			break;
		case GDK_KEY_Tab:
		case GDK_KEY_KP_Tab:
			code = 61;
			break;
		case GDK_KEY_space:
		case GDK_KEY_KP_Space:
			code = 62;
			break;
		case '`':
		case '~':
		case '!':
		case '@':
		case '#':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
		case ')':
		case '-':
		case '_':
		case '+':
		case '=':
		case '[':
		case '{':
		case '}':
		case ']':
		case '\\':
		case '|':
		case ':':
		case ';':
		case '\'':
		case '"':
		case '/':
		case '?':
		case '.':
		case '>':
		case ',':
		case '<':
			type = false;
			code = event->keyval;
			break;
		default:
			if((event->keyval >= '0' && event->keyval <= '9') || (event->keyval >= 'a' && event->keyval <= 'z') || (event->keyval >= 'A' && event->keyval <= 'Z')) {
				type = false;
				code = event->keyval;
				break;
			}
			printf("KEYCODE %u %02x %u %02x\n", code, code, event->keyval, event->keyval);
			return FALSE;
	}
	if(code && key_size < KEY_SIZE) {
		keycode_t *k = &keys[(key_idx + key_size) % KEY_SIZE];
		k->type = type;
		k->code = code;
		key_size ++;
	
		return TRUE;
	} else {
		return FALSE;
	}
}

static char *get_title() {
	char *mode = NULL, *title = NULL;
	
	switch(fft_mode) {
		case 0:
			mode = "Rectangle";
			break;
		case 1:
			mode = "Polygon";
			break;
		case 2:
		default:
			mode = "Lines";
			break;
	}
	
	asprintf(&title, "Sound FFT - %s:%d - %s - %d fps", servhost, servport, mode, video_fps);
	if(sound_loading) asprintf(&title, "%s - Sound loading", title);
	if(video_loading) asprintf(&title, "%s - Video loading", title);
	if(touch_event_loading) asprintf(&title, "%s - Touch loading", title);
	
	int i;
	asprintf(&title, "%s - ATX", title);
	for(i = 0; i < 3; i ++) asprintf(&title, "%s %.1lf%%", title, stats[i]);
	asprintf(&title, "%s - WDR", title);
	for(i = 3; i < 6; i ++) asprintf(&title, "%s %.1lf%%", title, stats[i]);
	
	return title;
}

static void gtk_begin() {
	GtkWidget *vbox;
	GtkWidget *frameVolume, *daVolume;
	GtkWidget *frameWave, *daWave;
	GtkWidget *frameFFT, *daFFT;
	
	g_signal_new("da_event", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	{
		char *title = get_title();
		gtk_window_set_title(GTK_WINDOW(window), title);
		free(title);
	}
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(scribble_delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(scribble_key_press_event), NULL);
	
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	
	sizeFrame = gtk_hbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(sizeFrame), 0);
	gtk_container_add(GTK_CONTAINER(window), sizeFrame);

	audioFrame = gtk_vbox_new(FALSE, 10);
	gtk_widget_set_size_request(audioFrame, 400, 200);
	gtk_container_set_border_width(GTK_CONTAINER(audioFrame), 0);
	gtk_box_pack_start(GTK_BOX(sizeFrame), audioFrame, TRUE, TRUE, 0);
	
#ifdef VIDEO_IMAGE
	videoImage = gtk_image_new();
	videoFrame = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(videoFrame), videoImage);
#else
	videoFrame = gtk_drawing_area_new();
#endif
	gtk_widget_set_size_request(videoFrame, 800, 200);
	gtk_box_pack_end(GTK_BOX(sizeFrame), videoFrame, TRUE, TRUE, 0);

	{
		GtkObject *obj = (GtkObject*) G_OBJECT(videoFrame);
		
		g_signal_connect(obj, "button-press-event", G_CALLBACK(scribble_button_press_event_video), NULL);
		g_signal_connect(obj, "motion-notify-event", G_CALLBACK(scribble_motion_notify_event_video), NULL);
		g_signal_connect(obj, "button-release-event", G_CALLBACK(scribble_button_release_event_video), NULL);
		
		gtk_widget_add_events(videoFrame, GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	}

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
	gtk_box_pack_start(GTK_BOX(audioFrame), vbox, TRUE, TRUE, 0);

	frameVolume = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameVolume), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), frameVolume, TRUE, TRUE, 0);

	daVolume = gtk_drawing_area_new();
	gtk_widget_set_size_request(daVolume, 400, 10);
	gtk_container_add(GTK_CONTAINER(frameVolume), daVolume);

	sigVolume = (GtkObject*) G_OBJECT(daVolume);
	g_signal_connect(sigVolume, "expose_event", G_CALLBACK(scribble_expose_event_volume), NULL);
	g_signal_connect(sigVolume, "configure_event", G_CALLBACK(scribble_configure_event_volume), NULL);
	g_signal_connect(sigVolume, "da_event", G_CALLBACK(scribble_da_event_volume), NULL);

	gtk_widget_add_events(daVolume, GDK_LEAVE_NOTIFY_MASK);

	frameWave = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameWave), GTK_SHADOW_IN);
	gtk_box_pack_end(GTK_BOX(vbox), frameWave, TRUE, TRUE, 0);

	daWave = gtk_drawing_area_new();
	gtk_widget_set_size_request(daWave, 400, 60);
	gtk_container_add(GTK_CONTAINER(frameWave), daWave);

	sigWave = (GtkObject*) G_OBJECT(daWave);
	g_signal_connect(sigWave, "expose_event", G_CALLBACK(scribble_expose_event_wave), NULL);
	g_signal_connect(sigWave, "configure_event", G_CALLBACK(scribble_configure_event_wave), NULL);
	g_signal_connect(sigWave, "da_event", G_CALLBACK(scribble_da_event_wave), NULL);

	gtk_widget_add_events(daWave, GDK_LEAVE_NOTIFY_MASK);

	frameFFT = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameFFT), GTK_SHADOW_IN);
	gtk_box_pack_end(GTK_BOX(audioFrame), frameFFT, TRUE, TRUE, 0);

	daFFT = gtk_drawing_area_new();
	gtk_widget_set_size_request(daFFT, 400, 40);
	gtk_container_add(GTK_CONTAINER(frameFFT), daFFT);

	sigFFT = (GtkObject*) G_OBJECT(daFFT);
	g_signal_connect(sigFFT, "expose_event", G_CALLBACK(scribble_expose_event_fft), NULL);
	g_signal_connect(sigFFT, "configure_event", G_CALLBACK(scribble_configure_event_fft), NULL);
	g_signal_connect(sigFFT, "button_press_event", G_CALLBACK(scribble_clicked_event_fft), NULL);
	g_signal_connect(sigFFT, "da_event", G_CALLBACK(scribble_da_event_fft), NULL);

	gtk_widget_add_events(daFFT, GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);

	gtk_widget_show_all(window);
}

static void gtk_end(void) {
	for(int i = 0; i < 2; i ++) {
		if(points[i]) free(points[i]);
		if(fft_val[i]) free(fft_val[i]);
		if(fft_res[i]) free(fft_res[i]);
		if(fft_pts[i]) free(fft_pts[i]);
		if(fft_mags[i]) free(fft_mags[i]);
	}
}

void sig_handle(int sig) {
	// fprintf(stderr, "sig: %d\n", sig);
	if(is_running) {
		is_running = false;
		gdk_threads_enter();
		gtk_main_quit();
		gdk_threads_leave();
	}
	fprintf(stderr, "\r");
}

static void *video_fps_thread(void *arg) {
	char *title;
	while(is_running) {
		video_fps = video_frames;
		video_frames = 0;
		title = get_title();
		gdk_threads_enter();
		if(window) gtk_window_set_title(GTK_WINDOW(window), title);
		gdk_threads_leave();
		free(title);

		sleep(1);
	}
}

int main(int argc, char *argv[]) {
	int ret, c;
	
	while((c = getopt(argc, argv, "H:P:c:p:ih?")) != -1) {
        switch(c) {
        	case 'H':
        		servhost = optarg;
        		break;
            case 'P':
                servport = atoi(optarg);
                break;
            case 'c':
                channels = atoi(optarg);
                break;
            case 'p':
                servpath = optarg;
                break;
            case 'i':
                is_ping = true;
                break;
            case '?':
            case 'h':
            default:
                fprintf(stderr,
                    "Usage: %s [-H <host>] [-P <port>] [-c <channels>] [-p <basepath>] [-i] [-h|-?]\n"
                    "  -H <host>     Server host(default: %s)\n"
                    "  -P <port>     Server port(default: %d)\n"
                    "  -c <channels> Audio channels(default: %d)\n"
                    "  -p <basepath> Show verbose(default: %s)\n"
                    "  -i            Ping(default: false)\n"
                    "  -h,-?         This help\n"
                    "Press Key:\n"
                    "  F1     KEYCODE_MUSIC\n"
                    "  F2     KEYCODE_VOLUME_UP\n"
                    "  F3     KEYCODE_VOLUME_DOWN\n"
                    "  F4     KEYCODE_VOLUME_MUTE\n"
                    "  F5     KEYCODE_MEDIA_PREVIOUS\n"
                    "  F6     KEYCODE_MEDIA_NEXT\n"
                    "  F7     KEYCODE_MEDIA_PLAY\n"
                    "  F8     KEYCODE_MEDIA_STOP\n"
                    "  F11    FullScreen\n"
                    "  Home   KEYCODE_HOME\n"
                    "  End    KEYCODE_POWER\n"
                    "  Esc    KEYCODE_BACK\n"
                    , argv[0], servhost, servport, channels, servpath
                );
                return 0;
        }
    }
	
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(servhost);
	servaddr.sin_port = htons(servport);

	ret = set_hardware_params();
	if (ret < 0) {
		fprintf(stderr, "set_hardware_params error\n");
		return -1;
	}

	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);
	signal(SIGPIPE, SIG_IGN);

	gdk_threads_init();

	fft_num = powf(2, floorf(log2f(g_frames)));

	sem_init(&play_sem, 0, 0);
	pthread_mutex_init(&touch_lock, NULL);
	
	memset(bufs, 0, sizeof(bufs));
	bufsize = g_frames * channels * 2;
	for(int i = 0; i < BUFSIZE; i ++) {
		bufs[i] = (short*) malloc(bufsize);
		if(bufs[i]) {
			memset(bufs[i], 0, bufsize);
		} else {
			fprintf(stderr, "malloc failure\n");
			return -1;
		}
	}

	gtk_init(&argc, &argv);

	gtk_begin();

	pthread_t play_tid = 0, sound_tid = 0, video_tid = 0, fps_tid = 0, touch_tid, key_tid;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if(ret) {
		fprintf(stderr, "pthread_attr_setdetachstate failure: %d", ret);
		return -1;
	}
	ret = pthread_create(&play_tid, &attr, play_sound_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		goto end;
	}

	ret = pthread_create(&sound_tid, &attr, conn_sound_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		goto end;
	}
	
	ret = pthread_create(&video_tid, &attr, conn_video_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		goto end;
	}
	
	ret = pthread_create(&fps_tid, &attr, video_fps_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		goto end;
	}
	
	ret = pthread_create(&touch_tid, &attr, touch_event_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		goto end;
	}
	
	ret = pthread_create(&key_tid, &attr, key_event_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		goto end;
	}
	
	sem_post(&play_sem);
	
	gdk_threads_enter();
	gtk_main();
	gtk_end();
	gdk_threads_leave();

end:
	is_running = false;
	sem_post(&play_sem);
	
	snd_pcm_drain(gp_handle);
	snd_pcm_reset(gp_handle);
	snd_pcm_close(gp_handle);

	if(pthread_join(play_tid, NULL)) {
		perror("pthread_join failure");
	}
	if(pthread_join(sound_tid, NULL)) {
		perror("pthread_join failure");
	}
	if(pthread_join(video_tid, NULL)) {
		perror("pthread_join failure");
	}
	if(pthread_join(fps_tid, NULL)) {
		perror("pthread_join failure");
	}
	if(pthread_join(touch_tid, NULL)) {
		perror("pthread_join failure");
	}
	if(pthread_join(key_tid, NULL)) {
		perror("pthread_join failure");
	}
	pthread_attr_destroy(&attr);

	sem_destroy(&play_sem);
	pthread_mutex_destroy(&touch_lock);

	for(int i = 0; i < BUFSIZE; i ++) {
		if(bufs[i]) free(bufs[i]);
	}

	fprintf(stderr, "Exited\n");
	return 0;
}

