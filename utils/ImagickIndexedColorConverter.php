<?php

class ImagickAtkinsonDither
{
    public function ditherImage($inputImagePath, $outputImagePath)
    {
        $image = new Imagick(realpath($inputImagePath));
        $image->transformImageColorspace(Imagick::COLORSPACE_GRAY);

        $width = $image->getImageWidth();
        $height = $image->getImageHeight();

        $outputImage = new Imagick();
        $outputImage->newImage($width, $height, new ImagickPixel('white'));
        $outputImage->setImageFormat('jpg'); // Change format as needed

        for ($y = 0; $y < $height; $y++) {
            for ($x = 0; $x < $width; $x++) {
                $pixel = $image->getImagePixelColor($x, $y)->getColor();
                $old = $pixel['r'];

                if ($old > 0.5) { // B/W threshold
                    $new = 1;
                } else {
                    $new = 0;
                }

                $quant_error = $old - $new;
                $error_diffusion = (1 / 8) * $quant_error;

                // Apply error diffusion to neighboring pixels
                if ($x + 1 < $width) {
                    $this->applyErrorDiffusion($image, $x + 1, $y, $error_diffusion);
                }
                if ($x + 2 < $width) {
                    $this->applyErrorDiffusion($image, $x + 2, $y, $error_diffusion);
                }
                if ($x - 1 >= 0 && $y + 1 < $height) {
                    $this->applyErrorDiffusion($image, $x - 1, $y + 1, $error_diffusion);
                }
                if ($y + 1 < $height) {
                    $this->applyErrorDiffusion($image, $x, $y + 1, $error_diffusion);
                }
                if ($x + 1 < $width && $y + 1 < $height) {
                    $this->applyErrorDiffusion($image, $x + 1, $y + 1, $error_diffusion);
                }
                if ($y + 2 < $height) {
                    $this->applyErrorDiffusion($image, $x, $y + 2, $error_diffusion);
                }

                // Set the grayscale value for the output pixel
                $outputImage->setImagePixelColor($x, $y,new ImagickPixel("gray($new)"));
            }
        }

        $outputImage->writeImage($outputImagePath);

        file_put_contents ("../images/last.jpg", $outputImage);

    }

    private function applyErrorDiffusion($image, $x, $y, $error_diffusion)
    {
        $pixel = $image->getImagePixelColor($x, $y)->getColor();
        $old = $pixel['r'];
        $new = max(0, min(1, $old + $error_diffusion));
        $image->setImagePixelColor($x, $y,new ImagickPixel("gray($new)"));
    }
}

// Example usage
$converter = new ImagickAtkinsonDither();
$sourceImage = 'Capture.PNG'; // Input image path
$targetImage = 'output.png'; // Output image path
$converter->ditherImage($sourceImage, $targetImage);


echo "done";



/*
class FloydSteinbergDither
{
    public function ditherImage($inputImagePath, $outputImagePath, $palette)
    {
        $inputImage = new Imagick(realpath($inputImagePath));
        $outputImage = new Imagick();

        $outputImage->newImage($inputImage->getImageWidth(), $inputImage->getImageHeight(), new ImagickPixel('white'));
        $outputImage->setImageFormat('png'); // Change format as needed

        $width = $inputImage->getImageWidth();
        $height = $inputImage->getImageHeight();

        for ($y = 0; $y < $height; $y++) {
            for ($x = 0; $x < $width; $x++) {
                $pixel = $inputImage->getImagePixelColor($x, $y)->getColor();
                $closestColorIndex = $this->getClosestColorIndex($pixel, $palette);
                $closestColor = $palette[$closestColorIndex];
                $outputImage->setImagePixelColor( $x, $y, $this->createImagickPixel($closestColor));

                // Calculate error
                $errorR = $pixel['r'] - $closestColor['r'];
                $errorG = $pixel['g'] - $closestColor['g'];
                $errorB = $pixel['b'] - $closestColor['b'];

                // Diffusion to neighboring pixels
                $this->applyErrorDiffusion($inputImage, $x, $y, $width, $height, $errorR, $errorG, $errorB);
            }
        }

        $outputImage->writeImage($outputImagePath);
    }

    private function getClosestColorIndex($pixel, $palette)
    {
        $closestColorIndex = 0;
        $closestDistance = $this->calculateColorDistance($pixel, $palette[0]);

        for ($i = 1; $i < count($palette); $i++) {
            $distance = $this->calculateColorDistance($pixel, $palette[$i]);
            if ($distance < $closestDistance) {
                $closestColorIndex = $i;
                $closestDistance = $distance;
            }
        }

        return $closestColorIndex;
    }

    private function calculateColorDistance($color1, $color2)
    {
        $distanceR = $color1['r'] - $color2['r'];
        $distanceG = $color1['g'] - $color2['g'];
        $distanceB = $color1['b'] - $color2['b'];

        return ($distanceR * $distanceR) + ($distanceG * $distanceG) + ($distanceB * $distanceB);
    }

    private function applyErrorDiffusion($image, $x, $y, $width, $height, $errorR, $errorG, $errorB)
    {
        // Floyd-Steinberg error diffusion coefficients
        $coefficients = [
            [1, 0, 7/16],
            [-1, 1, 3/16],
            [0, 1, 5/16],
            [1, 1, 1/16]
        ];

        for ($i = 0; $i < 4; $i++) {
            $newX = $x + $coefficients[$i][0];
            $newY = $y + $coefficients[$i][1];

            if ($newX >= 0 && $newX < $width && $newY >= 0 && $newY < $height) {
                $pixel = $image->getImagePixelColor($newX, $newY)->getColor();
                $newR = $pixel['r'] + $errorR * $coefficients[$i][2];
                $newG = $pixel['g'] + $errorG * $coefficients[$i][2];
                $newB = $pixel['b'] + $errorB * $coefficients[$i][2];
                $image->setImagePixelColor($newX, $newY,$this->createImagickPixel(['r' => $newR, 'g' => $newG, 'b' => $newB]));
            }
        }
    }

    private function createImagickPixel($color)
    {
        return new ImagickPixel('rgb(' . $color['r'] . ',' . $color['g'] . ',' . $color['b'] . ')');
    }
}

// Example usage
$converter = new FloydSteinbergDither();
$palette = [
    ['r' => 255, 'g' => 0, 'b' => 0],   // Red
    ['r' => 0, 'g' => 255, 'b' => 0],   // Green
    ['r' => 0, 'g' => 0, 'b' => 255]    // Blue
];
$sourceImage = '../images/input.png';  // Input image path
$targetImage = 'output.png'; // Output image path
$converter->ditherImage($sourceImage, $targetImage, $palette);
*/

?>
