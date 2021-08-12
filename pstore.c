#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <libgen.h>

#define PSTORE_DIR "/sys/fs/pstore"
#define DUMP_DIR "/var/crash_dump"
#define DUMP_LOG DUMP_DIR "/dump.log"
#define DUMP_ERR DUMP_DIR "/dump.err"
#define CUR_ID DUMP_DIR "/cur.id"
#define MAX_ID 30

#define LOG_INFO(fmt, args...) fprintf(stdout, "[%s] " fmt "\n", nowtime(), args)
#define LOG_ERR(fmt, args...) fprintf(stderr, "[%s] " fmt ": %s\n", nowtime(), args, strerror(errno))

/// @brief 获取ISO格式的当前日期时间字符串
///
/// @retval: ISO日期时间字符串
char *nowtime(void) {
	static char buf[24];
	static time_t tt;
	time_t t;
	struct tm tm;

	t = time(NULL);
	if(t == tt) return buf;
	tt = t;

	localtime_r(&t, &tm);

	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec
	);

	return buf;
}

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
			goto dir;
		} else {
			free(cdir);
			return 0;
		}
	} else if(S_ISDIR(st.st_mode)) {
		return 1;
	} else {
		unlink(path);
	dir:
		if(mkdir(path, 0755)) {
			LOG_ERR("mkdir %s failure", path);
			return 0;
		} else {
			LOG_INFO("Created dir %s", path);
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
	char buf[PATH_MAX];
	int fd1, fd2, n;

	fd1 = open(olddir, O_RDONLY);
	if(fd1 < 0) {
		LOG_ERR("open %s failure", olddir);
		return 0;
	}
	fd2 = open(newdir, O_CREAT|O_WRONLY, 0x755);
	if(fd2 < 0) {
		LOG_ERR("open %s failure", newdir);
		close(fd1);
		return 0;
	}

	while((n = read(fd1, buf, sizeof(buf))) > 0) {
		if(write(fd2, buf, n) != n) LOG_ERR("write %s failure", newdir);
	}

	close(fd1);
	close(fd2);

	return 1;
}

/// @brief 移动 /sys/fs/pstore中的文件到/var/crash_dump/CURID-DATE-TIME
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
	sprintf(curdir, "%02d-%04d%02d%02d-%02d%02d%02d",
		curId,
		tm.tm_year + 1900, tm.tm_mon, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec
	);

	dirp = opendir(PSTORE_DIR);
	if(!dirp) {
		LOG_ERR("opendir %s", PSTORE_DIR);
		return 0;
	}
	while((dir = readdir(dirp)) != NULL) {
		if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;

		if(is_mkdir) {
			is_mkdir = 0;
			sprintf(newdir, "%s/%s", DUMP_DIR, curdir);
			mkdir_p(newdir);
		}

		sprintf(olddir, "%s/%s", PSTORE_DIR, dir->d_name);
		sprintf(newdir, "%s/%s/%s", DUMP_DIR, curdir, dir->d_name);
		if(copy(olddir, newdir)) {
			LOG_INFO("Copied file %s to %s", olddir, newdir);
			count ++;
		}
		if(unlink(olddir)) LOG_ERR("unlink %s failure", olddir);
		else LOG_INFO("Deleted file %s", olddir);
	}
	closedir(dirp);

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
			LOG_ERR("opendir %s", path);
			return;
		}

		while((dir = readdir(dirp)) != NULL) {
			if(!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")) continue;

			sprintf(path2, "%s/%s", path, dir->d_name);

			removedir(path2);
		}
		closedir(dirp);

		if(rmdir(path)) LOG_ERR("rmdir %s failure", path);
		else LOG_INFO("Deleted folder %s", path);
	} else {
		if(unlink(path)) LOG_ERR("unlink %s failure", path);
		else LOG_INFO("Deleted file %s", path);
	}
}

/// @brief 移除下一个序号，即序号空洞处理
///
/// @param nextId 下一下序号
/// @retval: void
void removedump(int nextId) {
	char prefix[8];
	size_t prefix_len;
	DIR *dirp;
	struct dirent *dir;
	char path[PATH_MAX];

	prefix_len = sprintf(prefix, "%02d-", nextId);

	dirp = opendir(DUMP_DIR);
	if(!dirp) {
		LOG_ERR("opendir %s", DUMP_DIR);
		return;
	}

	while((dir = readdir(dirp)) != NULL) {
		if(strncmp(dir->d_name, prefix, prefix_len)) continue;

		sprintf(path, "%s/%s", DUMP_DIR, dir->d_name);

		removedir(path);
	}
	closedir(dirp);
}

/// @brief 主函数
///
/// @retval: 0 成功
/// @retval: 1 失败，pstore挂载失败
int main(int argc, char *argv[]) {
	int curId;
	int fd;

	mkdir_p(DUMP_DIR);

	fd = open(DUMP_LOG, O_CREAT|O_WRONLY|O_APPEND, 0755);
	if(fd > 0) {
		dup2(fd, 1); // stdout to DUMP_LOG
		close(fd);
	} else LOG_ERR("open %s", DUMP_LOG);

	fd = open(DUMP_ERR, O_CREAT|O_WRONLY|O_APPEND, 0755);
	if(fd > 0) {
		dup2(fd, 2); // stderr to DUMP_ERR
		close(fd);
	} else LOG_ERR("open %s", DUMP_ERR);

	if(mount("pstore", PSTORE_DIR, "pstore", 0, NULL) && errno != EBUSY) {
		LOG_ERR("Mount %s", PSTORE_DIR);
		fflush(stderr);
		return 1;
	}

	LOG_INFO("Mounted to %s", PSTORE_DIR);

	FILE *fp = fopen(CUR_ID, "r");
	if(fp) {
		if(fscanf(fp, "%d", &curId) != 1) {
			LOG_ERR("fscanf curId %s", CUR_ID);
			curId = 0;
		}
		fclose(fp);
	} else {
		curId = 0;
	}

	LOG_INFO("curId: %d", curId);

	if(!move2dump(curId)) goto flush;

	curId ++;
	if(curId > MAX_ID) curId = 0;

	fp = fopen(CUR_ID, "w");
	if(fp) {
		fprintf(fp, "%d", curId);
		fclose(fp);
	} else {
		LOG_ERR("fopen %s", CUR_ID);
	}

	removedump(curId);

flush:
	fflush(stdout);
	fflush(stderr);

	return 0;
}
