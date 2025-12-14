# Fugace -  XIAO Firmware

XIAO ESP32S3-based photo booth that captures photos, processes them on a server, and prints them on a thermal printer.

## Features

- **Button-triggered workflow**: Single button press triggers entire process
- **LED countdown**: 5-second visual countdown before capture
- **High-resolution capture**: UXGA (1600x1200) with PSRAM
- **Auto-processing**: Server handles image cropping (3:4 ratio) and dithering
- **Thermal printing**: Prints 384px wide bitmap on thermal printer

## Hardware Requirements

### Components
- **Seeed XIAO ESP32S3 Sense** - Microcontroller with OV2640 camera
- **Thermal Printer (JP-QR701)** - 203 DPI thermal printer
- **Push Button** - Momentary button for triggering
- **5V Power Supply** - Adequate for XIAO + printer

### Wiring

| Component | XIAO ESP32S3 Pin | Notes |
|-----------|------------------|-------|
| Printer RX | GPIO 44 (D8) | HardwareSerial1 RX |
| Printer TX | GPIO 43 (D7) | HardwareSerial1 TX |
| Printer DTR | GPIO 2 (D1) | Optional |
| Button | GPIO 1 (D0) | With internal pullup |
| Built-in LED | GPIO 21 | Internal (for status) |

**XIAO ESP32S3 Pinout Reference:**
```
        USB-C
    ┌─────────────┐
D0  │ GPIO 1      │  3V3
D1  │ GPIO 2      │  GND
D2  │ GPIO 3      │  D10 (GPIO 5)
D3  │ GPIO 4      │  D9 (GPIO 6)
D4  │ GPIO 5      │  D8 (GPIO 44) ← Printer RX
D5  │ GPIO 6      │  D7 (GPIO 43) ← Printer TX
D6  │ GPIO 7      │  D6 (GPIO 8)
    └─────────────┘
```

**Button Wiring:**
- One side → GPIO 1 (D0)
- Other side → GND
- Internal pullup enabled in code

## Software Requirements

### Arduino IDE Setup

1. **Install ESP32 Board Support**
   - Open Arduino IDE 
   - Tools (sidebar) → Boards Manager → Search "ESP32" → Install

2. **Install Required Library**
   - ThermalPrinter library: https://github.com/marcteys/ThermalPrinter (with support for old ESC/POS command)
   - Download and place in Arduino libraries folder

3. **Board Selection**
   - Board: "XIAO_ESP32S3"
   - USB CDC On Boot: "Enabled"
   - PSRAM: "OPI PSRAM"
   - Upload Speed: 921600
   - Partition Scheme: "8M with spiffs (3MB APP/1.5MB SPIFFS)"

## Installation

### 1. Configure WiFi and Server


Edit `secrets.h` with your settings:
```cpp
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* serverName = "your-domain.com";
const char* serverPath = "/path/to/upload.php";
```

### 2. Upload Firmware

1. Connect XIAO ESP32S3 to computer via USB-C

2. Select Port: Tools → Port → (your serial port)

3. Click Upload

4. Open Serial Monitor (115200 baud) to verify operation

## Usage

### Normal Operation

1. Power on the XIAO ESP32S3
2. Wait for Wifi connection
3. Press the button on GPIO 1 (D0)
4. LED countdown begins (5 seconds, increasing blink speed)
5. LED goes solid → photo captured
6. LED turns off → uploading and processing
7. Printer outputs the dithered photo
8. System returns to idle

### Workflow Details

```
IDLE → Button Press → COUNTDOWN (5s LED) → CAPTURE → UPLOAD → PRINT → IDLE
```

**State Indicators:**
- **IDLE**: LED off, waiting for button
- **COUNTDOWN**: LED blinking (faster as countdown progresses)
- **CAPTURING**: LED solid ON
- **UPLOADING**: LED OFF
- **ERROR**: 3 fast blinks, error printed on printer

### Serial Monitoring

Connect via Serial Monitor (115200 baud) to see:
- Initialization status
- WiFi connection
- State transitions
- Image upload progress
- BMP parsing progress
- Error messages
- Memory usage (heap + PSRAM)

## Hardware Differences from AI Thinker ESP32-CAM

| Feature | XIAO ESP32S3 | AI Thinker ESP32-CAM |
|---------|--------------|---------------------|
| **Chip** | ESP32-S3 | ESP32 |
| **PSRAM** | 8MB OPI PSRAM | 4MB PSRAM |
| **Camera** | OV2640 (built-in) | OV2640 (built-in) |
| **LED** | GPIO 21 | GPIO 4 (active LOW) |
| **Serial** | Serial1 (GPIO 43/44) | Serial2 (GPIO 16/17) |
| **Button** | GPIO 1 (D0) | GPIO 12 |
| **Size** | 21×17.5mm | 27×40.5mm |
| **USB** | USB-C built-in | External FTDI required |

## Troubleshooting

### Camera Init Failed
- Verify "XIAO_ESP32S3" board is selected
- Enable PSRAM: Tools → PSRAM → "OPI PSRAM"
- Check OV2640 camera is properly seated

### Printer Not Responding
- Check wiring (GPIO 43 TX, GPIO 44 RX)
- Verify baud rate (9600)
- Test printer independently

### WiFi Connection Failed
- Verify SSID and password in `secrets.h`
- Check WiFi signal strength
- Use 2.4GHz network (ESP32 doesn't support 5GHz)

### Upload/Download Failed
- Check server URL and path
- Verify internet connectivity
- Check server logs for errors
- Increase timeout if slow connection

### Memory Errors
- Ensure PSRAM is enabled in Arduino IDE
- Monitor heap usage via Serial
- Check "8M with spiffs" partition scheme

### Button Not Working
- Check button wiring (GPIO 1 to GND)
- Verify button only triggers in IDLE state
- Adjust DEBOUNCE_DELAY if needed
- Use multimeter to test button continuity

### LED Not Blinking
- XIAO ESP32S3 LED is on GPIO 21
- LED is active HIGH (opposite of AI Thinker)
- Check if LED is soldered (some XIAO boards ship without LED)

## Advanced Configuration

### Adjust Camera Resolution

Edit `initCamera()` function:
```cpp
config.frame_size = FRAMESIZE_UXGA;  // Options: UXGA, SVGA, VGA, CIF
config.jpeg_quality = 10;             // Lower = higher quality (0-63)
```

Available frame sizes:
- FRAMESIZE_UXGA: 1600x1200
- FRAMESIZE_SVGA: 800x600
- FRAMESIZE_VGA: 640x480
- FRAMESIZE_CIF: 352x288

### Adjust LED Countdown

Edit `ledCountdown()` function:
```cpp
void ledCountdown(int seconds) {
  // Change countdown duration (default: 5 seconds)
  for (int i = seconds; i > 0; i--) {
    // Adjust blink speed mapping
    int blinkDelay = map(i, seconds, 1, 500, 50);
    // ...
  }
}
```

### Adjust Printer Heat

Edit `initPrinter()` function:
```cpp
myPrinter.setHeat(1, 224, 40);
// setHeat(dots, time, interval)
// dots: heating dots (1-255)
// time: heating time (0-255 * 10us)
// interval: interval between lines (0-255 * 10us)
```

### Change Button Pin

Edit pin definition:
```cpp
const byte buttonPin = 1;  // Change to available GPIO
```

Available GPIO on XIAO ESP32S3 (not used by camera):
- D0 (GPIO 1) - Current button pin
- D1 (GPIO 2)
- D2 (GPIO 3)
- D3 (GPIO 4)
- D4 (GPIO 5)
- D5 (GPIO 6)
- D6 (GPIO 7)
- D8 (GPIO 8)
- D9 (GPIO 9)

## Technical Details

### Memory Management

The XIAO ESP32S3 has excellent memory:
- **SRAM**: ~512KB
- **PSRAM**: 8MB OPI (faster than SPI PSRAM)
- **Flash**: 8MB

Memory usage:
- Camera frame buffer: ~100KB (UXGA JPEG in PSRAM)
- Bitmap buffer: ~18KB (384px × 512px max in PSRAM)
- WiFi/TLS: ~30KB
- Remaining: Plenty for other tasks

### PSRAM Configuration

The firmware uses PSRAM for all large buffers:
```cpp
config.fb_location = CAMERA_FB_IN_PSRAM;  // Camera buffer in PSRAM
*bitmap = (uint8_t*)ps_malloc(size);       // Bitmap buffer in PSRAM
```

### Pin Usage Summary

**Camera Pins (reserved):**
- GPIO 10, 11, 12, 13, 14, 15, 16, 17, 18, 38, 39, 40, 47, 48

**Peripheral Pins (used):**
- GPIO 1: Button
- GPIO 2: Printer DTR
- GPIO 21: Built-in LED
- GPIO 43: Printer TX
- GPIO 44: Printer RX

**Available Pins:**
- GPIO 3, 4, 5, 6, 7, 8, 9 (expandable I/O)

## Server Configuration

The firmware expects the server endpoint to:
1. Accept POST requests with `multipart/form-data`
2. Accept parameters:
   - `imageFile` - JPEG image file
   - `ditherMode` - "Atkison" (hardcoded)
   - `auto` - "true" (hardcoded)
3. Return BMP image in response body:
   - 1-bit monochrome
   - 384px width
   - 3:4 aspect ratio

## Comparison with PhotoBoothUnified

| Feature | PhotoBoothXIAO | PhotoBoothUnified |
|---------|----------------|-------------------|
| **Board** | XIAO ESP32S3 Sense | AI Thinker ESP32-CAM |
| **Size** | Smaller (21×17.5mm) | Larger (27×40.5mm) |
| **USB** | Built-in USB-C | Requires FTDI |
| **PSRAM** | 8MB OPI (faster) | 4MB SPI |
| **Resolution** | UXGA (1600x1200) | SVGA (800x600) |
| **Firmware** | Nearly identical | Nearly identical |

Both firmwares share the same core logic, just adapted for different hardware.

## License

Based on code from:
- Seeed XIAO ESP32S3 examples
- ThermalPrinter library by BinaryWorlds

Free to use for personal projects. Not for commercial use without permission.

## Support

For issues or questions:
1. Check Serial Monitor output (115200 baud)
2. Verify hardware connections
3. Test camera and printer independently
4. Check server endpoint is accessible
5. Monitor memory usage (heap + PSRAM)

## Version History

**v1.0** - Initial release
- Button-triggered capture
- LED countdown (5s)
- Server upload and BMP download
- Thermal printer output
- XIAO ESP32S3 Sense support
- OPI PSRAM optimization




*Readme partially generated with Claude 4.5*