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
    <h1>Fugace</h1>
    <div style="text-align: center; margin-bottom: 20px;">
        <p style="color: #666;">Upload an image and adjust settings for real-time preview</p>
    </div>

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
                    <h2>Preview</h2>
                    <p style="color: #666; margin-top: 20px;">Upload an image to see the dithered result here.</p>
                    <p style="color: #999; margin-top: 10px; font-size: 14px;">Adjust the sliders on the left for real-time re-processing.</p>
                </div>

            </div>
        </form>
    </div>

    <script>
    let lastUploadedFilename = null;
    let lastSelectedFile = null;

    // Parse BMP header to extract metadata
    function parseBMPHeader(arrayBuffer) {
        const view = new DataView(arrayBuffer);

        // Check BMP signature
        const signature = String.fromCharCode(view.getUint8(0)) + String.fromCharCode(view.getUint8(1));
        if (signature !== 'BM') {
            return null;
        }

        const metadata = {
            fileSize: view.getUint32(2, true),
            dataOffset: view.getUint32(10, true),
            dibHeaderSize: view.getUint32(14, true),
            width: view.getUint32(18, true),
            height: Math.abs(view.getInt32(22, true)),
            planes: view.getUint16(26, true),
            bitsPerPixel: view.getUint16(28, true),
            compression: view.getUint32(30, true),
            imageSize: view.getUint32(34, true),
            xPixelsPerMeter: view.getUint32(38, true),
            yPixelsPerMeter: view.getUint32(42, true),
            colorsUsed: view.getUint32(46, true),
            colorsImportant: view.getUint32(50, true)
        };

        // Calculate DPI from pixels per meter
        metadata.xDPI = Math.round(metadata.xPixelsPerMeter * 0.0254);
        metadata.yDPI = Math.round(metadata.yPixelsPerMeter * 0.0254);

        // Compression types
        const compressionTypes = {
            0: 'None (BI_RGB)',
            1: 'RLE 8-bit',
            2: 'RLE 4-bit',
            3: 'Bitfields',
            4: 'JPEG',
            5: 'PNG'
        };
        metadata.compressionName = compressionTypes[metadata.compression] || 'Unknown';

        return metadata;
    }

    // Store file reference on selection
    document.getElementById('testImage').addEventListener('change', function() {
        if (this.files && this.files[0]) {
            lastSelectedFile = this.files[0];
            console.log('File selected:', this.files[0].name);
        }
    });

    // Handle form submission - initial upload
    document.querySelector('form').addEventListener('submit', function(e) {
        e.preventDefault();

        if (!lastSelectedFile) {
            alert('Please select an image file');
            return;
        }

        processImage(true); // true = initial upload
    });

    // Process or re-process image
    function processImage(isInitialUpload = false) {
        const formData = new FormData();
        const rightPanel = document.querySelector('.right-panel');

        if (isInitialUpload) {
            // Mode 1: Upload new file
            if (!lastSelectedFile) {
                alert('Please select an image file');
                return;
            }
            formData.append('imageFile', lastSelectedFile);
        } else {
            // Mode 2: Re-process existing file
            if (!lastUploadedFilename) {
                alert('Upload an image first');
                return;
            }
            formData.append('sourceImage', lastUploadedFilename);
        }

        // Add processing parameters
        formData.append('ditherMode', document.getElementById('ditherMode').value);
        formData.append('auto', document.querySelector('input[name="auto"]').checked ? 'true' : 'false');
        formData.append('gamma', document.getElementById('gamma').value);
        formData.append('brightness', document.getElementById('brightness').value);
        formData.append('contrast', document.getElementById('contrast').value);

        // Show processing state
        rightPanel.innerHTML = '<h2>Processing...</h2><p style="color: #666; margin-top: 20px;">Please wait...</p>';

        const startTime = performance.now();

        // Send request to upload.php
        fetch('upload.php', {
            method: 'POST',
            body: formData
        })
        .then(response => {
            if (!response.ok) {
                throw new Error('HTTP error ' + response.status);
            }

            // Store filename from header on initial upload
            if (isInitialUpload) {
                const filename = response.headers.get('X-Uploaded-Filename');
                if (filename) {
                    lastUploadedFilename = filename;
                    console.log('Saved filename for re-processing:', filename);
                }
            }

            return response.blob();
        })
        .then(blob => {
            const endTime = performance.now();
            const processingTime = Math.round(endTime - startTime);

            // Create object URL for the BMP
            const bmpUrl = URL.createObjectURL(blob);

            // Get current processing parameters
            const currentParams = {
                ditherMode: document.getElementById('ditherMode').value,
                auto: document.querySelector('input[name="auto"]').checked,
                gamma: document.getElementById('gamma').value,
                brightness: document.getElementById('brightness').value,
                contrast: document.getElementById('contrast').value
            };

            // Parse BMP metadata
            blob.arrayBuffer().then(arrayBuffer => {
                const bmpMetadata = parseBMPHeader(arrayBuffer);

                // Create image to get dimensions
                const img = new Image();
                img.onload = function() {
                    // Build metadata HTML
                    let metadataHTML = `
                        <h2>Result</h2>

                        <div class="bitmap-visual">
                            <img src="${bmpUrl}" alt="Processed image" style="max-width: 100%; border: 1px solid #ddd;">
                        </div>
                        <p style="color: #666; margin-top: 10px; font-style: italic;">Adjust sliders above to re-process in real-time</p>
                        <h3>Meta-data</h3>

                        <div style="margin-bottom: 15px; display: grid; grid-template-columns: 1fr 1fr; gap: 5px;">
                            <div class="stat">
                                <strong>Status:</strong> <span class="success">200</span>
                            </div>
                            <div class="stat">
                                <strong>Time:</strong> ${processingTime} ms
                            </div>
                            <div class="stat">
                                <strong>Size:</strong> ${(blob.size / 1024).toFixed(2)} KB
                            </div>
                            <div class="stat">
                                <strong>Dimensions:</strong> ${img.naturalWidth} × ${img.naturalHeight} px
                            </div>`;

                    if (bmpMetadata) {
                        metadataHTML += `
                            <div class="stat">
                                <strong>Color Depth:</strong> ${bmpMetadata.bitsPerPixel}-bit
                            </div>
                            <div class="stat">
                                <strong>Compression:</strong> ${bmpMetadata.compressionName}
                            </div>
                            <div class="stat">
                                <strong>Resolution:</strong> ${bmpMetadata.xDPI} × ${bmpMetadata.yDPI} DPI
                            </div>
                            <div class="stat">
                                <strong>Colors Used:</strong> ${bmpMetadata.colorsUsed || 'All'}
                            </div>`;
                    }

                    metadataHTML += `
                            <div class="stat">
                                <strong>Dither:</strong> ${currentParams.ditherMode}
                            </div>
                            <div class="stat">
                                <strong>Auto Adjust:</strong> ${currentParams.auto ? 'Yes' : 'No'}
                            </div>
                            <div class="stat">
                                <strong>Gamma:</strong> ${currentParams.gamma}
                            </div>
                            <div class="stat">
                                <strong>Brightness:</strong> ${currentParams.brightness}
                            </div>
                            <div class="stat">
                                <strong>Contrast:</strong> ${currentParams.contrast}
                            </div>
                        </div>
                    `;

                    rightPanel.innerHTML = metadataHTML;
                };

                // Initial display while image loads
                rightPanel.innerHTML = `
                    <h2>Result</h2>
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
                        <div class="stat">
                            <strong>Loading metadata...</strong>
                        </div>
                    </div>
                    <h3>Preview</h3>
                    <div class="bitmap-visual">
                        <img src="${bmpUrl}" alt="Processed image" style="max-width: 100%; border: 1px solid #ddd;">
                    </div>
                `;

                img.src = bmpUrl;
            });
        })
        .catch(error => {
            console.error('Error:', error);
            rightPanel.innerHTML = `
                <h2>Error</h2>
                <div class="error">
                    <strong>Error:</strong> ${error.message}
                </div>
            `;
        });
    }

    // Real-time re-processing on slider changes (debounced)
    let reprocessTimeout;
    function scheduleReprocess() {
        clearTimeout(reprocessTimeout);
        reprocessTimeout = setTimeout(() => processImage(false), 500); // 500ms debounce
    }

    // Attach to sliders for real-time preview
    document.getElementById('gamma').addEventListener('input', scheduleReprocess);
    document.getElementById('brightness').addEventListener('input', scheduleReprocess);
    document.getElementById('contrast').addEventListener('input', scheduleReprocess);
    document.getElementById('ditherMode').addEventListener('change', () => processImage(false));
    document.querySelector('input[name="auto"]').addEventListener('change', () => processImage(false));

    // Update slider value displays
    document.getElementById('gamma').addEventListener('input', function() {
        document.getElementById('gammaValue').textContent = this.value;
    });
    document.getElementById('brightness').addEventListener('input', function() {
        document.getElementById('brightnessValue').textContent = this.value;
    });
    document.getElementById('contrast').addEventListener('input', function() {
        document.getElementById('contrastValue').textContent = this.value;
    });
    </script>
</body>
</html>
