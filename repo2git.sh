#!/bin/bash -l

set -e

Q=$(git config --global --default '' --get core.quotepath)

git config --global core.quotepath false

repo forall -c 'echo "$REPO_PATH" "$(echo $REPO_RREV|cut -d / -f3)"' > repo2git.lst
CNT=$(cat repo2git.lst | wc -l)

I=0
$(cat repo2git.lst | while read dir ver; do
	I=$(expr $I + 1)
	echo
	echo -e "\033[34m$(expr $I \* 100 / $CNT)\033[0m% => \033[31m$I\e[0m/\033[32m$CNT\e[0m \033[33m$dir\033[0m" >&2

	if [ -f "$dir.lock" ]; then
		echo $(git -C "$dir" ls-files | wc -l) files >&2
		continue
	fi

	git -C "$dir" clean -fdx
	git -C "$dir" ls-files > repo2git.files
	C=$(cat repo2git.files|wc -l)

	if [ $C -gt 0 ]; then
		N=0
		cat repo2git.files | while read line; do
			N=$(expr $N + 1)
			echo -n -e "\033[2K$(expr $N \* 100 / $C)% => $N/$C\r" >&2
			if [ "${line:0:1}" = '"' ]; then
				echo "\"$dir/${line:1:-1}\""
			else
				echo "\"$dir/$line\""
			fi
		done | xargs -n100 git add -f --ignore-removal

		echo -e "\033[2K$C files">&2

		git commit -v -m"$ver: $dir" "$dir" >&2
		touch "$dir.lock" >&2
	else
		echo -e "\033[34m0 files\033[0m" >&2
	fi
done | wc -l)

echo "completed $CNT projects"

if [ ! -f ".lock" ]; then
	I=0
	repo manifest | xmllint --xpath '//manifest/project/*/@dest' - 2>/dev/null | while read dest; do
		I=$(expr $I + 1)
		eval $dest
		echo "\"$dest\""
		echo -n -e "\033[2Kadd soft link or copy file for $I files\r">&2
	done > repo2git.files
	if [ $(cat repo2git.files|wc -l) -gt 0 ]; then
		echo
		cat repo2git.files | xargs git add -f
		cat repo2git.file | xargs git commit -v -m"add soft link or copy file" >&2
		touch ".lock"
	else
		echo none soft link or copy file >&2
	fi
fi

if [ -z "$Q" ]; then
	git config --global --unset core.quotepath
else
	git config --global core.quotepath $Q
fi

