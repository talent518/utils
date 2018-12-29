#!/usr/bin/env php
<?php

empty($_SERVER['argv'][1]) and die("Too few parameters\n");

$db = new SQLite3($_SERVER['argv'][1], SQLITE3_OPEN_READONLY);

$results = $db->query('SELECT name FROM sqlite_master WHERE type=\'table\' ORDER BY name');
while(($name = $results->fetchArray(SQLITE3_NUM)) !== false) {
	$name = reset($name);
	
	$_results = $db->query('SELECT COUNT(*) FROM ' . $name);
	$count = $_results->fetchArray(SQLITE3_NUM);
	$_results->finalize();
	
	$count = reset($count);
	
	echo $name, ': ', $count, PHP_EOL;
}
$results->finalize();
$db->close();
