#!/bin/bash --login

ARG0=$0
HOST=
PORT=21
USER=
PASS=
METHOD=
LOCAL=
REMOTE=

usage() {
	cat - <<!
Shell scripts for generating FTP operations based on local directories

usage: $ARG0 <options>
options:
    -h <host>             FTP host
    -p <port>             FTP port
    -u <user>             FTP account
    -w <password>         FTP password
    -m <method>           FTP operation method, as follows:
        get               Recursively download directory
        put               Recursively upload directory
        del               Recursively delete directory
    -l <local>            Generating lists of directories and files based on local directories
    -r <remote>           Remote server directory

paramters:
    HOST="$HOST"
    PORT="$PORT"
    USER="$USER"
    PASS="$PASS"
    METHOD="$METHOD"
    LOCAL="$LOCAL"
    REMOTE="$REMOTE"
!
	exit 1
}

# 粘贴命令参数
while getopts "h:p:u:w:m:l:r:" arg; do
	case $arg in
		h)
			HOST=$OPTARG
			;;
		p)
			PORT=$OPTARG
			;;
		u)
			USER=$OPTARG
			;;
		w)
			PASS=$OPTARG
			;;
		m)
			METHOD=$OPTARG
			;;
		l)
			LOCAL=$OPTARG
			;;
		r)
			REMOTE=$OPTARG
			;;
		?)
			usage
			;;
	esac
done

if [[ "$HOST" == "" || "$PORT" == "" || "$USER" == "" || "$PASS" == "" || "$METHOD" != "get" && "$METHOD" != "put" && "$METHOD" != "del" || "$LOCAL" == "" || "$REMOTE" == "" ]]; then
	usage
fi

cat - <<!
#!/bin/bash --login

ftp -nv $HOST $PORT <<!!
user $USER $PASS
bin
prompt
!

case $METHOD in
	get)
		echo cd $REMOTE
		find $LOCAL -type d | awk -v q='"' 'NR>1{a=substr($1, length("'$LOCAL'")+2);print "!mkdir " q a q;}'
		find $LOCAL -type f | awk -v q='"' '{a=substr($1, length("'$LOCAL'")+2);print "get " q a q " " q a q;}'
		;;
	put)
		echo mkdir $REMOTE
		echo cd $REMOTE

		find $LOCAL -type d | awk -v q='"' 'NR>1{a=substr($1, length("'$LOCAL'")+2);print "mkdir " q a q;}'
		find $LOCAL -type f | awk -v q='"' '{a=substr($1, length("'$LOCAL'")+2);print "put " q a q " " q a q;}'
		;;
	del)
		echo cd $REMOTE

		find $LOCAL -type f | awk -v q='"' '{a=substr($1, length("'$LOCAL'")+2);print "delete " q a q;}'
		find $LOCAL -type d | awk -v q='"' 'NR>1{a=substr($1, length("'$LOCAL'")+2);print "rmdir " q a q;}' | sort -r

		echo rmdir $REMOTE
		;;
esac

echo !!

exit 0

