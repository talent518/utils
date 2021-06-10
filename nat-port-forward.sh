#!/bin/bash -l

saddr=$1
sport=$2
daddr=$3
dport=$4

if [ -z "$saddr" -o -z "$sport" -o -z "$daddr" -o -z "$dport" ]; then
	echo "usage: $0 srcaddr srcport destaddr destport";
	exit 0;
fi

sudo iptables -t nat -A PREROUTING  -p tcp -d $saddr --dport $sport -j DNAT --to-destination $daddr:$dport
sudo iptables -t nat -A POSTROUTING -p tcp -s $daddr --sport $dport -j SNAT --to-source 1.1.1.1

