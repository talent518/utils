#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

int main(int argc, char *argv[]) {
	int i;
	char *user, *pwd, *encrypted;
	struct passwd *pw;
	struct spwd *spw;

	for(i=2; i<argc; i+=2) {
		user = argv[i-1];
		pwd = argv[i];
		
		pw = getpwnam(user); // 查询用户是否存在
		if(!pw) {
			printf("%s %s\n", user, errno ? strerror(errno) : "is not exists");
			continue;
		}
		
		spw = getspnam(user); // 读取shadow密码文件中的用户信息
		if(spw) {
			pw->pw_passwd = spw->sp_pwdp;
		} else if(errno) {
			printf("getspnam error: %s\n", strerror(errno));
			return 1;
		}
		
		encrypted = crypt(pwd, pw->pw_passwd);
		printf("%s %s\n", user, strcmp(encrypted, pw->pw_passwd) ? "false" : "true");
	}
	
	return 0;
}

