alias listen='ss -ntl | php -B '\''$isFirst=1;$ports=[];'\'' -R '\''if(!$isFirst) $ports[preg_replace("/^.*:(\d+)\s.*$/", "$1",$argn)]=1;$isFirst=0;'\'' -E '\''echo implode(PHP_EOL, array_keys($ports)), PHP_EOL;'\'''

