<?php

if(isset($_SERVER['argv'][1])) {
	$frames = (int) $_SERVER['argv'][1];
} else {
	$imagick = new Imagick('battery_scale.png');
	$frames = (int) $imagick->getImageProperty('Frames');
	$imagick->destroy();
}

if($frames<2) {
	$frames = 6;
}

$N = floor(log10($frames-1)+1);
$imagicks = [];
$files = [];
$width = $height = 0;
for($i=0; $i<$frames; $i++) {
	$file = 'battery_scale_' . str_pad($i, $N, '0', STR_PAD_LEFT) . '.png';
	$files[] = $file;
	$imagicks[] = ($imagick = new Imagick($file));

	if($width && $imagick->getImageWidth() != $width) {
		throw new Exception('The image width of file ' . $file . ' is not consistent with ' . $width);
	} elseif(!$width) {
		$width = $imagick->getImageWidth();
	}

	if($height && $imagick->getImageHeight() != $height) {
		throw new Exception('The image height of file ' . $file . ' is not consistent with ' . $width);
	} elseif(!$height) {
		$height = $imagick->getImageHeight();
	}
}
echo 'Frames: ', $frames, PHP_EOL;

$imagick = new Imagick();
$imagick->newImage($width, $height * $frames, 'rgba(0,0,0,0)', 'png');
$imagick->setImageProperty('Frames', $frames);

echo 'Merge process ...', PHP_EOL;
foreach($imagicks as $i=>$_imagick) {
	echo $files[$i], PHP_EOL;
	$iter = $_imagick->getPixelIterator();
	foreach($iter as $y=>$pixels) {
		foreach($pixels as $x=>$pixel) {
			$draw = new ImagickDraw();
			$draw->setFillColor($pixel);
			$draw->point($x, $y*6+$i);
			$imagick->drawImage($draw);
		}
	}
	$_imagick->destroy();
}

echo 'Save: battery_scale.png ...', PHP_EOL;
file_put_contents('battery_scale.png', $imagick->getImageBlob());
$imagick->destroy();
