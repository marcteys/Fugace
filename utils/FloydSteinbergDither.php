<?php


function FloydSteinbergDither($imagick)
{
  $width = $imagick->getImageWidth();
  $height = $imagick->getImageHeight();
	$dither = clone $imagick;

	/* Floyd-Steinberg Error Diffusion Kernel:

	The current pixel distributes quantization error to neighboring pixels:

	+--------+-------+-------+
	|        | Curr. |  7/16 |
	+--------|-------|-------|
	|  3/16  |  5/16 |  1/16 |
	+--------|-------|-------|

	Error is distributed to:
	- Right pixel:  7/16 of error
	- Bottom-left:  3/16 of error
	- Bottom:       5/16 of error
	- Bottom-right: 1/16 of error
	*/

	// Export all pixels at once (MUCH faster than getImagePixelColor per pixel)
	$pixels = $dither->exportImagePixels(0, 0, $width, $height, "I", Imagick::PIXEL_CHAR);

	// Convert to flat 1D array of normalized values (0.0 to 1.0) - faster than 2D array
	$totalPixels = $width * $height;
	$img_arr = array();
	for ($i = 0; $i < $totalPixels; $i++) {
	    $img_arr[$i] = $pixels[$i] / 255.0;
	}

	// Prepare output pixel array
	$output_pixels = array_fill(0, $totalPixels, 0);

	// Pre-calculate Floyd-Steinberg error distribution factors
	$error_right = 7.0 / 16.0;
	$error_bottom_left = 3.0 / 16.0;
	$error_bottom = 5.0 / 16.0;
	$error_bottom_right = 1.0 / 16.0;

    for ($y = 0; $y < $height; $y++) {
        $rowOffset = $y * $width;
        $nextRowOffset = $rowOffset + $width;

        for ($x = 0; $x < $width; $x++) {
            $idx = $rowOffset + $x;
            $old = $img_arr[$idx];
            $new = $old > 0.5 ? 1 : 0;

            // Store the quantized pixel (0 = black, 255 = white)
            $output_pixels[$idx] = $new * 255;

            // Calculate quantization error
            $error = $old - $new;

            // Distribute error to neighboring pixels according to Floyd-Steinberg kernel
            // Right: 7/16
            if ($x + 1 < $width) {
               $img_arr[$idx + 1] += $error * $error_right;
            }

            // Next row pixels (if not on last row)
            if ($y + 1 < $height) {
                // Bottom-left: 3/16
                if ($x - 1 >= 0) {
		            $img_arr[$nextRowOffset + $x - 1] += $error * $error_bottom_left;
                }

                // Bottom: 5/16
		        $img_arr[$nextRowOffset + $x] += $error * $error_bottom;

                // Bottom-right: 1/16
                if ($x + 1 < $width) {
		            $img_arr[$nextRowOffset + $x + 1] += $error * $error_bottom_right;
                }
            }
        }
    }

	// Import all pixels back at once (MUCH faster than setImagePixelColor per pixel)
	$dither->importImagePixels(0, 0, $width, $height, "I", Imagick::PIXEL_CHAR, $output_pixels);

	return $dither;
}

?>