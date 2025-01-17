#include "WiFi.h"
#include "PubSubClient.h" //pio lib install "knolleary/PubSubClient"
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>
static void startListeningToNFC();
static void handleCardDetected();

// Pins used for I2C IRQ
#define PN532_IRQ   4
#define PN532_RESET 5 

#define SSID          "NETGEAR68"
#define PWD           "excitedtuba713"

#define MQTT_SERVER   "52.28.129.241"
#define MQTT_PORT     1883

#define LED_PIN       2

const int DELAY_BETWEEN_CARDS = 500;
long timeLastCardRead = 0;
boolean readerDisabled = false;
int irqCurr;
int irqPrev;
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;
const char* topic = "nieuwpoort/tags";

// This example uses the IRQ line, which is available when in I2C mode.
Adafruit_PN532 nfc(PN532_IRQ, PN532_RESET);

void callback(char *topic, byte *message, unsigned int length);

void setup_wifi()
{
  delay(10);
  Serial.println("Connecting to WiFi..");

  WiFi.begin(SSID, PWD);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setup()
{

  Serial.begin(115200);
  setup_wifi();
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);
  client.subscribe("nieuwpoort/tags");
  //client.beginPublish(topic,10,true);

  Serial.println("Begin NFC532 Scanning Software.");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    Serial.print("Didn't find PN532 board");
    while (1); // halt
  }
  // Got ok data, print it out!
  Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
  Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
  Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  
  // configure board to read RFID tags
  nfc.SAMConfig();

  startListeningToNFC();

}

void callback(char *topic, byte *message, unsigned int length)
{
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;

  for (int i = 0; i < length; i++)
  {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  if (String(topic) == "nieuwpoort/tags")
  {
    Serial.print("Changing output to ");
    if (messageTemp == "on")
    {
      Serial.println("on");
      digitalWrite(LED_PIN, HIGH);
    }
    else if (messageTemp == "off")
    {
      Serial.println("off");
      digitalWrite(LED_PIN, LOW);
    }
  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    // creat unique client ID
    // in Mosquitto broker enable anom. access
    if (client.connect("Jonathan"))
    {
      Serial.println("connected");
      // Subscribe
      client.subscribe("nieuwpoort/tags");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop()
{
  long now = millis();
  if (now - lastMsg > 5000)
  {
    lastMsg = now;
  }
  if (readerDisabled) {
    if (millis() - timeLastCardRead > DELAY_BETWEEN_CARDS) {
      readerDisabled = false;
      startListeningToNFC();
    }
  } else {
    irqCurr = digitalRead(PN532_IRQ);

    // When the IRQ is pulled low - the reader has got something for us.
    if (irqCurr == LOW && irqPrev == HIGH) {
       //Serial.println("Got NFC IRQ");  
       handleCardDetected(); 
    }
  
    irqPrev = irqCurr;
  }
}

void startListeningToNFC() {
  // Reset our IRQ indicators
  irqPrev = irqCurr = HIGH;
  
  Serial.println("Present an ISO14443A Card ...");
  nfc.startPassiveTargetIDDetection(PN532_MIFARE_ISO14443A);
}

void handleCardDetected() {
    uint8_t success = false;
    uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
    uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

    // read the NFC tag's info
    success = nfc.readDetectedPassiveTargetID(uid, &uidLength);
    Serial.println(success ? "Read successful" : "Read failed (not a card?)");

    if (success) {
      // Display some basic information about the card
      //Serial.println("Found an ISO14443A card");
      //Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
      //Serial.print("  UID Value: ");
      Serial.print("Card ID HEX Value: ");
      nfc.PrintHex(uid, uidLength);
      
      if (uidLength == 4)
      {
        // We probably have a Mifare Classic card ... 
        uint32_t cardid = uid[0];
        cardid <<= 8;
        cardid |= uid[1];
        cardid <<= 8;
        cardid |= uid[2];  
        cardid <<= 8;
        cardid |= uid[3]; 
        //Serial.print("Seems to be a Mifare Classic card #");
        Serial.print("Card ID NUMERIC Value: ");
        Serial.println(cardid);
        client.publish(topic,(const char*)String(cardid).c_str());
      }
      if (uidLength == 7)
      {
        // We probably have a Mifare Classic card ... 
        uint32_t cardid = uid[0];
        cardid <<= 8;
        cardid |= uid[1];
        cardid <<= 8;
        cardid |= uid[2];  
        cardid <<= 8;
        cardid |= uid[3]; 
        cardid <<= 8;
        cardid |= uid[4]; 
        cardid <<= 8;
        cardid |= uid[5]; 
        cardid <<= 8;
        cardid |= uid[6]; 

        //Serial.print("Seems to be a Mifare Classic card #");
        Serial.print("Card ID NUMERIC Value: ");
        Serial.println(cardid);
        client.publish(topic,(const char*)String(cardid).c_str());
      }
      Serial.println("");
      if (!client.connected()){
          reconnect();
      }
      //client.();
  //client.pub

      timeLastCardRead = millis();
    }
  

    // The reader will be enabled again after DELAY_BETWEEN_CARDS ms will pass.
    readerDisabled = true;
}