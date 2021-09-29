# ext4磁盘配额
quota 子系统用于限制磁盘的使用量。<br/>
从限制的主体进行分类，quota 包含 user quota、group quota 与 project quota 三部分。顾名思义，user quota、group quota 限制的主体分别是 user、user group，而 project quota 限制的主体则是 project id。当一个目录下的所有子目录和文件拥有相同的 project id 时，就可以限制一个目录下总的磁盘使用量。<br/><br/>
quota 子系统其实是一项“古老”的特性，user quota 与 group quota 早在 Linux v2.6 就开始支持，而 project quota 则来得相对晚一些。project quota 特性最初来源于 XFS，Linux v4.5 开始 ext4 才正式支持 project quota。

### 0. 安装quota工具
```sh
sudo apt install quota quotatool
```

### 1. 创建大小为1GB的quota.ext4和quota-proj.ext4文件
```sh
dd if=/dev/zero of=quota.ext4 bs=1M count=1024
dd if=/dev/zero of=quota-proj.ext4 bs=1M count=1024
```

### 2. 把quota.ext4和quota-proj.ext4文件格式化为ext4文件系统
```sh
mkfs.ext4 quota.ext4
mkfs.ext4 -O project,quota -E quotatype=prjquota quota-proj.ext4
```

### 3. 磁盘自动挂载配置: /etc/fstab
```fstab
/home/abao/quota.ext4 /home/abao/quota ext4 defaults,quota,usrquota,grpquota 0 0
/home/abao/quota-proj.ext4 /home/abao/quota-proj ext4 defaults,prjquota 0 0
```
**挂载选项：**
1. quota：默认为usrquota
2. usrquota：开启基于用户进行占用空间与inode数限制
3. grpquota：开启基于用户组进行占用空间与inode数限制
4. prjquota：开启基于项目ID进行占用空间与inode数限制

### 4. 创建用户和组的配额配置数据文件(bin)
```sh
sudo quotacheck -cugv /home/abao/quota
```
执行以上命令，会在/home/abao/quota目录下生成二进制格式的aquota.user和aquota.group文件。

### 5. 开启磁盘配额
```sh
# 开启基于用户和组的磁盘配额
sudo quotaon -ugpv /home/abao/quota

# 开启基于项目ID的磁盘配额
sudo quotaon -Ppv /home/abao/quota-proj
```

### 6. 设置磁盘配额
```sh
# setquota -u <username> <block-softlimit> <block-hardlimit> <inode-softlimit> <inode-hardlimit> <mntpoint>
sudo setquota -u abao 100000 102400 100 128 /home/abao/quota

# setquota -P <projectid> <block-softlimit> <block-hardlimit> <inode-softlimit> <inode-hardlimit> <mntpoint>
sudo setquota -P 123 100000 102400 100 128 /home/abao/quota-proj
```
* inode：可以创建文件或者目录的数量
* block：可以存储的容量大小
* softlimit(软限制): 最低限制容量，可以被超过，但会有警告信息，超过的部分会保存到宽限时期到期。一般是硬限制的80%，单位为KB
* hardlimit(硬限制): 绝对不能被超过限制。达到hard时，系统会禁止继续增加新的文件
* 宽限时间（一般为7天）单位为KB
  当用户使用的空间超过了软限制但还没达到硬限制，在这个宽限的时间到期前必须将超过的数据降低到软限制以下（默认是7天），当宽限时间到期，系统将自动清除超过的数据。

### 7. 用户磁盘配额测试
```sh
sudo mkdir /home/abao/quota/abao
sudo chown abao.abao /home/abao/quota/abao

# 超额写入文件
dd if=/dev/zero of=/home/abao/quota/abao/test.dat bs=1M count=128
```
* dd命令报 “Disk quota exceeded”错误代表限额已生效。

### 8. 项目ID磁盘配额测试
```sh
sudo mkdir /home/abao/quota-proj/123
sudo chown abao.abao /home/abao/quota-proj/123

# chattr +P -p <projectid> <file/directory>...
sudo chattr +P -p 123 /home/abao/quota-proj/123

# 超额写入文件
dd if=/dev/zero of=/home/abao/quota-proj/123/test.dat bs=1M count=128

# 查看项目ID属性列表
lsattr -lpv
```
* +P: 添加继承属性，子目录以及文件继承该属性
* dd命令报 “Disk quota exceeded”错误代表限额已生效。

### 9. 查询磁盘配额
```sh
sudo repquota -ugPa
```

### 10. 参考
* [Ext4 开启 project quota](https://developer.aliyun.com/article/763211)
* [linux磁盘配额详解（EXT4和XFS）](https://blog.csdn.net/shengjie87/article/details/106833741)
* [Disk quota](https://wiki.archlinux.org/title/Disk_quota)

### 11. kernel config
```
CONFIG_QUOTA=y
CONFIG_QUOTACTL=y
CONFIG_QUOTA_NETLINK_INTERFACE=y
CONFIG_QUOTA_TREE=y
CONFIG_PRINT_QUOTA_WARNING=y
CONFIG_QFMT_V2=y
```
