<?php



  if(
    !isset($_POST["sourceImage"]) &&
    !isset($_POST["gamma"]) &&
    !isset($_POST["brightness"]) &&
    !isset($_POST["ditherMode"]) &&
    !isset($_POST["contrast"])
  ) {

echo "error";


  } else {
        require_once("utils/ImageToText.php");



    $sourceImage = $_POST["sourceImage"];
    $gamma = $_POST["gamma"];
    $brightness = $_POST["brightness"];
    $ditherMode = $_POST["ditherMode"];
    $contrast = $_POST["contrast"];

    $image = DitherImage($sourceImage, floatval($gamma), floatval($brightness), floatval($contrast), $ditherMode  );

    /* Set the format to PNG */
    $image->setImageFormat('jpg');

    file_put_contents ("images/last.jpg", $image);
    $text = DitherImageToString("images/last.jpg");
    file_put_contents ("images/last.txt", $text);

    echo '<img src="data:image/jpg;base64,'.base64_encode($image->getImageBlob()).'" alt="" />'; 

      /* Notice writeImages instead of writeImage */
    // echo '<img src="data:image/jpg;base64,'.base64_encode($imagick->getImageBlob()).'" alt="" />';



  }



	function DitherImage($sourceImage, $gamma, $brightness, $contrast, $ditherMode) {

    $path = $sourceImage;
    $imagick = new \Imagick(realpath($path));
   // $deskewImagick = clone $imagick;

    // crop bottom "BNF "
  $width = $imagick->getImageWidth();
  $height = $imagick->getImageHeight();

  $imagick->whiteBalanceImage();


    $imagick->scaleImage(384, 0, false);

   // $imagick->autoLevelImage();
      //-200 200 // -100 -100
   // $imagick->brightnessContrastImage(5, 10);

    // Resize
    //Resolution: 203 DPI (8 points / mm, 384 points per line)
   // $targetRatio = $imagick->getImageWidth() / 384 ;

    // todo : round to nearest 8 bits multi^le in height

    // Dither
//  $imagick->setImageType(\Imagick::IMGTYPE_GRAYSCALEMATTE);
      $imagick = $imagick->fxImage('intensity');
      $imagick->gammaImage($gamma, Imagick::CHANNEL_DEFAULT);
      $imagick->brightnessContrastImage($brightness, $contrast);
      $imagick->orderedDitherImage($ditherMode);

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


      //  $imagick->remapImage($palette, Imagick::DITHERMETHOD_RIEMERSMA);
    //$imagick->setImageType(imagick::IMGTYPE_COLORSEPARATION);
      //  $imagick->posterizeimage(2, Imagick::DITHERMETHOD_FLOYDSTEINBERG);


	}


?>