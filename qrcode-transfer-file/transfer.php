<?php
include './phpqrcode.php';

if(!isset($_REQUEST['file']) || !strlen($_REQUEST['file'])) {
	header('Content-Type: text/html; charset=utf-8');
	?><form action="<?php echo $_SERVER['REQUEST_URI'];?>" method="post" style="margin:0 auto;width:552px;">
		<input type="text" value="" name="file" style="float:left;box-sizing:border-box;width:400px;height:50px;line-height:48px;border:1px #aaa solid;border-radius:5px;padding:0 2px;font-size:18px;margin-right:20px;"/>
		<button type="submit" style="display:inline-block;width:132px;box-sizing:border-box;height:50px;font-size:18px;padding:0 20px;border-radius:5px;border:1px #aa2a1a solid;background:#d31a08;color:#f1f1f1;cursor:pointer;">生成二维码</button>
	</form>
	<?php
	exit;
}

$file = $_REQUEST['file'];

is_file($file) or die('The file does not exist.');

$fileName = basename($file);
$qrcodePath = './' . ( ($pos=strrpos($fileName,'.')) !== false ? substr($fileName,0,$pos) : $fileName . '.d' ) .'/';

is_dir($qrcodePath) or @mkdir($qrcodePath , 0755);

set_time_limit(0);

$fileHash=hash_file('md5',$file);
$fileCode=hash('crc32',$fileHash);
$fileSize=filesize($file);

$blockSize = 512;
$blockCount = ceil($fileSize/$blockSize);

// FileTransfer.Transfer
$str = <<<EOD
<tf>
	<fc>{$fileCode}</fc>
	<fn>{$fileName}</fn>
	<fh>{$fileHash}</fh>
	<fs>{$fileSize}</fs>
	<bc>{$blockCount}</bc>
	<bs>{$blockSize}</bs>
	<cbc>0</cbc>
</tf>
EOD;
QRcode::png(preg_replace("/\s+/", '', $str), $qrcodePath.'0.png', QR_ECLEVEL_L, 10);
file_put_contents($qrcodePath.'0.txt',$str);

$blockSeek=0;
$fp=fopen($file,'rb');
while($data=fread($fp,$blockSize)) {
	$blockSeek++;
	$blockData=base64_encode(gzencode($data,9));
	$str = <<<EOD
<bk>
	<fc>{$fileCode}</fc>
	<bd>{$blockData}</bd>
	<bs>{$blockSeek}</bs>
</bk>
EOD;
	QRcode::png(preg_replace("/\s+/", '', $str), $qrcodePath.$blockSeek.'.png', QR_ECLEVEL_L, 10);
	file_put_contents($qrcodePath.$blockSeek.'.txt',$str);
}
fclose($fp);

exit('ok');