/*
  Fugace Firmware
  ESP32S3-based photo  with camera capture and thermal printer

  Hardware:
  - Seeed XIAO ESP32S3 Sense
  - Thermal Printer (JP-QR701)
  - Button on GPIO 1 (D0)

  Workflow:
  Button Press → LED Countdown → Capture → Upload → Print → Idle
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "TPrinter.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "TJpg_Decoder.h"  // TJpgDec for JPEG decoding

#include "secrets.h"

// ============================================================================
// CAMERA PIN DEFINITIONS (XIAO ESP32S3)
// ============================================================================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// ============================================================================
// PERIPHERAL PIN DEFINITIONS
// ============================================================================
const byte rxPin = 44;        // Printer RX (D8) - HardwareSerial1
const byte txPin = 43;        // Printer TX (D7) - HardwareSerial1
const byte dtrPin = 2;        // Printer DTR (D1)
const byte buttonPin = 1;     // Button input (D0) - internal pullup
const byte ledPin = 21;       // Built-in LED

// ============================================================================
// SLEEP CONFIGURATION
// ============================================================================
bool timerWakeEnable = false;
const uint64_t SLEEP_DURATION_US = 20 * 1000000; // 20 seconds in microseconds
const gpio_num_t WAKEUP_GPIO = GPIO_NUM_1;       // Button pin for wake

// RTC memory for statistics (preserved across deep sleep)
RTC_DATA_ATTR int sleepCount = 0;
RTC_DATA_ATTR int photoCount = 0;

// ============================================================================
// OFFLINE PROCESSING CONFIGURATION
// ============================================================================
const uint16_t TARGET_WIDTH = 384;         // Thermal printer width
const float TARGET_ASPECT_RATIO = 3.0f / 4.0f;  // 3:4 portrait
const float GAMMA_VALUE = 1.7f;
const int BRIGHTNESS_VALUE = 25;
const int CONTRAST_VALUE = -15;

// ============================================================================
// STATE MACHINE
// ============================================================================
enum SystemState {
  STATE_IDLE,
  STATE_COUNTDOWN,
  STATE_CAPTURING,
  STATE_UPLOADING,
  STATE_PROCESSING_LOCAL,
  STATE_PRINTING,
  STATE_SLEEP,
  STATE_ERROR
};

volatile SystemState currentState = STATE_IDLE;
const char* stateNames[] = {"IDLE", "COUNTDOWN", "CAPTURING", "UPLOADING", "PROCESSING_LOCAL", "PRINTING", "SLEEP", "ERROR"};

// ============================================================================
// HARDWARE OBJECTS
// ============================================================================
HardwareSerial printerSerial(2); // Use Serial1 for XIAO
Tprinter myPrinter(&printerSerial, 9600);
WiFiClientSecure wifiClient;

// ============================================================================
// BUTTON DEBOUNCING
// ============================================================================
volatile bool buttonPressed = false;
volatile unsigned long lastButtonPress = 0;
const unsigned long DEBOUNCE_DELAY = 300; // milliseconds

// ============================================================================
// CAMERA INITIALIZATION
// ============================================================================
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA; // 1600x1200
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 5;
  config.fb_count = 1;

  // Optimize based on PSRAM availability
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
      Serial.println("PSRAM found - using UXGA");
    } else {
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
      Serial.println("No PSRAM - using SVGA");
    }
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera initialized successfully");
  return true;
}

// ============================================================================
// PRINTER INITIALIZATION
// ============================================================================
bool initPrinter() {
  Serial.println("Initializing printer...");
  printerSerial.begin(9600, SERIAL_8N1, rxPin, txPin);
  delay(100);

 // myPrinter.enableDtr(dtrPin, LOW);
  myPrinter.begin();
  myPrinter.setHeat(1, 224, 40);
  myPrinter.justify('C');
 // myPrinter.invert(true);

  Serial.println("Printer initialized successfully");
  return true;
}

// ============================================================================
// BUTTON INITIALIZATION
// ============================================================================
void IRAM_ATTR buttonISR() {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress > DEBOUNCE_DELAY) {
    if (currentState == STATE_IDLE) {
      buttonPressed = true;
      lastButtonPress = currentTime;
    }
  }
}

bool initButton() {
  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), buttonISR, FALLING);
  Serial.printf("Button initialized on GPIO %d\n", buttonPin);
  return true;
}

bool checkButton() {
  if (buttonPressed) {
    buttonPressed = false;
    return true;
  }
  return false;
}

// ============================================================================
// LED COUNTDOWN
// ============================================================================
void ledCountdown(int seconds) {
  Serial.println("Starting LED countdown...");

  for (int i = seconds; i > 0; i--) {
    Serial.printf("Countdown: %d\n", i);

    // Map delay: 5s=500ms, 1s=50ms (faster as countdown progresses)
    int blinkDelay = map(i, seconds, 1, 500, 50);
    int blinksPerSecond = 1000 / (blinkDelay * 2);

    for (int j = 0; j < blinksPerSecond; j++) {
      digitalWrite(ledPin, HIGH); // LED ON
      delay(blinkDelay);
      digitalWrite(ledPin, LOW);  // LED OFF
      delay(blinkDelay);
    }
  }

  // Solid ON for capture
  digitalWrite(ledPin, HIGH);
  Serial.println("LED solid ON - ready to capture");
}

// ============================================================================
// BMP PARSING HELPER FUNCTIONS
// ============================================================================
uint16_t read16(WiFiClient& client) {
  uint16_t result;
  ((uint8_t*)&result)[0] = client.read();
  ((uint8_t*)&result)[1] = client.read();
  return result;
}

uint32_t read32(WiFiClient& client) {
  uint32_t result;
  ((uint8_t*)&result)[0] = client.read();
  ((uint8_t*)&result)[1] = client.read();
  ((uint8_t*)&result)[2] = client.read();
  ((uint8_t*)&result)[3] = client.read();
  return result;
}

uint32_t skip(WiFiClient& client, int32_t bytes) {
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0)) {
    if (client.available()) {
      client.read();
      remain--;
    } else {
      delay(1);
    }
    if (millis() - start > 5000) break;
  }
  return bytes - remain;
}

uint32_t read8n(WiFiClient& client, uint8_t* buffer, int32_t bytes) {
  int32_t remain = bytes;
  uint32_t start = millis();
  while ((client.connected() || client.available()) && (remain > 0)) {
    if (client.available()) {
      *buffer++ = (uint8_t)client.read();
      remain--;
    } else {
      delay(1);
    }
    if (millis() - start > 5000) break;
  }
  return bytes - remain;
}

// ============================================================================
// UPLOAD AND RECEIVE BMP
// ============================================================================
bool uploadAndReceiveBMP(camera_fb_t* fb, uint8_t** bitmap, uint16_t* width, uint16_t* height) {
  Serial.println("Uploading image to server...");

  wifiClient.setInsecure(); // Skip certificate validation

  if (!wifiClient.connect(serverName, serverPort)) {
    Serial.println("Connection to server failed");
    return false;
  }

  Serial.println("Connected to server");

  // Build multipart form data
  String head = "--Cam\r\n";
  head += "Content-Disposition: form-data; name=\"imageFile\"; filename=\"xiao-cam.jpg\"\r\n";
  head += "Content-Type: image/jpeg\r\n\r\n";

  String params = "\r\n--Cam\r\n";
  params += "Content-Disposition: form-data; name=\"ditherMode\"\r\n\r\n";
  params += "Atkison\r\n";
  params += "--Cam\r\n";
  params += "Content-Disposition: form-data; name=\"auto\"\r\n\r\n";
  params += "true\r\n";
  params += "--Cam--\r\n";

  uint32_t imageLen = fb->len;
  uint32_t totalLen = head.length() + imageLen + params.length();

  // Send HTTP POST request
  wifiClient.println("POST " + String(serverPath) + " HTTP/1.1");
  wifiClient.println("Host: " + String(serverName));
  wifiClient.println("Content-Length: " + String(totalLen));
  wifiClient.println("Content-Type: multipart/form-data; boundary=Cam");
  wifiClient.println();
  wifiClient.print(head);

  // Send image in chunks
  uint8_t *fbBuf = fb->buf;
  size_t fbLen = fb->len;
  for (size_t n = 0; n < fbLen; n += 1024) {
    if (n + 1024 < fbLen) {
      wifiClient.write(fbBuf, 1024);
      fbBuf += 1024;
    } else if (fbLen % 1024 > 0) {
      size_t remainder = fbLen % 1024;
      wifiClient.write(fbBuf, remainder);
    }
  }

  wifiClient.print(params);

  Serial.println("Image uploaded, waiting for BMP response...");

  // Wait for response headers
  unsigned long timeout = millis();
  while (wifiClient.connected() && !wifiClient.available()) {
    if (millis() - timeout > 60000) { // 60 second timeout
      Serial.println("Response timeout");
      wifiClient.stop();
      return false;
    }
    delay(10);
  }

  // Skip HTTP headers to find BMP data
  String line;
  bool foundBMP = false;
  String responseHeaders = "";
  while (wifiClient.available()) {
    line = wifiClient.readStringUntil('\n');
    responseHeaders += line + "\n";
    if (line == "\r" || line.length() == 0) {
      foundBMP = true;
      break;
    }
  }

  Serial.println("=== HTTP Response Headers ===");
  Serial.println(responseHeaders);
  Serial.println("=============================");

  if (!foundBMP) {
    Serial.println("No BMP data in response");
    wifiClient.stop();
    return false;
  }

  Serial.println("Parsing BMP from response...");

  // Find BMP signature
  uint16_t signature = 0;
  for (int i = 0; i < 50; i++) {
    if (!wifiClient.available()) delay(100);
    else {
      signature = read16(wifiClient);
      if (signature == 0x4D42) break; // "BM" signature
    }
  }

  if (signature != 0x4D42) {
    Serial.printf("Invalid BMP signature: 0x%04X\n", signature);
    Serial.println("=== Server Response Body (first 500 bytes) ===");

    // Read and log the response body
    String responseBody = "";
    int bytesRead = 0;
    while (wifiClient.available() && bytesRead < 500) {
      char c = wifiClient.read();
      responseBody += c;
      bytesRead++;
    }

    Serial.println(responseBody);
    Serial.println("==============================================");

    wifiClient.stop();
    return false;
  }

  Serial.println("BMP signature found!");

  // Read BMP header
  uint32_t fileSize = read32(wifiClient);
  uint32_t creatorBytes = read32(wifiClient);
  uint32_t imageOffset = read32(wifiClient);
  uint32_t headerSize = read32(wifiClient);
  uint32_t bmpWidth = read32(wifiClient);
  int32_t bmpHeight = (int32_t)read32(wifiClient);
  uint16_t planes = read16(wifiClient);
  uint16_t depth = read16(wifiClient);
  uint32_t format = read32(wifiClient);

  Serial.printf("BMP: %dx%d, %d-bit\n", bmpWidth, abs(bmpHeight), depth);

  // Validate format
  if (planes != 1 || (format != 0 && format != 3)) {
    Serial.println("Unsupported BMP format");
    wifiClient.stop();
    return false;
  }

  // Determine if flipped
  bool flip = (bmpHeight > 0);
  if (bmpHeight < 0) bmpHeight = -bmpHeight;

  // Limit to printer width
  uint16_t w = min((uint16_t)bmpWidth, (uint16_t)384);
  uint16_t h = (uint16_t)bmpHeight;

  *width = w;
  *height = h;

  // Calculate row size (padded to 4-byte boundary)
  uint32_t rowSize = (bmpWidth * depth / 8 + 3) & ~3;
  if (depth < 8) rowSize = ((bmpWidth * depth + 8 - depth) / 8 + 3) & ~3;

  // Allocate bitmap buffer (1 bit per pixel)
  uint32_t bitmapSize = ((uint32_t)w * h + 7) / 8;
  *bitmap = (uint8_t*)ps_malloc(bitmapSize); // Use PSRAM

  if (*bitmap == NULL) {
    Serial.println("Failed to allocate bitmap buffer. Use Tools>PSRAM>OPI PSRAM");
    wifiClient.stop();
    return false;
  }

  memset(*bitmap, 0x00, bitmapSize); // Initialize to white

  Serial.printf("Allocated %d bytes for bitmap\n", bitmapSize);

  uint8_t bitmask = 0xFF;
  uint8_t bitshift = 8 - depth;
  if (depth < 8) bitmask >>= depth;

  uint32_t bytes_read = 7 * 4 + 3 * 2;

  // Read color palette if needed
  uint8_t mono_palette[256];
  if (depth <= 8) {
    bytes_read += skip(wifiClient, imageOffset - (4 << depth) - bytes_read);

    Serial.printf("Reading %d palette entries\n", 1 << depth);

    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
      uint8_t blue = wifiClient.read();
      uint8_t green = wifiClient.read();
      uint8_t red = wifiClient.read();
      wifiClient.read(); // Skip alpha
      bytes_read += 4;

      // Convert to monochrome
      mono_palette[pn] = ((red > 0x80) && (green > 0x80) && (blue > 0x80)) ? 1 : 0;
    }
  }

  // Position to image data
  uint32_t rowPosition = flip ? imageOffset + (bmpHeight - h) * rowSize : imageOffset;
  bytes_read += skip(wifiClient, rowPosition - bytes_read);

  Serial.println("Processing image data...");

  // Input buffer for row data
  const uint16_t input_buffer_size = 800;
  uint8_t* input_buffer = (uint8_t*)malloc(input_buffer_size);
  if (!input_buffer) {
    Serial.println("Failed to allocate input buffer");
    free(*bitmap);
    wifiClient.stop();
    return false;
  }

  // Process each row
  for (uint16_t row = 0; row < h; row++) {
    if (!(wifiClient.connected() || wifiClient.available())) {
      Serial.printf("Connection lost at row %d\n", row);
      break;
    }

    if (row % 50 == 0) {
      Serial.printf("Row %d/%d\n", row, h);
    }

    uint32_t in_remain = rowSize;
    uint32_t in_idx = 0;
    uint32_t in_bytes = 0;
    uint8_t in_byte = 0;
    uint8_t in_bits = 0;

    uint32_t row_offset = ((uint32_t)row * w);

    // Process each pixel
    for (uint16_t col = 0; col < w; col++) {
      // Read more data if needed
      if (in_idx >= in_bytes) {
        uint32_t get = min(in_remain, (uint32_t)input_buffer_size);
        uint32_t got = read8n(wifiClient, input_buffer, get);
        in_bytes = got;
        in_remain -= got;
        in_idx = 0;
        bytes_read += got;
      }

      bool whitish = false;

      // Decode pixel based on depth
      if (depth <= 8) {
        if (in_bits == 0) {
          in_byte = input_buffer[in_idx++];
          in_bits = 8;
        }
        uint16_t pn = (in_byte >> bitshift) & bitmask;
        whitish = mono_palette[pn];
        in_byte <<= depth;
        in_bits -= depth;
      } else if (depth == 24) {
        uint8_t blue = input_buffer[in_idx++];
        uint8_t green = input_buffer[in_idx++];
        uint8_t red = input_buffer[in_idx++];
        whitish = (red > 0x80) && (green > 0x80) && (blue > 0x80);
      } else if (depth == 32) {
        uint8_t blue = input_buffer[in_idx++];
        uint8_t green = input_buffer[in_idx++];
        uint8_t red = input_buffer[in_idx++];
        in_idx++; // Skip alpha
        whitish = (red > 0x80) && (green > 0x80) && (blue > 0x80);
      }

      // Set pixel (black = 1 for thermal printer)
      if (!whitish) {
        uint32_t bit_idx = row_offset + col;
        (*bitmap)[bit_idx / 8] |= (0x80 >> (bit_idx % 8));
      }
    }
  }

  free(input_buffer);
  wifiClient.stop();

  Serial.println("BMP parsed successfully!");
  return true;
}

// ============================================================================
// PRINT BITMAP
// ============================================================================
void printBitmap(uint8_t* bitmap, uint16_t w, uint16_t h) {
  Serial.printf("Printing bitmap %dx%d\n", w, h);
  myPrinter.printBitmapOld(bitmap, w, h, 0, true);
  delay(1000);
  myPrinter.feed(4);
  delay(1000);
  Serial.println("Print complete!");
}

// ============================================================================
// ERROR HANDLING
// ============================================================================
void handleError(const char* msg) {
  Serial.print("ERROR: ");
  Serial.println(msg);

  // Print error to thermal printer
  myPrinter.justify('C');
  myPrinter.println("ERROR");
  myPrinter.justify('L');
  myPrinter.println(msg);
  myPrinter.feed(3);

  // Flash LED error pattern (3 fast blinks)
  for (int i = 0; i < 3; i++) {
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(ledPin, LOW);
    delay(200);
  }

  currentState = STATE_ERROR;
}

// ============================================================================
// OFFLINE IMAGE PROCESSING
// ============================================================================

// TJpgDec type definitions (if not already defined)
#ifndef UINT
typedef unsigned int UINT;
#endif
#ifndef BYTE
typedef unsigned char BYTE;
#endif

// TJpg_Decoder callback structure for grayscale conversion
struct GrayscaleDecodeContext {
  uint8_t* buffer;
  uint16_t width;
  uint16_t height;
};

// Global context for TJpg_Decoder callback
static GrayscaleDecodeContext* g_decodeContext = NULL;

// TJpg_Decoder callback - converts RGB565 blocks to grayscale
bool tjpgd_grayscale_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (!g_decodeContext || !g_decodeContext->buffer) return 0;

  // Stop if rendering off bottom
  if (y >= g_decodeContext->height) return 0;

  // Convert RGB565 block to grayscale
  for (uint16_t dy = 0; dy < h; dy++) {
    if ((y + dy) >= g_decodeContext->height) break;

    for (uint16_t dx = 0; dx < w; dx++) {
      if ((x + dx) >= g_decodeContext->width) break;

      // Get RGB565 pixel
      uint16_t rgb565 = bitmap[dy * w + dx];

      // Extract RGB components from RGB565
      uint8_t r = (rgb565 >> 11) & 0x1F;  // 5 bits red
      uint8_t g = (rgb565 >> 5) & 0x3F;   // 6 bits green
      uint8_t b = rgb565 & 0x1F;          // 5 bits blue

      // Scale to 8-bit
      r = (r * 255) / 31;
      g = (g * 255) / 63;
      b = (b * 255) / 31;

      // Convert to grayscale (standard luminance formula)
      uint8_t gray = (uint8_t)((r * 0.299f) + (g * 0.587f) + (b * 0.114f));

      // Store in buffer
      uint32_t idx = (y + dy) * g_decodeContext->width + (x + dx);
      g_decodeContext->buffer[idx] = gray;
    }
  }

  return 1;  // Continue decoding
}

// Decode JPEG to grayscale using TJpg_Decoder library
uint8_t* decodeJpegToGrayscale(const uint8_t* jpegData, size_t jpegSize,
                               uint16_t* outWidth, uint16_t* outHeight) {
  Serial.println("Decoding JPEG with TJpg_Decoder...");

  // Get JPEG dimensions first
  uint16_t w = 0, h = 0;
  TJpgDec.getJpgSize(&w, &h, (uint8_t*)jpegData, jpegSize);

  if (w == 0 || h == 0) {
    Serial.println("Failed to get JPEG dimensions");
    return NULL;
  }

  *outWidth = w;
  *outHeight = h;

  Serial.printf("JPEG size: %dx%d\n", w, h);

  // Allocate output buffer in PSRAM
  uint32_t bufferSize = w * h;
  uint8_t* grayscale = (uint8_t*)ps_malloc(bufferSize);

  if (!grayscale) {
    Serial.println("Failed to allocate grayscale buffer");
    return NULL;
  }

  // Setup decode context
  GrayscaleDecodeContext context = {grayscale, w, h};
  g_decodeContext = &context;

  // Configure TJpg_Decoder
  TJpgDec.setJpgScale(1);        // 1:1 scale (no downsampling)
  TJpgDec.setSwapBytes(false);   // No byte swapping needed for our callback
  TJpgDec.setCallback(tjpgd_grayscale_output);

  // Decode JPEG
  if (TJpgDec.drawJpg(0, 0, (uint8_t*)jpegData, jpegSize) != 0) {
    Serial.println("JPEG decode failed");
    free(grayscale);
    g_decodeContext = NULL;
    return NULL;
  }

  g_decodeContext = NULL;
  Serial.println("JPEG decoded successfully");
  return grayscale;
}

// Crop to aspect ratio (center crop)
uint8_t* cropToAspectRatio(uint8_t* grayscale, uint16_t inW, uint16_t inH,
                           float targetRatio, uint16_t* outW, uint16_t* outH) {
  Serial.println("Cropping to aspect ratio...");

  float currentRatio = (float)inW / inH;
  uint16_t cropW, cropH, offsetX, offsetY;

  if (currentRatio > targetRatio) {
    // Too wide - crop width
    cropW = (uint16_t)(inH * targetRatio);
    cropH = inH;
    offsetX = (inW - cropW) / 2;
    offsetY = 0;
  } else {
    // Too tall - crop height
    cropW = inW;
    cropH = (uint16_t)(inW / targetRatio);
    offsetX = 0;
    offsetY = (inH - cropH) / 2;
  }

  Serial.printf("Crop: %dx%d from %dx%d (offset %d,%d)\n",
                cropW, cropH, inW, inH, offsetX, offsetY);

  // Allocate cropped buffer
  uint8_t* cropped = (uint8_t*)ps_malloc(cropW * cropH);
  if (!cropped) {
    Serial.println("Failed to allocate crop buffer");
    return NULL;
  }

  // Copy cropped region
  for (uint16_t y = 0; y < cropH; y++) {
    for (uint16_t x = 0; x < cropW; x++) {
      uint32_t srcIdx = (offsetY + y) * inW + (offsetX + x);
      uint32_t destIdx = y * cropW + x;
      cropped[destIdx] = grayscale[srcIdx];
    }
  }

  *outW = cropW;
  *outH = cropH;

  return cropped;
}

// Bilinear resize
uint8_t* resizeGrayscale(uint8_t* grayscale, uint16_t inW, uint16_t inH,
                         uint16_t targetW, uint16_t* outW, uint16_t* outH) {
  Serial.println("Resizing image...");

  uint16_t targetH = (uint16_t)((float)targetW / inW * inH);

  Serial.printf("Resize: %dx%d -> %dx%d\n", inW, inH, targetW, targetH);

  uint8_t* resized = (uint8_t*)ps_malloc(targetW * targetH);
  if (!resized) {
    Serial.println("Failed to allocate resize buffer");
    return NULL;
  }

  float xRatio = (float)inW / targetW;
  float yRatio = (float)inH / targetH;

  for (uint16_t y = 0; y < targetH; y++) {
    for (uint16_t x = 0; x < targetW; x++) {
      float srcX = x * xRatio;
      float srcY = y * yRatio;

      uint16_t x0 = (uint16_t)srcX;
      uint16_t y0 = (uint16_t)srcY;
      uint16_t x1 = min((uint16_t)(x0 + 1), (uint16_t)(inW - 1));
      uint16_t y1 = min((uint16_t)(y0 + 1), (uint16_t)(inH - 1));

      float xFrac = srcX - x0;
      float yFrac = srcY - y0;

      // Bilinear interpolation
      uint8_t p00 = grayscale[y0 * inW + x0];
      uint8_t p10 = grayscale[y0 * inW + x1];
      uint8_t p01 = grayscale[y1 * inW + x0];
      uint8_t p11 = grayscale[y1 * inW + x1];

      float val = p00 * (1 - xFrac) * (1 - yFrac) +
                  p10 * xFrac * (1 - yFrac) +
                  p01 * (1 - xFrac) * yFrac +
                  p11 * xFrac * yFrac;

      resized[y * targetW + x] = (uint8_t)val;
    }
  }

  *outW = targetW;
  *outH = targetH;

  return resized;
}

// Normalize (histogram stretching)
void normalizeImage(uint8_t* grayscale, uint32_t size) {
  Serial.println("Normalizing image...");

  uint8_t minVal = 255, maxVal = 0;
  for (uint32_t i = 0; i < size; i++) {
    if (grayscale[i] < minVal) minVal = grayscale[i];
    if (grayscale[i] > maxVal) maxVal = grayscale[i];
  }

  Serial.printf("Normalize: min=%d, max=%d\n", minVal, maxVal);

  if (maxVal == minVal) return;  // Avoid division by zero

  float scale = 255.0f / (maxVal - minVal);
  for (uint32_t i = 0; i < size; i++) {
    grayscale[i] = (uint8_t)((grayscale[i] - minVal) * scale);
  }
}

// Auto-level (2%-98% percentiles)
void autoLevelImage(uint8_t* grayscale, uint32_t size) {
  Serial.println("Auto-leveling image...");

  // Build histogram
  uint32_t histogram[256] = {0};
  for (uint32_t i = 0; i < size; i++) {
    histogram[grayscale[i]]++;
  }

  // Find 2% and 98% percentiles
  uint32_t threshold = size * 0.02f;
  uint32_t cumulative = 0;
  uint8_t blackPoint = 0, whitePoint = 255;

  for (int i = 0; i < 256; i++) {
    cumulative += histogram[i];
    if (cumulative >= threshold) {
      blackPoint = i;
      break;
    }
  }

  cumulative = 0;
  for (int i = 255; i >= 0; i--) {
    cumulative += histogram[i];
    if (cumulative >= threshold) {
      whitePoint = i;
      break;
    }
  }

  Serial.printf("Auto-level: black=%d, white=%d\n", blackPoint, whitePoint);

  if (whitePoint <= blackPoint) return;

  float scale = 255.0f / (whitePoint - blackPoint);
  for (uint32_t i = 0; i < size; i++) {
    int val = grayscale[i];
    if (val <= blackPoint) grayscale[i] = 0;
    else if (val >= whitePoint) grayscale[i] = 255;
    else grayscale[i] = (uint8_t)((val - blackPoint) * scale);
  }
}

// Apply gamma correction (LUT-based)
void applyGamma(uint8_t* grayscale, uint32_t size, float gamma) {
  Serial.printf("Applying gamma: %.2f\n", gamma);

  // Pre-compute gamma LUT
  static uint8_t gammaLUT[256];
  static bool lutInit = false;
  static float lastGamma = 0;

  if (!lutInit || gamma != lastGamma) {
    for (int i = 0; i < 256; i++) {
      float normalized = i / 255.0f;
      float corrected = powf(normalized, 1.0f / gamma);
      gammaLUT[i] = (uint8_t)(corrected * 255.0f);
    }
    lutInit = true;
    lastGamma = gamma;
  }

  // Apply gamma
  for (uint32_t i = 0; i < size; i++) {
    grayscale[i] = gammaLUT[grayscale[i]];
  }
}

// Apply brightness and contrast
void applyBrightnessContrast(uint8_t* grayscale, uint32_t size,
                             int brightness, int contrast) {
  Serial.printf("Applying brightness: %d, contrast: %d\n", brightness, contrast);

  // Build adjustment LUT
  uint8_t adjustLUT[256];
  float contrastFactor = (259.0f * (contrast + 255.0f)) /
                        (255.0f * (259.0f - contrast));

  for (int i = 0; i < 256; i++) {
    int val = i + brightness;
    val = (int)(contrastFactor * (val - 128) + 128);

    // Clamp
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    adjustLUT[i] = (uint8_t)val;
  }

  // Apply adjustment
  for (uint32_t i = 0; i < size; i++) {
    grayscale[i] = adjustLUT[grayscale[i]];
  }
}

// Atkinson dithering
uint8_t* atkinsonDither(const uint8_t* grayscale, uint16_t width, uint16_t height) {
  Serial.println("Applying Atkinson dithering...");

  // Allocate bitmap
  uint32_t bitmapSize = ((uint32_t)width * height + 7) / 8;
  uint8_t* bitmap = (uint8_t*)ps_malloc(bitmapSize);
  if (!bitmap) {
    Serial.println("Failed to allocate bitmap");
    return NULL;
  }
  memset(bitmap, 0x00, bitmapSize);  // Initialize to white

  // Working buffer for error diffusion
  float* workingImage = (float*)ps_malloc(width * height * sizeof(float));
  if (!workingImage) {
    Serial.println("Failed to allocate working buffer");
    free(bitmap);
    return NULL;
  }

  // Normalize to 0.0-1.0
  for (uint32_t i = 0; i < width * height; i++) {
    workingImage[i] = grayscale[i] / 255.0f;
  }

  // Error diffusion
  const float ERROR_FACTOR = 0.125f;  // 1/8

  for (uint16_t y = 0; y < height; y++) {
    for (uint16_t x = 0; x < width; x++) {
      uint32_t idx = y * width + x;
      float oldPixel = workingImage[idx];
      float newPixel = (oldPixel > 0.5f) ? 1.0f : 0.0f;

      // Set bitmap pixel (black=1, white=0)
      if (newPixel < 0.5f) {
        bitmap[idx / 8] |= (0x80 >> (idx % 8));
      }

      // Calculate error
      float error = (oldPixel - newPixel) * ERROR_FACTOR;

      // Diffuse error to 6 neighbors
      if (x + 1 < width) workingImage[idx + 1] += error;
      if (x + 2 < width) workingImage[idx + 2] += error;

      if (y + 1 < height) {
        if (x - 1 >= 0) workingImage[idx + width - 1] += error;
        workingImage[idx + width] += error;
        if (x + 1 < width) workingImage[idx + width + 1] += error;
      }

      if (y + 2 < height) {
        workingImage[idx + 2 * width] += error;
      }
    }

    // Progress indicator
    if (y % 50 == 0) {
      Serial.printf("Dithering: %d/%d\n", y, height);
    }
  }

  free(workingImage);

  Serial.println("Dithering complete");
  return bitmap;
}

// Main offline processing pipeline
bool processImageLocally(camera_fb_t* jpeg, uint8_t** outBitmap,
                        uint16_t* outWidth, uint16_t* outHeight) {
  Serial.println("\n=== PROCESSING IMAGE LOCALLY ===");

  unsigned long startTime = millis();

  // Step 1: Decode JPEG
  uint16_t w, h;
  uint8_t* grayscale = decodeJpegToGrayscale(jpeg->buf, jpeg->len, &w, &h);
  if (!grayscale) {
    Serial.println("JPEG decode failed");
    return false;
  }

  Serial.printf("Free PSRAM after decode: %d bytes\n", ESP.getFreePsram());

  // Step 2: Crop to 3:4
  uint16_t cropW, cropH;
  uint8_t* cropped = cropToAspectRatio(grayscale, w, h, TARGET_ASPECT_RATIO,
                                       &cropW, &cropH);
  free(grayscale);  // Free original

  if (!cropped) {
    Serial.println("Crop failed");
    return false;
  }

  // Step 3: Resize to 384px
  uint16_t resizeW, resizeH;
  uint8_t* resized = resizeGrayscale(cropped, cropW, cropH, TARGET_WIDTH,
                                     &resizeW, &resizeH);
  free(cropped);  // Free cropped

  if (!resized) {
    Serial.println("Resize failed");
    return false;
  }

  Serial.printf("Free PSRAM after resize: %d bytes\n", ESP.getFreePsram());

  // Step 4: Apply adjustments (in-place)
  uint32_t size = resizeW * resizeH;
  normalizeImage(resized, size);
  autoLevelImage(resized, size);
  applyGamma(resized, size, GAMMA_VALUE);
  applyBrightnessContrast(resized, size, BRIGHTNESS_VALUE, CONTRAST_VALUE);

  // Step 5: Dither to 1-bit
  *outBitmap = atkinsonDither(resized, resizeW, resizeH);
  *outWidth = resizeW;
  *outHeight = resizeH;

  free(resized);  // Free resized grayscale

  if (!*outBitmap) {
    Serial.println("Dithering failed");
    return false;
  }

  unsigned long elapsed = millis() - startTime;
  Serial.printf("\n=== LOCAL PROCESSING COMPLETE: %lu ms ===\n", elapsed);
  Serial.printf("Output: %dx%d bitmap\n", *outWidth, *outHeight);
  Serial.printf("Free PSRAM: %d bytes\n", ESP.getFreePsram());

  return true;
}

// ============================================================================
// PERIPHERAL SHUTDOWN FOR DEEP SLEEP
// ============================================================================
void shutdownPeripherals() {
  Serial.println("Shutting down peripherals for deep sleep...");

  // 1. Turn off LED
  digitalWrite(ledPin, LOW);

  // 2. Shutdown WiFi completely
  Serial.println("Stopping WiFi...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
 // esp_wifi_stop();

  // 3. Shutdown UART (printer)
  Serial.println("Stopping printer UART...");
  printerSerial.flush();
  printerSerial.end();
  gpio_reset_pin(GPIO_NUM_43); // TX
  gpio_reset_pin(GPIO_NUM_44); // RX

  // 4. Detach button interrupt (EXT0 will handle wake)
  Serial.println("Detaching button interrupt...");
  detachInterrupt(digitalPinToInterrupt(buttonPin));

  // 5. Shutdown camera (may cause watchdog issues - known bug)
  Serial.println("Deinitializing camera...");
  esp_err_t err = esp_camera_deinit();
  if (err != ESP_OK) {
    Serial.printf("Camera deinit warning: 0x%x (may be expected)\n", err);
  }

  Serial.println("Peripheral shutdown complete");
}

// ============================================================================
// ENTER DEEP SLEEP
// ============================================================================
void enterDeepSleep() {
  Serial.println("\n=== ENTERING DEEP SLEEP ===");

  // Increment sleep counter (RTC memory)
  sleepCount++;
  Serial.printf("Sleep cycle #%d\n", sleepCount);

  // Configure wake sources
  if(timerWakeEnable) {
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_US);
    Serial.printf("Timer wake: %d seconds\n", (int)(SLEEP_DURATION_US / 1000000));
  }

  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0); // Wake on LOW (button press)
  Serial.println("EXT0 wake: GPIO 1 (button)");

  // Shutdown all peripherals
  shutdownPeripherals();

  // Final flush before sleep
  Serial.println("Going to sleep NOW...");
  Serial.flush();
  delay(100); // Allow serial to complete

  // Enter deep sleep (does not return)
  esp_deep_sleep_start();
}

// ============================================================================
// STATE MACHINE
// ============================================================================
void setState(SystemState newState) {
  Serial.printf("State: %s -> %s\n", stateNames[currentState], stateNames[newState]);
  currentState = newState;
}

void runStateMachine() {
  static camera_fb_t* fb = NULL;
  static uint8_t* bitmap = NULL;
  static uint16_t w = 0, h = 0;

  switch (currentState) {
    case STATE_IDLE:
      digitalWrite(ledPin, LOW); // LED OFF
      if (checkButton()) {
        Serial.println("Button pressed!");
        setState(STATE_COUNTDOWN);
      }
      break;

    case STATE_COUNTDOWN:
      ledCountdown(5);
      setState(STATE_CAPTURING);
      break;

    case STATE_CAPTURING:
      Serial.println("Capturing photo...");
      fb = esp_camera_fb_get();
      if (!fb) {
        digitalWrite(ledPin, LOW); // LED OFF
        handleError("Camera capture failed");
      } else {
        Serial.printf("Photo captured: %d bytes\n", fb->len);
        digitalWrite(ledPin, LOW); // LED OFF after capture
        setState(STATE_UPLOADING);
      }
      break;

    case STATE_UPLOADING:
      if (uploadAndReceiveBMP(fb, &bitmap, &w, &h)) {
        esp_camera_fb_return(fb);
        fb = NULL;
        setState(STATE_PRINTING);
      } else {
        // Server failed - try local processing
        Serial.println("Server failed, switching to local processing");
        setState(STATE_PROCESSING_LOCAL);
      }
      break;

    case STATE_PROCESSING_LOCAL:
      if (processImageLocally(fb, &bitmap, &w, &h)) {
        esp_camera_fb_return(fb);
        fb = NULL;
        setState(STATE_PRINTING);
      } else {
        esp_camera_fb_return(fb);
        fb = NULL;
        handleError("Local processing failed");
      }
      break;

    case STATE_PRINTING:
      printBitmap(bitmap, w, h);
      free(bitmap);
      bitmap = NULL;
      photoCount++; // Increment photo counter
      setState(STATE_SLEEP);
      break;

    case STATE_SLEEP:
      delay(5000); // Allow printer to finish
      enterDeepSleep(); // Does not return
      break;

    case STATE_ERROR:
      delay(5000);
      setState(STATE_IDLE);
      break;
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  // Check wake reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  Serial.println("\n================================");
  Serial.println("Fugace XIAO Firmware v1.2");
  Serial.println("Seeed XIAO ESP32S3 Sense");
  Serial.println("Offline Processing Enabled");
  Serial.println("================================\n");

  // Log wake reason
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0:
      Serial.println("Wake: Button pressed (EXT0)");
      break;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println("Wake: Timer (20s timeout)");
      break;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
      Serial.println("Wake: First boot or hardware reset");
      sleepCount = 0; // Reset RTC counter
      photoCount = 0;
      break;
  }

  Serial.printf("Sleep cycles: %d | Photos: %d\n\n", sleepCount, photoCount);

  // Initialize LED
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW); // LED OFF

  // Initialize camera
  if (!initCamera()) {
    Serial.println("FATAL: Camera initialization failed");
    while(1) delay(1000);
  }

  // Initialize printer
  if (!initPrinter()) {
    Serial.println("FATAL: Printer initialization failed");
    while(1) delay(1000);
  }

  // Initialize button
  if (!initButton()) {
    Serial.println("FATAL: Button initialization failed");
    while(1) delay(1000);
  }

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  Serial.print("Connecting to WiFi (10s): ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int timeout = 2;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed!");
  } else {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
    Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());
  }

  // Print ready message
  //myPrinter.justify('C');
  //myPrinter.println("PhotoBooth Ready!");
 // myPrinter.println("XIAO ESP32S3");
 // myPrinter.println("Press button to");
 // myPrinter.println("take a photo");
  myPrinter.feed(2);

  setState(STATE_IDLE);
  Serial.println("\nReady! Press button to start.\n");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  runStateMachine();
  delay(10); // Small delay to prevent watchdog issues
}
