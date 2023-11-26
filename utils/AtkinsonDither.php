<?php


function AtkinsonDither($imagick)
{
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

	$white = 'rgb(255, 255, 255)';
	$black = 'rgb(0, 0, 0)';

	for($x=0; $x < $width; $x++){
	    for($y=0; $y < $height; $y++){
	        $img_arr[$x][$y] =  $dither->getImagePixelColor($x, $y)->getHSL()['luminosity'];
	    }
	}

    for ($y = 0; $y < $height - 1; $y++) {
        for ($x = 0; $x < $width; $x++) {

            //$old = $image->getImagePixelColor($x, $y)->getHSL()['luminosity'];
      		$old = $img_arr[$x][$y];

            $new = $old > 0.5 ? 1 : 0;
            if( $old > 0.5) {
            	$dither->setImagePixelColor($x, $y, $white); // Only setting white pixels, because the image is already black.
            } else {
            	$dither->setImagePixelColor($x, $y, $black); // Only setting white pixels, because the image is already black.
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

	return $dither;
}

?>