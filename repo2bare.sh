#!/bin/bash

if [ ! -d manifest ]; then
	git clone git://mirrors.ustc.edu.cn/aosp/platform/manifest.git manifest
fi

git -C manifest pull -v

truncate --size=0 git-bare.lst

I=0
xmllint --xpath '//manifest/project[not(contains(@groups,"notdefault"))]/@name' manifest/default.xml | sort | while read name; do
	I=$(expr $I + 1)
	eval $name
	echo $name >> git-bare.lst
done

N=$(wc -l git-bare.lst|awk '{print $1;}')
I=0
cat git-bare.lst | while read name; do
	I=$(expr $I + 1)
	p=$(expr $I \* 100 / $N)
	printf "\033[31mMK\033[0m \033[32m%d/%d\033[0m \033[33m%d%%\033[0m \033[34m%s\033[0m\n" $I $N $p "$name"
	if [ ! -d "$name.git" ]; then
		mkdir -p $(dirname "$name")
		git init --bare "$name.git"
		git -C "$name.git" remote add origin "git://mirrors.ustc.edu.cn/aosp/$name.git"
	fi
done
I=0
cat git-bare.lst | while read name; do
	I=$(expr $I + 1)
	p=$(expr $I \* 100 / $N)
	printf "\033[31mFT\033[0m \033[32m%d/%d\033[0m \033[33m%d%%\033[0m \033[34m%s\033[0m\n" $I $N $p "$name"
	git -C "$name.git" fetch -v
done
