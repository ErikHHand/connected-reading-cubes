#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>


/**************************
 *  RFID INITIALIZATIONS  *
 **************************/
 
#define RST_PIN         4         // White wire
#define SS_PIN          5         // Green cable, as every digital-signal wire
#define LED_PIN         10        // Red cable, it is a digital pin but it is also power for the led, so: red.

#define RGB_PIN     8
#define NUM_LEDS    18
CRGB leds[NUM_LEDS];

#define BRIGHTNESS_OWN_BOOKMARK    10
#define BRIGHTNESS_OTHER_BOOKMARK    100
#define BRIGHTNESS_BLINK_OWN_BOOKMARK    127
#define BRIGHTNESS_BLINK_OTHER_BOOKMARK    200


const int ledPin =  LED_PIN;// the number of the LED pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

uint8_t buf[10] = {};
MFRC522::Uid id;
MFRC522::Uid id2;

String cardBlue = "0x17 0x2f 0x5b 0x52 ";
String cardRed = "0x67 0x29 0x62 0x52 ";

bool ownBookmarkPlaced = false;
bool otherBookmarkPlaced = false;
CRGB ownBookmarkColor;
CRGB otherBookmarkColor;

DynamicJsonDocument response(1024);

uint8_t control = 0x00;

String stringHex(uint8_t *data, uint8_t lenght){
  char tmp[16];
  String temp;
  for (int i = 0; i < lenght; i++){
    sprintf(tmp, "0x%.2x", data[i]);
    temp = temp + tmp + " ";
  }
  return temp;
}

void printHex(uint8_t *data, uint8_t lenght){
  Serial.print(stringHex(data, lenght));
}

void cpid(MFRC522::Uid *id) {
  memset(id, 0, sizeof(MFRC522::Uid));
  memcpy(id->uidByte, mfrc522.uid.uidByte, mfrc522.uid.size);
  id->size = mfrc522.uid.size;
  id->sak = mfrc522.uid.sak;
}

void card_detected(MFRC522::Uid id) {  
  Serial.print("New card: ");
  printHex(id.uidByte, id.size);
  Serial.println("");
  ownBookmarkPlaced = true;
}

void card_removed() {
  Serial.println("Card Removed.");
  ownBookmarkPlaced = false;
  delay(1000);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}


/*************************
 *  WiFi INITIALIZATION  *
 *************************/

#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key Index number (needed only for WEP)

IPAddress Arduino1(192,168,43,198);
IPAddress Arduino2(192,168,43,140);

WiFiServer server(80);
// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiClient wifi;

HttpClient client1 = HttpClient(wifi, Arduino2, 80);
HttpClient client2 = HttpClient(wifi, Arduino1, 80);

String requestID1 = "{\"bookmark\":\"on\",\"color\":\"red\"}";
String requestID2 = "{\"bookmark\":\"on\",\"color\":\"blue\"}";

int status = WL_IDLE_STATUS;

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void look_for_new_clients() {
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {

        // Here we read all the http request, char by char.
        int c = client.read();
        Serial.write(c);
        
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          String msg;
          while(client.available()) {
            c = client.read();
            Serial.write(c);
            msg = String(msg + (char) c);
          }
          deserializeJson(response, msg);
          JsonObject JSONmsg = response.as<JsonObject>();
          
          if (JSONmsg["bookmark"] == "on") {
            otherBookmarkPlaced = true;
            RGBOtherBookmarkPlaced(JSONmsg["color"]);
          } else if(JSONmsg["bookmark"] == "off") {
            otherBookmarkPlaced = false;
            RGBOtherBookmarkRemoved();
          }
  
          send_standard_reply(client);
          led_blink(800, 200, 2);
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);

    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }
}

void send_standard_reply(WiFiClient client){
  Serial.println("Reply");
  // send a standard http response header
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: text/html");
  client.println("Connection: close");  // the connection will be closed after completion of the response
  client.println("Refresh: 5");  // refresh the page automatically every 5 sec
  client.println();
  client.println("<!DOCTYPE HTML>");
  client.println("<html>");
  // output the value of each analog input pin
  for (int analogChannel = 0; analogChannel < 6; analogChannel++) {
    int sensorReading = analogRead(analogChannel);
    client.print("analog input ");
    client.print(analogChannel);
    client.print(" is ");
    client.print(sensorReading);
    client.println("<br />");
  }
  client.println("</html>");
}

void on_wifi_connected() {
  printWifiStatus();
  //led_blink(1000, 500, 3);
}

void connectSample(){
  led_blink(100,100, 10);
  if (wifi.connect(Arduino1, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    wifi.println("GET /search?q=arduino HTTP/1.1");
    wifi.println("Host: www.google.com");
    wifi.println("Connection: close");
    wifi.println();
  }
  led_blink(200,100, 10);
}

void sendBookmarkOn(MFRC522::Uid id){
  String postData;

  if (stringHex(id.uidByte, id.size) == cardBlue) {
    postData = "{\"bookmark\":\"on\",\"color\":\"blue\"}";
  } else if (stringHex(id.uidByte, id.size) == cardRed) {
    postData = "{\"bookmark\":\"on\",\"color\":\"red\"}";
  }

  makePOSTRequest(postData);
}

void sendBookmarkOff(MFRC522::Uid id){
  String postData;

  if (stringHex(id.uidByte, id.size) == cardBlue) {
    postData = "{\"bookmark\":\"off\",\"color\":\"blue\"}";
  } else if (stringHex(id.uidByte, id.size) == cardRed) {
    postData = "{\"bookmark\":\"off\",\"color\":\"red\"}";
  }

  makePOSTRequest(postData);
}

void makePOSTRequest(String postData) {
  Serial.println("making POST request");
  String contentType = "application/json";

  int statusCode;
  String response;
  
  if (WiFi.localIP() == Arduino2) {
    client2.post("/", contentType, postData);
    statusCode = client2.responseStatusCode();
    response = client2.responseBody();
  } else {
    client1.post("/", contentType, postData);
    statusCode = client1.responseStatusCode();
    response = client1.responseBody();
  }

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
}

/*********************
 *  RGB Behavior  *
 *********************/

void RGBOwnBookmarkPlaced(MFRC522::Uid id) {

  CRGB color;
  if (stringHex(id.uidByte, id.size) == cardBlue) {
    color = CRGB::Blue;
  } else if (stringHex(id.uidByte, id.size) == cardRed) {
    color = CRGB::Red;
  }
  ownBookmarkColor = color;
  
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(color, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    delay(500);
    if (otherBookmarkPlaced) {
      FastLED.showColor(otherBookmarkColor, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  if (otherBookmarkPlaced) {
    FastLED.showColor(otherBookmarkColor, BRIGHTNESS_OTHER_BOOKMARK);
  } else {
    FastLED.showColor(color, BRIGHTNESS_OWN_BOOKMARK);
  }
}

void RGBOwnBookmarkRemoved() {
  
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(ownBookmarkColor, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    delay(500);
    if (otherBookmarkPlaced) {
      FastLED.showColor(otherBookmarkColor, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  if (otherBookmarkPlaced) {
    FastLED.showColor(otherBookmarkColor, BRIGHTNESS_OTHER_BOOKMARK);
  } else {
    FastLED.showColor(CRGB::Black, 0);
  }
}

void RGBOtherBookmarkPlaced(String recievedColor) {

  CRGB color;
  if (recievedColor == "blue") {
    color = CRGB::Blue;
  } else if (recievedColor == "red") {
    color = CRGB::Red;
  }
  otherBookmarkColor = color;

  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(color, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    delay(500);
    if (ownBookmarkPlaced) {
      FastLED.showColor(ownBookmarkColor, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }
  
  FastLED.showColor(otherBookmarkColor, BRIGHTNESS_OTHER_BOOKMARK);  
}

void RGBOtherBookmarkRemoved() {
  
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(otherBookmarkColor, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    delay(500);
    if (ownBookmarkPlaced) {
      FastLED.showColor(ownBookmarkColor, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  if (ownBookmarkPlaced) {
    FastLED.showColor(ownBookmarkColor, BRIGHTNESS_OWN_BOOKMARK);
  } else {
    FastLED.showColor(CRGB::Black, 0);
  }
}

/*********************
 *  OTHER UTILITIES  *
 *********************/
 
void led_blink(int delay_time_on, int delay_time_off, int times){
  for (int i = 0; i < times; i++){
      digitalWrite(ledPin, HIGH);
      delay(delay_time_on);
      digitalWrite(ledPin, LOW);
      delay(delay_time_off);
  }
}

void turn_on_led() {
  digitalWrite(ledPin, HIGH);
}

void turn_off_led() {
  digitalWrite(ledPin, LOW);
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

void setup() {
  // led_blink(200, 300, 3);
  
  /****************
   *  RFID SETUP  *
   ****************/
  Serial.begin(9600);   // Initialize serial communications with the PC
  //while (!Serial);    // Do nothing if no serial port is opened (added for Arduinos based on ATMEGA32U4)
  SPI.begin();      // Init SPI bus
  mfrc522.PCD_Init();   // Init MFRC522
  delay(4);       // Optional delay. Some board do need more time after init to be ready, see Readme
  mfrc522.PCD_DumpVersionToSerial();  // Show details of PCD - MFRC522 Card Reader details
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

  // led_blink(2000, 300, 1);

  /****************
   *  WiFi SETUP  *
   ****************/
   
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  //while (!Serial) {
  //  ; // wait for serial port to connect. Needed for native USB port only
  //}

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();
  // you're connected now, so print out the status:
  on_wifi_connected();

  /****************
   *  RGB LEDs SETUP  *
   ****************/
  FastLED.addLeds<WS2812, RGB_PIN, GRB>(leds, NUM_LEDS);
  FastLED.showColor(CRGB::Black, 0);
}


void loop() {
  look_for_new_clients();
  MFRC522::MIFARE_Key key;
  for (byte i = 0; i < 6; i++){
    key.keyByte[i] = 0xFF;
  }
  MFRC522::StatusCode status;
  
 // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  // OR
  // Select one of the cards
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
   return;
  }

  bool result = true;
  uint8_t buf_len = 4;
  cpid(&id);

  card_detected(id);
  RGBOwnBookmarkPlaced(id);
  sendBookmarkOn(id);
  Serial.print("Bookmark place complete \n");

  while (true){
    turn_on_led();
    look_for_new_clients();
    
    control = 0;
    for (int i=0; i < 3; i++){
      if (!mfrc522.PICC_IsNewCardPresent()) {
        if(mfrc522.PICC_ReadCardSerial()) {
          //Serial.print("a");
          control |= 0x16;
        }
        if(mfrc522.PICC_ReadCardSerial()) {
          //Serial.print("b");
          control |= 0x16;
        }
        //Serial.print("c");
        control += 0x1;
      }
      //Serial.print("d");
      control += 0x4;
    }
    if (control == 13 || control == 14){
      //card is still there
    }
    else {
      Serial.print("Break loop \n");
      break;
    }
  }
  turn_off_led();
  RGBOwnBookmarkRemoved();
  sendBookmarkOff(id);
  card_removed();
}
