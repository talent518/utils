#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

#define RDIR_BLOCK_SIZE (sizeof(rdir_t)-sizeof(void*)*2)

typedef struct _rdir_t {
	unsigned int dirId;
	unsigned int parentId;
	char dirName[NAME_MAX];
	char pathName[PATH_MAX];
	char linkTarget[PATH_MAX];
	unsigned int nlinks;
	unsigned short dirMode;
	char dirType[5];
	unsigned int uid;
	unsigned int gid;
	unsigned long int size;
	char accessTime[20];
	char modifyTime[20];
	char changeTime[20];
	
	struct _rdir_t *prev;
	struct _rdir_t *next;
} rdir_t;

typedef struct _type_count_t {
	unsigned int REG;
	unsigned int DIR;
	unsigned int CHR;
	unsigned int BLK;
	unsigned int FIFO;
	unsigned int LNK;
	unsigned int SOCK;
} type_count_t;

static rdir_t *headDir = NULL;
static rdir_t *tailDir = NULL;
static unsigned int dirId = 0;
static type_count_t typeCount;
static unsigned int modeCount[07777+1];

int recursion_directory(const char *path, const char *name, unsigned int parentId) {
	static char linkTarget[PATH_MAX];
	int ret = 0, i;
	rdir_t *rdir;
	DIR *dir;
	struct dirent *d;
	char sPath[PATH_MAX], *dirType;
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
			typeCount.DIR++;
			break;
		case S_IFCHR:
			dirType = "CHR";
			typeCount.CHR++;
			break;
		case S_IFBLK:
			dirType = "BLK";
			typeCount.BLK++;
			break;
		case S_IFIFO:
			dirType = "FIFO";
			typeCount.FIFO++;
			break;
		case S_IFLNK:
			dirType = "LNK";
			typeCount.LNK++;
			break;
		case S_IFSOCK:
			dirType = "SOCK";
			typeCount.SOCK;
			break;
		case S_IFREG:
		default:
			dirType = "REG";
			typeCount.REG++;
			break;
	}
	
	rdir = (rdir_t*) malloc(sizeof(rdir_t));
	memset(rdir, 0, sizeof(rdir_t));
	
	rdir->dirId = ++dirId;
	rdir->parentId = parentId;
	strcpy(rdir->dirName, name);
	strcpy(rdir->pathName, sPath);
	
	if(S_ISLNK(st.st_mode)) {
		readlink(sPath, rdir->linkTarget, PATH_MAX);
	}
	
	rdir->nlinks = st.st_nlink;
	rdir->dirMode = st.st_mode & 07777;
	strcpy(rdir->dirType, dirType);
	rdir->uid = st.st_uid;
	rdir->gid = st.st_gid;
	rdir->size = st.st_size;

	t = st.st_atime;
	localtime_r(&t, &tm);
	strftime(rdir->accessTime, 20, "%F %T", &tm);

	t = st.st_mtime;
	localtime_r(&t, &tm);
	strftime(rdir->modifyTime, 20, "%F %T", &tm);

	t = st.st_ctime;
	localtime_r(&t, &tm);
	strftime(rdir->changeTime, 20, "%F %T", &tm);
	
	if(!headDir || !tailDir) {
		rdir->next = rdir->prev = NULL;
		headDir = tailDir = rdir;
	} else {
		tailDir->next = rdir;
		rdir->prev = tailDir;
		rdir->next = NULL;
		tailDir = rdir;
	}
	
	modeCount[st.st_mode & 07777]++;
	
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
		if(strcmp(d->d_name, ".") && strcmp(d->d_name, "..") && (ret = recursion_directory(sPath, d->d_name, dirId)) != 0) break;
	}

	closedir(dir);
	return ret;
}

int main(int argc, char *argv[]) {
	const char *dbfile, *path, *name;
	char sPath[PATH_MAX];
	int ret = 0, i;
	rdir_t *rdir, dir, *tmp;
	FILE *fp;
	
	if(argc < 2) goto usage;
	
	memset(&typeCount, 0, sizeof(typeCount));
	memset(modeCount, 0, sizeof(modeCount));
	
	dbfile = argv[1];
	if(argc == 2) {
		fp = fopen(dbfile, "r");
		if(!fp) {
			ret = 2;
			fprintf(stderr, "The file %s open error\n", dbfile);
			goto err;
		}
		
		while(!feof(fp)) {
			ret = fread(&dir, 1, RDIR_BLOCK_SIZE, fp);
			if(ret != RDIR_BLOCK_SIZE) {
				fprintf(stderr, "The file %s read error %d\n", dbfile, ret);
				ret = 4;
				goto rerr;
			}

			if(!strcmp(dir.dirType, "LNK")) {
				printf("%s %s => %s\n", dir.pathName, dir.dirType, dir.linkTarget);
			} else {
				printf("%s %s\n", dir.pathName, dir.dirType);
			}
			
			rdir = (rdir_t*)malloc(sizeof(rdir_t));
			memcpy(rdir, &dir, RDIR_BLOCK_SIZE);
			
			if(!strcmp(rdir->dirType, "DIR")) typeCount.DIR++;
			else if(!strcmp(rdir->dirType, "CHR")) typeCount.CHR++;
			else if(!strcmp(rdir->dirType, "BLK")) typeCount.BLK++;
			else if(!strcmp(rdir->dirType, "FIFO")) typeCount.FIFO++;
			else if(!strcmp(rdir->dirType, "LNK")) typeCount.LNK++;
			else if(!strcmp(rdir->dirType, "SOCK")) typeCount.SOCK++;
			else typeCount.REG++;
			
			modeCount[rdir->dirMode]++;
			
			if(!headDir || !tailDir) {
				rdir->next = rdir->prev = NULL;
				headDir = tailDir = rdir;
			} else {
				tailDir->next = rdir;
				rdir->prev = tailDir;
				rdir->next = NULL;
				tailDir = rdir;
			}
		}
		
	rerr:
		fclose(fp);

		printf("Readed data file from %s\n", dbfile);
	} else {
		for(i=2; i<argc; i++) {
			if(strcmp(".", argv[i]) && strcmp("..", argv[i])) {
				strcpy(sPath, argv[i]);
				name = basename(sPath);
				path = dirname(sPath);
			} else {
				name = "";
				path = argv[i];
			}

			ret = recursion_directory(path, name, 0);
			if(ret) break;
		}
		
		fp = fopen(dbfile, "w");
		
		if(!fp) {
			ret = 2;
			fprintf(stderr, "The file %s open error\n", dbfile);
			goto err;
		}
		
		rdir = headDir;
		while(rdir) {
			ret = fwrite(rdir, 1, RDIR_BLOCK_SIZE, fp);
			if(ret != RDIR_BLOCK_SIZE) {
				fprintf(stderr, "The file %s write error %d\n", dbfile, ret);
				ret = 3;
				goto werr;
			}
			rdir = rdir->next;
		}
		
		printf("Saved date file to %s\n", dbfile);
	werr:
		fclose(fp);
	}

err:
	rdir = headDir;
	while(rdir) {
		fprintf(stderr, "\ndirId: %u\nparentId: %u\ndirName: %s\npathName: %s\nlinkTarget: %s\nnlinks: %u\ndirMode: %05o\ndirType: %s\nuid: %u\ngid: %u\nsize: %lu\naccessTime: %s\nmodifyTime: %s\nchangeTime: %s\n", rdir->dirId, rdir->parentId, rdir->dirName, rdir->pathName, rdir->linkTarget, rdir->nlinks, rdir->dirMode, rdir->dirType, rdir->uid, rdir->gid, rdir->size, rdir->accessTime, rdir->modifyTime, rdir->changeTime);
		tmp = rdir;
		rdir = rdir->next;
		free(tmp);
	}
	
	printf("\n");
	for(i=0; i<=0777; i++) {
		if(modeCount[i]) printf("%05o: %u\n", i, modeCount[i]);
	}

	const char *types[] = {"REG", "DIR", "CHR", "BLK", "FIFO", "LNK", "SOCK"};
	unsigned int *typeCounts = (unsigned int *) &typeCount;
	printf("\n");
	for(i=0; i<7; i++) {
		if(typeCounts[i]) printf("%4s: %u\n", types[i], typeCounts[i]);
	}
	
	return -ret;
usage:
	fprintf(stderr, "usage: %s <dbfile> [directory]...\n", argv[0]);
	return 2;
}
