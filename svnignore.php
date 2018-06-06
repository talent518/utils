#!/bin/env php
<?php
$path = '.';
if(!empty($_SERVER['argv'][1])) {
	$path = $_SERVER['argv'][1];
}

$exitCode = 0;
ob_start();
ob_implicit_flush(false);
passthru('svn status --xml "' . $path . '" 2>&1', $exitCode);
$xml = ob_get_clean();

if($exitCode) {
	echo $xml;
	exit($exitCode);
}

$entrys = simplexml_load_string($xml)->target->entry;

$paths = [];
foreach($entrys as $xml) {
	if($xml->{'wc-status'}['item'] != 'unversioned') {
		continue;
	}
	
	$path = (string) $xml['path'];
	
	$paths[dirname($path)][] = basename($path);
}

$fname = tempnam(dirname(__FILE__), basename(__FILE__, '.php') . '-');

foreach($paths as $path => $names) {
	echo $path, PHP_EOL, '    ', implode(PHP_EOL . '    ', $names), PHP_EOL;
	
	ob_start();
	ob_implicit_flush(false);
	system('svn propget svn:ignore "' . $path . '" > ' . $fname . ' 2>/dev/null');
	ob_end_clean();
	
	@file_put_contents($fname, implode(PHP_EOL, $names), FILE_APPEND);
	
	system('svn propset svn:ignore -F "' . $fname . '" "' . $path. '" >/dev/null');
	@unlink($fname);
}

