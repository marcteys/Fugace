<?php

function DitherImage($sourceImage,$auto, $gamma, $brightness, $contrast, $ditherMode) {

    $path = $sourceImage;
    $imagick = new \Imagick(realpath($path));

    // If image has transparency (PNG with alpha channel), flatten it onto white background
    if ($imagick->getImageAlphaChannel()) {
        $imagick->setImageBackgroundColor('white');
        $imagick = $imagick->flattenImages();
    }

    $imagick->scaleImage(384, 0, false);
    $width = $imagick->getImageWidth();
    $height = $imagick->getImageHeight();

    // Dither
    $imagick->setImageType(\Imagick::IMGTYPE_GRAYSCALEMATTE);

    if($auto) {
        $imagick->normalizeImage();
        $imagick->autoLevelImage();
        $imagick->gammaImage(1.7, Imagick::CHANNEL_DEFAULT);
        $imagick->brightnessContrastImage(25, -15);
    } else {
        $imagick->gammaImage($gamma, Imagick::CHANNEL_DEFAULT);
        $imagick->brightnessContrastImage($brightness, $contrast);
    }

    if($ditherMode == "FloydSteinberg") {
        require_once("FloydSteinbergDither.php");
        $imagick = FloydSteinbergDither($imagick);
    } else if($ditherMode == "Quantize") {
        $imagick->quantizeImage(2, imagick::COLORSPACE_GRAY, 2, false, false);
    } else if($ditherMode == "Atkison") {
        require_once("AtkinsonDither.php");
        $imagick = AtkinsonDither($imagick);
    }else {
        if(Imagick::getVersion()['versionNumber'] < 1800)
            $imagick->OrderedPosterizeImage ($ditherMode.",2");
        else
            $imagick->orderedDitherImage($ditherMode.",2");
    }

    return $imagick;
}

?>
