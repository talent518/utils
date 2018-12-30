#!/usr/bin/env php
<?php

($n=count($_SERVER['argv']))<2 and die("Too few parameters\n");

$units = ['B', 'KB', 'MB', 'GB', 'TB'];
for($i=1; $i<$n; $i++):
	$path = $_SERVER['argv'][$i];
	if(!is_file($path)):
		//echo $path, ': NOT FILE', PHP_EOL;
		continue;
	endif;

	$size = filesize($path);
	$unit = min(4, (int)log($size, 1024));
	echo $path, ': ', round($size/pow(1024, $unit), 3), " ", $units[$unit], PHP_EOL;
endfor;

