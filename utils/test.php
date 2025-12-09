<?php


error_reporting(E_ALL);
ini_set('display_errors', 'On');
if (!extension_loaded('imagick')){
    echo 'imagick not installed';
}

    $imagick = new \Imagick(realpath("mel.PNG"));
            $imagick->scaleImage(38, 0, false);
       $imagick->transformImageColorspace(Imagick::COLORSPACE_GRAY);

     $width = $imagick->getImageWidth();
  $height = $imagick->getImageHeight();
    $dither = clone $imagick; 

    /* Atkinson Error Diffusion Kernel:

    1/8 is 1/8 * quantization error.

    +-------+-------+-------+-------+
    |       | Curr. |  1/8  |  1/8  |
    +-------|-------|-------|-------|
    |  1/8  |  1/8  |  1/8  |       |
    +-------|-------|-------|-------|
    |       |  1/8  |       |       |
    +-------+-------+-------+-------+

    Floyd-Steinberg 

    +-- -----+----- --+--- --+
    |        | Curr. |  7/16 |
    +---- ---|-------|-------|
    |  3/16  |  5/16 |  1/16 |
    +----- --|-------|-------|
    |        |  0    |       |
    +-- -----+-------+-------+

    */



/*///////////////////////////// */

    $white = 'rgb(255, 255, 255)';
    $black = 'rgb(0, 0, 0)';


    for($x=0; $x < $width; $x++){
        for($y=0; $y < $height; $y++){
            $img_arr[$x][$y] =  $dither->getImagePixelColor($x, $y)->getHSL()['luminosity'];
        }
    }


$iterator = $dither->getPixelIterator();
$x = 0;
$y = 0;
foreach ($iterator as $row=>$pixels) { // height
  foreach ( $pixels as $col=>$pixel ){ // width
    $color = $pixel->getHSL()['luminosity'];      // values are 0-255

    //$old = $image->getImagePixelColor($x, $y)->getHSL()['luminosity'];
        $old = $img_arr[$x][$y];
        echo $old ." ";
        /*
        $new = $old > 0.5 ? 1 : 0;
        if( $old > 0.5) {
            $pixel->setColor($white); // Only setting white pixels, because the image is already black.
        } else {
            $pixel->setColor($black); // Only setting white pixels, because the image is already black.
        }

        $quant_error = $old - $new;
        $error_diffusion = (1 / 8) * $quant_error;

        // Apply error diffusion to neighboring pixels
        if ($x + 1 < $width) {
           $img_arr[$x+1][$y] += $error_diffusion;
        }
        if ($x + 2 < $width) {
            $img_arr[$x+2][$y] += $error_diffusion;
        }
        if ($x - 1 >= 0 && $y + 1 < $height) {
            $img_arr[$x-1][$y+1] += $error_diffusion;
        }
        if ($y + 1 < $height) {
            $img_arr[$x][$y+1] += $error_diffusion;
        }
        if ($x + 1 < $width && $y + 1 < $height) {
            $img_arr[$x+1][$y+1] += $error_diffusion;
        }
         if ($y + 2 < $height) {
            $img_arr[$x][$y+2] += $error_diffusion;
        }*/
    $y++;
  }
  echo '<br>';
  $iterator->syncIterator();
  $x++;
}



/*////////////////////////////////////////////////////////

    $it = $dither->getPixelIterator();
    for ($y = 0; $y < $height - 1; $y++) {
        for ($x = 0; $x < $width; $x++) {

            //$old = $image->getImagePixelColor($x, $y)->getHSL()['luminosity'];
            $old = $img_arr[$x][$y];
            $pixel = $dither->getImagePixelColor($x, $y);

            $new = $old > 0.5 ? 1 : 0;
            if( $old > 0.5) {
                $pixel->setColor($white); // Only setting white pixels, because the image is already black.
            } else {
                $pixel->setColor($black); // Only setting white pixels, because the image is already black.
            }
    
            $quant_error = $old - $new;
            $error_diffusion = (1 / 8) * $quant_error;

            // Apply error diffusion to neighboring pixels
            if ($x + 1 < $width) {
               $img_arr[$x+1][$y] += $error_diffusion;
            }
            if ($x + 2 < $width) {
                $img_arr[$x+2][$y] += $error_diffusion;
            }
            if ($x - 1 >= 0 && $y + 1 < $height) {
                $img_arr[$x-1][$y+1] += $error_diffusion;
            }
            if ($y + 1 < $height) {
                $img_arr[$x][$y+1] += $error_diffusion;
            }
            if ($x + 1 < $width && $y + 1 < $height) {
                $img_arr[$x+1][$y+1] += $error_diffusion;
            }
             if ($y + 2 < $height) {
                $img_arr[$x][$y+2] += $error_diffusion;
            }

        }
    }
        $it->syncIterator();*/


   // $imagick = AtkinsonDither($imagick);
    $dither->setImageFormat('png');
    echo '<img src="data:image/jpg;base64,'.base64_encode($dither->getImageBlob()).'" alt="" />';




function quantizeImage($image_path, $numberColors, $colorSpace, $treeDepth, $dither)
{
    $imagick = new \Imagick(realpath($image_path));
        $imagick->scaleImage(384, 0, false);

       $imagick->transformImageColorspace(Imagick::COLORSPACE_GRAY);

    $imagick->quantizeImage($numberColors, $colorSpace, $treeDepth, $dither, false);
    $imagick->setImageFormat('png');
	return '<img src="data:image/jpg;base64,'.base64_encode($imagick->getImageBlob()).'" alt="" />';
}



function colorAdjust($image_path, $gamma,$brightness, $contrast, $auto)
{
    $imagick = new \Imagick(realpath($image_path));
       $imagick->scaleImage(384, 0, false);
  $imagick->setImageBackgroundColor('white'); 

      $imagick->transformImageColorspace(Imagick::COLORSPACE_GRAY);
       if($auto) {
        $imagick->normalizeImage();
        $imagick->autoLevelImage();
		      $imagick->gammaImage(1.2, Imagick::CHANNEL_DEFAULT);


       	  // $imagick->gammaImage($gamma, Imagick::CHANNEL_DEFAULT);
       } else {
		    //  $imagick = $imagick->fxImage('intensity');
		      $imagick->gammaImage($gamma, Imagick::CHANNEL_DEFAULT);
		      $imagick->brightnessContrastImage($brightness, $contrast);

		}
    $imagick->setImageFormat('png');
	return '<img src="data:image/jpg;base64,'.base64_encode($imagick->getImageBlob()).'" alt="" />';
}



/*

	echo ditherImage("mel.PNG");
	echo quantizeImage("mel.PNG", 2, imagick::COLORSPACE_GRAY, 18, false );
	echo quantizeImage("sauc.jpg", 2, imagick::COLORSPACE_GRAY, 18, false );
*/


/*
echo colorAdjust("mel.PNG",1,0, 0, false);
echo colorAdjust("mel.PNG",1,0, 0, true);

echo colorAdjust("sauc.jpg",1,0, 0, false);
echo colorAdjust("sauc.jpg",1,0, 0, true);

echo colorAdjust("me.PNG",1,0, 0, false);
echo colorAdjust("me.PNG",1,0, 0, true);*/

?>