<?php
/**
 * 响应浏览器另存为文件（下载），支持大文件下载，支持断点续传
 *
 * @param String  $filePath   要下载的文件路径
 * @param String  $name   文件名称,为空则与下载的文件名称一样
 * @param integer $speed 下载速度(单位：KB)
 * @param boolean debug 是否开启调试模式
 */
function downloadFile($filePath, $saveName = null, $speed = 512, $debug = false) {
	if (! is_readable ( $filePath )) {
		header ( 'HTTP/1.1 404 Not Found' );
		return;
	}

	set_time_limit(0);

	if($saveName === null) {
		$saveName = basename($filePath);
	}

	header('Content-Type: application/octet-stream');

	$ua = (isset($_SERVER['HTTP_USER_AGENT']) ? $_SERVER['HTTP_USER_AGENT'] : '');
	if(preg_match('/MSIE/', $ua)) {
		header('Content-Disposition: attachment; filename="' . str_replace('+', '%20', urlencode($saveName)) . '"');
	} elseif(preg_match('/Firefox/', $ua)) {
		header('Content-Disposition: attachment; filename*="utf8\'\'' . $saveName . '"');
	} else {
		header('Content-Disposition: attachment; filename="' . $saveName . '"');
	}

	if(function_exists('apache_get_modules') && in_array ( 'mod_xsendfile', apache_get_modules () )) {
		header('X-Sendfile: ' . $filePath);
		exit;
	}

	$speed = ($speed < 16 ? 16 : ($speed > 4096 ? 4096 : intval($speed))) * 1024;

	$fp = fopen($filePath, 'rb');
	$fileSize = filesize($filePath);

	header('ETag: ' . md5($filePath) . '-' . crc32(filemtime($filePath)));
	header('Accept-Ranges: bytes');

	if(isset($_SERVER['HTTP_RANGE']) && !strncasecmp($_SERVER['HTTP_RANGE'], 'bytes=', 6)) { // 使用续传
		@list($start, $end) = explode('-', substr($_SERVER['HTTP_RANGE'], 6));
		$start = max($start, 0);
		$end = $end ? min($end, $fileSize) : $fileSize;

		$length = $end - $start + 1;

		$debug and file_put_contents('./range.log', $start . '-' . $end . '=' . $length . PHP_EOL . PHP_EOL, FILE_APPEND);

		header('HTTP/1.1 206 Partial Content');
		
		// 剩余长度
		header('Content-Length: ' . $length );
		
		// range信息
		header(sprintf('Content-Range: bytes %s-%s/%s', $start, $end, $fileSize));
		
		// fp指针跳到断点位置
		fseek($fp, sprintf('%u', $start));

		ob_end_clean();
		ob_start();
		while(!feof($fp) && $length > 0){
			echo $buf = fread($fp, min($speed, $length));
			$length -= strlen($buf);
			ob_flush();
			flush();
			$debug and sleep(1); // 用于测试,减慢下载速度
		}
	} else {
		header('HTTP/1.1 200 OK');
		header('Content-Length:' . $fileSize);

		ob_end_clean();
		ob_start();
		while(!feof($fp)){
			echo fread($fp, $speed);
			ob_flush();
			flush();
			$debug and sleep(1); // 用于测试,减慢下载速度
		}
	}

	fclose($fp);
}

downloadFile('./jdk1.7-src.zip', null, 128, true);
