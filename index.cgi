#!/bin/sh

n=$(expr index "$REQUEST_URI" ? - 1)
if [ $n -gt 0 ]; then
	path=${REQUEST_URI:0:$n}
else
	path=$REQUEST_URI
fi
tmpfile=$(mktemp -u)

cat - > $tmpfile <<!
<!DOCTYPE html>
<html>
<head>
<meta http-equiv="content-type" content="text/html;charset=utf-8">
<meta name="viewport" content="width=device-width, user-scalable=no, initial-scale=1.0, maximum-scale=1.0, minimum-scale=1.0">
<title>PATH: $path</title>
<style type="text/css">
html,body{font-size:14px;}
table,th,td{border:1px #ccc solid;border-collapse:collapse;border-spacing:0;padding:10px;}
tr:nth-child(odd){background:#eee;}
caption{font-size:18px;font-weight:bold;padding:0 0 10px;}

@media only screen and (max-width: 500px) {
	table,th,td{padding:5px;}
	table{width:100%;}
}
</style>
</head>
<body>
!

printf '<table>\n<caption>PATH: %s</caption>\n<tr><th>Name</th><th>Size</th><th>Type</th><th>Last modified</th></tr>' "$path" >> $tmpfile
test "$path" = "/" || printf '<tr><td colspan="4"><a href="..">..</a></td></tr>\n' >> $tmpfile
dir="$(dirname $PWD)${REQUEST_URI}"
ls --group-directories-first -a $dir | while read name; do
    if [ "$name" = "cgi-bin" -o "$name" = "." -o "$name" = ".." ]; then
        continue;
    fi
    file=${dir}${name}
    size=$(stat -c %s "$file")
    type=$(stat -c %F "$file")
    mtime=$(stat -c %y "$file")
    if [ -d "$file" ]; then
        printf '<tr><td><a href="%s">%s</a></td><td>%d</td><td>%s</td><td>%s</td></tr>\n' "$name/" "$name/" $size "$type" "${mtime:0:19}" >> $tmpfile
    else
        printf '<tr><td><a href="%s">%s</a></td><td>%d</td><td>%s</td><td>%s</td></tr>\n' "$name" "$name" $size "$type" "${mtime:0:19}" >> $tmpfile
    fi
done
printf '</table>\n</body>\n</html>' >> $tmpfile

length=$(stat -c %s "$tmpfile")

printf "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: $length\r\n\r\n"
cat $tmpfile
rm -f $tmpfile

