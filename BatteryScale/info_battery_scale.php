<?php
$imagick = new Imagick('battery_scale.png');

print_r($imagick->getImageProperties());

$imagick->destroy();
