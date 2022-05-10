#!/bin/bash -l

lines=$(grep -A3 executable: /var/log/apport.log | tail -n3)
re='executable: ([^ ]+).+writing core dump to ([^ ]+)'

if [[ "$lines" =~ $re ]]; then
	printf "  Progrom: \033[31m%s\033[0m\n" "${BASH_REMATCH[1]}"
	printf "Core file: \033[31m%s\033[0m\n" "${BASH_REMATCH[2]}"
	echo
	gdb -q -ex bt -ex q "${BASH_REMATCH[1]}" "/var/lib/apport/coredump/${BASH_REMATCH[2]}"
fi


