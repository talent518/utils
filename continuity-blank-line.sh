#!/bin/bash -l

while [ $# -gt 0 ]; do
	z=$(nl -ba "$1" | grep -P '^\s*\d+\s*$' | awk '{if($1 == a+1) print $1;a=$1;}')
	test -z "$z" || echo $1 $z
	shift
done

