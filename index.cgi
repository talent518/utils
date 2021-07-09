#!/bin/sh

tmpfile=/tmp/index.html

cat - > $tmpfile <<!
<style type="text/css">
table,th,td{border:0 none;cellspacing:0;padding:5px;}
</style>
!

echo '<table><tr><th>Name</th><th>Size</th></tr>' >> $tmpfile
echo '<tr><td cellspan=2><a href="..">..</a></td></tr>' >> $tmpfile
dir="${HOME}${REQUEST_URI}"
ls -a $dir | while read name; do
    if [ "$name" = "cgi-bin" -o "$name" = "." -o "$name" = ".." ]; then
        continue;
    fi
    file=${dir}${name}
    size=$(stat -c %s "$file")
    if [ -d "$file" ]; then
        printf '<tr><td><a href="%s">%s</a></td><td>%d</td></tr>' "$name/" "$name/" $size >> $tmpfile
    else
        printf '<tr><td><a href="%s">%s</a></td><td>%d</td></tr>' "$name" "$name" $size >> $tmpfile
    fi
done
echo '</table>'

printf "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: $length\r\n\r\n"
cat $tmpfile
