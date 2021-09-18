#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>

#define PSTORE_DIR "/sys/fs/pstore"

static const char *g_dump_dir = "/var/pstore";
static const char *g_next_id = "/var/pstore/next.id";
static int g_max_id = 30;
static int g_file_mode = (S_IWUSR | S_IRUSR);

#ifdef USE_SYSLOG
#	include <syslog.h>

#	define LOG_OPEN() openlog("pstore", LOG_PID, LOG_USER)
#	define LOGI(fmt, args...) syslog(LOG_INFO, fmt, args)
#	define LOGE(fmt, args...) syslog(LOG_ERR, fmt ": %s", args, strerror(errno))
#	define LOG_CLOSE() closelog()
#else // ifdef USE_SYSLOG
	static const char *g_dump_log = "/var/pstore/stdout.log";
	static const char *g_dump_err = "/var/pstore/stderr.log";
	static int g_max_log_size = 2 * 1024 * 1024;

#	define LOG_OPEN() { \
		std2file(g_dump_log, 1); \
		std2file(g_dump_err, 2); \
	}
#	define LOGI(fmt, args...) fprintf(stdout, "[%s] " fmt "\n", nowtime(), args)
#	define LOGE(fmt, args...) fprintf(stderr, "[%s] " fmt ": %s\n", nowtime(), args, strerror(errno))
#	define LOG_CLOSE() { \
		fflush(stdout); \
		fflush(stderr); \
	}

/// @brief 获取ISO格式的当前日期时间字符串
///
/// @retval: ISO日期时间字符串
char *nowtime(void) {
	static char buf[32];
	struct timeval tv = {0, 0};
	struct tm tm;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm);

	snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%06ld",
		tm.tm_year + 1900, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec,
		tv.tv_usec
	);

	return buf;
}
#endif // ifndef USE_SYSLOG

/// @brief 递归创建目录
///
/// @param path 目录路径
/// @retval: 0 失败
/// @retval: 1 成功
int mkdir_p(const char *path) {
	struct stat st;

	if(lstat(path, &st)) {
		char *cdir = strdup(path);
		if(mkdir_p((const char *) dirname(cdir))) {
			free(cdir);

			if(mkdir(path, g_file_mode)) {
				LOGE("mkdir %s failure", path);
				return 0;
			} else {
				LOGI("Created dir %s", path);
				return 1;
			}
		} else {
			free(cdir);
			return 0;
		}
	} else if(S_ISDIR(st.st_mode)) {
		return 1;
	} else {
		unlink(path);

		if(mkdir(path, g_file_mode)) {
			LOGE("mkdir %s failure", path);
			return 0;
		} else {
			LOGI("Created dir %s", path);
			return 1;
		}
	}
}

/// @brief 复制文件
///
/// @param olddir 原文件路径
/// @param newdir 新文件路径
/// @retval: 0 失败
/// @retval: 1 成功
int copy(const char *olddir, const char *newdir) {
	char buf[8*1024];
	int fd1, fd2, n;
	int ret = 1;

	fd1 = open(olddir, O_RDONLY);
	if(fd1 < 0) {
		LOGE("open %s failure", olddir);
		return 0;
	}
	fd2 = open(newdir, O_CREAT|O_WRONLY, g_file_mode);
	if(fd2 < 0) {
		LOGE("open %s failure", newdir);
		close(fd1);
		return 0;
	}

	while((n = read(fd1, buf, sizeof(buf))) > 0) {
		if(write(fd2, buf, n) != n) {
			LOGE("write %s failure", newdir);
			ret = 0;
			break;
		}
	}

	close(fd1);
	close(fd2);

	return ret;
}

/// @brief 移动 /sys/fs/pstore中的文件到/var/pstore/CURID-DATE-TIME
///
/// @param curId 当前序号
/// @retval: 0 失败，没有要复制的文件
/// @retval: >1 成功，复制的文件个数
int move2dump(int curId) {
	time_t t;
	struct tm tm;
	char curdir[32];
	char olddir[PATH_MAX];
	char newdir[PATH_MAX];
	DIR *dirp;
	struct dirent *dir;
	int count = 0;
	int is_mkdir = 1;

	t = time(NULL);
	localtime_r(&t, &tm);
	snprintf(curdir, sizeof(curdir), "%02d-%04d%02d%02d-%02d%02d%02d",
		curId,
		tm.tm_year + 1900, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec
	);

	dirp = opendir(PSTORE_DIR);
	if(!dirp) {
		LOGE("opendir %s", PSTORE_DIR);
		return 0;
	}
	while((dir = readdir(dirp)) != NULL) {
		if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;

		if(is_mkdir) {
			is_mkdir = 0;
			snprintf(newdir, sizeof(newdir), "%s/%s", g_dump_dir, curdir);
			if(!mkdir_p(newdir)) {
				closedir(dirp);
				return 0;
			}
		}

		snprintf(olddir, sizeof(olddir), "%s/%s", PSTORE_DIR, dir->d_name);
		snprintf(newdir, sizeof(newdir), "%s/%s/%s", g_dump_dir, curdir, dir->d_name);
		if(copy(olddir, newdir)) {
			LOGI("Copied file %s to %s", olddir, newdir);
			if(unlink(olddir)) LOGE("unlink %s failure", olddir);
			else LOGI("Deleted file %s", olddir);

			count ++;
		}
	}
	closedir(dirp);

	if(count) {
		snprintf(newdir, sizeof(newdir), "getprop > %s/%s/system.prop", g_dump_dir, curdir);

		if(system(newdir)) LOGE("getprop to %s failure", newdir + 10);
	}

	return count;
}

/// @brief 递归移除目录或文件
///
/// @param path 要移除的目录或文件路径
/// @retval: void
void removedir(const char *path) {
	struct stat st;

	if(lstat(path, &st)) return;

	if(S_ISDIR(st.st_mode)) {
		DIR *dirp;
		struct dirent *dir;
		char path2[PATH_MAX];

		dirp = opendir(path);
		if(!dirp) {
			LOGE("opendir %s", path);
			return;
		}

		while((dir = readdir(dirp)) != NULL) {
			if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;

			snprintf(path2, sizeof(path2), "%s/%s", path, dir->d_name);

			removedir(path2);
		}
		closedir(dirp);

		if(rmdir(path)) LOGE("rmdir %s failure", path);
		else LOGI("Deleted folder %s", path);
	} else {
		if(unlink(path)) LOGE("unlink %s failure", path);
		else LOGI("Deleted file %s", path);
	}
}

/// @brief 移除下一个序号，即序号空洞处理
///
/// @param nextId 下一下序号
/// @retval: void
void removedump(int nextId) {
	char prefix[8];
	int prefix_len;
	DIR *dirp;
	struct dirent *dir;
	char path[PATH_MAX];

	prefix_len = snprintf(prefix, sizeof(prefix), "%02d-", nextId);

	dirp = opendir(g_dump_dir);
	if(!dirp) {
		LOGE("opendir %s", g_dump_dir);
		return;
	}

	while((dir = readdir(dirp)) != NULL) {
		if(strncmp(dir->d_name, prefix, prefix_len)) continue;

		snprintf(path, sizeof(path), "%s/%s", g_dump_dir, dir->d_name);

		removedir(path);
	}
	closedir(dirp);
}

#ifndef USE_SYSLOG
/// @brief 把标准输出重定向到指定的文件
///
/// @param file 文件路径
/// @param fileno 重定向到那个标准输出号(1 stdout, 2 stderr)
void std2file(const char *file, int fileno) {
	int fd;
	struct stat st;

	if(!lstat(file, &st) && st.st_size >= g_max_log_size) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s.old", file);
		if(rename(file, path)) LOGE("rename %s to %s", file, path);
	}

	fd = open(file, O_CREAT|O_WRONLY|O_APPEND, g_file_mode);
	if(fd > 0) {
		dup2(fd, fileno);
		close(fd);
		LOGI("Redirect %d> %s", fileno, file);
	} else LOGE("open %s", file);
}
#endif

/// @brief 主函数
///
/// @retval: 0 成功
/// @retval: 1 失败，pstore挂载失败
int main(int argc, char *argv[]) {
	int curId;
	int delay = 0;
	int opt;

	while((opt = getopt(argc, argv, "d:n:m:f:l:e:s:D:h?")) != -1) {
		switch(opt) {
			case 'd':
				g_dump_dir = optarg;
				break;
			case 'n':
				g_next_id = optarg;
				break;
			case 'm':
				g_max_id = atoi(optarg);
				break;
			case 'f': {
				char *endptr = NULL;
				g_file_mode = (int) strtol(optarg, &endptr, 8);
				break;
			}
		#ifdef USE_SYSLOG
			case 'l':
			case 'e':
			case 's':
				break;
		#else
			case 'l':
				g_dump_log = optarg;
				break;
			case 'e':
				g_dump_err = optarg;
				break;
			case 's':
				g_max_log_size = atoi(optarg);
				break;
		#endif
			case 'D':
				delay = atoi(optarg);
				break;
			case 'h':
			case '?':
			default:
				printf(
					"usage: %s [options]\n\n"
					"options:\n"
					"    -d    kernel log dump dir(value: %s)\n"
					"    -n    next id save to file(value: %s)\n"
					"    -m    max id(value: %d)\n"
					"    -f    file mode is octal integer number(value: %o)\n"
				#ifndef USE_SYSLOG
					"    -l    info log file for this program(value: %s)\n"
					"    -e    error log file for this program(value: %s)\n"
					"    -s    max log file size for this program(value: %d)\n"
				#endif
					"    -D    this program start before delay seconds(value: %d)\n"
					, argv[0], g_dump_dir, g_next_id, g_max_id, g_file_mode
				#ifndef USE_SYSLOG
					, g_dump_log, g_dump_err, g_max_log_size
				#endif
					, delay
				);
				return 255;
		}
	}

	if(delay > 0) sleep(delay);

	if(!mkdir_p(g_dump_dir)) return 1;

	LOG_OPEN();

	if(mount("pstore", PSTORE_DIR, "pstore", 0, NULL) && errno != EBUSY) {
		LOGE("Mount %s", PSTORE_DIR);
		LOG_CLOSE();
		sync();
		return 1;
	}

	LOGI("Mounted to %s", PSTORE_DIR);

	FILE *fp = fopen(g_next_id, "r");
	if(fp) {
		if(fscanf(fp, "%d", &curId) != 1) {
			LOGE("fscanf curId %s", g_next_id);
			curId = 0;
		}
		fclose(fp);
	} else {
		curId = 0;
	}

	LOGI("current id is %d", curId);

	if(!move2dump(curId)) {
		LOG_CLOSE();
		sync();

		return 0;
	}

	curId ++;
	if(curId > g_max_id) curId = 0;

	fp = fopen(g_next_id, "w");
	if(fp) {
		fprintf(fp, "%d", curId);
		fclose(fp);
	} else {
		LOGE("fopen %s", g_next_id);
	}

	LOGI("next id is %d", curId);

	removedump(curId);

	LOG_CLOSE();
	sync();

	return 0;
}
