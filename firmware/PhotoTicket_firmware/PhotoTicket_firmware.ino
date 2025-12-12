// made by BinaryWorlds
// Not for commercial use, in other case by free to use it.
// Just copy this text and link to oryginal repository:
// https://github.com/BinaryWorlds/ThermalPrinter

// I am not responsible for errors in the library. I deliver it "as it is".
// I will be grateful for all suggestions.

// Tested on firmware 2.69 and JP-QR701
// Some features may not work on the older firmware.

// This example demonstrates downloading a BMP image from a URL
// and printing it using the printBitmapOld() function

#include <Arduino.h>

#include <HardwareSerial.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "TPrinter.h"
#include "secrets.h"

// Printer Configuration
const byte rxPin = 16;
const byte txPin = 17;
const byte dtrPin = 14;  // optional

HardwareSerial mySerial(2);
Tprinter myPrinter(&mySerial, 9600);

// BMP Parsing Helper Functions
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

// Download and print BMP image
bool downloadAndPrintBMP(const char* url) {
  WiFiClientSecure *client = new WiFiClientSecure;
  if (!client) {
    Serial.println("Failed to create WiFi client");
    return false;
  }

  // Ignore SSL certificate validation
  client->setInsecure();

  HTTPClient https;

  Serial.print("[HTTPS] Connecting to: ");
  Serial.println(url);

  if (!https.begin(*client, url)) {
    Serial.println("[HTTPS] Unable to connect");
    delete client;
    return false;
  }

  Serial.println("[HTTPS] Sending GET request...");
  int httpCode = https.GET();

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[HTTPS] GET failed, error: %d\n", httpCode);
    https.end();
    delete client;
    return false;
  }

  Serial.println("[HTTPS] Response received, parsing BMP...");

  // Get the stream
  WiFiClient& stream = https.getStream();

  // Find BMP signature
  uint16_t signature = 0;
  for (int i = 0; i < 50; i++) {
    if (!stream.available()) delay(100);
    else {
      signature = read16(stream);
      if (signature == 0x4D42) break;  // "BM" signature
    }
  }

  if (signature != 0x4D42) {
    Serial.println("Invalid BMP file!");
    https.end();
    delete client;
    return false;
  }

  Serial.println("BMP signature found!");

  // Read BMP header
  uint32_t fileSize = read32(stream);
  uint32_t creatorBytes = read32(stream);
  uint32_t imageOffset = read32(stream);
  uint32_t headerSize = read32(stream);
  uint32_t width = read32(stream);
  int32_t height = (int32_t)read32(stream);
  uint16_t planes = read16(stream);
  uint16_t depth = read16(stream);
  uint32_t format = read32(stream);

  Serial.printf("Image: %dx%d, %d-bit\n", width, abs(height), depth);

  // Check format
  if (planes != 1 || (format != 0 && format != 3)) {
    Serial.println("Unsupported BMP format!");
    https.end();
    delete client;
    return false;
  }

  // Determine if image is flipped
  bool flip = (height > 0);
  if (height < 0) height = -height;

  // Limit width to printer capability (384 pixels)
  uint16_t w = min((uint16_t)width, (uint16_t)384);
  uint16_t h = (uint16_t)height;

  // Calculate row size (padded to 4-byte boundary)
  uint32_t rowSize = (width * depth / 8 + 3) & ~3;
  if (depth < 8) rowSize = ((width * depth + 8 - depth) / 8 + 3) & ~3;

  // Calculate bitmap size (1 bit per pixel, rounded to bytes)
  uint32_t bitmapSize = ((uint32_t)w * h + 7) / 8;

  // Allocate bitmap buffer
  uint8_t* bitmap = (uint8_t*)malloc(bitmapSize);
  if (!bitmap) {
    Serial.println("Failed to allocate memory!");
    https.end();
    delete client;
    return false;
  }

  // Initialize to white (all 0s for thermal printer)
  memset(bitmap, 0x00, bitmapSize);

  uint8_t bitmask = 0xFF;
  uint8_t bitshift = 8 - depth;
  if (depth < 8) bitmask >>= depth;

  uint32_t bytes_read = 7 * 4 + 3 * 2;

  // Read color palette if needed
  uint8_t mono_palette[256];
  if (depth <= 8) {
    bytes_read += skip(stream, imageOffset - (4 << depth) - bytes_read);

    Serial.printf("Reading %d color palette entries...\n", 1 << depth);

    for (uint16_t pn = 0; pn < (1 << depth); pn++) {
      uint8_t blue = stream.read();
      uint8_t green = stream.read();
      uint8_t red = stream.read();
      stream.read();  // Skip alpha
      bytes_read += 4;

      // Convert to monochrome: white if bright, black otherwise
      mono_palette[pn] = ((red > 0x80) && (green > 0x80) && (blue > 0x80)) ? 1 : 0;
    }
  }

  // Position to start of image data
  uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
  bytes_read += skip(stream, rowPosition - bytes_read);

  Serial.println("Processing image data...");

  // Input buffer for reading row data
  const uint16_t input_buffer_size = 800;
  uint8_t* input_buffer = (uint8_t*)malloc(input_buffer_size);
  if (!input_buffer) {
    Serial.println("Failed to allocate input buffer!");
    free(bitmap);
    https.end();
    delete client;
    return false;
  }

  // Process each row
  for (uint16_t row = 0; row < h; row++) {
    if (!(stream.connected() || stream.available())) {
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

    // Calculate row offset in bitmap buffer
    uint32_t row_offset = ((uint32_t)row * w);

    // Process each pixel in the row
    for (uint16_t col = 0; col < w; col++) {
      // Read more data if needed
      if (in_idx >= in_bytes) {
        uint32_t get = min(in_remain, (uint32_t)input_buffer_size);
        uint32_t got = read8n(stream, input_buffer, get);
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
        in_idx++;  // Skip alpha
        whitish = (red > 0x80) && (green > 0x80) && (blue > 0x80);
      }

      // Set pixel in bitmap (black = 1 bit for thermal printer)
      if (!whitish) {
        uint32_t bit_idx = row_offset + col;
        bitmap[bit_idx / 8] |= (0x80 >> (bit_idx % 8));
      }
    }
  }

  free(input_buffer);

  Serial.println("Image processed!");
  Serial.printf("Bitmap size: %d bytes\n", bitmapSize);

  // Print the bitmap using printBitmapOld
  // Mode 0: Normal size, centered
  myPrinter.printBitmapOld(bitmap, w, h, 0, true);
  myPrinter.feed(2);

  // Clean up
  free(bitmap);
  https.end();
  delete client;

  Serial.println("Print complete!");
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n================================");
  Serial.println("BMP Download & Print Example");
  Serial.println("================================\n");

  // Initialize printer
  mySerial.begin(9600, SERIAL_8N1, rxPin, txPin);

  // Uncomment to enable DTR pin
  myPrinter.enableDtr(dtrPin, LOW);

  myPrinter.begin();
  myPrinter.setHeat(1, 224, 40);
  myPrinter.justify('C');

  Serial.println("Printer initialized\n");

  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int timeout = 30;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nWiFi connection failed!");
    myPrinter.println("WiFi Error");
    myPrinter.feed(3);
    return;
  }

  Serial.println("\nWiFi connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Download and print the BMP image
  Serial.println("\nDownloading and printing BMP...");
  bool success = downloadAndPrintBMP(imageUrl);

  if (success) {
   // myPrinter.println("Image printed!");
  } else {
    myPrinter.println("Print failed");
  }

  myPrinter.feed(3);

  Serial.println("\n================================");
  Serial.println("Complete!");
  Serial.println("================================\n");
}

void loop() {}
