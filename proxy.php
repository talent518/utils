<?php
/**
 * <VirtualHost *:80>
 * 	ServerName php.example.com
 * 
 * 	<Proxy *>
 * 		Order Deny,Allow
 * 		Allow from all
 * 	</Proxy>
 * 
 * 	RequestHeader set Src-Url http://php.example.com
 * 	RequestHeader set Dst-Url http://182.119.138.180
 * 
 * 	ProxyPass / http://example.com/proxy.php/
 * 	ProxyPassReverse / http://example.com/proxy.php/
 * </VirtualHost>
 */


if(!isset($_SERVER['HTTP_SRC_URL']) || !isset($_SERVER['HTTP_DST_URL'])) {
	header('HTTP/1.0 404 Not Found');

	exit;
}

ini_set('default_mimetype', '');
ini_set('default_charset', '');

define('SRC_URL', $_SERVER['HTTP_SRC_URL']);
define('DST_URL', $_SERVER['HTTP_DST_URL']);

define('URL', DST_URL . substr($_SERVER['REQUEST_URI'], strlen($_SERVER['SCRIPT_NAME'])));

$_headers = apache_request_headers();

$_headers['Connection'] = 'Close';

unset($_headers['Src-Url'], $_headers['Dst-Url'], $_headers['Content-Type'], $_headers['Content-Length']);

$headers = array();

foreach($_headers as $key=>$val) {
	$headers[] = "$key: $val";
}

$ch = curl_init ();

if($_SERVER['REQUEST_METHOD'] === 'POST') {
	foreach($_FILES as $key => $FILE) {
		$_POST[$key] = (function_exists('curl_file_create') ? curl_file_create($FILE['tmp_name'], $FILE['type'], $FILE['name']) : sprintf('@%s;type=%s;filename=%s', $FILE['tmp_name'], $FILE['type'], $FILE['name']));
	}
	
	curl_setopt ( $ch, CURLOPT_POST, 1 );
	curl_setopt ( $ch, CURLOPT_POSTFIELDS, empty($_FILES) ? (($input = file_get_contents('php://input')) ? $input : http_build_query($_POST)) : $_POST );
}

curl_setopt ( $ch, CURLOPT_URL, URL );
curl_setopt ( $ch, CURLOPT_RETURNTRANSFER, 0 );
curl_setopt ( $ch, CURLOPT_HEADER, 0 );
curl_setopt ( $ch, CURLOPT_HEADERFUNCTION, 'curlHeaderFunctionHandler' );
curl_setopt ( $ch, CURLOPT_HTTPHEADER, $headers );

curl_exec ( $ch );

curl_close ( $ch );

function curlHeaderFunctionHandler($ch, $str) {
	if(strncasecmp($str, 'Transfer-Encoding: chunked', 26)) {
		if(!strncasecmp($str, 'Location:', 9)) {
			$str = preg_replace('/https?\:\/\/[^\/]+/', SRC_URL, $str);
		}
		header($str);
	}

	return strlen($str);
}
