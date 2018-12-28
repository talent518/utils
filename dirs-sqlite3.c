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

static const char *transactionSqls[] = {
	"COMMIT", // 提交事务
	"BEGIN TRANSACTION" // 开启事务
};

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
		printf("%s STAT\n", sPath);
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
				printf("%s BUSY\n", sPath);
				break;
			case SQLITE_ROW:
				printf("%s ROW\n", sPath);
				break;
			case SQLITE_ERROR:
				printf("%s ERROR\n", sPath);
				break;
			case SQLITE_MISUSE:
				printf("%s MISUSE\n", sPath);
				break;
			default:
				printf("%s UNKNOWN\n", sPath);
				break;
		}
		
		return 1;
	}

	dirId = sqlite3_last_insert_rowid(db);
	if(dirId&1024) {
		for(i=0; i<sizeof(transactionSqls)/sizeof(char*); i++) {
			ret = sqlite3_exec(db, transactionSqls[i], NULL, NULL, &errmsg);
			if(ret != SQLITE_OK) {
				fprintf(stderr, "SQL: %s\nError: %s\n", transactionSqls[i], errmsg);
				return ret;
			}
		}
	}
	
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
		printf("%s NOT DIR\n", sPath);
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
		"DELETE FROM sqlite_sequence",
		"COMMIT", // 提交事务
		"BEGIN TRANSACTION", // 开启事务
		"PRAGMA foreign_keys=ON"
	};
	
	if(argc < 3) goto usage;
	
	dbfile = argv[1];
	name = basename(argv[2]);
	path = dirname(argv[2]);
	
	ret = sqlite3_open(dbfile, &db);
	if(ret != SQLITE_OK){
		fprintf(stderr, "Cannot open db: %s\n", sqlite3_errmsg(db));
		goto err;
	}
	
	for(i=0; i<sizeof(sqls)/sizeof(char*); i++) {
		ret = sqlite3_exec(db, sqls[i], NULL, NULL, &errmsg);
		if(ret != SQLITE_OK) {
			fprintf(stderr, "SQL: %s\nError: %s\n", sqls[i], errmsg);
			goto dberr;
		}
	}
	
	sqlite3_prepare_v2(db, "INSERT INTO directories (parentId, dirName, pathName, linkTarget, nlinks, dirMode, dirType, uid, gid, size, accessTime, modifyTime, changeTime)VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)", -1, &stmt, NULL); // ????
	ret = recursion_directory(db, stmt, path, name, 0);
	sqlite3_finalize(stmt);
	
	ret = sqlite3_exec(db, "COMMIT", NULL, NULL, &errmsg); // ????
	if(ret != SQLITE_OK) {
		fprintf(stderr, "SQL: COMMIT\nError: %s\n", errmsg);
		goto dberr;
	}
	
dberr:
	sqlite3_free(errmsg);
	sqlite3_close(db);
err:
	return -ret;
usage:
	printf("usage: %s <dbfile> <directory>\n", argv[0]);
	return 2;
}
