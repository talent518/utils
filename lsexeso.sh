#!/bin/bash

set -e

lsall() {
	i=1
	args='echo "'
	while [ $i -le $# ]; do
		args="$args '\$$i'"
		i=$(expr $i + 1)
	done
	args="$args\""
	eval $args
}

strace -o /tmp/lsexeso.txt "$@"

echo
printf "\033[31mThe list of so files loaded by \"$1\" program is as follows(show all: \033[32mALL_SO=1$(lsall "$@")\033[31m):\033[0m\n"
cat /tmp/lsexeso.txt | grep ^openat | awk -F\" '{print $2;}' | egrep "\.so(\.[0-9])*$" | sort | uniq | while read f; do
	test -f "$f" -o -n "$ALL_SO" && echo "$f"
done
