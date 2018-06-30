<?php
$imagick = new Imagick('battery_scale.png');

$frames = (int) $imagick->getImageProperty('Frames');

echo 'Frames: ', $frames, PHP_EOL;

$imagicks = [];
for($i=1;$i<=$frames;$i++) {
	$imagicks[] = ($_imagick = new Imagick());
	$_imagick->newImage($imagick->getImageWidth(), $imagick->getImageHeight()/$frames, 'rgb(0,0,0)', 'png');
}

echo 'Pixel process ...', PHP_EOL;
$iter = $imagick->getPixelIterator();
$percent = floor($imagick->getImageHeight()/20);
foreach($iter as $y=>$pixels) {
	if($y%$percent==0 && ($p = floor($y/$percent))<=20) {
		echo 'Percent: ', $p*5, '%', PHP_EOL;
	}

	foreach($pixels as $x=>$pixel) {
		$draw = new ImagickDraw();
		$draw->setFillColor($pixel);
		$draw->point($x, floor($y/6));
		$imagicks[$y%$frames]->drawImage($draw);
	}
}

foreach($imagicks as $i=>$_imagick) {
	$file = 'battery_scale_' . $i . '.png';
	echo 'Save: ', $file, PHP_EOL;
	$_imagick->writeImage($file);
	$_imagick->destroy();
}

$imagick->destroy();
