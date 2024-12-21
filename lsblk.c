#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mntent.h>
#include <sys/statvfs.h>
#include <linux/loop.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define MNTENT_FILE "/proc/mounts"

int main(int argc, char *argv[]) {
    struct mntent *mnt;
    struct statvfs vfs;
    int fd;
    FILE *mnt_fp;
    struct loop_info64 loop;
    
    mnt_fp = setmntent(MNTENT_FILE, "r");
    if(mnt_fp == NULL) {
        perror("setmntent()");
        return 1;
    }

    while((mnt = getmntent(mnt_fp)) != NULL) {
        printf("%s => %s\n", mnt->mnt_fsname, mnt->mnt_dir);
        printf("    mnt_fsname: %s\n", mnt->mnt_fsname);
        printf("    mnt_dir: %s\n", mnt->mnt_dir);
        printf("    mnt_type: %s\n", mnt->mnt_type);
        printf("    mnt_opts: %s\n", mnt->mnt_opts);
        printf("    mnt_freq: %d\n", mnt->mnt_freq);
        printf("    mnt_passno: %d\n", mnt->mnt_passno);
        if(statvfs(mnt->mnt_dir, &vfs)) {
            perror("statvfs()");
            continue;
        }
        printf("    f_bsize: %lu\n", vfs.f_bsize);
        printf("    f_frsize: %lu\n", vfs.f_frsize);
        printf("    f_blocks: %lu\n", vfs.f_blocks);
        printf("    f_bfree: %lu\n", vfs.f_bfree);
        printf("    f_bavail: %lu\n", vfs.f_bavail);
        printf("    f_files: %lu\n", vfs.f_files);
        printf("    f_ffree: %lu\n", vfs.f_ffree);
        printf("    f_favail: %lu\n", vfs.f_favail);
        printf("    f_fsid: %lu\n", vfs.f_fsid);
        printf("    f_flag: %lu\n", vfs.f_flag);
        printf("    f_namemax: %lu\n", vfs.f_namemax);

        if(!strncmp(mnt->mnt_fsname, "/dev/loop", 9)) {
            fd = open(mnt->mnt_fsname, O_RDONLY);
            if(fd == -1) {
                perror("open()");
                continue;
            }
            if(ioctl(fd, LOOP_GET_STATUS64, &loop) == -1) {
                perror("ioctl()");
                close(fd);
                continue;
            }
            close(fd);

            printf("    lo_device: %llu\n", loop.lo_device);
            printf("    lo_inode: %llu\n", loop.lo_inode);
            printf("    lo_rdevice: %llu\n", loop.lo_rdevice);
            printf("    lo_offset: %llu\n", loop.lo_offset);
            printf("    lo_sizelimit: %llu\n", loop.lo_sizelimit);
            printf("    lo_number: %u\n", loop.lo_number);
            printf("    lo_encrypt_type: %u\n", loop.lo_encrypt_type);
            printf("    lo_encrypt_key_size: %u\n", loop.lo_encrypt_key_size);
            printf("    lo_flags: %u\n", loop.lo_flags);
            printf("    lo_file_name: %s\n", loop.lo_file_name);
            printf("    lo_crypt_name: %s\n", loop.lo_crypt_name);
            printf("    lo_encrypt_key: %s\n", loop.lo_encrypt_key);
            printf("    lo_init: %llu %llu\n", loop.lo_init[0], loop.lo_init[1]);
        }
    }

    endmntent(mnt_fp);

    return 0;
}
