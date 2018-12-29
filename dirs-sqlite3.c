#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sqlite3.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

#define RETRANSACTION(i, err) { \
	for(i=0; transactionSqls[i]; i++) { \
		ret = sqlite3_exec(db, transactionSqls[i], NULL, NULL, &errmsg); \
		if(ret != SQLITE_OK) { \
			fprintf(stderr, "SQL: %s\nError: %s\n", transactionSqls[i], errmsg); \
			err; \
		} \
	} \
}

#define STMT(stmt, t, i, err, fmt, args...) { \
	sqlite3_bind_##t(stmt, 1, i); \
	ret = sqlite3_step(stmt); \
	sqlite3_reset(stmt); \
	if(ret != SQLITE_DONE) { \
		switch(ret) { \
			case SQLITE_BUSY: \
				fprintf(stderr, fmt " BUSY\n", args); \
				break; \
			case SQLITE_ROW: \
				fprintf(stderr, fmt " ROW\n", args); \
				break; \
			case SQLITE_ERROR: \
				fprintf(stderr, fmt " ERROR\n", args); \
				break; \
			case SQLITE_MISUSE: \
				fprintf(stderr, fmt " MISUSE\n", args); \
				break; \
			default: \
				fprintf(stderr, fmt " UNKNOWN\n", args); \
				break; \
		} \
		err; \
	} \
}

#define STMT_STR(stmt, s, err, fmt, args...) { \
	sqlite3_bind_text(stmt, 1, s, strlen(s), NULL); \
	ret = sqlite3_step(stmt); \
	sqlite3_reset(stmt); \
	if(ret != SQLITE_DONE) { \
		switch(ret) { \
			case SQLITE_BUSY: \
				fprintf(stderr, fmt " BUSY\n", args); \
				break; \
			case SQLITE_ROW: \
				fprintf(stderr, fmt " ROW\n", args); \
				break; \
			case SQLITE_ERROR: \
				fprintf(stderr, fmt " ERROR\n", args); \
				break; \
			case SQLITE_MISUSE: \
				fprintf(stderr, fmt " MISUSE\n", args); \
				break; \
			default: \
				fprintf(stderr, fmt " UNKNOWN\n", args); \
				break; \
		} \
		err; \
	} \
}

static const char *transactionSqls[] = {
	"COMMIT", // 提交事务
	"BEGIN TRANSACTION", // 开启事务
	NULL
};
static sqlite3_stmt *stmtType = NULL;
static sqlite3_stmt *stmtMode = NULL;

int recursion_directory(sqlite3 *db, sqlite3_stmt *stmt, const char *path, const char *name, sqlite3_uint64 parentId) {
	static char linkTarget[PATH_MAX];
	int ret = 0, i;
	sqlite3_uint64 dirId;
	DIR *dir;
	struct dirent *d;
	char sPath[PATH_MAX], *dirType, atime[20], mtime[20], ctime[20], *errmsg = NULL;
	struct stat st;
	struct tm tm;
	time_t t;
	
	if(sprintf(sPath, "%s/%s", path, name) <= 0) return 1;
	
	ret = lstat(sPath, &st);
	if(ret) {
		fprintf(stderr, "%s STAT\n", sPath);
		return ret;
	}
	
	switch(st.st_mode & S_IFMT) {
		case S_IFDIR:
			dirType = "DIR";
			break;
		case S_IFCHR:
			dirType = "CHR";
			break;
		case S_IFBLK:
			dirType = "BLK";
			break;
		case S_IFIFO:
			dirType = "FIFO";
			break;
		case S_IFLNK:
			dirType = "LNK";
			break;
		case S_IFSOCK:
			dirType = "SOCK";
			break;
		case S_IFREG:
		default:
			dirType = "REG";
			break;
	}
	
	t = st.st_atime;
	localtime_r(&t, &tm);
	strftime(atime, 20, "%F %T", &tm);

	t = st.st_mtime;
	localtime_r(&t, &tm);
	strftime(mtime, 20, "%F %T", &tm);

	t = st.st_ctime;
	localtime_r(&t, &tm);
	strftime(ctime, 20, "%F %T", &tm);

	sqlite3_bind_int64(stmt, 1, parentId); // parentId
	sqlite3_bind_text(stmt, 2, name, strlen(name), NULL); // dirName
	sqlite3_bind_text(stmt, 3, sPath, strlen(sPath), NULL); // pathName
	if(S_ISLNK(st.st_mode)) {
		linkTarget[0] = '\0';
		readlink(sPath, linkTarget, PATH_MAX);
		sqlite3_bind_text(stmt, 4, linkTarget, PATH_MAX, NULL); // linkTarget
	} else {
		sqlite3_bind_null(stmt, 4); // linkTarget
	}
	sqlite3_bind_int(stmt, 5, (int) st.st_nlink); // nlinks
	sqlite3_bind_int(stmt, 6, (unsigned short) (st.st_mode & 07777)); // dirMode
	sqlite3_bind_text(stmt, 7, dirType, strlen(dirType), NULL); // dirType
	sqlite3_bind_int(stmt, 8, (int) st.st_uid); // uid
	sqlite3_bind_int(stmt, 9, (int) st.st_gid); // gid
	sqlite3_bind_int(stmt, 10, (sqlite3_int64) st.st_size); // size
	sqlite3_bind_text(stmt, 11, atime, 19, NULL); // accessTime
	sqlite3_bind_text(stmt, 12, mtime, 19, NULL); // modifyTime
	sqlite3_bind_text(stmt, 13, ctime, 19, NULL); // changeTime
	ret = sqlite3_step(stmt);
	sqlite3_reset(stmt);
	if(ret != SQLITE_DONE) {
		switch(ret) {
			case SQLITE_BUSY:
				fprintf(stderr, "%s BUSY\n", sPath);
				break;
			case SQLITE_ROW:
				fprintf(stderr, "%s ROW\n", sPath);
				break;
			case SQLITE_ERROR:
				fprintf(stderr, "%s ERROR\n", sPath);
				break;
			case SQLITE_MISUSE:
				fprintf(stderr, "%s MISUSE\n", sPath);
				break;
			default:
				fprintf(stderr, "%s UNKNOWN\n", sPath);
				break;
		}
		
		return 1;
	}

	dirId = sqlite3_last_insert_rowid(db);
	if(dirId&1024) {
		RETRANSACTION(i, return ret);
	}
	
	STMT_STR(stmtType, dirType, return ret, "%s dirType %s", sPath, dirType);
	STMT(stmtType, int, (unsigned short) (st.st_mode & 07777), return ret, "%s dirMode 0%04o", sPath, (unsigned short) (st.st_mode & 07777));
	
	if(S_ISLNK(st.st_mode)) {
		printf("%s %s => %s\n", sPath, dirType, linkTarget);
	} else {
		printf("%s %s\n", sPath, dirType);
	}

	if(!S_ISDIR(st.st_mode)) {
		return 0;
	}

	dir = opendir(sPath);
	if(!dir) {
		fprintf(stderr, "%s NOT DIR\n", sPath);
		return 1;
	}
	
	ret = 0;
	while((d = readdir(dir)) != NULL) {
		if(strcmp(d->d_name, ".") && strcmp(d->d_name, "..") && (ret = recursion_directory(db, stmt, sPath, d->d_name, dirId)) != 0) break;
	}

	closedir(dir);
	return ret;
}

int main(int argc, char *argv[]){
	const char *dbfile, *path, *name;
	char sPath[PATH_MAX];
	int ret = 0, i;
	char *errmsg = NULL;
	sqlite3 *db = NULL;
	sqlite3_stmt *stmt = NULL;
	const char *sqls[] = {
		"PRAGMA synchronous=OFF", // 减少IO，提高速度
		"PRAGMA foreign_keys=OFF",
		"BEGIN TRANSACTION", // 开启事务
		"DROP TABLE IF EXISTS directories",
		"DROP INDEX IF EXISTS i_directories_parentId_dirName",
		"CREATE TABLE directories(dirId INTEGER PRIMARY KEY AUTOINCREMENT, parentId BIGINT, dirName VARCHAR(255), pathName VARCHAR(4096), linkTarget VARCHAR(4096), nlinks INTEGER, dirMode UNSIGNED MEDIUMINT, dirType ENUM CHECK(dirType IN('REG','DIR','CHR','BLK','FIFO','LNK','SOCK')), uid UNSIGNED INTEGER, gid UNSIGNED INTEGER, size UNSIGNED BIGINT, accessTime DATETIME, modifyTime DATETIME, changeTime DATETIME)",
		"CREATE UNIQUE INDEX i_directories_parentId_dirName ON directories (parentId, dirName)",
		"DROP TABLE IF EXISTS directory_type_counts",
		"CREATE TABLE directory_type_counts(dirType ENUM CHECK(dirType IN('REG','DIR','CHR','BLK','FIFO','LNK','SOCK')) PRIMARY KEY, nCounts UNSIGNED BIGINT)",
		"DROP TABLE IF EXISTS directory_mode_counts",
		"CREATE TABLE directory_mode_counts(dirMode UNSIGNED MEDIUMINT PRIMARY KEY, nCounts UNSIGNED BIGINT)",
		"DELETE FROM sqlite_sequence",
		"COMMIT", // 提交事务
		"BEGIN TRANSACTION", // 开启事务
		"PRAGMA foreign_keys=ON",
		NULL
	};
	const char *types[] = {"REG", "DIR", "CHR", "BLK", "FIFO", "LNK", "SOCK", NULL};
	
	if(argc < 3) goto usage;
	
	dbfile = argv[1];
	
	ret = sqlite3_open(dbfile, &db);
	if(ret != SQLITE_OK){
		fprintf(stderr, "Cannot open db: %s\n", sqlite3_errmsg(db));
		goto err;
	}
	
	for(i=0; sqls[i]; i++) {
		ret = sqlite3_exec(db, sqls[i], NULL, NULL, &errmsg);
		if(ret != SQLITE_OK) {
			fprintf(stderr, "SQL: %s\nError: %s\n", sqls[i], errmsg);
			goto dberr;
		}
	}
	
	sqlite3_prepare_v2(db, "INSERT INTO directory_type_counts(dirType, nCounts)VALUES(?, 0)", -1, &stmt, NULL);
	for(i=0; types[i]; i++) {
		STMT_STR(stmt, types[i], goto stmterr, "dirType %s", types[i]);
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	
	sqlite3_prepare_v2(db, "INSERT INTO directory_mode_counts(dirMode, nCounts)VALUES(?, 0)", -1, &stmt, NULL);
	for(i=0; i<=07777; i++) {
		STMT(stmt, int, i, goto stmterr, "dirMode 0%04o", i);
	}
	sqlite3_finalize(stmt);
	stmt = NULL;
	
	RETRANSACTION(i, goto dberr);
	
	sqlite3_prepare_v2(db, "UPDATE directory_type_counts SET nCounts = nCounts + 1 WHERE dirType=?", -1, &stmtType, NULL);
	sqlite3_prepare_v2(db, "UPDATE directory_mode_counts SET nCounts = nCounts + 1 WHERE dirMode=?", -1, &stmtMode, NULL);
	
	sqlite3_prepare_v2(db, "INSERT INTO directories (parentId, dirName, pathName, linkTarget, nlinks, dirMode, dirType, uid, gid, size, accessTime, modifyTime, changeTime)VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL);
	
	for(i=2; i<argc; i++) {
		strcpy(sPath, argv[i]);
		name = basename(sPath);
		path = dirname(sPath);
		if(!strcmp(name, ".")) {
			name = "";
		}
		ret = recursion_directory(db, stmt, path, name, 0);
		if(ret) break;
	}
	
	ret = sqlite3_exec(db, "COMMIT", NULL, NULL, &errmsg);
	if(ret != SQLITE_OK) {
		fprintf(stderr, "SQL: COMMIT\nError: %s\n", errmsg);
		goto stmterr;
	}

stmterr:
	sqlite3_finalize(stmt);
	
dberr:
	sqlite3_finalize(stmtType);
	sqlite3_finalize(stmtMode);
	sqlite3_free(errmsg);
	sqlite3_close(db);
err:
	return -ret;
usage:
	fprintf(stderr, "usage: %s <dbfile> <directory>\n", argv[0]);
	return 2;
}
