<?php
// API-only endpoint for image upload and processing
// Handles both ESP32-CAM uploads and test.php requests

if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    header('HTTP/1.1 405 Method Not Allowed');
    header('Content-Type: application/json');
    die(json_encode(['error' => 'Only POST requests are allowed']));
}

// Log incoming POST data for debugging
$logFile = 'post_log.txt';
$logData = [
    'timestamp' => date('Y-m-d H:i:s'),
    'POST' => $_POST,
    'FILES' => isset($_FILES) ? array_map(function($file) {
        return [
            'name' => $file['name'],
            'type' => $file['type'],
            'size' => $file['size'],
            'tmp_name' => $file['tmp_name'],
            'error' => $file['error']
        ];
    }, $_FILES) : []
];
file_put_contents($logFile, print_r($logData, true) . "\n" . str_repeat('=', 80) . "\n", FILE_APPEND);

// Determine processing mode: new upload or re-process existing file
$originalPath = null;

if (isset($_FILES['imageFile']) && $_FILES['imageFile']['error'] === UPLOAD_ERR_OK) {
    // Mode 1: New file upload

    // Validate file type
    $allowedTypes = ['jpeg', 'jpg', 'png', 'gif', 'bmp'];
    $fileExt = strtolower(pathinfo($_FILES['imageFile']['name'], PATHINFO_EXTENSION));

    if (!in_array($fileExt, $allowedTypes)) {
        header('HTTP/1.1 400 Bad Request');
        header('Content-Type: application/json');
        die(json_encode(['error' => 'FILE_TYPE_ERROR', 'message' => 'Only JPEG, PNG, GIF, and BMP files are allowed']));
    }

    // Validate file size (max 10MB)
    if ($_FILES['imageFile']['size'] > 10000000) {
        header('HTTP/1.1 400 Bad Request');
        header('Content-Type: application/json');
        die(json_encode(['error' => 'FILE_SIZE_ERROR', 'message' => 'File must be under 10MB']));
    }

    // Save uploaded file with timestamp
    $timestamp = time();
    $originalPath = "images/" . $timestamp . "-image.jpg";

    if (!move_uploaded_file($_FILES['imageFile']['tmp_name'], $originalPath)) {
        header('HTTP/1.1 500 Internal Server Error');
        header('Content-Type: application/json');
        die(json_encode(['error' => 'Failed to save uploaded file']));
    }

} elseif (isset($_POST['sourceImage'])) {
    // Mode 2: Re-process existing file

    // Sanitize filename to prevent directory traversal
    $filename = basename($_POST['sourceImage']);
    $originalPath = "images/" . $filename;

    if (!file_exists($originalPath)) {
        header('HTTP/1.1 404 Not Found');
        header('Content-Type: application/json');
        die(json_encode(['error' => 'Source image not found']));
    }

} else {
    // No valid input provided
    header('HTTP/1.1 400 Bad Request');
    header('Content-Type: application/json');
    die(json_encode(['error' => 'No image file or source image provided']));
}

// Extract dither parameters from POST (with defaults)
$ditherMode = isset($_POST['ditherMode']) ? $_POST['ditherMode'] : 'Atkison';
$auto = isset($_POST['auto']) ? ($_POST['auto'] === 'true') : true;
$gamma = isset($_POST['gamma']) ? floatval($_POST['gamma']) : 1.0;
$brightness = isset($_POST['brightness']) ? floatval($_POST['brightness']) : 0;
$contrast = isset($_POST['contrast']) ? floatval($_POST['contrast']) : 0; // Fixed bug: was checking 'brightness'

// Load image processing function (includes aspect ratio cropping)
require_once("dither.php");

// Process image
try {
    $processedImage = DitherImage($originalPath, $auto, $gamma, $brightness, $contrast, $ditherMode);
} catch (Exception $e) {
    header('HTTP/1.1 500 Internal Server Error');
    header('Content-Type: application/json');
    die(json_encode(['error' => 'Image processing failed', 'message' => $e->getMessage()]));
}

// Return processed BMP image
header('X-Uploaded-Filename: ' . basename($originalPath));
header('Content-Type: image/bmp');
header('Content-Length: ' . strlen($processedImage));
echo $processedImage;

exit();
?>
