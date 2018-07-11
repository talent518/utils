<?php

$options = getopt('h:p:u:w:m:l:r:H');

if(empty($options['p'])) $options['p'] = 21;

if(empty($options) || empty($options['h']) || empty($options['u']) || empty($options['w']) || empty($options['m']) || !in_array($options['m'], ['get', 'put', 'del']) || (empty($options['l']) && $options['m'] !== 'del') || empty($options['r'])) {
	echo @<<<EOT
Shell scripts for generating FTP operations based on local directories

usage: {$_SERVER['argv'][0]} <options>
options:
    -h <host>             FTP host
    -p <port>             FTP port
    -u <user>             FTP account
    -w <password>         FTP password
    -m <method>           FTP operation method, as follows:
        get               Recursively download directory
        put               Recursively upload directory
        del               Recursively delete directory
    -l <local>            Generating lists of directories and files based on local directories
    -r <remote>           Remote server directory
    -H                    Prohibition of highlighting

paramters:
    HOST="{$options['h']}"
    PORT="{$options['p']}"
    USER="{$options['u']}"
    PASS="{$options['w']}"
    METHOD="{$options['m']}"
    LOCAL="{$options['l']}"
    REMOTE="{$options['r']}"

EOT;
	exit;
}

if(!isset($options['H'])) {
	define('WAITING', "\033[34m ...\033[0m\r");
	define('CLEAR_COLOR', "\033[0m");
	define('MSG_COLOR', "\033[35m");
	define('SKIP_COLOR', "\033[33m");
	define('SUCCESS_COLOR', "\033[32m");
	define('FAILURE_COLOR', "\033[31m");
} else {
	define('WAITING', null);
	define('CLEAR_COLOR', null);
	define('MSG_COLOR', null);
	define('SKIP_COLOR', null);
	define('SUCCESS_COLOR', null);
	define('FAILURE_COLOR', null);
}

($ftp = @ftp_connect($options['h'], $options['p'])) or die(FAILURE_COLOR . 'Not connection' . CLEAR_COLOR . PHP_EOL);

@ftp_login($ftp, $options['u'], $options['w']) or die(FAILURE_COLOR . 'Login failed' . CLEAR_COLOR . PHP_EOL);

if($options['m'] === 'get') {
	if(!is_dir($options['l'])) {
		echo MSG_COLOR, 'Creating Directory ', CLEAR_COLOR, $options['l'], WAITING;
		mkdir($options['l'], 0755, true);
		echo MSG_COLOR, ' Created Directory ', CLEAR_COLOR, $options['l'], SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
	}
	
	ftpget($options['r'], $options['l']);
} elseif($options['m'] === 'put') {
	ftpput($options['l'], $options['r']);
} else {
	ftpremove($options['r']);
}

@ftp_close($ftp);

function ftpput($local, $remote) {
	global $ftp;
	
	if(is_file($local)) {
		echo MSG_COLOR, '    Uploading file ', CLEAR_COLOR, $remote, WAITING;
		$size = @ftp_size($ftp, $remote);
		if(($fsize = @filesize($local)) !== false && $size === $fsize) {
			echo MSG_COLOR, '     Uploaded file ', CLEAR_COLOR, $remote, SKIP_COLOR, ' skip', CLEAR_COLOR, PHP_EOL;
		} elseif(@ftp_put($ftp, $remote, $local, FTP_BINARY, $size)) {
			echo MSG_COLOR, '     Uploaded file ', CLEAR_COLOR, $remote, SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
		} else {
			echo MSG_COLOR, '     Uploaded file ', CLEAR_COLOR, $remote, FAILURE_COLOR, ' failure', CLEAR_COLOR, PHP_EOL;
		}
		
		return;
	}
	
	echo MSG_COLOR, 'Creating Directory ', CLEAR_COLOR, $remote, WAITING;
	if(@ftp_chdir($ftp, $remote)) {
		echo MSG_COLOR, ' Created Directory ', CLEAR_COLOR, $remote, SKIP_COLOR, ' skip', CLEAR_COLOR, PHP_EOL;
	} elseif(@ftp_mkdir($ftp, $remote)) {
		echo MSG_COLOR, ' Created Directory ', CLEAR_COLOR, $remote, SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
	} else {
		echo MSG_COLOR, ' Created Directory ', CLEAR_COLOR, $remote, FAILURE_COLOR, ' failure', CLEAR_COLOR, PHP_EOL;
	}
	
	$dh = opendir($local);
	if(!$dh) {
		return;
	}
	
	while (($file = readdir($dh)) !== false) {
		if($file === '.' || $file === '..') {
			continue;
		}
		
		ftpput($local . '/' . $file, $remote . '/' . $file);
	}
	
	closedir($dh);
}

function ftpget($path, $localpath = '.') {
	global $ftp;
	
	$list = ftplist($path);
	
	foreach($list as $std) {
		$remote = $path . '/' . $std->name;
		$local = $localpath . '/' . $std->name;
		if($std->isdir) {
			if(!is_dir($local)) {
				echo MSG_COLOR, 'Creating Directory ', CLEAR_COLOR, $local, WAITING;
				mkdir($local, 0755, true);
				echo MSG_COLOR, ' Created Directory ', CLEAR_COLOR, $local, SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
			}
			
			ftpget($remote, $local);
		} else {
			echo MSG_COLOR, '  Downloading file ', CLEAR_COLOR, $local, WAITING;
			if(($fsize = @filesize($local)) !== false && $std->size == $fsize) {
				echo MSG_COLOR, '   Downloaded file ', CLEAR_COLOR, $local, SKIP_COLOR, ' skip', CLEAR_COLOR, PHP_EOL;
			} elseif(@ftp_get($ftp, $local, $remote, FTP_BINARY, $fsize)) {
				echo MSG_COLOR, '   Downloaded file ', CLEAR_COLOR, $local, SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
			} else {
				echo MSG_COLOR, '   Downloaded file ', CLEAR_COLOR, $local, FAILURE_COLOR, ' failure', CLEAR_COLOR, PHP_EOL;
			}
		}
	}
}

function ftpremove($path) {
	global $ftp;
	
	$list = ftplist($path);
	
	foreach($list as $std) {
		$remote = $path . '/' . $std->name;
		if($std->isdir) {
			ftpremove($remote);
		} elseif(@ftp_delete($ftp, $remote)) {
			echo MSG_COLOR, '     Deleted file ', CLEAR_COLOR, $remote, SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
		} else {
			echo MSG_COLOR, 'Deleted Directory ', CLEAR_COLOR, $remote, FAILURE_COLOR, ' failure', CLEAR_COLOR, PHP_EOL;
		}
	}

	if(@ftp_rmdir($ftp, $path)) {
		echo MSG_COLOR, 'Deleted Directory ', CLEAR_COLOR, $path, SUCCESS_COLOR, ' success', CLEAR_COLOR, PHP_EOL;
	} else {
		echo MSG_COLOR, 'Deleted Directory ', CLEAR_COLOR, $path, FAILURE_COLOR, ' failure', CLEAR_COLOR, PHP_EOL;
	}
}

function ftplist($path) {
	global $ftp;

	$list = ftp_rawlist($ftp, '-a ' . $path);
	$paths = [];

	foreach ($list as $current) {
		$split = preg_split('/[ ]+/', $current, 9, PREG_SPLIT_NO_EMPTY);
		if (strcasecmp($split[0], 'total') && strcmp($split[8], '.') && strcmp($split[8], '..')) {
			$std = new stdClass();
			
			$std->isdir    = $split[0]{0} === 'd';
			$std->perms    = $split[0];
			$std->number   = $split[1];
			$std->owner    = $split[2];
			$std->group    = $split[3];
			$std->size     = $split[4];
			$std->datetime = $split[5] . ' ' . $split[6] . ' ' . $split[7];
			
			if($split[0]{0} === 'l') {
				@list($std->name, $std->link) = explode(' -> ', $split[8]);
			} else {
				$std->name = $split[8];
				$std->link = false;
			}
			
			$paths[] = $std;
		}
	}
	
	return $paths;
}

