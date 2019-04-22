#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>
#include <mysql.h>

#define STRARG(s) s,sizeof(s)-1

#define RETRANSACTION(i, err) { \
	for(i=0; transactionSqls[i]; i++) { \
		ret = mysql_query(db, transactionSqls[i]); \
		if(ret) { \
			fprintf(stderr, "SQL: %s\nError: %s\n", transactionSqls[i], mysql_error(db)); \
			err; \
		} \
	} \
}

#define bind_null(i) \
	stmtBind[i].buffer_type = MYSQL_TYPE_NULL; \
	stmtBind[i].buffer = NULL; \
	stmtBind[i].is_unsigned = 0; \
	stmtBind[i].is_null = (my_bool*) 1; \
	stmtBind[i].length = 0;

#define bind_int(i, n) \
	int int_data_##i = n; \
	stmtBind[i].buffer_type = MYSQL_TYPE_LONG; \
	stmtBind[i].buffer = (char *)&int_data_##i; \
	stmtBind[i].is_unsigned = 0; \
	stmtBind[i].is_null = (my_bool*) 0; \
	stmtBind[i].length = 0;

#define bind_uint(i, n) \
	unsigned int uint_data_##i = n; \
	stmtBind[i].buffer_type = MYSQL_TYPE_LONG; \
	stmtBind[i].buffer = (char *)&uint_data_##i; \
	stmtBind[i].is_unsigned = 1; \
	stmtBind[i].is_null = (my_bool*) 0; \
	stmtBind[i].length = 0;

#define bind_long(i, n) \
	long long int long_data_##i = n; \
	stmtBind[i].buffer_type = MYSQL_TYPE_LONGLONG; \
	stmtBind[i].buffer = (char *)&long_data_##i; \
	stmtBind[i].is_unsigned = 0; \
	stmtBind[i].is_null = (my_bool*) 0; \
	stmtBind[i].length = 0;

#define bind_ulong(i, n) \
	unsigned long long int ulong_data_##i = n; \
	stmtBind[i].buffer_type = MYSQL_TYPE_LONGLONG; \
	stmtBind[i].buffer = (char *)&ulong_data_##i; \
	stmtBind[i].is_unsigned = 1; \
	stmtBind[i].is_null = (my_bool*) 0; \
	stmtBind[i].length = 0;

#define bind_text(i, s, n) \
	unsigned long str_length_##i = n; \
	stmtBind[i].buffer_type = MYSQL_TYPE_STRING; \
	stmtBind[i].buffer = (char *)s; \
	stmtBind[i].buffer_length = n; \
	stmtBind[i].is_unsigned = 0; \
	stmtBind[i].is_null = (my_bool*) 0; \
	stmtBind[i].length = &str_length_##i;

#define STMT(stmt, t, i, err, fmt, args...) { \
	memset(stmtBind, 0, sizeof(stmtBind)); \
	bind_##t(0, i); \
	mysql_stmt_bind_param(stmt, stmtBind); \
	ret = mysql_stmt_execute(stmt); \
	if(ret) { \
		fprintf(stderr, fmt ": %s\n", args, mysql_error(db)); \
		err; \
	} \
}

#define STMT_STR(stmt, s, err, fmt, args...) { \
	memset(stmtBind, 0, sizeof(stmtBind)); \
	bind_text(0, s, strlen(s)); \
	mysql_stmt_bind_param(stmt, stmtBind); \
	ret = mysql_stmt_execute(stmt); \
	if(ret) { \
		fprintf(stderr, fmt ": %s\n", args, mysql_error(db)); \
		err; \
	} \
}

static const char *transactionSqls[] = {
	"COMMIT", // 提交事务
	"START TRANSACTION", // 开启事务
	NULL
};
static MYSQL_STMT *stmtType = NULL;
static MYSQL_STMT *stmtMode = NULL;
static MYSQL_BIND stmtBind[13];

int recursion_directory(MYSQL *db, MYSQL_STMT *stmt, const char *path, const char *name, unsigned int parentId) {
	static char linkTarget[PATH_MAX];
	int ret = 0, i;
	unsigned int dirId;
	DIR *dir;
	struct dirent *d;
	char sPath[PATH_MAX], *dirType, atime[20], mtime[20], ctime[20], *errmsg = NULL;
	struct stat st;
	struct tm tm;
	time_t t;
	
	if(*name && sprintf(sPath, "%s/%s", path, name) <= 0 || !*name && !strcpy(sPath, path)) return 1;
	
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

	memset(stmtBind, 0, sizeof(stmtBind));

	bind_uint(0, parentId); // parentId
	bind_text(1, name, strlen(name)); // dirName
	bind_text(2, sPath, strlen(sPath)); // pathName
	if(S_ISLNK(st.st_mode)) {
		linkTarget[0] = '\0';
		readlink(sPath, linkTarget, PATH_MAX);
		bind_text(3, linkTarget, strlen(linkTarget)); // linkTarget
	} else {
		bind_null(3); // linkTarget
	}
	bind_int(4, (int) st.st_nlink); // nlinks
	bind_int(5, (int) (st.st_mode & 07777)); // dirMode
	bind_text(6, dirType, strlen(dirType)); // dirType
	bind_int(7, st.st_uid); // uid
	bind_int(8, st.st_gid); // gid
	bind_ulong(9, st.st_size); // size
	bind_text(10, atime, 19); // accessTime
	bind_text(11, mtime, 19); // modifyTime
	bind_text(12, ctime, 19); // changeTime
	mysql_stmt_bind_param(stmt, stmtBind);
	ret = mysql_stmt_execute(stmt);
	if(ret) {
		fprintf(stderr, "%s: %s\n", mysql_error(db));
		
		return ret;
	}

	dirId = mysql_insert_id(db);
	if(dirId%1024 == 1023) {
		RETRANSACTION(i, return ret);
	}
	
	STMT_STR(stmtType, dirType, return ret, "%s dirType %s", sPath, dirType);
	STMT(stmtMode, int, (unsigned short) (st.st_mode & 07777), return ret, "%s dirMode 0%04o", sPath, (unsigned short) (st.st_mode & 07777));
	
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
	const char *path, *name;
	char sPath[PATH_MAX];
	int ret = 0, i;
	MYSQL *db = NULL;
	MYSQL_STMT *stmt = NULL;
	const char *sqls[] = {
		"DROP TABLE IF EXISTS directories",
		"CREATE TABLE directories(dirId INT(11) UNSIGNED NOT NULL AUTO_INCREMENT, parentId INT(11) NOT NULL, dirName VARCHAR(255) BINARY NOT NULL, pathName VARCHAR(4096) BINARY NOT NULL, linkTarget VARCHAR(4096) BINARY, nlinks INT(11) UNSIGNED, dirMode MEDIUMINT UNSIGNED, dirType ENUM('REG','DIR','CHR','BLK','FIFO','LNK','SOCK') NOT NULL, uid INT(11) UNSIGNED NOT NULL, gid INT(11) UNSIGNED NOT NULL, size BIGINT UNSIGNED NOT NULL, accessTime DATETIME NOT NULL, modifyTime DATETIME NOT NULL, changeTime DATETIME NOT NULL, PRIMARY KEY(dirId), UNIQUE INDEX parentId_dirName (parentId, dirName)) ENGINE=InnoDB DEFAULT CHARSET=utf8",
		"DROP TABLE IF EXISTS directory_type_counts",
		"CREATE TABLE directory_type_counts(dirType ENUM('REG','DIR','CHR','BLK','FIFO','LNK','SOCK'), nCounts BIGINT UNSIGNED, PRIMARY KEY(dirType)) ENGINE=InnoDB DEFAULT CHARSET=utf8",
		"DROP TABLE IF EXISTS directory_mode_counts",
		"CREATE TABLE directory_mode_counts(dirMode MEDIUMINT UNSIGNED, nCounts BIGINT UNSIGNED, PRIMARY KEY(dirMode)) ENGINE=InnoDB DEFAULT CHARSET=utf8",
		"START TRANSACTION", // 开启事务
		NULL
	};
	const char *types[] = {"REG", "DIR", "CHR", "BLK", "FIFO", "LNK", "SOCK", NULL};

	if(argc<2) goto usage;

	char *tmp = NULL;
	const char *host, *user, *pass, *dbname, *unix_socket;
	int port;

	if((tmp = getenv("DBHOST"))) {
		host = tmp;
	} else {
		host = "localhost";
	}

	if((tmp = getenv("DBPORT"))) {
		port = atoi(tmp);
	} else {
		port = 3306;
	}

	if((tmp = getenv("DBUSERT"))) {
		user = tmp;
	} else {
		user = "root";
	}

	if((tmp = getenv("DBPASS"))) {
		pass = tmp;
	} else {
		pass = "123456";
	}

	if((tmp = getenv("DBNAME"))) {
		dbname = tmp;
	} else {
		dbname = "test";
	}

	unix_socket = getenv("DBSOCK");

	mysql_library_init(0, NULL, NULL);
	db = mysql_init(NULL);
	if(!mysql_real_connect(db, host, user, pass, dbname, port, unix_socket, 0)) {
		fprintf(stderr, "connect to mysql failure: %s\n", mysql_error(db));
		ret = mysql_errno(db);
		goto err;
	}

	ret = mysql_set_character_set(db, "utf8");
	if(ret) {
		fprintf(stderr, "set character-set failure: %s\n", mysql_error(db));
		goto err;
	}

	//mysql_autocommit(db, 0);
	
	for(i=0; sqls[i]; i++) {
		ret = mysql_query(db, sqls[i]);
		if(ret) {
			fprintf(stderr, "SQL: %s\nError: %s\n", sqls[i], mysql_error(db));
			goto dberr;
		}
	}
	
	stmt = mysql_stmt_init(db);
	mysql_stmt_prepare(stmt, STRARG("INSERT INTO directory_type_counts(dirType, nCounts)VALUES(?, 0)"));
	for(i=0; types[i]; i++) {
		STMT_STR(stmt, types[i], goto stmterr, "dirType %s", types[i]);
	}

	mysql_stmt_reset(stmt);
	mysql_stmt_prepare(stmt, STRARG("INSERT INTO directory_mode_counts(dirMode, nCounts)VALUES(?, 0)"));
	for(i=0; i<=07777; i++) {
		STMT(stmt, int, i, goto stmterr, "dirMode 0%04o", i);
	}
	
	RETRANSACTION(i, goto dberr);
	
	stmtType = mysql_stmt_init(db);
	mysql_stmt_prepare(stmtType, STRARG("UPDATE directory_type_counts SET nCounts = nCounts + 1 WHERE dirType=?"));
	stmtMode = mysql_stmt_init(db);
	mysql_stmt_prepare(stmtMode, STRARG("UPDATE directory_mode_counts SET nCounts = nCounts + 1 WHERE dirMode=?"));
	
	mysql_stmt_reset(stmt);
	mysql_stmt_prepare(stmt, STRARG("INSERT INTO directories (parentId, dirName, pathName, linkTarget, nlinks, dirMode, dirType, uid, gid, size, accessTime, modifyTime, changeTime)VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
	
	for(i=1; i<argc; i++) {
		if(strcmp(".", argv[i]) && strcmp("..", argv[i])) {
			strcpy(sPath, argv[i]);
			name = basename(sPath);
			path = dirname(sPath);
		} else {
			name = "";
			path = argv[i];
		}
		
		ret = recursion_directory(db, stmt, path, name, 0);
		if(ret) break;
	}
	
	ret = mysql_query(db, "COMMIT");
	if(ret) {
		fprintf(stderr, "SQL: COMMIT\nError: %s\n", mysql_error(db));
		goto stmterr;
	}

	ret = mysql_query(db, "SELECT dirMode, nCounts FROM directory_mode_counts WHERE nCounts>0");
	if(ret) {
		fprintf(stderr, "SQL: SELECT dirMode, nCounts FROM directory_mode_counts WHERE nCounts>0\nError: %s\n", mysql_error(db));
		goto stmterr;
	}
	MYSQL_RES *result = mysql_store_result(db);
	if(!result) {
		fprintf(stderr, "SQL: SELECT dirMode, nCounts FROM directory_mode_counts WHERE nCounts>0\nError: %s\n", mysql_error(db));
		goto stmterr;
	}
	MYSQL_ROW row;
	printf("\n");
	while((row = mysql_fetch_row(result))) {
		printf("%05o: %s\n", atoi(row[0]), row[1]);
	}
	mysql_free_result(result);

	ret = mysql_query(db, "SELECT dirType, nCounts FROM directory_type_counts WHERE nCounts>0");
	if(ret) {
		fprintf(stderr, "SQL: SELECT dirType, nCounts FROM directory_type_counts WHERE nCounts>0\nError: %s\n", mysql_error(db));
		goto stmterr;
	}
	result = mysql_store_result(db);
	if(!result) {
		fprintf(stderr, "SQL: SELECT dirType, nCounts FROM directory_type_counts WHERE nCounts>0\nError: %s\n", mysql_error(db));
		goto stmterr;
	}
	printf("\n");
	while((row = mysql_fetch_row(result))) {
		printf("%4s: %s\n", row[0], row[1]);
	}
	mysql_free_result(result);

stmterr:
	mysql_stmt_close(stmt);
	
dberr:
	mysql_stmt_close(stmtType);
	mysql_stmt_close(stmtMode);
err:
	mysql_close(db);
	mysql_library_end();
	return ret;
usage:
	fprintf(stderr, "usage: %s <directory>...\n"
					"environment variable:\n"
					"    DBHOST   Connect to host(default: localhost)\n"
					"    DBPORT   Port number to use for connection(default 3306).\n"
					"    DBUSER   User for login(default: root).\n"
					"    DBPASS   Password to use when connecting to server(default 123456).\n"
					"    DBNAME   database name(default: test)\n"
					"    DBSOCK   The socket file to use for connection(default: NULL).\n"
					, argv[0]);
	return 2;
}