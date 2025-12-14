<?php

// Always load required utilities
require_once("utils/ImageToText.php");
require_once("utils/AtkinsonDither.php");
require_once("utils/FloydSteinbergDither.php");

// Only execute this block if accessed directly (not included)
if (isset($_GET["sourceImage"]) || isset($_GET["ditherMode"])) {
    if(
        !isset($_GET["sourceImage"]) ||
        !isset($_GET["auto"]) ||
        !isset($_GET["gamma"]) ||
        !isset($_GET["brightness"]) ||
        !isset($_GET["ditherMode"]) ||
        !isset($_GET["contrast"])
    ) {
        // Debug output for missing parameters
        echo "Missing parameters:<br>";
        echo "sourceImage: " . (isset($_GET["sourceImage"]) ? $_GET["sourceImage"] : "MISSING") . "<br>";
        echo "auto: " . (isset($_GET["auto"]) ? $_GET["auto"] : "MISSING") . "<br>";
        echo "gamma: " . (isset($_GET["gamma"]) ? $_GET["gamma"] : "MISSING") . "<br>";
        echo "brightness: " . (isset($_GET["brightness"]) ? $_GET["brightness"] : "MISSING") . "<br>";
        echo "ditherMode: " . (isset($_GET["ditherMode"]) ? $_GET["ditherMode"] : "MISSING") . "<br>";
        echo "contrast: " . (isset($_GET["contrast"]) ? $_GET["contrast"] : "MISSING") . "<br>";
    } else {
        $sourceImage = $_GET["sourceImage"];
        $gamma = $_GET["gamma"];
        $auto = $_GET["auto"] == "true" ? true:false;
        $brightness = $_GET["brightness"];
        $ditherMode = $_GET["ditherMode"];
        $contrast = $_GET["contrast"];

        $image = DitherImage($sourceImage,$auto, floatval($gamma), floatval($brightness), floatval($contrast), $ditherMode);

        /* Set the format to BMP3 with 1-bit depth (black and white only) */


       // echo '<img src="images/last.bmp" alt="" />';

        /* Notice writeImages instead of writeImage */
         echo '<img src="data:image/jpg;base64,'.base64_encode($image->getImageBlob()).'" alt="" />';
    }
}



	function DitherImage($sourceImage,$auto, $gamma, $brightness, $contrast, $ditherMode) {
    $path = __DIR__ . '/' . $sourceImage;
    if (!file_exists($path)) {
        throw new Exception("Image file not found: " . $sourceImage);
    }

    $imagick = new \Imagick($path);
    // $deskewImagick = clone $imagick;
    $imagick->setImageBackgroundColor('white');
    // crop bottom "BNF "
    //$imagick->whiteBalanceImage();

    // Crop to 16:9 portrait aspect ratio (9:16)
    $originalWidth = $imagick->getImageWidth();
    $originalHeight = $imagick->getImageHeight();
    $targetAspectRatio = 3.0 / 4.0; // Portrait 16:9
    $currentAspectRatio = $originalWidth / $originalHeight;

    if ($currentAspectRatio > $targetAspectRatio) {
        // Image is too wide, crop width
        $newWidth = intval($originalHeight * $targetAspectRatio);
        $newHeight = $originalHeight;
        $x = intval(($originalWidth - $newWidth) / 2);
        $y = 0;
    } else {
        // Image is too tall, crop height
        $newWidth = $originalWidth;
        $newHeight = intval($originalWidth / $targetAspectRatio);
        $x = 0;
        $y = intval(($originalHeight - $newHeight) / 2);
    }

    $imagick->cropImage($newWidth, $newHeight, $x, $y);
    $imagick->setImagePage(0, 0, 0, 0); // Reset image page

    $imagick->scaleImage(384, 0, false);
    $width = $imagick->getImageWidth();
    $height = $imagick->getImageHeight();

    // Resize
    //Resolution: 203 DPI (8 points / mm, 384 points per line)
    // $targetRatio = $imagick->getImageWidth() / 384 ;

    // Dither
    $imagick->setImageType(\Imagick::IMGTYPE_GRAYSCALEMATTE);
  //  $imagick = $imagick->fxImage('intensity');

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
      $imagick = FloydSteinbergDither($imagick);
    } else if($ditherMode == "Quantize") {
        $imagick->quantizeImage(2, imagick::COLORSPACE_GRAY, 2, false, false);
    } else if($ditherMode == "Atkison") {
        $imagick = AtkinsonDither($imagick);
    }else {
        if(Imagick::getVersion()['versionNumber'] < 1800)
          $imagick->OrderedPosterizeImage ($ditherMode.",2");
        else 
      $imagick->orderedDitherImage($ditherMode.",2");
    }

    // Save as BMP3 with 1-bit depth
    $imagick->setImageFormat('bmp');
  //  $imagick->quantizeImage(2, Imagick::COLORSPACE_RGB, 0, true, false);
   $imagick->setImageType(Imagick::IMGTYPE_PALETTE);
   // $imagick->setImageDepth(4);
    $imagick->stripImage();

    file_put_contents ("images/last.bmp", $imagick);

    return $imagick;
    /**
     *    Threshold Maps for Ordered Dither Operations

    Map              Alias        Description
    ----------------------------------------------------
    threshold        1x1          Threshold 1x1 (non-dither)
    checks           2x1          Checkerboard 2x1 (dither)
    o2x2             2x2          Ordered 2x2 (dispersed)
    o3x3             3x3          Ordered 3x3 (dispersed)
    o4x4             4x4          Ordered 4x4 (dispersed)
    o8x8             8x8          Ordered 8x8 (dispersed)
    h4x4a            4x1          Halftone 4x4 (angled)
    h6x6a            6x1          Halftone 6x6 (angled)
    h8x8a            8x1          Halftone 8x8 (angled)
    h4x4o                         Halftone 4x4 (orthogonal)
    h6x6o                         Halftone 6x6 (orthogonal)
    h8x8o                         Halftone 8x8 (orthogonal)
    h16x16o                       Halftone 16x16 (orthogonal)
    c5x5b            c5x5         Circles 5x5 (black)
    c5x5w                         Circles 5x5 (white)
    c6x6b            c6x6         Circles 6x6 (black)
    c6x6w                         Circles 6x6 (white)
    c7x7b            c7x7         Circles 7x7 (black)
    c7x7w                         Circles 7x7 (white)

    Map              Alias        Description
    ----------------------------------------------------
    diag5x5          diag         Simple Diagonal Line Dither
    hlines2x2        hlines2      Horizontal Lines 2x2
    hlines2x2a                    Horizontal Lines 2x2 (bounds adjusted)
    hlines6x4                     Horizontal Lines 6x4
    hlines12x4       hlines       Horizontal Lines 12x4*/
	}


?>