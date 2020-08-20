/*
 MQTT <--> RF <--> BLYNK

 Wemos D1 mini is connected to a 433MHz transmitter and a 433MHz receiver
 which are used to communicate with Zap Transmitter and Receiver pair.
 
 When the Zap controller is activated, this device as well as the Zap receiver
 are notified. This device will convert the received RF message to a MQTT msg
 and publish it and to BLYNK messages.

 When this device receives an MQTT msg it converts it to a 433MHz Zap message
 and transmits it to the Zap receiver.

 When this device receives BLYNK virtual button messages for V1 and V2, it
 converts them to Zap messages and transmits them to the 433MHz receiver.
 
 

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

   ZAP    ZAP
   ON     OFF
   -----  -----
1  21811  21820
2  21955  21964
3  22275  22284
4  23811  32820
5  29955  29964

*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RCSwitch.h>
#include <SimpleDHT.h>
#include <BlynkSimpleEsp8266.h>

#define RF_TX_PIN     D4
#define RF_RX_PIN     D7
#define RF_TX_EN_PIN  D8
#define DHT11_PIN     D1

//#define RF_PUB_TOPIC   "rfgw/tomqtt"
//#define RF_SUB_TOPIC   "rfgw/torf"
#define PUB_DHT11_TOPIC   "rfgw/dht11"

#define NEXT_DELAY      500
#define NEXT_RF_DELAY   500  
#define NEXT_DHT_DELAY  (1000 * 60)

#define EN_BLYNK    1

RCSwitch rf = RCSwitch();

SimpleDHT11 dht11(DHT11_PIN);

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "GX7Nr2dTzCsVpUrq8MkOeR6-J7zaj9GW";

const char *ssid = "";
const char *pass = "";
const char *mqtt_server = "192.168.1.65";

WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
int value = 0;
unsigned long next;
unsigned long next_rf;  
unsigned long next_dht;



void setup_wifi(void)
{

  delay(10);

  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

#if EN_BLYNK
  Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,65), 8080);
#endif

  randomSeed(micros());
}


#if 1
void callback(char *topic, byte *payload, unsigned int length)
{
  char tpayload[8];
  char ttopic[16];
  unsigned long data;

  strncpy(tpayload, (const char *)payload, length);
  tpayload[length] = '\0';
  strncpy(ttopic, topic, 12);
  ttopic[12] = '\0';

  Serial.print(ttopic); Serial.print("/set");
  Serial.print("  ");
  Serial.println(tpayload);

  if (strcmp(topic, "rfgw/switch1/set") == 0) {
    if (strcmp(tpayload, "ON") == 0) {
      data = 21811;
    } else if (strcmp(tpayload, "OFF") == 0) {
      data = 21820;
    } else {
      return;
    }
  } else if (strcmp(topic, "rfgw/switch2/set") == 0) {
    if (strcmp(tpayload, "ON") == 0) {
      data = 21955;
    } else if (strcmp(tpayload, "OFF") == 0) {
      data = 21964;
    } else {
      return;
    }
  }
  if (next < millis()) {
    digitalWrite(RF_TX_EN_PIN, HIGH);
    delay(50);    // delay to allow relay to switch
//    topic[12] = '\0';
    Serial.printf("%s %d\n\r", topic, data);
    txCode(data, 186, 3);
    next = millis() + NEXT_DELAY;
    client.publish(ttopic, (byte *)tpayload, strlen(tpayload), true);
    delay(50);    // delay to allow relay to switch
    digitalWrite(RF_TX_EN_PIN, LOW);
  }
}
#else
void callback(char *topic, byte *payload, unsigned int length)
{
  char buff[16];

  if (strcmp(topic, RF_SUB_TOPIC) == 0) {
    unsigned long data = strtoul((const char *)payload, NULL, 10);
    if (next < millis()) {
      digitalWrite(RF_TX_EN_PIN, HIGH);
      delay(50);    // delay to allow relay to switch
      Serial.printf("%s %d\n\r", topic, data);
      txCode(data, 186, 3);
      next = millis() + NEXT_DELAY;
      delay(50);    // delay to allow relay to switch
      digitalWrite(RF_TX_EN_PIN, LOW);
    }
  }
}
#endif

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("rfgw", "online");
      // ... and resubscribe
///      client.subscribe("/lofi/node/12/ctr");
      client.subscribe("rfgw/switch1/set"); //RF_SUB_TOPIC);
      client.subscribe("rfgw/switch2/set"); //RF_SUB_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup()
{
//  pinMode(BUILTIN_LED, OUTPUT);     // Initialize the BUILTIN_LED pin as an output
//  digitalWrite(BUILTIN_LED, LOW);
  pinMode(RF_TX_EN_PIN, OUTPUT);     // Initialize TX/RX relay control
  digitalWrite(RF_TX_EN_PIN, LOW);
  pinMode(RF_TX_PIN, OUTPUT);     // Initialize the RF_TX_PIN pin as an output
  digitalWrite(RF_TX_PIN, LOW);

  Serial.begin(115200);
  rf.enableReceive(RF_RX_PIN);  // Receiver on interrupt D7
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  next = millis() + NEXT_DELAY;
  next_rf = millis() + NEXT_RF_DELAY;
  next_dht = millis() + NEXT_DHT_DELAY;
  
}

// This function will be called every time Overhead Button Widget
// in Blynk app writes values to the Virtual Pin V1
BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  // process received value
  if (pinValue == 1) {
    txCode(21811, 186, 3);
    client.publish("rfgw/switch1", (byte*)"ON", 2, true);
  } else if (pinValue == 0) {
    txCode(21820, 186, 3);
    client.publish("rfgw/switch1", (byte*)"OFF", 3, true);
  }
}

// This function will be called every time Lamp Button Widget
// in Blynk app writes values to the Virtual Pin V2
BLYNK_WRITE(V2)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  // process received value
  if (pinValue == 1) {
    txCode(21955, 186, 3);
    client.publish("rfgw/switch2", (byte*)"ON", 2, true);
  } else if (pinValue == 0) {
    txCode(21964, 186, 3);
    client.publish("rfgw/switch2", (byte*)"OFF", 3, true);
  }
}

void loop()
{
  char buff[32];

#if EN_BLYNK
  Blynk.run();
#endif

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (rf.available()) {
    Serial.printf("%d %d %d %d %d\n\r", rf.getReceivedValue(), rf.getReceivedBitlength(), rf.getReceivedDelay(), rf.getReceivedRawdata(),rf.getReceivedProtocol());
    // don't respond to every rf transmission (it is repeated several times and as long as button is pressed)
    if (next_rf < millis()) {
      //client.publish(RF_PUB_TOPIC, itoa(rf.getReceivedValue(), buff, 10));
      next_rf = millis() + NEXT_RF_DELAY;
      if (rf.getReceivedValue() == 21811) {
        Blynk.virtualWrite(V1, 1);
        client.publish("rfgw/switch1", (byte*)"ON", 2, true);
      } else if (rf.getReceivedValue() == 21820) {
        Blynk.virtualWrite(V1, 0);
        client.publish("rfgw/switch1", (byte*)"OFF", 3, true);
      } else if (rf.getReceivedValue() == 21955) {
        Blynk.virtualWrite(V2, 1);
        client.publish("rfgw/switch2", (byte*)"ON", 2, true);
      } else if (rf.getReceivedValue() == 21964) {
        Blynk.virtualWrite(V2, 0);
        client.publish("rfgw/switch2", (byte*)"OFF", 3, true);
      }
   }
    rf.resetAvailable();
  }

  if (next_dht < millis()) {
    next_dht = millis() + NEXT_DHT_DELAY;
    // read without samples.
    byte temperature = 0;
    byte humidity = 0;
    if (dht11.read(&temperature, &humidity, NULL) == SimpleDHTErrSuccess) {
      sprintf(buff, "{\"hum\":\"%d\",\"temp\":\"%d\"}", (int)humidity, (int)temperature);
      client.publish(PUB_DHT11_TOPIC, (byte*)buff, strlen(buff), true);
      Serial.println(buff);
    }
  
  }
}

void txCode(int val, int dly, int rpt)
{
  int digit;
  int dlyh, dlyl;

  dlyh = dly*3;
  dlyl = dly;
  
  if (rpt < 1)
    rpt = 1;
  if (rpt > 20)
    rpt = 5;
    
  do {
    digit = 1<<23;
    do {
      if (digit&val) {
        digitalWrite(RF_TX_PIN, HIGH);    // set the LED off
        delayMicroseconds(dlyh);
        digitalWrite(RF_TX_PIN, LOW);    // set the LED off
        delayMicroseconds(dlyl);
      } else {
        digitalWrite(RF_TX_PIN, HIGH);    // set the LED off
        delayMicroseconds(dlyl);
        digitalWrite(RF_TX_PIN, LOW);    // set the LED off
        delayMicroseconds(dlyh);
      }
      digit >>= 1;
    } while (digit);
    
    digitalWrite(RF_TX_PIN, HIGH);    // set the LED off
    delayMicroseconds(dlyl);
    digitalWrite(RF_TX_PIN, LOW);    // set the LED off
    delayMicroseconds(dlyh);
    delay(5);
  } while (--rpt);
}
