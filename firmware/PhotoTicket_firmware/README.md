# PhotoTicket Firmware

ESP32 firmware for downloading and printing BMP images on a thermal printer.

## Requirements

- ESP32 board
- Thermal printer compatible with Adafruit protocol
- Arduino IDE with ESP32 board support

## Library

This firmware requires the ThermalPrinter library:
https://github.com/marcteys/ThermalPrinter

Install it in your Arduino libraries folder.

## Setup

1. Copy `secrets.h.example` to `secrets.h`
2. Edit `secrets.h` with your WiFi credentials and image URL:
   ```cpp
   const char* ssid = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   const char* imageUrl = "https://your-domain.com/path/to/image.bmp";
   ```

3. Configure printer pins if needed (in the main .ino file):
   ```cpp
   const byte rxPin = 16;
   const byte txPin = 17;
   const byte dtrPin = 14;
   ```

4. Upload the firmware to your ESP32

## Usage

The firmware automatically:
1. Connects to WiFi
2. Downloads the BMP image from the configured URL
3. Prints the image on the thermal printer
4. Feeds paper and completes

Monitor the Serial output (115200 baud) for debugging information.
