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
// STATE MACHINE
// ============================================================================
enum SystemState {
  STATE_IDLE,
  STATE_COUNTDOWN,
  STATE_CAPTURING,
  STATE_UPLOADING,
  STATE_PRINTING,
  STATE_ERROR
};

volatile SystemState currentState = STATE_IDLE;
const char* stateNames[] = {"IDLE", "COUNTDOWN", "CAPTURING", "UPLOADING", "PRINTING", "ERROR"};

// ============================================================================
// HARDWARE OBJECTS
// ============================================================================
HardwareSerial printerSerial(1); // Use Serial1 for XIAO
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
  printerSerial.begin(9600, SERIAL_8N1, rxPin, txPin);
  delay(100);

  myPrinter.enableDtr(dtrPin, LOW);
  myPrinter.begin();
  myPrinter.setHeat(1, 224, 40);
  myPrinter.justify('C');

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
    Serial.println("Failed to allocate bitmap buffer");
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
  myPrinter.feed(3);
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
        esp_camera_fb_return(fb);
        fb = NULL;
        handleError("Upload/Download failed");
      }
      break;

    case STATE_PRINTING:
      printBitmap(bitmap, w, h);
      free(bitmap);
      bitmap = NULL;
      setState(STATE_IDLE);
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

  Serial.println("\n================================");
  Serial.println("PhotoBooth XIAO Firmware v1.0");
  Serial.println("Seeed XIAO ESP32S3 Sense");
  Serial.println("================================\n");

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
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int timeout = 60;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nFATAL: WiFi connection failed");
    while(1) delay(1000);
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
  Serial.printf("PSRAM size: %d bytes\n", ESP.getPsramSize());

  // Print ready message
  myPrinter.justify('C');
  myPrinter.println("PhotoBooth Ready!");
  myPrinter.println("XIAO ESP32S3");
  myPrinter.println("Press button to");
  myPrinter.println("take a photo");
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
               