#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>
#include <ArduinoHttpClient.h>
#include <ArduinoJson.h>
#include <FastLED.h>


#define LED_PIN         10        // Red cable, it is a digital pin but it is also power for the led, so: red.
const int ledPin =  LED_PIN;// the number of the LED pin

/**************************
 *  RFID INITIALIZATIONS  *
 **************************/
 
#define RST_PIN         4         // White wire
#define SS_PIN          5         // Green cable, as every digital-signal wire

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

uint8_t buf[10] = {};
MFRC522::Uid id;
MFRC522::Uid id2;

String cardBlue = "0x17 0x2f 0x5b 0x52 ";
String cardRed = "0x67 0x29 0x62 0x52 ";


/**************************
 *  RGB INITIALIZATIONS  *
 **************************/

#define RGB_PIN     8
#define NUM_LEDS    18
CRGB leds[NUM_LEDS];

// Brightness settings
#define BRIGHTNESS_OWN_BOOKMARK    10
#define BRIGHTNESS_OTHER_BOOKMARK    100
#define BRIGHTNESS_BLINK_OWN_BOOKMARK    127
#define BRIGHTNESS_BLINK_OTHER_BOOKMARK    200

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

int status = WL_IDLE_STATUS;

/**************************
 *  OTHER INITIALIZATIONS  *
 **************************/

// Variables for keeping track of bookmarks placed
bool ownBookmarkPlaced = false;
bool otherBookmarkPlaced = false;
CRGB ownBookmarkColor;
CRGB otherBookmarkColor;

DynamicJsonDocument response(1024);

uint8_t control = 0x00;

/**************************
 *  BOOKMARK HANDLING  *
 **************************/

// Function for converting hex to string
String stringHex(uint8_t *data, uint8_t lenght){
  char tmp[16];
  String temp;
  for (int i = 0; i < lenght; i++){
    sprintf(tmp, "0x%.2x", data[i]);
    temp = temp + tmp + " ";
  }
  return temp;
}

// Function for printing hex to terminal
void printHex(uint8_t *data, uint8_t lenght){
  Serial.print(stringHex(data, lenght));
}

void cpid(MFRC522::Uid *id) {
  memset(id, 0, sizeof(MFRC522::Uid));
  memcpy(id->uidByte, mfrc522.uid.uidByte, mfrc522.uid.size);
  id->size = mfrc522.uid.size;
  id->sak = mfrc522.uid.sak;
}

// Function called when card is detected
void card_detected(MFRC522::Uid id) {  
  Serial.print("New card: ");
  printHex(id.uidByte, id.size);
  Serial.println("");
  ownBookmarkPlaced = true;
}

// Function called when card is removed
void card_removed() {
  Serial.println("Card Removed.");
  ownBookmarkPlaced = false;
  delay(1000);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

/*************************
 *  WiFi FUNCTIONS  *
 *************************/

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

// Function called regularly to check for incomming messages from the other cube 
void look_for_new_clients() {
  // listen for incoming clients
  WiFiClient client = server.available();
  if (client) {
    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {

        // Read all the http request, char by char, and print.
        // I think this just reads the head of it though?
        int c = client.read();
        Serial.write(c);
        
        // end of line + line is blank means end of http request
        if (c == '\n' && currentLineIsBlank) {
          // This is where we read the content of the http request
          String msg;
          while(client.available()) {
            c = client.read(); // read char by char
            Serial.write(c);
            msg = String(msg + (char) c); // add to string
          }

          // Convert to JSON
          deserializeJson(response, msg);
          JsonObject JSONmsg = response.as<JsonObject>();

          // Check if bookmark was placed or removed
          if (JSONmsg["bookmark"] == "placed") {
            otherBookmarkPlaced = true;
            RGBOtherBookmarkPlaced(JSONmsg["color"]);
          } else if(JSONmsg["bookmark"] == "removed") {
            otherBookmarkPlaced = false;
            RGBOtherBookmarkRemoved();
          }
  
          send_standard_reply(client);
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

// Function for sending reply to http request
// TODO: This is a very long reply, what do we actually need to send in a reply?
// Guess it doesn't matter too much do, it still works, the content of the reply 
// is not that important
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

// Function called before sending http request with bookmark placed data
void sendBookmarkPlaced(MFRC522::Uid id){
  String postData;

  // Determine color of bookmark
  if (stringHex(id.uidByte, id.size) == cardBlue) {
    postData = "{\"bookmark\":\"placed\",\"color\":\"blue\"}";
  } else if (stringHex(id.uidByte, id.size) == cardRed) {
    postData = "{\"bookmark\":\"placed\",\"color\":\"red\"}";
  }

  makePOSTRequest(postData);
}

// Function called before sending http request with bookmark removed data
void sendBookmarkRemoved(MFRC522::Uid id){
  String postData;

  // Determine color of bookmark
  if (stringHex(id.uidByte, id.size) == cardBlue) {
    postData = "{\"bookmark\":\"removed\",\"color\":\"blue\"}";
  } else if (stringHex(id.uidByte, id.size) == cardRed) {
    postData = "{\"bookmark\":\"removed\",\"color\":\"red\"}";
  }

  makePOSTRequest(postData);
}

// Function for making an http request. Data to be sent has been determined in 
// previous functions
void makePOSTRequest(String postData) {
  Serial.println("making POST request");
  String contentType = "application/json";

  int statusCode;
  String response;

  // IP addresses are pretty hard coded, so depending on which arduino it is
  // sent from, it will just send it to the IP address of the other Arduino
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

  // Determine color
  if (stringHex(id.uidByte, id.size) == cardBlue) {
    color = CRGB::Blue;
  } else if (stringHex(id.uidByte, id.size) == cardRed) {
    color = CRGB::Red;
  }
  ownBookmarkColor = color; // Save color for later blinking :)

  // Loop for doing the initial blinking 
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(color, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    delay(500);
    if (otherBookmarkPlaced) { // Base the blinking color on how many bookmarks are placed
      FastLED.showColor(otherBookmarkColor, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  if (otherBookmarkPlaced) { // Prioritise other bookmark color over own bookmark color
    FastLED.showColor(otherBookmarkColor, BRIGHTNESS_OTHER_BOOKMARK);
  } else {
    FastLED.showColor(color, BRIGHTNESS_OWN_BOOKMARK);
  }
}

void RGBOwnBookmarkRemoved() {

  // Loop for doing the initial blinking 
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(ownBookmarkColor, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    delay(500);
    if (otherBookmarkPlaced) { // Base the blinking color on how many bookmarks are placed
      FastLED.showColor(otherBookmarkColor, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  if (otherBookmarkPlaced) { // Prioritise other bookmark color over no color
    FastLED.showColor(otherBookmarkColor, BRIGHTNESS_OTHER_BOOKMARK);
  } else {
    FastLED.showColor(CRGB::Black, 0);
  }
}

void RGBOtherBookmarkPlaced(String recievedColor) {

  CRGB color;

  // Determine color
  if (recievedColor == "blue") {
    color = CRGB::Blue;
  } else if (recievedColor == "red") {
    color = CRGB::Red;
  }
  otherBookmarkColor = color;

  // Loop for doing the initial blinking 
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(color, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    delay(500);
    if (ownBookmarkPlaced) { // Base the blinking color on how many bookmarks are placed
      FastLED.showColor(ownBookmarkColor, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  // Always show other bookmark color if that is placed
  FastLED.showColor(otherBookmarkColor, BRIGHTNESS_OTHER_BOOKMARK);  
}

void RGBOtherBookmarkRemoved() {

  // Loop for doing the initial blinking 
  for (int j = 0; j <= 3; j++) {
    FastLED.showColor(otherBookmarkColor, BRIGHTNESS_BLINK_OTHER_BOOKMARK);
    delay(500);
    if (ownBookmarkPlaced) { // Base the blinking color on how many bookmarks are placed
      FastLED.showColor(ownBookmarkColor, BRIGHTNESS_BLINK_OWN_BOOKMARK);
    } else {
      FastLED.showColor(CRGB::Black, 0);
    }
    delay(500);
  }

  if (ownBookmarkPlaced) { // Prioritise own bookmark color over no color
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
  printWifiStatus();

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
  sendBookmarkPlaced(id);
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
  sendBookmarkRemoved(id);
  card_removed();
}
