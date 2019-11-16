#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#include <bson.h>
#include <mongoc.h>

// #define NO_USE_BULK

#ifndef BULK_SIZE
#define BULK_SIZE 10000
#endif

#ifndef NO_USE_BULK
#define BULK_PARAM mongoc_bulk_operation_t *bulk
#define BULK_INSERT(doc) mongoc_bulk_operation_insert(bulk, doc)
#define BULK_UPDATE_BEGIN(expr) \
	static mongoc_bulk_operation_t *bulk = NULL; \
	static int bulks = 0; \
	bson_error_t error; \
	if (expr) { \
		int r = !mongoc_bulk_operation_execute(bulk, NULL, &error); \
		mongoc_bulk_operation_destroy(bulk); \
		bulk = NULL; \
		bulks = 0; \
		if (r) fprintf(stderr, "%s bulk execute failure: %s\n", __func__, error.message); \
		return r; \
	}
#define BULK_UPDATE_EXEC() \
	if (!bulk) bulk = mongoc_collection_create_bulk_operation(collection, true, NULL); \
	mongoc_bulk_operation_update(bulk, query, update, false); \
	if (++bulks == BULK_SIZE) { \
		ret = mongoc_bulk_operation_execute(bulk, NULL, &error); \
		mongoc_bulk_operation_destroy(bulk); \
		bulk = NULL; \
		bulks = 0; \
		if (ret) { \
			ret = 0; \
		} else { \
			fprintf(stderr, "%s bulk execute failure: %s\n", __func__, error.message); \
			return 1; \
		} \
	}
#define BULK_INSERT_BEGIN() \
	static mongoc_bulk_operation_t *bulk = NULL; \
	static int bulks = 0; \
	bson_error_t error; \
	if (!path && !name && parentId < 0 && bulks > 0) { \
		int r = !mongoc_bulk_operation_execute(bulk, NULL, &error); \
		mongoc_bulk_operation_destroy(bulk); \
		bulk = NULL; \
		bulks = 0; \
		if (r) fprintf(stderr, "%s bulk execute failure: %s\n", __func__, error.message); \
		return r; \
	}
#define BULK_INSERT_END() \
	if (!bulk) bulk = mongoc_collection_create_bulk_operation(collection, true, NULL); \
	mongoc_bulk_operation_insert(bulk, doc); \
	bson_destroy(doc); \
	if (++bulks == BULK_SIZE) { \
		ret = mongoc_bulk_operation_execute(bulk, NULL, &error); \
		mongoc_bulk_operation_destroy(bulk); \
		bulk = NULL; \
		bulks = 0; \
		if (ret) { \
			ret = 0; \
		} else { \
			fprintf(stderr, "%s bulk execute failure: %s\n", __func__, error.message); \
			return 1; \
		} \
	}
#define BULK_DEFINE() mongoc_bulk_operation_t *bulk
#define BULK_VAR(collection) bulk = mongoc_collection_create_bulk_operation(collection, true, NULL)
#define BULK_ARG(collection) bulk
#define BULK_EXEC() \
	if (!mongoc_bulk_operation_execute(bulk, NULL, &error)) { \
		fprintf(stderr, "%s %d bulk execute failure: %s\n", __func__, __LINE__, error.message); \
		mongoc_bulk_operation_destroy(bulk); \
		goto err; \
	} \
	mongoc_bulk_operation_destroy(bulk)
#else
#define BULK_PARAM mongoc_collection_t *collection
#define BULK_INSERT(doc) \
	if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, doc, NULL, &error)) { \
		fprintf(stderr, "%s failure: %s\n", __func__, error.message); \
		ret = 1; \
	}
#define BULK_UPDATE_BEGIN(expr) bson_error_t error
#define BULK_UPDATE_EXEC() \
	if (!mongoc_collection_update(collection, MONGOC_UPDATE_NONE, query, update, NULL, &error)) { \
		fprintf(stderr, "%s update failure: %s\n", __func__, error.message); \
		ret = 1; \
	}
#define BULK_INSERT_BEGIN() \
	if (!path && !name && parentId < 0) return 0; \
	bson_error_t error
#define BULK_INSERT_END() \
	if (!mongoc_collection_insert(collection, MONGOC_INSERT_NONE, doc, NULL, &error)) { \
		bson_destroy(doc); \
		fprintf(stderr, "%s insert failure: %s\n", __func__, error.message); \
		return 1; \
	} \
	bson_destroy(doc)
#define BULK_DEFINE()
#define BULK_VAR(collection)
#define BULK_ARG(collection) collection
#define BULK_EXEC()
#endif

int insert_type(BULK_PARAM, int i, const char *type) {
	int ret = 0;
	bson_error_t error;
	bson_t *doc = bson_new();

	BSON_APPEND_INT32(doc, "_id", i);
	BSON_APPEND_UTF8(doc, "dirType", type);
	BSON_APPEND_INT64(doc, "nCounts", 0);

	BULK_INSERT(doc);

	bson_destroy(doc);

	return ret;
}

int update_type(mongoc_collection_t *collection, const char *type) {
	BULK_UPDATE_BEGIN(!type);
	bson_t *update, *query;
	int ret = 0;

	query = BCON_NEW("dirType", BCON_UTF8(type));
	update = BCON_NEW("$inc", "{", "nCounts", BCON_INT64(1), "}");

	BULK_UPDATE_EXEC();

	bson_destroy(update);
	bson_destroy(query);

	return ret;
}

int insert_mode(BULK_PARAM, int mode) {
	int ret = 0;
	bson_error_t error;
	bson_t *doc = bson_new();

	BSON_APPEND_INT32(doc, "_id", mode);
	BSON_APPEND_INT32(doc, "dirMode", mode);
	BSON_APPEND_INT64(doc, "nCounts", 0);

	BULK_INSERT(doc);

	bson_destroy(doc);

	return ret;
}

int update_mode(mongoc_collection_t *collection, int mode) {
	BULK_UPDATE_BEGIN(mode<0);
	int ret = 0;
	bson_t *update, *query;

	query = BCON_NEW("dirMode", BCON_INT64(mode));
	update = BCON_NEW("$inc", "{", "nCounts", BCON_INT64(1), "}");

	BULK_UPDATE_EXEC();

	bson_destroy(update);
	bson_destroy(query);

	return ret;
}

static int64_t maxDirId = 0;

int recursion_directory(mongoc_collection_t *collection, mongoc_collection_t *typecollect, mongoc_collection_t *modecollect, const char *path, const char *name, int64_t parentId) {
	static char linkTarget[PATH_MAX];
	BULK_INSERT_BEGIN();

	int ret = 0, i;
	DIR *dir;
	struct dirent *d;
	char sPath[PATH_MAX], *dirType, atime[20], mtime[20], ctime[20], *errmsg = NULL;
	struct stat st;
	struct tm tm;
	time_t t;

	int64_t dirId = ++maxDirId;
	bson_t *doc = bson_new();

	if (sprintf(sPath, "%s/%s", strcmp(path, "/") ? path : "", name) <= 0)
		return 1;

	ret = lstat(sPath, &st);
	if (ret) {
		fprintf(stderr, "%s STAT\n", sPath);
		return ret;
	}

	switch (st.st_mode & S_IFMT) {
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

	BSON_APPEND_INT64(doc, "_id", dirId);
	BSON_APPEND_INT64(doc, "dirId", dirId);
	BSON_APPEND_INT64(doc, "parentId", parentId);
	BSON_APPEND_UTF8(doc, "dirName", name);
	BSON_APPEND_UTF8(doc, "pathName", sPath);
	if (S_ISLNK(st.st_mode)) {
		linkTarget[0] = '\0';
		readlink(sPath, linkTarget, PATH_MAX);
		BSON_APPEND_UTF8(doc, "linkTarget", linkTarget);
	} else {
		BSON_APPEND_NULL(doc, "linkTarget");
	}
	BSON_APPEND_INT64(doc, "nlinks", st.st_nlink);
	BSON_APPEND_INT32(doc, "dirMode", (int32_t ) (st.st_mode & 07777));
	BSON_APPEND_UTF8(doc, "dirType", dirType);
	BSON_APPEND_INT32(doc, "uid", (int ) st.st_uid);
	BSON_APPEND_INT32(doc, "gid", (int ) st.st_gid);
	BSON_APPEND_INT64(doc, "size", st.st_size);
	BSON_APPEND_UTF8(doc, "accessTime", atime);
	BSON_APPEND_UTF8(doc, "modifyTime", mtime);
	BSON_APPEND_UTF8(doc, "changeTime", ctime);

	BULK_INSERT_END();

	// type count
	if (update_type(typecollect, dirType))
		return 1;

	// mode count
	if (update_mode(modecollect, (int32_t) (st.st_mode & 07777)))
		return 1;

	if (S_ISLNK(st.st_mode)) {
		printf("%s %s => %s\n", sPath, dirType, linkTarget);
	} else {
		printf("%s %s\n", sPath, dirType);
	}

	if (!S_ISDIR(st.st_mode)) {
		return 0;
	}

	dir = opendir(sPath);
	if (!dir) {
		fprintf(stderr, "%s NOT DIR\n", sPath);
		return 1;
	}

	ret = 0;
	while ((d = readdir(dir)) != NULL) {
		if (strcmp(d->d_name, ".") && strcmp(d->d_name, "..") && (ret = recursion_directory(collection, typecollect, modecollect, sPath, d->d_name, dirId)) != 0)
			break;
	}

	closedir(dir);
	return ret;
}

int main(int argc, char *argv[]) {
	mongoc_client_t *client;
	mongoc_collection_t *type_collect, *mode_collect, *dir_collect;
	bson_t *doc;
	char sPath[PATH_MAX], *path, *name;
	const char *types[] = { "REG", "DIR", "CHR", "BLK", "FIFO", "LNK", "SOCK", NULL };
	mongoc_index_opt_t opt;
	bson_error_t error;
	int i;
	const char *uri = "mongodb://localhost:27017/";
	if (argc >= 2 && !strncmp(argv[1], "mongodb://", 10))
		uri = argv[1];

	if (argc == 1 || (uri == argv[1] && argc == 2)) {
		fprintf(stderr, "usage: %s mongodb://localhost:27017/ directory...\n"
			"       %s directory...\n", argv[0], argv[0]);
		return 1;
	}

	mongoc_index_opt_init(&opt);
	opt.unique = true;

	mongoc_init();

	client = mongoc_client_new(uri);
	type_collect = mongoc_client_get_collection(client, "test", "directory_type_counts");
	mode_collect = mongoc_client_get_collection(client, "test", "directory_mode_counts");
	dir_collect = mongoc_client_get_collection(client, "test", "directories");

	doc = bson_new();

	// insert file type
	// drop collection
	if (!mongoc_collection_drop(type_collect, &error)) {
		fprintf(stderr, "drop type failure: %s\n", error.message);
		goto err;
	}
	// insert
	BULK_DEFINE();
	BULK_VAR(type_collect);
	for (i = 0; types[i]; i++) {
		if (insert_type(BULK_ARG(type_collect), i, types[i]))
			goto err;
	}
	BULK_EXEC();
	// create index
	BSON_APPEND_INT32(doc, "dirType", 1);
	if (!mongoc_collection_create_index(type_collect, doc, &opt, &error)) {
		fprintf(stderr, "create dirType index failure: %s\n", error.message);
		goto err;
	}

	bson_destroy(doc);
	doc = bson_new();

	// insert file mode
	// drop collection
	if (!mongoc_collection_drop(mode_collect, &error)) {
		fprintf(stderr, "drop mode failure: %s\n", error.message);
		goto err;
	}
	// insert
	BULK_VAR(mode_collect);
	for (i = 0; i <= 07777; i++) {
		if (insert_mode(BULK_ARG(mode_collect), i))
			goto err;
	}
	BULK_EXEC();
	// create index
	BSON_APPEND_INT32(doc, "dirMode", 1);
	if (!mongoc_collection_create_index(mode_collect, doc, &opt, &error)) {
		fprintf(stderr, "create dirMode index failure: %s\n", error.message);
		goto err;
	}

	bson_destroy(doc);
	doc = bson_new();

	// recursion directory
	// drop collection
	if (!mongoc_collection_drop(dir_collect, &error)) {
		fprintf(stderr, "drop dir failure: %s\n", error.message);
		goto err;
	}
	// recursion insert
	i = 1;
	if (uri == argv[1])
		i++;
	for (; i < argc; i++) {
		path = realpath(argv[i], sPath);
		if (!path)
			continue;
		name = strrchr(path, '/');
		if (!name)
			continue;
		*name++ = '\0';

		if (recursion_directory(dir_collect, type_collect, mode_collect, path, name, 0))
			goto err;
	}
	recursion_directory(dir_collect, type_collect, mode_collect, NULL, NULL, -1);
	update_type(type_collect, NULL);
	update_mode(mode_collect, -1);
	// create index
	BSON_APPEND_INT32(doc, "dirId", 1);
	if (!mongoc_collection_create_index(dir_collect, doc, &opt, &error)) {
		fprintf(stderr, "create dirId index failure: %s\n", error.message);
		goto err;
	}
	bson_destroy(doc);
	doc = bson_new();
	BSON_APPEND_INT32(doc, "parentId", 1);
	opt.unique = false;
	if (!mongoc_collection_create_index(dir_collect, doc, &opt, &error)) {
		fprintf(stderr, "create index failure: %s\n", error.message);
		goto err;
	}
	bson_destroy(doc);
	doc = bson_new();
	BSON_APPEND_INT32(doc, "dirName", 1);
	if (!mongoc_collection_create_index(dir_collect, doc, &opt, &error)) {
		fprintf(stderr, "create index failure: %s\n", error.message);
		goto err;
	}
	bson_destroy(doc);
	doc = bson_new();
	BSON_APPEND_INT32(doc, "pathName", 1);
	if (!mongoc_collection_create_index(dir_collect, doc, &opt, &error)) {
		fprintf(stderr, "create index failure: %s\n", error.message);
		goto err;
	}

	// find type and mode
	mongoc_cursor_t *cursor;
	bson_t *query;
	const bson_t *_doc;
	bson_iter_t iter;
	int64_t n = 0;

	printf("\nCount: %ld - maxDirId\nCount: %ld - mongo\n\n", maxDirId, mongoc_collection_count(dir_collect, MONGOC_QUERY_NONE, NULL, 0, 0, NULL, &error));

	query = BCON_NEW("nCounts", "{", "$gt", BCON_INT64(0), "}");
	cursor = mongoc_collection_find(type_collect, MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);
	while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &_doc)) {
		bson_iter_init(&iter, _doc);
		if (bson_iter_find(&iter, "dirType")) {
			printf("%5s: ", bson_iter_utf8(&iter, NULL));
			if (bson_iter_find(&iter, "nCounts")) {
				n += bson_iter_int64(&iter);
				printf("%ld\n", bson_iter_int64(&iter));
			} else {
				printf("0\n");
			}
		}
	}
	if (mongoc_cursor_error(cursor, &error)) {
		fprintf(stderr, "An error occurred: %s\n", error.message);
	}
	if (n > 0) {
		printf("Count: %ld - mongo\n", n);
		n = 0;
	}

	mongoc_cursor_destroy(cursor);

	printf("\n");

	cursor = mongoc_collection_find(mode_collect, MONGOC_QUERY_NONE, 0, 0, 0, query, NULL, NULL);
	while (mongoc_cursor_more(cursor) && mongoc_cursor_next(cursor, &_doc)) {
		bson_iter_init(&iter, _doc);
		if (bson_iter_find(&iter, "dirMode")) {
			printf("%05o: ", bson_iter_int32(&iter));
			if (bson_iter_find(&iter, "nCounts")) {
				n += bson_iter_int64(&iter);
				printf("%ld\n", bson_iter_int64(&iter));
			} else {
				printf("0\n");
			}
		}
	}
	if (mongoc_cursor_error(cursor, &error)) {
		fprintf(stderr, "An error occurred: %s\n", error.message);
	}
	if (n > 0) {
		printf("Count: %ld - mongo\n", n);
		n = 0;
	}

	bson_destroy(query);
	mongoc_cursor_destroy(cursor);

	err: bson_destroy(doc);
	mongoc_collection_destroy(dir_collect);
	mongoc_collection_destroy(mode_collect);
	mongoc_collection_destroy(type_collect);
	mongoc_client_destroy(client);
	mongoc_cleanup();

	return 0;
}
