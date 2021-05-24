#!/bin/bash -l

file=$1
if [ -z "$file" ]; then
	file=/boot/vmlinuz-$(uname -r)
fi
test -f "$file" || exit 1
echo skip... >&2
skip=$(grep -aobe "$(printf '\x1f\x8b\x08')" "$file" | awk -F: '{print $1;}')
echo extract... >&2
dd if="$file" bs=1 skip=$skip | zcat
echo ok >&2

