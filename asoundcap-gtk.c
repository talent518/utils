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

#include <gtk/gtk.h>

#define gc_fg_new_with_rgb(draw, gc, r, g, b) { \
	GdkColor c; \
	\
	c.red = r; \
	c.green = g; \
	c.blue = b; \
	\
	gc = gdk_gc_new(draw); \
	gdk_gc_set_rgb_fg_color(gc, &c); \
}

snd_pcm_t *gp_handle;  //调用snd_pcm_open打开PCM设备返回的文件句柄，后续的操作都使用是、这个句柄操作这个PCM设备
snd_pcm_hw_params_t *gp_params;  //设置流的硬件参数
snd_pcm_uframes_t g_frames; //snd_pcm_uframes_t其实是unsigned long类型
int sample_rate = 0, play_ch = 2;
const int format_size = 16;
char *name = "default";

int set_hardware_params() {
	int rc;

	/* Open PCM device for playback */
	rc = snd_pcm_open(&gp_handle, name, SND_PCM_STREAM_CAPTURE, 0);
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
	rc = snd_pcm_hw_params_set_channels(gp_handle, gp_params, play_ch);
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
	gtk_main_quit();
	fprintf(stderr, "\r");
}

static sem_t sem;
#define PCM_PLAY_BUF_SIZE 32
static void *pcm_play_buf_ptr = NULL;
static short *pcm_play_bufs[PCM_PLAY_BUF_SIZE];
static int pcm_play_buf_pos = -1;

typedef struct {
	int sum;
	short db;
} calc_t;

static GtkWidget *window = NULL;
static GdkPixmap *pixmapVolume = NULL, *pixmapWave = NULL, *pixmapFFT = NULL;
static GtkObject *sigVolume = NULL, *sigWave = NULL, *sigFFT = NULL;

static void *calc_thread(void *arg) {
	sem_wait(&sem);
	while(is_running) {
		pcm_play_buf_pos ++;
		if(pcm_play_buf_pos >= PCM_PLAY_BUF_SIZE) pcm_play_buf_pos = 0;

		gdk_threads_enter();
		gtk_signal_emit_by_name(sigVolume, "da_event");
		gtk_signal_emit_by_name(sigWave, "da_event");
		gtk_signal_emit_by_name(sigFFT, "da_event");
		gdk_threads_leave();

		sem_wait(&sem);
	}
	pthread_exit(NULL);
}

static void *cap_thread(void *arg) {
	int ret, pos = -1;

	snd_pcm_pause(gp_handle, 0);
	snd_pcm_prepare(gp_handle);
	while(is_running) {
		pos ++;
		if(pos >= PCM_PLAY_BUF_SIZE) pos = 0;

	prepare:
		ret = snd_pcm_readi(gp_handle, pcm_play_bufs[pos], g_frames);
		if (ret == -EPIPE) {
			fprintf(stderr, "read pipe\n");
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
		} else {
			if(!sigVolume || !pixmapVolume || !sigWave || !pixmapWave || !sigFFT || !pixmapFFT) {
				pos = -1;
			} else {
				sem_post(&sem);
			}
		}
	}

	pthread_exit(NULL);
}

static GdkGC *gc_volume_bg = NULL;
static GdkGC *gc_volume_left_cur = NULL, *gc_volume_left_max = NULL;
static GdkGC *gc_volume_right_cur = NULL, *gc_volume_right_max = NULL;

static gboolean scribble_configure_event_volume(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	GdkColor color;

	if(pixmapVolume) g_object_unref(G_OBJECT(pixmapVolume));
	if(gc_volume_bg) g_object_unref(G_OBJECT(gc_volume_bg));
	if(gc_volume_left_cur) g_object_unref(G_OBJECT(gc_volume_left_cur));
	if(gc_volume_left_max) g_object_unref(G_OBJECT(gc_volume_left_max));
	if(gc_volume_right_cur) g_object_unref(G_OBJECT(gc_volume_right_cur));
	if(gc_volume_right_max) g_object_unref(G_OBJECT(gc_volume_right_max));

	pixmapVolume = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);

	gc_fg_new_with_rgb(pixmapVolume, gc_volume_bg, 0xffff, 0xffff, 0xffff);
	gc_fg_new_with_rgb(pixmapVolume, gc_volume_left_cur, 0xffff, 0x3333, 0x0000);
	gc_fg_new_with_rgb(pixmapVolume, gc_volume_left_max, 0x9999, 0x3333, 0x0000);
	gc_fg_new_with_rgb(pixmapVolume, gc_volume_right_cur, 0x3333, 0xffff, 0x0000);
	gc_fg_new_with_rgb(pixmapVolume, gc_volume_right_max, 0x3333, 0x9999, 0x0000);

	return TRUE;
}

static gboolean scribble_expose_event_volume(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	gdk_draw_drawable(widget->window, gc_volume_bg, pixmapVolume, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

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

	dBs = (calc_t*) malloc(play_ch * sizeof(calc_t));

	data = pcm_play_bufs[pcm_play_buf_pos];
	for(i = 0; i < play_ch; i++) dBs[i].sum = 0;
	for(i = 0; i < g_frames * play_ch; i += play_ch) {
		for(c = 0; c < play_ch; c ++) {
			m = abs(data[i + c]);
			dBs[c].sum += m;
			if(m > maxs[c]) {
				maxs[c] = m;
			}
		}
	}

	for(i = 0; i < play_ch; i++) {
		dBs[i].db = dBs[i].sum * 100.0 / (g_frames * 32767.0);
		if(dBs[i].db > 100) dBs[i].db = 100;
	}

	gdk_draw_rectangle(pixmapVolume, gc_volume_bg, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	gint h = widget->allocation.height / play_ch;
	GdkGC *gc_curs[] = {gc_volume_left_cur, gc_volume_right_cur};
	GdkGC *gc_maxs[] = {gc_volume_left_max, gc_volume_right_max};
	for(i = 0; i < play_ch; i++) {
		gdk_draw_rectangle(pixmapVolume, gc_curs[i], TRUE, 0, i * h, widget->allocation.width * dBs[i].db / 100, h);
		gdk_draw_rectangle(pixmapVolume, gc_maxs[i], TRUE, (widget->allocation.width * maxs[i] / 32767) - 2, i * h, 4, h);
	}

	gdk_window_invalidate_rect(widget->window, &update_rect, FALSE);

	free(dBs);
}

static GdkGC *gc_wave_bg = NULL;
static GdkGC *gc_wave_left = NULL;
static GdkGC *gc_wave_right = NULL;

static gboolean scribble_configure_event_wave(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	if(pixmapWave) g_object_unref(G_OBJECT(pixmapWave));
	if(gc_wave_bg) g_object_unref(G_OBJECT(gc_wave_bg));
	if(gc_wave_left) g_object_unref(G_OBJECT(gc_wave_left));
	if(gc_wave_right) g_object_unref(G_OBJECT(gc_wave_right));

	pixmapWave = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);

	gc_fg_new_with_rgb(pixmapWave, gc_wave_bg, 0xffff, 0xffff, 0xffff);
	gc_fg_new_with_rgb(pixmapWave, gc_wave_left, 0xffff, 0x3333, 0x0000);
	gc_fg_new_with_rgb(pixmapWave, gc_wave_right, 0x3333, 0xffff, 0x0000);

	return TRUE;
}

static gboolean scribble_expose_event_wave(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	gdk_draw_drawable(widget->window, gc_wave_bg, pixmapWave, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

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

	gint h = widget->allocation.height / play_ch, h2 = h / 2;
	double w = (double) widget->allocation.width / (double) g_frames;
	for(i = 0; i < play_ch; i ++) {
		if(!points[i]) points[i] = (GdkPoint*) malloc(sizeof(GdkPoint) * g_frames);
	}
	data = pcm_play_bufs[pcm_play_buf_pos];
	GdkPoint *p;
	for(i = 0; i < g_frames * play_ch; i += play_ch) {
		for(c = 0; c < play_ch; c ++) {
			int x = i/play_ch;
			p = &points[c][x];
			p->x = x * w;
			p->y = c * h + h2 - data[i + c] * h2 / 32767;
		}
	}

	gdk_draw_rectangle(pixmapWave, gc_wave_bg, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	GdkGC *gcs[] = {gc_wave_left, gc_wave_right};
	for(i = 0; i < play_ch; i ++) {
		if(i % 2) gdk_draw_line(pixmapWave, widget->style->black_gc, 0, i * h, widget->allocation.width, h);
		gdk_draw_lines(pixmapWave, gcs[i], points[i], g_frames);
	}

	gdk_window_invalidate_rect(widget->window, &update_rect, FALSE);
}

static GdkGC *gc_fft_bg = NULL;
static GdkGC *gc_fft_left = NULL;
static GdkGC *gc_fft_right = NULL;

static gboolean scribble_configure_event_fft(GtkWidget *widget, GdkEventConfigure *event, gpointer data) {
	GdkColor color;

	if(pixmapFFT) g_object_unref(G_OBJECT(pixmapFFT));
	if(gc_fft_bg) g_object_unref(G_OBJECT(gc_fft_bg));
	if(gc_fft_left) g_object_unref(G_OBJECT(gc_fft_left));
	if(gc_fft_right) g_object_unref(G_OBJECT(gc_fft_right));

	pixmapFFT = gdk_pixmap_new(widget->window, widget->allocation.width, widget->allocation.height, -1);

	gc_fg_new_with_rgb(pixmapFFT, gc_fft_bg, 0xffff, 0xffff, 0xffff);
	gc_fg_new_with_rgb(pixmapFFT, gc_fft_left, 0xffff, 0x3333, 0x0000);
	gc_fg_new_with_rgb(pixmapFFT, gc_fft_right, 0x3333, 0xffff, 0x0000);

	return TRUE;
}

static gboolean scribble_expose_event_fft(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	gdk_draw_drawable(widget->window, gc_fft_bg, pixmapFFT, event->area.x, event->area.y, event->area.x, event->area.y, event->area.width, event->area.height);

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

	char *title;
	switch(fft_mode) {
		case 0:
			title = "Sound FFT - Rectangle";
			break;
		case 1:
			title = "Sound FFT - Polygon";
			break;
		case 2:
		default:
			title = "Sound FFT - Lines";
			break;
	}
	gtk_window_set_title(GTK_WINDOW(window), title);
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

	for(c = 0; c < play_ch; c ++) {
		if(!fft_val[c]) fft_val[c] = (complex_t*) malloc(sizeof(complex_t) * fft_num);
		if(!fft_res[c]) fft_res[c] = (complex_t*) malloc(sizeof(complex_t) * fft_num);
	}

	data = pcm_play_bufs[pcm_play_buf_pos];
	for(i = 0; i < fft_num * play_ch; i += play_ch) {
		for(c = 0; c < play_ch; c ++) {
			v = &fft_val[c][i/play_ch];
			v->real = (float) data[i + c] / 32767.0f;
			v->imag = 0;
		}
	}

	int h = widget->allocation.height / play_ch, h2 = h / 2;
	int Ns[] = {0, 0};
	int N = ((fft_num / 2) * 5 / 6), j, n;
	const int NN = (fft_mode == 0 ? 50 : (fft_mode == 1 ? 200 : 300));
	for(c = 0; c < play_ch; c ++) {
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

	gdk_draw_rectangle(pixmapFFT, gc_fft_bg, TRUE, 0, 0, widget->allocation.width, widget->allocation.height);

	GdkGC *gcs[] = {gc_fft_left, gc_fft_right};
	for(c = 0; c < play_ch; c ++) {
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

static void gtk_loop(int argc, char *argv[]) {
	GtkWidget *vbox;
	GtkWidget *frameVolume, *daVolume;
	GtkWidget *frameWave, *daWave;
	GtkWidget *frameFFT, *daFFT;

	gtk_init(&argc, &argv);

	g_signal_new("da_event", G_TYPE_OBJECT, G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), "Sound FFT - Rectangle");
	gtk_window_resize(GTK_WINDOW(window), 800, 600);
	g_signal_connect(G_OBJECT(window), "delete_event", G_CALLBACK(gtk_main_quit), NULL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	vbox = gtk_vbox_new(FALSE, 10);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	frameVolume = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameVolume), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), frameVolume, FALSE, FALSE, 0);

	daVolume = gtk_drawing_area_new();
	gtk_widget_set_size_request(daVolume, 800, 50);
	gtk_container_add(GTK_CONTAINER(frameVolume), daVolume);

	sigVolume = (GtkObject*) G_OBJECT(daVolume);
	g_signal_connect(sigVolume, "expose_event", G_CALLBACK(scribble_expose_event_volume), NULL);
	g_signal_connect(sigVolume, "configure_event", G_CALLBACK(scribble_configure_event_volume), NULL);
	g_signal_connect(sigVolume, "da_event", G_CALLBACK(scribble_da_event_volume), NULL);

	gtk_widget_set_events(daVolume, gtk_widget_get_events(daVolume) | GDK_LEAVE_NOTIFY_MASK);

	frameWave = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameWave), GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), frameWave, TRUE, TRUE, 0);

	daWave = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(frameWave), daWave);

	sigWave = (GtkObject*) G_OBJECT(daWave);
	g_signal_connect(sigWave, "expose_event", G_CALLBACK(scribble_expose_event_wave), NULL);
	g_signal_connect(sigWave, "configure_event", G_CALLBACK(scribble_configure_event_wave), NULL);
	g_signal_connect(sigWave, "da_event", G_CALLBACK(scribble_da_event_wave), NULL);

	gtk_widget_set_events(daWave, gtk_widget_get_events(daWave) | GDK_LEAVE_NOTIFY_MASK);

	frameFFT = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(frameFFT), GTK_SHADOW_IN);
	gtk_box_pack_end(GTK_BOX(vbox), frameFFT, TRUE, TRUE, 0);

	daFFT = gtk_drawing_area_new();
	gtk_container_add(GTK_CONTAINER(frameFFT), daFFT);

	sigFFT = (GtkObject*) G_OBJECT(daFFT);
	g_signal_connect(sigFFT, "expose_event", G_CALLBACK(scribble_expose_event_fft), NULL);
	g_signal_connect(sigFFT, "configure_event", G_CALLBACK(scribble_configure_event_fft), NULL);
	g_signal_connect(sigFFT, "button_press_event", G_CALLBACK(scribble_clicked_event_fft), NULL);
	g_signal_connect(sigFFT, "da_event", G_CALLBACK(scribble_da_event_fft), NULL);

	gtk_widget_set_events(daFFT, gtk_widget_get_events(daFFT) | GDK_LEAVE_NOTIFY_MASK | GDK_BUTTON_PRESS_MASK);

	gtk_widget_show_all(window);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	for(int i = 0; i < 2; i ++) {
		if(points[i]) free(points[i]);
		if(fft_val[i]) free(fft_val[i]);
		if(fft_res[i]) free(fft_res[i]);
		if(fft_pts[i]) free(fft_pts[i]);
		if(fft_mags[i]) free(fft_mags[i]);
	}
}

int main(int argc, char *argv[]) {
	int ret;
	if (argc < 2 || argc > 4) {
		fprintf(stderr, "usage: %s sample_rate [channels] [device]\n", argv[0]);
		return -1;
	}

	sample_rate = atoi(argv[1]);
	if(argc > 2) {
		play_ch = atoi(argv[2]);
		if(play_ch > 2) play_ch = 2;
		if(argc > 3) name = argv[3];
	}
	ret = set_hardware_params();
	if (ret < 0) {
		fprintf(stderr, "set_hardware_params error\n");
		return -1;
	}

	fprintf(stderr, "sample_rate: %d, channels: %d, format_size: %d, frames: %lu\n", sample_rate, play_ch, format_size, g_frames);

	signal(SIGINT, sig_handle);

	gdk_threads_init();

	fft_num = powf(2, floorf(log2f(g_frames)));

	sem_init(&sem, 0, 0);
	{
		const int size = g_frames * play_ch * format_size / 8;
		void *ptr;
		pcm_play_buf_ptr = malloc(PCM_PLAY_BUF_SIZE * size);
		ptr = pcm_play_buf_ptr;
		for(int i = 0; i < PCM_PLAY_BUF_SIZE; i ++) {
			pcm_play_bufs[i] = (short*) ptr;
			ptr += size;
		}
	}

	pthread_t thread, thread2;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	if(ret) {
		fprintf(stderr, "pthread_attr_setdetachstate failure: %d", ret);
		return -1;
	}
	ret = pthread_create(&thread, &attr, cap_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		return -1;
	}
	ret = pthread_create(&thread2, &attr, calc_thread, NULL);
	if(ret) {
		fprintf(stderr, "pthread_create failure: %d", ret);
		return -1;
	}

	gtk_loop(argc, argv);

	is_running = false;
	sem_post(&sem);
	if(pthread_join(thread, NULL)) {
		perror("pthread_join failure");
	}
	if(pthread_join(thread2, NULL)) {
		perror("pthread_join failure");
	}
	pthread_attr_destroy(&attr);
	sem_destroy(&sem);

	snd_pcm_drop(gp_handle);
	snd_pcm_close(gp_handle);

	free(pcm_play_buf_ptr);

	fprintf(stderr, "Exited\n");
	return 0;
}
