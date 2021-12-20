#!/bin/bash --login

if [ -z "$1" -o -z "$2" -o ! -d "$1" -o ! -d "$2" ]; then
    echo "usage: $0 dir1 dir2"
    exit 1
fi

D=$(cd `dirname $0`;pwd)
N1=$(basename $1)
N2=$(basename $2)

pushd $1
find . -type f | sort | xargs md5sum > $D/$N1.txt
popd

pushd $2
find . -type f | sort | xargs md5sum > $D/$N2.txt
popd

diff $D/$N1.txt $D/$N2.txt

