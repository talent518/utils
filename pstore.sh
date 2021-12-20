#!/bin/sh

set -e

if [ ${1:-0} -gt 0 ]; then
	sleep $1
fi

mount -t pstore pstore /sys/fs/pstore

dmpDir=/var/pstore
dmpLog=$dmpDir/sh.log

files=$(find /sys/fs/pstore -name "*-ramoops-*" | xargs -n 100 echo)

mkdir -p $dmpDir

if [ ${#files} -eq 0 ]; then
	echo "Not found file" >> $dmpLog
	exit 0
fi

echo "Found files: '$files'" >> $dmpLog

curIdFile=$dmpDir/cur.id
if [ -f "$curIdFile" ]; then
	curId=$(cat $curIdFile)
else
	curId=0
fi
curDir="$(printf "%02d" $curId)-$(date +%Y%m%d-%H%M%S)"

nextId=$(expr $curId + 1)
if [ $nextId -gt 30 ]; then
	nextId=0
fi
echo $nextId > $dmpDir/cur.id
busybox rm -vrf $dmpDir/$(printf "%02d" $nextId)-* 2>&1 >> $dmpLog

echo "Move..." >> $dmpLog

mkdir -p $dmpDir/$curDir
busybox mv -v $files $dmpDir/$curDir 2>&1 >> $dmpLog
