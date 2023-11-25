#include <Arduino.h>

//##################################
//####         WIFI
//##################################
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

// Replace with your network credentials
const char* ssid = "SSID";
const char* password = "password";

//##################################
//####         FILES
//##################################
#include <FS.h>
String filePath = "/phototicket.txt";

//##################################
//####    PRINTER
//##################################

#define USE_PRINTER 1
#include "Adafruit_Thermal.h"

#include "SoftwareSerial.h"
#define TX_PIN D6
#define RX_PIN D5
#define DTR_PIN D7
SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Thermal printer(&mySerial, DTR_PIN);

#include<Uduino.h>
Uduino uduino("myArduinoName"); // Declare and name your object




// Button 

const int buttonPin = D1;  // the number of the pushbutton pin

int buttonState;            // the current reading from the input pin
int lastButtonState = LOW;  // the previous reading from the input pin

unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers




void setup() {
  Serial.begin(115200);
  //Connect to Wi-Fi

  //##################################
  //####         WIFI
  //##################################

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println("Wifi connected! Init SPIFFS...");

  //##################################
  //####         FILES
  //##################################

  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
  // SPIFFS.format();

  //##################################
  //####    PRINTER
  //##################################

  mySerial.begin(9600);
  printer.begin(270);  // Init printer (same regardless of serial type)
  Serial.println("Printer init");

  printer.wake();        // MUST wake() before printing again, even if reset
  printer.setDefault();  // Restore printer to defaults

  uduino.addCommand("heat", heat);
  uduino.addCommand("density", density);
  uduino.addCommand("download", dw);
  uduino.addCommand("print", convertFile);
  uduino.addCommand("times", times);
  uduino.addCommand("feed", f);
  uduino.addCommand("test", test);
  uduino.addCommand("rst", rst);


  pinMode(buttonPin, INPUT_PULLUP);
// heat 7 80 40
// times 30000 2100
// heat 11 120 40

//bon parametre : heat 22 200 200 

//printer.setHeatConfig(22, 200, 200);
}

void rst () {
   printer.reset();
}

void test() {
  printer.begin();

  printer.setFont('B');
  printer.println("Hello, World!");
  printer.setFont('A');
  printer.println("Hello, World!");

  printer.inverseOn();
  printer.println(F("Good Bye, World!"));
  printer.inverseOff();

  printer.doubleHeightOn();
  printer.println(F("Large Text"));
  printer.doubleHeightOff();
}

void f() {
  char * a = uduino.getParameter(0);
  int one = uduino.charToInt(a);
  
  printer.feed(one);
}
void density() {
  char * a = uduino.getParameter(0);
  int one = uduino.charToInt(a);

  char * b = uduino.getParameter(1);
  int two = uduino.charToInt(b);

  Serial.println("setting print density");
  printer.setPrintDensity(one, two);
}

/*
 * 
 *   dotPrintTime = 30000; // See comments near top of file for
  dotFeedTime = 2100;   // an explanation of these values.
*/// Printer performance may vary based on the power supply voltage,
// thickness of paper, phase of the moon and other seemingly random
// variables.  This method sets the times (in microseconds) for the
// paper to advance one vertical 'dot' when printing and when feeding.
// For example, in the default initialized state, normal-sized text is
// 24 dots tall and the line spacing is 30 dots, so the time for one
// line to be issued is approximately 24 * print time + 6 * feed time.
// The default print and feed times are based on a random test unit,
// but as stated above your reality may be influenced by many factors.
// This lets you tweak the timing to avoid excessive delays and/or
// overrunning the printer buffer.

void times() {
  char * a = uduino.getParameter(0);
  int one = uduino.charToInt(a);

  char * b = uduino.getParameter(1);
  int two = uduino.charToInt(b);


  Serial.println("setting print times");

  printer.setTimes(one, two);

}

void dw() {
  downloadAndStoreFile("https://marcteyssier.com/experiment/PhotoTicket/phototicket.txt", "/phototicket.txt");
}

// n1 = "max heating dots" 0-255 -- max number of thermal print head
//      elements that will fire simultaneously.  Units = 8 dots (minus 1).
//      Printer default is 7 (64 dots, or 1/6 of 384-dot width), this code
//      sets it to 11 (96 dots, or 1/4 of width).
// n2 = "heating time" 3-255 -- duration that heating dots are fired.
//      Units = 10 us.  Printer default is 80 (800 us), this code sets it
//      to value passed (default 120, or 1.2 ms -- a little longer than
//      the default because we've increased the max heating dots).
// n3 = "heating interval" 0-255 -- recovery time between groups of
//      heating dots on line; possibly a function of power supply.
//      Units = 10 us.  Printer default is 2 (20 us), this code sets it
//      to 40 (throttled back due to 2A supply).
// More heating dots = more peak current, but faster printing speed.
// More heating time = darker print, but slower printing speed and
// possibly paper 'stiction'.  More heating interval = clearer print,
// but slower printing speed.

void heat() {
  char * a = uduino.getParameter(0);
  int one = uduino.charToInt(a);

  char * b = uduino.getParameter(1);
  int two = uduino.charToInt(b);

  char * c = uduino.getParameter(2);
  int three = uduino.charToInt(c);

  printer.setHeatConfig(one, two, three);

  Serial.println("setting print heat");

}

void loop() {
  uduino.update();
  ReadButton();
}



void ReadButton() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        DownloadAndPrint();
      }
    }
  }

  lastButtonState = reading;
}


void DownloadAndPrint() {
  downloadAndStoreFile("https://marcteyssier.com/experiment/PhotoTicket/phototicket.txt", "/phototicket.txt");
  convertFile();
  printer.feed(4);

}

void downloadAndStoreFile(const String& url, const String& filename) {

  // wait for WiFi connection
  if ((WiFi.status() == WL_CONNECTED)) {

    std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);

    // Ignore SSL certificate validation
    client->setInsecure();

    //create an HTTPClient instance
    HTTPClient https;

    //Initializing an HTTPS communication using the secure client
    Serial.print("[HTTPS] begin...\n");
    if (https.begin(*client, url)) {
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      int httpCode = https.GET();
      // httpCode will be negative on error
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTPS] GET... code: %d\n", httpCode);
        // file found at server
        if (httpCode == HTTP_CODE_OK) {
          File file = SPIFFS.open(filePath, "w");
          if (!file) {
            Serial.println("Failed to create file");
            return;
          }
          const size_t bufferSize = 1024;
          uint8_t buffer[bufferSize];
          Stream& response = https.getStream();
          size_t bytesRead;
          size_t totalBytesWritten = 0;

          while ((bytesRead = response.readBytes(buffer, bufferSize)) > 0) {
            file.write(buffer, bytesRead);
            totalBytesWritten += bytesRead;
          }

          file.close();
          Serial.println("File downloaded and stored in SPIFFS");
          Serial.print("Total bytes written: ");
          Serial.println(totalBytesWritten);
        }
      } else {
        Serial.printf("[HTTPS] GET... failed, error: %s\n", https.errorToString(httpCode).c_str());
      }
      https.end();
    } else {
      Serial.printf("[HTTPS] Unable to connect\n");
    }
  }
}


const size_t BUFFER_SIZE = 256;


// Test image is 384 * 139
// one line is 48 bytes
//
void convertFile() {
  File file = SPIFFS.open(filePath, "r");
  if (!file) {
    Serial.println("Failed to open file");
    return;
  }
  //##################################
  //####    PRINTER
  //##################################

  int file_width = 384;
  int file_height = 421;
  // Calculate height : 
  size_t fileSize = file.size();
  file_height = fileSize * 4 / file_width;
  

  
  int rounded_width = (file_width + 7) / 8;

  Serial.println("Converting and sending line by line");
  const size_t stringBufferSize = rounded_width * 2;
  uint8_t buffer[stringBufferSize]; // This is one signe line
  size_t bytesRead;
  const size_t byteBufferLineSize = stringBufferSize;
 uint8_t bitmapLine[byteBufferLineSize]; // todo : this should be  rounded_w
  int currentByte = 0;

  char tmp[3];
  tmp[2] = '\0';

  int i = 0;
  // uint8_t ttt[rounded_width*file_height];
  //int t = 0;
    printer.initPrintRasterBitmap(file_width, file_height);

  while ((bytesRead = file.read(buffer, stringBufferSize)) > 0) {
    currentByte = 0;
    for (i = 0; i < bytesRead; i = i + 2) {
      tmp[0] = (char)buffer[i];
      tmp[1] = (char)buffer[i + 1];
      bitmapLine[currentByte] = hexCharsToUint8(tmp[0], tmp[1]);
      //   ttt[t] = bitmapLine[currentByte];
      currentByte++;
      //   t++;
    }
   
        for(int c = 0; c <rounded_width; c++){
          Serial.print(bitmapLine[c]);
          Serial.print(" ");
        }
        Serial.println();
        if(currentByte < file_height * rounded_width) // avoid wrong height print 
       printer.printRasterBitmapLine(rounded_width, file_height, bitmapLine);
  // printer.printRasterBitmap(rounded_width, 1, bitmapLine);
 //  delay(100);  
  }

   //printer.printRasterBitmap(file_width, file_height, ttt);
  Serial.println("done converting to array");
  file.close();

  printer.feed(5);
}



uint8_t hexCharsToUint8(char c1, char c2) {
  uint8_t value = 0;

  if (c1 >= '0' && c1 <= '9')
    value = (c1 - '0') << 4;
  else if (c1 >= 'A' && c1 <= 'F')
    value = (c1 - 'A' + 10) << 4;
  else if (c1 >= 'a' && c1 <= 'f')
    value = (c1 - 'a' + 10) << 4;

  if (c2 >= '0' && c2 <= '9')
    value |= (c2 - '0');
  else if (c2 >= 'A' && c2 <= 'F')
    value |= (c2 - 'A' + 10);
  else if (c2 >= 'a' && c2 <= 'f')
    value |= (c2 - 'a' + 10);

  return value;
}





void DisableWifi(void)
{
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}
