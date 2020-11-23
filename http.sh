#!/bin/bash

host=${1:-baidu.com}
port=${2:-80}

# connect
exec 8<>/dev/tcp/$host/$port
if [ $? -ne 0 ]; then
	echo connect to $ip:80 failure
	exit 1
fi

# request
echo -e "GET / HTTP/1.1\r\nHOST: $host\r\nConnection: Close\r\n\r\n" >&8

# response
cat <&8

# close input and output
exec 8<&-
exec 8>&-

exit 0
