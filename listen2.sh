netstat -ntl | php -r '$s=file_get_contents("php://stdin");preg_match_all("/\:(\d+)\s+/", $s, $matches);$ports=array_unique($matches[1]);sort($ports, SORT_NUMERIC);echo implode(" ", $ports),PHP_EOL;'
