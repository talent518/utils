#!/bin/bash

function color() {
	printf "%2d \033[%dm%s\033[0m\n" $1 $1 $2
}

color 31 Red
color 32 Green
color 33 Yellow
color 34 Blue
color 35 Purple
color 36 Cyan
color 37 Grey

echo

color 41 Red
color 42 Green
color 43 Yellow
color 44 Blue
color 45 Purple
color 46 Cyan
color 47 Grey

echo

i=0
n=60
while [ $i -lt 60 ]; do
	i=$(expr $i + 1)
	p=$(expr $i \* 100 / $n)
	printf "\033[2K\033[32m%d%%\033[0m \033[34m%d\033[0m/\033[35m%d\033[0m\r" $p $i $n
	sleep 0.33
done

echo

