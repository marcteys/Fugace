<?php
$config['base_url'] = 'http://localhost/phototicket/';
$testResult = null;
$errorMessage = null;

// Handle form submission
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_POST['test_upload'])) {
    // Get parameters
    $ditherMode = $_POST['ditherMode'] ?? 'Atkison';
    $auto = $_POST['auto'] ?? 'true';
    $gamma = $_POST['gamma'] ?? '1.0';
    $brightness = $_POST['brightness'] ?? '0';
    $contrast = $_POST['contrast'] ?? '0';

    // Check if file was uploaded
    if (!isset($_FILES['testImage']) || $_FILES['testImage']['error'] !== UPLOAD_ERR_OK) {
        $errorMessage = "No image uploaded or upload error occurred.";
    } else {
        $startTime = microtime(true);

        // Prepare multipart POST data
        $boundary = '----WebKitFormBoundaryESP32PhotoTicket';
        $eol = "\r\n";

        $filePath = $_FILES['testImage']['tmp_name'];
        $fileName = $_FILES['testImage']['name'];
        $fileData = file_get_contents($filePath);

        // Build multipart body
        $body = '';

        // Image file part
        $body .= '--' . $boundary . $eol;
        $body .= 'Content-Disposition: form-data; name="imageFile"; filename="' . $fileName . '"' . $eol;
        $body .= 'Content-Type: image/jpeg' . $eol . $eol;
        $body .= $fileData . $eol;

        // ditherMode parameter
        $body .= '--' . $boundary . $eol;
        $body .= 'Content-Disposition: form-data; name="ditherMode"' . $eol . $eol;
        $body .= $ditherMode . $eol;

        // auto parameter
        $body .= '--' . $boundary . $eol;
        $body .= 'Content-Disposition: form-data; name="auto"' . $eol . $eol;
        $body .= $auto . $eol;

        // gamma parameter
        $body .= '--' . $boundary . $eol;
        $body .= 'Content-Disposition: form-data; name="gamma"' . $eol . $eol;
        $body .= $gamma . $eol;

        // brightness parameter
        $body .= '--' . $boundary . $eol;
        $body .= 'Content-Disposition: form-data; name="brightness"' . $eol . $eol;
        $body .= $brightness . $eol;

        // contrast parameter
        $body .= '--' . $boundary . $eol;
        $body .= 'Content-Disposition: form-data; name="contrast"' . $eol . $eol;
        $body .= $contrast . $eol;

        // End boundary
        $body .= '--' . $boundary . '--' . $eol;

        // Prepare cURL request
        $ch = curl_init();

        $targetUrl = $config['base_url'] . 'index.php';

        curl_setopt($ch, CURLOPT_URL, $targetUrl);
        curl_setopt($ch, CURLOPT_POST, true);
        curl_setopt($ch, CURLOPT_POSTFIELDS, $body);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_HEADER, true);
        curl_setopt($ch, CURLOPT_HTTPHEADER, array(
            'Content-Type: multipart/form-data; boundary=' . $boundary,
            'Content-Length: ' . strlen($body)
        ));

        // Execute request
        $response = curl_exec($ch);
        $httpCode = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        $headerSize = curl_getinfo($ch, CURLINFO_HEADER_SIZE);

        $endTime = microtime(true);
        $processingTime = round(($endTime - $startTime) * 1000, 2);

        if (curl_errno($ch)) {
            $errorMessage = 'cURL Error: ' . curl_error($ch);
        } else {
            // Parse response
            $responseHeaders = substr($response, 0, $headerSize);
            $responseBody = substr($response, $headerSize);

            // Extract headers
            $headers = array();
            foreach (explode("\r\n", $responseHeaders) as $line) {
                if (strpos($line, ':') !== false) {
                    list($key, $value) = explode(':', $line, 2);
                    $headers[trim($key)] = trim($value);
                }
            }

            $testResult = array(
                'httpCode' => $httpCode,
                'headers' => $headers,
                'bodyLength' => strlen($responseBody),
                'body' => $responseBody,
                'processingTime' => $processingTime,
                'uploadSize' => strlen($fileData),
                'requestSize' => strlen($body)
            );
        }

        curl_close($ch);
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
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'VT323', monospace;
            background: #eee;
            color: black;
            padding: 20px;
        }

        h1 {
            text-align: center;
            font-size: 36px;
            margin-bottom: 20px;
        }

        h2 {
            font-size: 24px;
            margin-bottom: 15px;
            border-bottom: 2px solid black;
            padding-bottom: 5px;
        }

        h3 {
            font-size: 20px;
            margin-top: 15px;
            margin-bottom: 10px;
        }

        .container {
            max-width: 1000px;
            margin: 0 auto;
            background: #FBFBFB;
            border: 3px solid black;
            border-radius: 5px;
            padding: 25px;
            box-shadow: rgba(1,1,1,.2) 0px 0px 20px;
            position: relative;
        }

        /* Ticket-style perforated edges */
        .container:before, .container:after {
            content: "";
            position: absolute;
            left: 5px;
            height: 6px;
            width: calc(100% - 10px);
        }
        .container:before {
            top: -5px;
            background: radial-gradient(circle, transparent, transparent 50%, #FBFBFB 50%, #FBFBFB 100%) -7px -8px/16px 16px repeat-x;
        }
        .container:after {
            bottom: -6px;
            background: radial-gradient(circle, transparent, transparent 50%, #000 50%, #000 100%) -7px -2px/16px 16px repeat-x;
        }

        .split-layout {
            display: grid;
            grid-template-columns: 400px 1fr;
            gap: 30px;
            margin-top: 20px;
        }

        @media (max-width: 900px) {
            .split-layout {
                grid-template-columns: 1fr;
            }
        }

        .left-panel {
            border-right: 2px dotted black;
            padding-right: 30px;
        }

        .right-panel {
            min-height: 400px;
        }

        label {
            display: block;
            margin-top: 12px;
            font-size: 18px;
            font-weight: bold;
        }

        input[type="file"] {
            display: none;
        }

        .custom-file-upload {
            border: 2px black dotted;
            display: block;
            padding: 10px;
            cursor: pointer;
            text-align: center;
            border-radius: 2px;
            margin-top: 10px;
            margin-bottom: 15px;
            font-size: 18px;
        }

        .custom-file-upload:hover {
            background: #eee;
        }

        input[type="number"],
        select {
            width: 100%;
            padding: 8px;
            margin-top: 5px;
            background: white;
            border: 2px solid black;
            font-family: 'VT323', monospace;
            font-size: 16px;
        }

        input[type="checkbox"] {
            margin-right: 10px;
            transform: scale(1.2);
        }

        button {
            background: black;
            color: white;
            border: none;
            padding: 12px 30px;
            font-size: 20px;
            cursor: pointer;
            margin-top: 20px;
            font-family: 'VT323', monospace;
            width: 100%;
            border-radius: 2px;
        }

        button:hover {
            background: #333;
        }

        .error {
            background: #ffebee;
            border: 2px solid #c62828;
            color: #c62828;
            padding: 15px;
            margin-bottom: 20px;
            border-radius: 2px;
        }

        .stat {
            display: inline-block;
            background: #f5f5f5;
            padding: 5px 12px;
            margin: 5px 5px 5px 0;
            border: 2px solid black;
            font-size: 16px;
        }

        .success {
            color: #2e7d32;
        }

        pre {
            background: #f5f5f5;
            border: 2px solid black;
            padding: 10px;
            overflow-x: auto;
            max-height: 200px;
            overflow-y: auto;
            font-family: 'VT323', monospace;
            font-size: 14px;
        }

        .bitmap-visual {
            background: white;
            padding: 15px;
            border: 2px solid black;
            margin-top: 10px;
            text-align: center;
        }

        .bitmap-visual canvas {
            border: 1px solid #ddd;
            image-rendering: pixelated;
            max-width: 100%;
        }

        hr {
            margin: 15px 0;
            border: 1px solid black;
        }

        .checkbox-label {
            font-size: 18px;
            margin-top: 15px;
        }

        .slider {
            position: relative;
            width: 100%;
            display: block;
            clear: both;
            margin-top: 12px;
        }

        .slider label {
            float: left;
            width: 30%;
            font-size: 18px;
        }

        .slider input[type="range"] {
            width: 55%;
            float: left;
        }

        .slider .value {
            width: 15%;
            float: right;
            text-align: right;
            font-size: 18px;
            font-weight: bold;
        }
    </style>
</head>
<body>
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
        <form method="POST" enctype="multipart/form-data">
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
                            <?php if (isset($testResult['headers']['X-Image-Height'])): ?>
                                <div class="stat">
                                    <strong>Height:</strong> <?php echo $testResult['headers']['X-Image-Height']; ?> px
                                </div>
                            <?php endif; ?>
                        </div>

                        <?php if (isset($testResult['headers']['X-Image-Height'])): ?>
                            <h3>Bitmap Preview</h3>
                            <div class="bitmap-visual">
                                <canvas id="bitmapCanvas"></canvas>
                            </div>

                            <script>
                                const canvas = document.getElementById('bitmapCanvas');
                                const ctx = canvas.getContext('2d');
                                const hexData = <?php echo json_encode($testResult['body']); ?>;
                                const imageHeight = <?php echo $testResult['headers']['X-Image-Height']; ?>;
                                const imageWidth = 384;
                                const maxRows = imageHeight;

                                canvas.width = imageWidth;
                                canvas.height = maxRows;

                                const imageData = ctx.createImageData(imageWidth, maxRows);

                                function hexToByte(h, l) {
                                    const high = h <= '9' ? h.charCodeAt(0) - 48 : h.toUpperCase().charCodeAt(0) - 55;
                                    const low = l <= '9' ? l.charCodeAt(0) - 48 : l.toUpperCase().charCodeAt(0) - 55;
                                    return (high << 4) | low;
                                }

                                for (let row = 0; row < maxRows; row++) {
                                    for (let byteCol = 0; byteCol < 48; byteCol++) {
                                        const hexPos = (row * 48 * 2) + (byteCol * 2);
                                        if (hexPos + 1 < hexData.length) {
                                            const byte = hexToByte(hexData[hexPos], hexData[hexPos + 1]);

                                            for (let bit = 0; bit < 8; bit++) {
                                                const pixelX = byteCol * 8 + bit;
                                                const pixelY = row;
                                                const pixelIndex = (pixelY * imageWidth + pixelX) * 4;

                                                const isBlack = (byte & (1 << (7 - bit))) !== 0;
                                                const color = isBlack ? 0 : 255;

                                                imageData.data[pixelIndex] = color;
                                                imageData.data[pixelIndex + 1] = color;
                                                imageData.data[pixelIndex + 2] = color;
                                                imageData.data[pixelIndex + 3] = 255;
                                            }
                                        }
                                    }
                                }

                                ctx.putImageData(imageData, 0, 0);
                            </script>
                        <?php endif; ?>

                        <h3>ESP32 Compatibility</h3>
                        <pre><?php
                            $compatible = true;
                            if ($testResult['httpCode'] == 200) {
                                echo "✓ HTTP 200 OK\n";
                            } else {
                                echo "✗ HTTP " . $testResult['httpCode'] . "\n";
                                $compatible = false;
                            }

                            if (isset($testResult['headers']['X-Image-Height'])) {
                                echo "✓ Height header present\n";
                            } else {
                                echo "✗ Height header missing\n";
                                $compatible = false;
                            }

                            if (ctype_xdigit(substr($testResult['body'], 0, 10))) {
                                echo "✓ Valid hexadecimal data\n";
                            } else {
                                echo "✗ Invalid hex data\n";
                                $compatible = false;
                            }

                            echo "\n";
                            if ($compatible) {
                                echo "✓ COMPATIBLE";
                            } else {
                                echo "✗ FIX ERRORS ABOVE";
                            }
                        ?></pre>

                    <?php else: ?>
                        <h2>Preview</h2>
                        <p style="color: #666; margin-top: 20px;">Upload an image to see the dithered result here.</p>
                    <?php endif; ?>
                </div>
            </div>
        </form>
    </div>
</body>
</html>
