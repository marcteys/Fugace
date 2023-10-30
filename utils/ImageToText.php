<?php

function DitherImageToString($sourcePath) { 
    

    $outputString = "";



    $image = new Imagick(realpath($sourcePath));

    // Calculate output size
    $rowBytes = ceil($image->getImageWidth() / 8);
    $totalBytes = $rowBytes * $image->getImageHeight();

    $pixels = $image->exportImagePixels(0, 0, $image->getImageWidth(), $image->getImageHeight(), "I", Imagick::PIXEL_CHAR);

    // Generate body of array
    $byteNum = 0;
    $bytesOnLine = 0;

    $count = 0;

    $imageWidth = $image->getImageWidth();
    $imageHeight = $image->getImageHeight();

    // Constants
    $rowBytes = ceil($imageWidth / 8); // should be 48 for a full width image

    // Variables
    // Generate body of array
    for ($y = 0; $y < $imageHeight; $y++) { // Each row...
       
        for ($x = 0; $x < $rowBytes; $x++) { // Each 8-pixel block within row...
       $byteSum = ""; // Clear accumulated 8 bits

            for ($bit = 0; $bit < 8; $bit++) { // Each pixel within block...
                $pixel = $image->getImagePixelColor($x * 8 + $bit, $y);
                $color = $pixel->getColor();
               // echo $color['r'] . ' ';
                // Assuming the image is black and white (1-bit depth)
                $byteSum[$bit] = $color['r'] <= 128 ? "1" : "0"; 
            }
           // echo $sum; // Write accumulated bits
            $byte = chr(bindec($byteSum));
            $hex = sprintf("%02X", ord($byte));
            $outputString .= $hex;
           // echo $hex;
        }

    }

    return $outputString;
}

?>