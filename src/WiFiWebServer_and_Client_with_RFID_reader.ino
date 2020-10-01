#include <SPI.h>
#include <MFRC522.h>
#include <WiFiNINA.h>


/**************************
 *  RFID INITIALIZATIONS  *
 **************************/
 
#define RST_PIN         4         // White wire
#define SS_PIN          5         // Green cable, as every digital-signal wire
#define LED_PIN         10        // Red cable, it is a digital pin but it is also power for the led, so: red.


const int ledPin =  LED_PIN;// the number of the LED pin

MFRC522 mfrc522(SS_PIN, RST_PIN);  // Create MFRC522 instance

uint8_t buf[10] = {};
MFRC522::Uid id;
MFRC522::Uid id2;
bool is_card_present = false;
uint8_t control = 0x00;

void printHex(uint8_t *data, uint8_t lenght){
  char tmp[16];
  for (int i = 0; i < lenght; i++){
    sprintf(tmp, "0x%.2x", data[i]);
    Serial.print(tmp);
    Serial.print(" ");
  }
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
  
  digitalWrite(ledPin, HIGH);
}

void card_removed() {
  Serial.println("Card Removed.");
  digitalWrite(ledPin, LOW);
  delay(1000);

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  connectSample();
}


/*************************
 *  WiFi INITIALIZATION  *
 *************************/

#include "arduino_secrets.h" 
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;    // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;                 // your network key Index number (needed only for WEP)

IPAddress otherArduino(192,168,43,198);

int status = WL_IDLE_STATUS;

WiFiServer server(80);
// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiClient thisAsClient;


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
        // TODO: Figure out a method for reading properly the request so that we can parse the parameters
        int c = client.read();
        Serial.write(c);
        
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          send_standard_reply(client);
          led_blink(500, 300, 3);
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
  led_blink(1000, 500, 3);
}

void connectSample(){
  led_blink(100,100, 10);
  if (thisAsClient.connect(otherArduino, 80)) {
    Serial.println("connected to server");
    // Make a HTTP request:
    thisAsClient.println("GET /search?q=arduino HTTP/1.1");
    thisAsClient.println("Host: www.google.com");
    thisAsClient.println("Connection: close");
    thisAsClient.println();
  }
  led_blink(200,100, 10);
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
  led_blink(200, 300, 3);
  
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

  led_blink(2000, 300, 1);

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
  connectSample();

  while (true){
    turn_on_led();
    
    look_for_new_clients();
    
    control = 0;
    for (int i=0; i < 3; i++){
      if (!mfrc522.PICC_IsNewCardPresent()) {
        if(mfrc522.PICC_ReadCardSerial()) {
          // Serial.print("a");
          control |= 0x16;
        }
        if(mfrc522.PICC_ReadCardSerial()) {
          // Serial.print("b");
          control |= 0x16;
        }
        // Serial.print("c");
        control += 0x1;
      }
      // Serial.print("d");
      control += 0x4;
    }
    if (control == 13 || control == 14){
      //card is still there
    }
    else {
      break;
    }
  }
  turn_off_led();
  card_removed();
}
