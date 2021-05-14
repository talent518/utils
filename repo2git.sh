#!/bin/bash -l

set -e

I=0
repo forall -c 'echo "$REPO_PATH" "$(echo $REPO_RREV|cut -d / -f3)"' | while read dir ver; do
	I=$(expr $I + 1)
	echo -e "\033[31m$I\e[0m \033[33m$dir\033[0m"
	if [ -f "$dir.lock" ]; then
		continue
	fi

	N=0
	sh -c "cd '$dir';git ls-files" | while read line; do
		N=$(expr $N + 1)
		echo -n -e "\033[2K$N files\r"
		git add -f "$dir/$line"
	done
	
	echo -n -e "\033[2K"

	git repack --max-pack-size 500M
	git commit -a -m"$ver: $dir"
	touch "$dir.lock"
done

