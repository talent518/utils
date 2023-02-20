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

static snd_pcm_t *gp_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
static snd_pcm_hw_params_t *gp_params;  //设置流的硬件参数
static snd_pcm_uframes_t g_frames; //snd_pcm_uframes_t其实是unsigned long类型

static const int sample_rate = 44100, channels = 2, format_size = 16;

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

static sem_t sem;
#define SIZE 128
static short *bufs[SIZE];
static int bufsize;
static int bufpos = 0;

typedef struct {
	int sum;
	short db;
} calc_t;

static GtkWidget *window = NULL;
static GdkPixmap *pixmapVolume = NULL, *pixmapWave = NULL, *pixmapFFT = NULL;
static GtkObject *sigVolume = NULL, *sigWave = NULL, *sigFFT = NULL;

static int playpos = -1;
static void *play_sound_thread(void *arg) {
	const int MAX_QUE = 20; // 20 frames per second
	int ret;
	
	snd_pcm_pause(gp_handle, 0);
	sem_wait(&sem);
	while(is_running) {
		playpos ++;
		if(playpos >= SIZE) playpos = 0;

		if(!sem_getvalue(&sem, &ret) && ret > MAX_QUE) {
			sem_wait(&sem);
			continue;
		}
		
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

		sem_wait(&sem);
	}
	pthread_exit(NULL);
}

static void ws_setopt(int s, int send_timeout, int recv_timeout, int send_buffer, int recv_buffer) {
	setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(int)); // 发送超时
	setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(int)); // 接收超时

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

static char *servhost;
static int servport;
static struct sockaddr_in servaddr;
static int ws_conn(const char *func, const char *path, int timeout) {
	char buf[2048];
	int size = 0, ret, sz = 0;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	int flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	
	if(connect(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		if(EINPROGRESS == errno) {
			struct timeval tv;
			fd_set set;
			
			tv.tv_sec = timeout;
			tv.tv_usec = 0;
			
			FD_ZERO(&set);
			FD_SET(fd, &set);
			
			ret = select(fd+1, NULL, &set, NULL, &tv);
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
		fprintf(stderr, "[%s] connect %s success\n", nowtime(buf, sizeof(buf)), func);
		
		size += snprintf(buf + size, sizeof(buf), "GET %s HTTP/1.1\r\n", path);
		size += snprintf(buf + size, sizeof(buf), "Host: %s:%d\r\n", servhost, servport);
		size += snprintf(buf + size, sizeof(buf), "Connection: Upgrade\r\n");
		size += snprintf(buf + size, sizeof(buf), "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n");
		size += snprintf(buf + size, sizeof(buf), "Sec-WebSocket-Key: uQQ1BAbtsumAi1y7Mu4spQ==\r\n");
		size += snprintf(buf + size, sizeof(buf), "Sec-WebSocket-Version: 13\r\n");
		size += snprintf(buf + size, sizeof(buf), "Upgrade: websocket\r\n");
		size += snprintf(buf + size, sizeof(buf), "User-Agent: weditor for c language client\r\n");
		size += snprintf(buf + size, sizeof(buf), "\r\n");
		
		fcntl(fd, F_SETFL, flags);
		
	loopsend:
		ret = send(fd, buf + sz, size - sz, 0);
		if(ret > 0) {
			sz += ret;
			if(sz < size) goto loopsend;
		} else {
			goto err;
		}
		
		sz = 0;
		memset(buf, 0, sizeof(buf));
		
	looprecv:
		ret = recv(fd, buf + sz, sizeof(buf) - sz, 0);
		if(ret > 0) {
			sz += ret;
			
			char *ptr = strstr(buf, "\r\n\r\n");
			if(!ptr) {
				if(sz < sizeof(buf)) goto looprecv;
				else {
					fprintf(stderr, "[%s] buffer full\n", nowtime(buf, sizeof(buf)));
					goto err;
				}
			}
			
			if(strncmp(buf, "HTTP/1.1 101 ", sizeof("HTTP/1.1 101 ") - 1)) goto err;
			
			fprintf(stderr, "[%s] websocket %s connected\n", nowtime(buf, sizeof(buf)), func);
		} else {
		err:
			close(fd);
			fd = 0;
		}
		return fd;
	}
}

static int ws_send(int fd, int ctl, char *buf, int size) {
	char *ptr = (char*) malloc(size + 14), *mask = "1234";
	int i = 0, j, sz, ret;
	ptr[i++] = ctl;
	if(size <= 125) {
		ptr[i++] = size;
	} else if(size > 125 && size < 65536) {
		ptr[i++] = 126;
		ptr[i++] = size >> 8;
		ptr[i++] = size;
	} else {
		ptr[i++] = 127;
		ptr[i++] = 0;
		ptr[i++] = 0;
		ptr[i++] = 0;
		ptr[i++] = 0;
		for(j = 24; j >= 0; j -= 8) {
			ptr[i++] = size >> j;
		}
	}
	
	memcpy(ptr + i, mask, 4);
	i += 4;
	
	j = 0;
	while(size > 0) {
		ptr[i++] = buf[j] ^ mask[j % 4];
	}
	
	sz = 0;
loopsend:
	ret = send(fd, ptr + sz, i - sz, 0);
	if(ret > 0) {
		sz += ret;
		
		if(sz < i) goto loopsend;
		
		free(ptr);
		return 1;
	} else {
		free(ptr);
		return 0;
	}
}

static char *ws_recv(int fd, int *size, char **prev_ptr, int *prev_n, const char *func) {
	char *ptr = NULL;
	int ret, i, n = 0, sz, ctl;
	unsigned char buf[1024] = {0};
	char mask[8];
	
begin:
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

	do {
		struct timeval tv;
		fd_set set;
		
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		FD_ZERO(&set);
		FD_SET(fd, &set);
		
		ret = select(fd+1, &set, NULL, NULL, &tv);
	} while(ret == 0 && is_running);
	
	if(ret < 0 || !is_running) goto err;
	
head:
	ret = recv(fd, buf + n, sizeof(buf) - n, 0);
	if(ret > 0) {
		n += ret;
		
	#ifdef RECV_MASK
		if(n < 6) goto head;
		
		if(!(buf[0] & 0x80) || (buf[0] & 0x70) || !(buf[1] & 0x80)) { // !FIN || (RSV1 || RSV2 || RSV3) || !Mask
			fprintf(stderr, "[%s] websocket %s protocol type error\n", nowtime(buf, sizeof(buf)), func);
			goto err;
		}
		
		ctl = buf[0] & 0xf;
		sz = buf[1] & 0x7f;
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
			memcpy(mask, buf + 2, 4);
			*size = sz;
			i = 6;
		}
	#else
		if(n < 2) goto head;

		if(!(buf[0] & 0x80) || (buf[0] & 0x70) || (buf[1] & 0x80)) { // !FIN || (RSV1 || RSV2 || RSV3) || Mask
			fprintf(stderr, "[%s] websocket %s protocol type error: %02x %02x\n", nowtime(buf, sizeof(buf)), func, buf[0], buf[1]);
			goto err;
		}
		
		ctl = buf[0] & 0xf;
		sz = buf[1] & 0x7f;
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
	#endif
		
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
			memcpy(ptr, buf + i, n);
			
			while(n < sz) {
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
		
		if((buf[1] & 0x80)) { // Mask
			for(i = 0; i < sz; i++) {
				ptr[i] ^= mask[i % 4];
			}
		}
		
		switch(ctl) {
			case 0x1: // text
			case 0x2: // binary
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
				if(!ws_send(fd, 0x8a, ptr, sz)) goto err;
				goto gobegin;
			case 0xA: // pong
				fprintf(stderr, "[%s] websocket %s pong: %s\n", nowtime(buf, sizeof(buf)), func, ptr);
				
				gobegin:
				if(ptr) {
					free(ptr);
					ptr = NULL;
				}
				n = 0;
				sz = 0;
				goto begin;
				break;
			default:
				fprintf(stderr, "unknown control code: %d\n", ctl);
				goto err;
				break;
		}
	} else {
		char *err = strerror(errno);
		fprintf(stderr, "[%s] websocket %s protocol head error: %s\n", nowtime(buf, sizeof(buf)), func, err);
	}
	
	return ptr;

err:
	if(ptr) {
		free(ptr);
		ptr = NULL;
	}
	
	return NULL;
}

static void *conn_sound_thread(void *arg) {
	const int DELAY = 5;
	int fd = 0, sz = 0, n = 0, delay = 0, i;
	char *ptr, *old = NULL;
	
	while(is_running) {
		if(fd) {
			ptr = ws_recv(fd, &sz, &old, &n, __func__);
			if(ptr) {
				if(sz != bufsize) {
					fprintf(stderr, "sound buffer size %d is not equals %d\n", sz, bufsize);
				} else {
					bufpos ++;
					if(bufpos >= SIZE) bufpos = 0;
					
					memcpy(bufs[bufpos], ptr, sz);

					if(delay > DELAY) {
						sem_post(&sem);
					} else if(delay < DELAY) {
						delay ++;
					} else {
						for(i = 0; i < delay; i ++) {
							sem_post(&sem);
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
				
				free(ptr);
			} else {
				close(fd);
				fd = 0;
				if(delay < DELAY) {
					for(i = 0; i < delay; i ++) {
						sem_post(&sem);
					}
				}
				delay = 0;
			}
		} else {
			fd = ws_conn(__func__, "/ws/v1/minisound", 5);
		}
	}
	
	if(fd) close(fd);
	if(old) free(old);

	pthread_exit(NULL);
}

static GtkWidget *videoFrame = NULL, *sizeFrame = NULL;
static GtkImage *videoImg = NULL;
static int video_fps = 0, video_frames = 0;

static void *conn_video_thread(void *arg) {
	int fd = 0, sz = 0, n = 0;
	char *ptr, *old = NULL;
	
	while(is_running) {
		if(fd) {
			ptr = ws_recv(fd, &sz, &old, &n, __func__);
			if(ptr) {
				if(is_running) {
					gdk_threads_enter();
					video_frames ++;
					if(videoImg) {
						GdkPixbufLoader *loader = gdk_pixbuf_loader_new_with_mime_type("image/jpeg", NULL);
						GdkPixbuf *pixbuf;

						gdk_pixbuf_loader_write(loader, ptr, sz, NULL);
						if(gdk_pixbuf_loader_close(loader, NULL)) {
							pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
							if(pixbuf) {
								if(sizeFrame) gtk_widget_set_size_request(sizeFrame, 400, gdk_pixbuf_get_height(pixbuf));
								if(videoFrame) gtk_widget_set_size_request(videoFrame, gdk_pixbuf_get_width(pixbuf), gdk_pixbuf_get_height(pixbuf));
								if(videoImg) gtk_image_set_from_pixbuf(videoImg, pixbuf);
							}
						}
						g_object_unref(loader);
					}
					gdk_threads_leave();
				}
				
				free(ptr);
				ptr = NULL;
			} else {
				close(fd);
			}
		} else {
			fd = ws_conn(__func__, "/ws/v1/minicap?deviceId=android:", 5);
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

	// printf("%s:%d %d\n", __func__, __LINE__, pos);

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

	//printf("%s", nowtime(buf, sizeof(buf)));
	for(i = 0; i < channels; i++) {
		dBs[i].db = dBs[i].sum * 100.0 / (g_frames * 32767.0);
		if(dBs[i].db > 100) dBs[i].db = 100;
		// printf("%9d", dBs[i].db);
	}
	// printf("\n");

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

	// printf("%s:%d %d\n", __func__, __LINE__, pos);

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

	// printf("%s:%d %d\n", __func__, __LINE__, pos);

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

static char *get_title() {
	char *mode = NULL;
	
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
	
	char *title = NULL;
	asprintf(&title, "Sound FFT - %s:%d - %s - %d", servhost, servport, mode, video_fps);
	
	return title;
}

static void gtk_begin() {
	GtkWidget *vbox, *vbox2, *sizeFrame;
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
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(scribble_delete_event), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);
	
	sizeFrame = gtk_hbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(sizeFrame), 0);
	gtk_container_add(GTK_CONTAINER(window), sizeFrame);

	vbox2 = gtk_vbox_new(FALSE, 10);
	gtk_widget_set_size_request(vbox2, 400, 200);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 0);
	gtk_box_pack_start(GTK_BOX(sizeFrame), vbox2, TRUE, TRUE, 0);
	
	videoFrame = gtk_image_new();
	gtk_widget_set_size_request(videoFrame, 200, 200);
	gtk_box_pack_start(GTK_BOX(sizeFrame), videoFrame, TRUE, TRUE, 0);
	
	videoImg = GTK_IMAGE(videoFrame);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
	gtk_box_pack_start(GTK_BOX(vbox2), vbox, TRUE, TRUE, 0);

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

	gtk_widget_set_events(daVolume, gtk_widget_get_events(daVolume) | GDK_LEAVE_NOTIFY_MASK);

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

	gtk_widget_set_events(daWave, gtk_widget_get_events(daWave) | GDK_LEAVE_NOTIFY_MASK);

	frameFFT = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameFFT), GTK_SHADOW_IN);
	gtk_box_pack_end(GTK_BOX(vbox2), frameFFT, TRUE, TRUE, 0);

	daFFT = gtk_drawing_area_new();
	gtk_widget_set_size_request(daFFT, 400, 40);
	gtk_container_add(GTK_CONTAINER(frameFFT), daFFT);

	sigFFT = (GtkObject*) G_OBJECT(daFFT);
	g_signal_connect(sigFFT, "expose_event", G_CALLBACK(scribble_expose_event_fft), NULL);
	g_signal_connect(sigFFT, "configure_event", G_CALLBACK(scribble_configure_event_fft), NULL);
	g_signal_connect(sigFFT, "button_press_event", G_CALLBACK(scribble_clicked_event_fft), NULL);
	g_signal_connect(sigFFT, "da_event", G_CALLBACK(scribble_da_event_fft), NULL);

	gtk_widget_set_events(daFFT, gtk_widget_get_events(daFFT) | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);

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
	int ret;
	if (argc < 2) {
		fprintf(stderr, "usage: %s <weditor ipv4 address>\n", argv[0]);
		return -1;
	}
	
	servhost = argv[1];
	servport = (argc > 2 ? atoi(argv[2]) : 17310);
	
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

	sem_init(&sem, 0, 0);
	memset(bufs, 0, sizeof(bufs));
	bufsize = g_frames * channels * 2;
	for(int i = 0; i < SIZE; i ++) {
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

	pthread_t play_tid = 0, sound_tid = 0, video_tid = 0, fps_tid = 0;
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
	} else {
		sem_post(&sem);
		
		gdk_threads_enter();
		gtk_main();
		gdk_threads_leave();
	}

end:
	is_running = false;
	sem_post(&sem);
	
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
	
	pthread_attr_destroy(&attr);
	
	gtk_end();

	sem_destroy(&sem);

	snd_pcm_drain(gp_handle);
	snd_pcm_close(gp_handle);

	for(int i = 0; i < SIZE; i ++) {
		if(bufs[i]) free(bufs[i]);
	}

	fprintf(stderr, "Exited\n");
	return 0;
}

