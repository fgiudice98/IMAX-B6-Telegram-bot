/*********************************************************************************
   IMAX B6 telegram bot
   by fgiudice98
   Sends the content of the IMAX B6 LCD to your telegram

   NOT 100% working, sometimes can miss a message or sniff the wrong character
   Works with 80MHz CPU and 40MHz Flash

   I'm not responsible of any damage to you batteries, your ESP32, your charger,
   yourself or anything.
   Be kind with your batteries and you charger!

   Wiring
   ESP32 PIN    32  33  26  27  14  13  GND
   IMAX B6 LCD  E   RS  D4  D5  D6  D7  VSS

   Except GND all pin are connected with a voltage divider like

    ^ ESP32 PIN
    |
   [#] 15k
   [#]
    +--[##]--GND
    |  22k
    |
    ^ IMAX B6 LCD

   I strongly recommend to power the ESP32 from an USB source
 *********************************************************************************/

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "chars.h"

const char* ssid = "XXXXXXXX"; //WiFi SSID
const char* password = "XXXXXXXxx"; //WiFi password

const String botapi = "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"; // Telegram bot API key
const String chatId = "XXXXXXXXXXX"; // Sends messages to this chat ID

WiFiClientSecure clientTCP;

#define SAMPLES 200 // Num of LCD commands saved
#define GOODTRAIN 2 // Sniffed string good if 2 in a row are the same
#define DELAY 120000 // Messages delay in ms

//#define TEXTMESSAGE // Uncomment to send text instead of image
//#define ARROW // Uncomment to convert ~ to →. Useful in text mode. /!\ Don't use on image mode

byte readings[SAMPLES];
byte incremental = 0;
byte train = 0;
bool analyzing = false; // If is sniffing/analyzing LCD data
bool isEnd = false; // If flashing END is displayed

String prevText = "     Charger\n    IMAX B6";

#ifndef TEXTMESSAGE
const uint8_t header[] = {0x42, 0x4d, 0xbe, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x28, 0x00, // BMP Header. /!\ Don't change unless you know what you are doing
                          0x00, 0x00, 0xa0, 0x01, 0x00, 0x00, 0xa0, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x80, 0x20, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x12, 0x0b, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                         };
const uint8_t firstColor[] = {0xff, 0x53, 0x1a, 0x00}; // Background color (BGRA32)
const uint8_t secondColor[] = {0xff, 0xf7, 0xf7, 0x00}; // Text color (BGRA32)
#endif

void IRAM_ATTR readLcd() {
  uint32_t bottom = GPIO.in; //gpio 0-31
  uint32_t top = GPIO.in1.val; //gpio 39-32

  byte temp = 0;
  temp = (bottom >> 13) & 0x3; //pin 13 e 14 in bit 0 e 1
  temp += (bottom >> 25) & 0x4; //pin 27 in bit 2
  temp += (bottom >> 23) & 0x8; //pin 26 in bit 3
  temp += (top & 0x2) << 6; //pin 33 in bit 7
  readings[incremental] = temp;
  incremental++;
  if (incremental == SAMPLES) { // If read all samples stop interrupt and analyze the data
    detachInterrupt(32);
    incremental = 0;
    getText();
  }
}

void setup() {
  pinMode(32, INPUT);
  pinMode(33, INPUT);
  pinMode(26, INPUT);
  pinMode(27, INPUT);
  pinMode(14, INPUT);
  pinMode(13, INPUT);

  WiFi.begin(ssid, password);
  for (uint8_t t = 30; (t > 0 && (WiFi.status() != WL_CONNECTED)); t--) { // Try to connect to WiFi every second for 30s
    delay(1000);
  }

  if (WiFi.status() != WL_CONNECTED) { // If still not connected reboot the ESP32
    ESP.restart();
  }
}

void loop() {
  sending(); // Bot "is typing" or "is sending a photo"
  requestLcd(); // Begin LCD sniffing
  while (analyzing) { // If it's still analyzing wait
    delay(500);
  }
#ifdef TEXTMESSAGE
  String message = URLEncode(prevText);
  HTTPClient tgapi;
  tgapi.begin("https://api.telegram.org/bot" + botapi + "/sendMessage?chat_id=" + chatId + "&text=`" + message + "`&parse_mode=MarkdownV2"); // Sends LCD data as mono text in telegram
  tgapi.GET();
  tgapi.end();
#else
  generateAndSend(prevText); // Sends LCD data as image
#endif
  delay(DELAY);
}

void requestLcd() {
  analyzing = true; // Mark as analyzing
  attachInterrupt(32, readLcd, RISING); // Start the interrupt
}

void sending() {
  HTTPClient tgapi;
#ifdef TEXTMESSAGE
  tgapi.begin("https://api.telegram.org/bot" + botapi + "/sendChatAction?chat_id=" + chatId + "&action=typing");
#else
  tgapi.begin("https://api.telegram.org/bot" + botapi + "/sendChatAction?chat_id=" + chatId + "&action=upload_photo");
#endif
  tgapi.GET();
  tgapi.end();
}

void getText() {
  bool start = true;
  String text = "";
  for (byte i = 0; i < SAMPLES - 1;) {
    if (start) { // Waits for a clean LCD command to sync with the data
      if (readings[i] == 0b00001000) {
        if (readings[i + 1] == 0b00000000) {
          start = false;
          text = "";
          i++;
        }
      }
      i++;
    } else {
      if (readings[i] & 0b10000000) { // If it's a character
        char c = (readings[i] & 0b00001111) << 4; // Read the LSB
        i++;
        c += readings[i] & 0b00001111; // Read the MSB
        i++;
        if (((c < '(' || c > '~') && (c != ' ')) || c == '\\' || c == '/' || c == '<' || c == '@') { // If an unexpected char occurs return to the beginning
          start = true;
        }
#ifdef ARROW
        if (c == '~') {
          text += "→";
        } else {
          text += c;
        }
#else
        text += c; // Adds char to string
#endif
      } else {
        if (readings[i + 1] < 100) { // If it's a command
          byte r = (readings[i] & 0b00001111) << 4; // Read the LSB
          i++;
          r = r | (readings[i] & 0b00001111); // Read the MSB
          i++;
          if (r == 0b10000000) { // End/clear LCD command
            if (text != "") {
              if (text != prevText) {
                if (text.indexOf(':') > 0) { // LCD has charging time
                  if (text.indexOf("END") == 0 && !isEnd) {
                    isEnd = true;
                  } else if (text.indexOf('V') > 0) {
                    train = GOODTRAIN; // If it is here 98% the string is good, no need to check if another is the same (and seconds have passed, so it's impossible to have the same screen)
                    if (isEnd) { // Add "END" at the beginning
                      text.setCharAt(0, 'E');
                      text.setCharAt(1, 'N');
                      text.setCharAt(2, 'D');
                      isEnd = false;
                    }
                  } else {
                    train = 0; // String is not good
                  }
                } else {
                  prevText = text; // Text isn't the same, reset
                  train = 0;
                }
              } else {
                train++; // Text is the same, so it seems good
              }
              if (train == GOODTRAIN) { // If text is good mark as not analyzing and exit the function
                analyzing = false;
                prevText = text;
                return;
              }
            }
            text = "";
          } else if (r == 0b11000000) { // New line command
            text += '\n';
          } else { // Every other command (mostly trailing and text mode change, a ' ' is a good fit)
            text += ' ';
          }
        } else { // If not in sync just advance a sample
          i++;
        }
      }
    }
  }
  attachInterrupt(32, readLcd, RISING); // If no text is good try reading other samples
}

#ifndef TEXTMESSAGE
void generateAndSend(String text) {
  uint8_t image[192]; // 16x96, 1 bpp image
  memset(image, 0x00, sizeof(image));
  uint8_t imageI = 0;
  for (uint8_t stringI = 0; stringI < text.length(); stringI++) { // Convert string to LCD image
    char c = text[stringI];
    if (c == '\n') { // If it's a newline jump to the second LCD image line
      imageI = 96;
    } else {
      if (c != ' ') { // Don't copy bytes if it's a space
        for (uint8_t i = 0; i < 5; i++) { // Copy char bytes
          image[imageI + i] = chars[c - 32][i];
        }
      }
      imageI += 6;
    }
  }

  uint8_t rotated_image[16][12]; // 96x16, 1bb image
  for (int8_t i = 7; i >= 0; i--) { //rotate previous image by 90°
    for (uint8_t j = 0; j < 12; j++) {
      uint8_t temp = 0, temp2 = 0;
      for (uint8_t k = 0; k < 8; k++) {
        temp |= ((image[(j * 8) + k] >> i) & 0x01) << (7 - k);
        temp2 |= ((image[(j * 8) + 96 + k] >> i) & 0x01) << (7 - k);
      }
      rotated_image[7 - i][j] = temp;
      rotated_image[15 - i][j] = temp2;
    }
  }

  const char* myDomain = "api.telegram.org";
  if (clientTCP.connect(myDomain, 443)) { // Begin telegram image sending (image is 4x the previous one, otherwise telegram compress them and isn't a good pixellated image anymore)
    String head = "--IMAXB6\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + chatId + "\r\n--IMAXB6\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"LCD.bmp\"\r\nContent-Type: image/bmp\r\n\r\n";
    String tail = "\r\n--IMAXB6--\r\n";

    uint16_t imageLen = 0x20be; // BMP file size. /!\ Don't change unless you know what you are doing
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;

    clientTCP.println("POST /bot" + botapi + "/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=IMAXB6");
    clientTCP.println();
    clientTCP.print(head);
    clientTCP.flush();

    clientTCP.write(header, sizeof(header)); // Sends BMP header
    clientTCP.flush();
    clientTCP.write(firstColor, sizeof(firstColor)); // Sends background color
    clientTCP.write(secondColor, sizeof(secondColor)); // Sends text color
    clientTCP.flush();

    uint8_t row[52];
    memset(row, 0x00, sizeof(row));
    for (uint8_t bottomMargin = 0; bottomMargin < (12 * 4); bottomMargin++) { // 48px bottom margin (12 big pixels)
      clientTCP.write(row, sizeof(row));
      clientTCP.flush();
    }

    for (int8_t y = 15; y >= 0; y--) { // Text image
      row[0] = 0x00; row[1] = 0x00; row[50] = 0x00; row[51] = 0x00; // left and right margin
      for (uint8_t x = 0; x < 12; x++) {
        uint8_t pixels = rotated_image[y][x];
        row[(x * 4) + 2] = (((pixels >> 3) & 0b00010000) | ((pixels >> 6) & 0b00000001)) * 0xf;
        row[(x * 4) + 3] = (((pixels >> 1) & 0b00010000) | ((pixels >> 4) & 0b00000001)) * 0xf;
        row[(x * 4) + 4] = (((pixels << 1) & 0b00010000) | ((pixels >> 2) & 0b00000001)) * 0xf;
        row[(x * 4) + 5] = (((pixels << 3) & 0b00010000) | (pixels & 0b00000001)) * 0xf;
      }
      clientTCP.write(row, sizeof(row)); // 4x
      clientTCP.flush();
      clientTCP.write(row, sizeof(row));
      clientTCP.flush();
      clientTCP.write(row, sizeof(row));
      clientTCP.flush();
      clientTCP.write(row, sizeof(row));
      clientTCP.flush();
    }

    memset(row, 0x00, sizeof(row));
    for (uint8_t topMargin = 0; topMargin < (12 * 4); topMargin++) { // 48px top margin (12 big pixels)
      clientTCP.write(row, sizeof(row));
      clientTCP.flush();
    }

    clientTCP.print(tail);
    clientTCP.stop();
  }
}
#endif

String URLEncode(String text) {
  String urlEncoded = "";
  for (byte i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c < 0x10) {
      urlEncoded += "%0";
      urlEncoded.concat(String(c, HEX));
    } else if (c < '0' || c > '~' || (c > '9' && c < 'A') || (c > 'Z' && c < 'a')) {
      urlEncoded += '%';
      urlEncoded.concat(String(c, HEX));
    } else {
      urlEncoded += c;
    }
  }
  return urlEncoded;
}
