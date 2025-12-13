<?php
$testResult = null;
$errorMessage = null;

// Handle form submission
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['test_upload'])) {
    // Get parameters
    $ditherMode = $_POST['ditherMode'] ?? 'Atkison';
    $auto = isset($_POST['auto']) ? 'true' : 'false';
    $gamma = $_POST['gamma'] ?? '1.0';
    $brightness = $_POST['brightness'] ?? '0';
    $contrast = $_POST['contrast'] ?? '0';

    // Check if file was uploaded
    if (!isset($_FILES['testImage']) || $_FILES['testImage']['error'] !== UPLOAD_ERR_OK) {
        $errorMessage = "No image uploaded or upload error occurred.";
    } else {
        $startTime = microtime(true);

        // Save uploaded image temporarily
        $tempImagePath = 'images/temp-test-' . time() . '.jpg';
        if (!move_uploaded_file($_FILES['testImage']['tmp_name'], $tempImagePath)) {
            $errorMessage = "Failed to save uploaded file.";
        } else {
            // Include dither functionality
            require_once("dither.php");

            try {
                // Process image using dither function
                $image = DitherImage($tempImagePath, $auto == 'true', floatval($gamma), floatval($brightness), floatval($contrast), $ditherMode);


                $endTime = microtime(true);
                $processingTime = round(($endTime - $startTime) * 1000, 2);

                $testResult = array(
                    'httpCode' => 200,
                    'processingTime' => $processingTime,
                    'bmpExists' => file_exists('images/last.bmp'),
                    'imageHeight' => $image->getImageHeight()
                );

                // Clean up temp file
                @unlink($tempImagePath);

            } catch (Exception $e) {
                $errorMessage = "Error processing image: " . $e->getMessage();
                @unlink($tempImagePath);
            }
        }
    }
}
?>
<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32-CAM API Tester</title>
    <link rel="preconnect" href="https://fonts.googleapis.com">
    <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
    <link href="https://fonts.googleapis.com/css2?family=VT323&display=swap" rel="stylesheet">
    <link rel="stylesheet" type="text/css" href="style.css">
</head>
<body class="test-page">
    <h1>ESP32-CAM API Tester</h1>
    <div style="text-align: center; margin-bottom: 20px;">
        <a href="index.php" style="color: black; font-size: 18px; text-decoration: underline;">← Back to Photo Ticket</a>
    </div>

    <?php if ($errorMessage): ?>
        <div class="error">
            <strong>Error:</strong> <?php echo htmlspecialchars($errorMessage); ?>
        </div>
    <?php endif; ?>

    <div class="container">
        <form method="POST" action="index.php" enctype="multipart/form-data">
            <div class="split-layout">
                <!-- LEFT PANEL: Upload & Controls -->
                <div class="left-panel">
                    <h2>Upload & Settings</h2>

                    <label for="testImage">Select Image:</label>
                    <label for="testImage" class="custom-file-upload">
                        Choose File
                    </label>
                    <input type="file" id="testImage" name="testImage" accept="image/*" required>

                    <label for="ditherMode">Dither Mode:</label>
                    <select id="ditherMode" name="ditherMode">
                        <option value="Atkison" selected>Atkison</option>
                        <option value="FloydSteinberg">Floyd-Steinberg</option>
                        <option value="Quantize">Quantize</option>
                        <option value="o2x2">Ordered 2x2</option>
                        <option value="o3x3">Ordered 3x3</option>
                        <option value="o4x4">Ordered 4x4</option>
                        <option value="o8x8">Ordered 8x8</option>
                        <option value="h4x4a">Halftone 4x4a</option>
                        <option value="h6x6a">Halftone 6x6a</option>
                        <option value="h8x8a">Halftone 8x8a</option>
                        <option value="h4x4o">Halftone 4x4o</option>
                        <option value="h6x6o">Halftone 6x6o</option>
                        <option value="h8x8o">Halftone 8x8o</option>
                        <option value="h16x16o">Halftone 16x16o</option>
                        <option value="c5x5b">Circles 5x5 Black</option>
                        <option value="c5x5w">Circles 5x5 White</option>
                        <option value="c6x6b">Circles 6x6 Black</option>
                        <option value="c6x6w">Circles 6x6 White</option>
                        <option value="c7x7b">Circles 7x7 Black</option>
                        <option value="c7x7w">Circles 7x7 White</option>
                    </select>

                    <div class="checkbox-label">
                        <input type="checkbox" name="auto" value="true" checked>
                        Auto Adjust
                    </div>

                    <hr>

                    <div class="slider">
                        <label for="gamma">Gamma</label>
                        <input type="range" id="gamma" name="gamma" min="0.01" max="10.0" value="1.0" step="0.01"
                               oninput="document.getElementById('gammaValue').textContent = this.value">
                        <span class="value" id="gammaValue">1.0</span>
                    </div>

                    <div class="slider">
                        <label for="brightness">Brightness</label>
                        <input type="range" id="brightness" name="brightness" min="-200" max="200" value="0" step="1"
                               oninput="document.getElementById('brightnessValue').textContent = this.value">
                        <span class="value" id="brightnessValue">0</span>
                    </div>

                    <div class="slider">
                        <label for="contrast">Contrast</label>
                        <input type="range" id="contrast" name="contrast" min="-100" max="100" value="0" step="1"
                               oninput="document.getElementById('contrastValue').textContent = this.value">
                        <span class="value" id="contrastValue">0</span>
                    </div>

                    <button type="submit" name="test_upload">Send Test Request</button>
                </div>

                <!-- RIGHT PANEL: Preview & Results -->
                <div class="right-panel">
                    <?php if ($testResult): ?>
                        <h2>Test Results</h2>

                        <div style="margin-bottom: 15px;">
                            <div class="stat">
                                <strong>Status:</strong> <span class="<?php echo $testResult['httpCode'] == 200 ? 'success' : ''; ?>"><?php echo $testResult['httpCode']; ?></span>
                            </div>
                            <div class="stat">
                                <strong>Time:</strong> <?php echo $testResult['processingTime']; ?> ms
                            </div>
                            <?php if (isset($testResult['imageHeight'])): ?>
                                <div class="stat">
                                    <strong>Height:</strong> <?php echo $testResult['imageHeight']; ?> px
                                </div>
                            <?php endif; ?>
                        </div>

                        <?php if ($testResult['bmpExists'] && file_exists('images/last.bmp')): ?>
                            <h3>Bitmap Preview</h3>
                            <div class="bitmap-visual">
                                <img src="images/last.bmp?<?php echo time(); ?>" alt="Dithered bitmap" style="max-width: 100%; border: 1px solid #ddd;">
                            </div>
                        <?php endif; ?>

                        <h3>Processing Status</h3>
                        <pre><?php
                            $compatible = true;
                            if ($testResult['httpCode'] == 200) {
                                echo "✓ Processing successful\n";
                            } else {
                                echo "✗ Processing failed\n";
                                $compatible = false;
                            }

                            if ($testResult['bmpExists']) {
                                echo "✓ BMP file created\n";
                            } else {
                                echo "✗ BMP file not created\n";
                                $compatible = false;
                            }

                            echo "\n";
                            if ($compatible) {
                                echo "✓ ALL CHECKS PASSED";
                            } else {
                                echo "✗ FIX ERRORS ABOVE";
                            }
                        ?></pre>

                    <?php else: ?>
                        <h2>Preview</h2>
                        <p style="color: #666; margin-top: 20px;">Upload an image to see the dithered result here.</p>
                        <h3>Last image</h3>
                      <img src="images/last.bmp" alt="">
                        <?php endif; ?>
                </div>

            </div>
        </form>
    </div>

    <script>
    document.querySelector('form').addEventListener('submit', function(e) {
        e.preventDefault(); // Prevent default form submission

        // Get form data
        const formData = new FormData();
        const fileInput = document.getElementById('testImage');
        const ditherMode = document.getElementById('ditherMode').value;
        const auto = document.querySelector('input[name="auto"]').checked ? 'true' : 'false';
        const gamma = document.getElementById('gamma').value;
        const brightness = document.getElementById('brightness').value;
        const contrast = document.getElementById('contrast').value;

        // Validate file
        if (!fileInput.files || fileInput.files.length === 0) {
            alert('Please select an image file');
            return;
        }

        // Append data to FormData
        formData.append('imageFile', fileInput.files[0]);
        formData.append('ditherMode', ditherMode);
        formData.append('auto', auto);
        formData.append('gamma', gamma);
        formData.append('brightness', brightness);
        formData.append('contrast', contrast);

        // Show processing state
        const rightPanel = document.querySelector('.right-panel');
        rightPanel.innerHTML = '<h2>Processing...</h2><p style="color: #666; margin-top: 20px;">Please wait while the image is being processed...</p>';

        const startTime = performance.now();

        // Send AJAX request
        fetch('index.php', {
            method: 'POST',
            body: formData
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('HTTP error ' + response.status);
            }
            return response.blob();
        })
        .then(blob => {
            const endTime = performance.now();
            const processingTime = Math.round(endTime - startTime);

            // Create object URL for the BMP
            const bmpUrl = URL.createObjectURL(blob);

            // Display result
            rightPanel.innerHTML = `
                <h2>Test Results</h2>
                <div style="margin-bottom: 15px;">
                    <div class="stat">
                        <strong>Status:</strong> <span class="success">200</span>
                    </div>
                    <div class="stat">
                        <strong>Time:</strong> ${processingTime} ms
                    </div>
                    <div class="stat">
                        <strong>Size:</strong> ${(blob.size / 1024).toFixed(2)} KB
                    </div>
                </div>
                <h3>Bitmap Preview</h3>
                <div class="bitmap-visual">
                    <img src="${bmpUrl}" alt="Dithered bitmap" style="max-width: 100%; border: 1px solid #ddd;">
                </div>
                <h3>Processing Status</h3>
                <pre>✓ Processing successful
✓ BMP file received

✓ ALL CHECKS PASSED</pre>
            `;
        })
        .catch(error => {
            console.error('Error:', error);
            rightPanel.innerHTML = `
                <h2>Error</h2>
                <div class="error">
                    <strong>Error:</strong> ${error.message}
                </div>
                <h3>Processing Status</h3>
                <pre>✗ Request failed

✗ FIX ERRORS ABOVE</pre>
            `;
        });
    });

    // Update filename display
    document.getElementById('testImage').addEventListener('change', function() {
        const fileName = this.files[0] ? this.files[0].name : 'No file chosen';
        console.log('File selected:', fileName);
    });
    </script>
</body>
</html>
