#!/bin/bash -l

set -e

I=0
$(repo forall -c 'echo "$REPO_PATH" "$(echo $REPO_RREV|cut -d / -f3)"' | while read dir ver; do
	I=$(expr $I + 1)
	echo
	echo -e "\033[31m$I\e[0m \033[33m$dir\033[0m" >&2
	if [ -f "$dir.lock" ]; then
		continue
	fi

	N=0
	N=$(sh -c "cd '$dir';git ls-files" | while read line; do
		N=$(expr $N + 1)
		echo -n -e "\033[2K$N\r" >&2
		git add -f "$dir/$line" >&2
		echo
	done | wc -l)
	
	echo -n -e "\033[2K" >&2

	if [ $N -gt 0 ]; then
		git repack --max-pack-size 500M >&2
		git commit -a -m"$ver: $dir" >&2
		touch "$dir.lock" >&2
	fi
done | wc -l)

echo "completed $I projects"

