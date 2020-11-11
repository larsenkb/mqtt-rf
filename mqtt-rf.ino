/*
 Zap(RF) <--> MQTT <--> BLYNK

 Wemos D1 mini is connected to a 433MHz transmitter and a 433MHz receiver
 which are used to communicate with Zap Etecity wireless system.
     https://www.amazon.com/Etekcity-5-Channel-Wireless-Control-Receivers/dp/B00FK6SK24
 
 When a button on the Zap controller is depressed, this bridge as well as the Zap socket
 receive the RF code. This bridge will:
     1) convert the received RF code to a MQTT msg and publish it.
     2) convert the received RF code to a BLYNK message and push it to the local Blynk server.
     3) re-transmit the RF code one time via the 433MHz transmitter.

 When this bridge receives an MQTT message:
     1) it converts it to a 433MHz Zap code and transmits it to the Zap socket.
     2) it converts it to a Blynk message and pushes it to the local Blynk server.
     3) publishes an MQTT status message.
     
 When this bridge receives BLYNK virtual button messages for V1 through V5:
     1) it converts it to a 433MHz Zap code and transmits it to the Zap socket.
     2) publishes an MQTT status message.
 
 
 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.

 This bridge will function properly without a local Blynk Server acting as a bridge
 between the MQTT broker and the Zap controller. When and if a local Blynk Server becomes
 active, this bridge will add it to its bridge functions.
 
 To install the ESP8266 board, (using Arduino 1.6.4+):
  - Add the following 3rd party board manager under "File -> Preferences -> Additional Boards Manager URLs":
       http://arduino.esp8266.com/stable/package_esp8266com_index.json
  - Open the "Tools -> Board -> Board Manager" and click install for the ESP8266"
  - Select your ESP8266 in "Tools -> Board"

These are the ON/OFF codes of my Zap controller:
   ZAP    ZAP
   ON     OFF
   -----  -----
1  21811  21820
2  21955  21964
3  22275  22284
4  23811  32820
5  29955  29964

*/

#define BLYNK_TIMEOUT_MS  750  // must be BEFORE BlynkSimpleEsp8266.h doesn't work !!!
#define BLYNK_HEARTBEAT   17   // must be BEFORE BlynkSimpleEsp8266.h works OK as 17s

#include <BlynkSimpleEsp8266.h>
#include <PubSubClient.h>
#include <RCSwitch.h>


#define RF_TX_PIN     D4
#define RF_RX_PIN     D7

#define EN_BLYNK      1


typedef struct {
  int   buttonCode;
  char  mqttMsg[16];
  char  mqttPld[8];
  int   blynkVirtPin;
  int   blynkVirtPinValue;
} ZapButton_t;

// Initalize zap array, place OFF button before ON button
ZapButton_t zap[5*2] = {
  {21820, "rfgw/switch1", "OFF", V1, 0},
  {21811, "rfgw/switch1", "ON",  V1, 1},
  {21964, "rfgw/switch2", "OFF", V2, 0},
  {21955, "rfgw/switch2", "ON",  V2, 1},
  {22284, "rfgw/switch3", "OFF", V3, 0},
  {22275, "rfgw/switch3", "ON",  V3, 1},
  {32820, "rfgw/switch4", "OFF", V4, 0},
  {23811, "rfgw/switch4", "ON",  V4, 1},
  {29964, "rfgw/switch5", "OFF", V5, 0},
  {29955, "rfgw/switch5", "ON",  V5, 1}
};

BlynkTimer timer;

RCSwitch rf = RCSwitch();


// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = "GX7Nr2dTzCsVpUrq8MkOeR6-J7zaj9GW";

const char *ssid = "chewbacca";
const char *pass = "Car voice, une oeuvre...";
const char *mqtt_server = "192.168.1.65";

WiFiClient espClient;
PubSubClient client(espClient);


// check every 11s if connected to Blynk server
void CheckConnection()
{
  if (!Blynk.connected()) {
    Blynk.connect();  // try to connect to server with default timeout
  }
}


void mqttCB(char *topic, byte *payload, unsigned int length)
{
  char tpayload[8];
  char ttopic[16];
  unsigned long data;
  int j;

  // message should be in form of "rfgw/switchn/set"
  strncpy(tpayload, (const char *)payload, length);
  tpayload[length] = '\0';
  // remove "/set" from message
  strncpy(ttopic, topic, 12);
  ttopic[12] = '\0';

  // search for message in Zap database and retrieve RF code, if found.
  for (j = 0; j < 10; j += 2) {
    if (strcmp(ttopic, zap[j].mqttMsg) == 0) {
      if (strcmp(tpayload, zap[j].mqttPld) == 0) {
        data = zap[j].buttonCode;
        break;
      } else if (strcmp(tpayload, zap[j+1].mqttPld) == 0) {
        data = zap[j+1].buttonCode;
        j += 1;
        break;
      } else {
        return;
      }
    }
  }
  // if we fell out of the loop and didn't find a match just return and do nothing
  if (j == 10) return;

  // Transmit code to the Zap socket
  txCode(data, 186, 3);

  // Push message to Blynk Server
  if (Blynk.connected()) {
    Blynk.virtualWrite(zap[j].blynkVirtPin, zap[j].blynkVirtPinValue);
  }

  // Publish MQTT status
  client.publish(ttopic, (byte *)tpayload, strlen(tpayload), true);
  
  delay(50);    // delay to allow relay to switch
}


void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected()) {
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      // Once connected, publish an announcement...
      client.publish("rfgw", "online");
      // ... and resubscribe
      client.subscribe("rfgw/switch1/set"); //RF_SUB_TOPIC);
      client.subscribe("rfgw/switch2/set"); //RF_SUB_TOPIC);
      client.subscribe("rfgw/switch3/set"); //RF_SUB_TOPIC);
      client.subscribe("rfgw/switch4/set"); //RF_SUB_TOPIC);
      client.subscribe("rfgw/switch5/set"); //RF_SUB_TOPIC);
      // would "rfgw/*/set" work???
    } else {
      // Wait 1 second before retrying
      delay(1000);
    }
  }
}

void setup()
{
  pinMode(RF_TX_PIN, OUTPUT);     // Initialize the RF_TX_PIN pin as an output
  digitalWrite(RF_TX_PIN, LOW);

  rf.enableReceive(RF_RX_PIN);  // Receiver on pin D7
  
  delay(10);

#if EN_BLYNK
  Blynk.connectWiFi(ssid, pass);
  
  // line below needs to be BEFORE Blynk.connect()
  timer.setInterval(11000L, CheckConnection); // check if still connected every 11s  
  
  Blynk.config(auth, IPAddress(192,168,1,65), 8080);
  Blynk.connect();
#endif

  randomSeed(micros());

  // Connect to MQTT broker
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqttCB);
  
}


// This function will be called every time Overhead Button Widget
// in Blynk app writes values to the Virtual Pin V1
BLYNK_WRITE(V1)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable

  if (pinValue != 0 && pinValue != 1)
    return;
    
  // process received value
  txCode(zap[pinValue].buttonCode, 186, 3);
  client.publish(zap[pinValue].mqttMsg, (byte*)zap[pinValue].mqttPld, strlen(zap[pinValue].mqttPld), true);
}

// This function will be called every time Lamp Button Widget
// in Blynk app writes values to the Virtual Pin V2
BLYNK_WRITE(V2)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V2 to a variable

  if (pinValue != 0 && pinValue != 1)
    return;
    
  // process received value
  txCode(zap[2 + pinValue].buttonCode, 186, 3);
  client.publish(zap[2 + pinValue].mqttMsg, (byte*)zap[2 + pinValue].mqttPld, strlen(zap[2 + pinValue].mqttPld), true);
}

// This function will be called every time Virtual Button 3 
// in Blynk app writes values to the Virtual Pin V3
BLYNK_WRITE(V3)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V3 to a variable

  if (pinValue != 0 && pinValue != 1)
    return;
    
  // process received value
  txCode(zap[4 + pinValue].buttonCode, 186, 3);
  client.publish(zap[4 + pinValue].mqttMsg, (byte*)zap[4 + pinValue].mqttPld, strlen(zap[4 + pinValue].mqttPld), true);
}

// This function will be called every time Virtual Button 4 
// in Blynk app writes values to the Virtual Pin V4
BLYNK_WRITE(V4)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable

  if (pinValue != 0 && pinValue != 1)
    return;
    
  // process received value
  txCode(zap[6 + pinValue].buttonCode, 186, 3);
  client.publish(zap[6 + pinValue].mqttMsg, (byte*)zap[6 + pinValue].mqttPld, strlen(zap[6 + pinValue].mqttPld), true);
}

// This function will be called every time Virtual Button 5 
// in Blynk app writes values to the Virtual Pin V5
BLYNK_WRITE(V5)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V5 to a variable

  if (pinValue != 0 && pinValue != 1)
    return;
    
  // process received value
  txCode(zap[8 + pinValue].buttonCode, 186, 3);
  client.publish(zap[8 + pinValue].mqttMsg, (byte*)zap[8 + pinValue].mqttPld, strlen(zap[8 + pinValue].mqttPld), true);
}


void loop()
{
  char buff[32];
  int lastCode = 0;

  
  if (Blynk.connected()) {
    Blynk.run();
  }

  timer.run();
  
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (rf.available()) {
    int rxValue = rf.getReceivedValue();
    // don't respond to every rf transmission (it is repeated several times and as long as button is pressed)
    for (int j = 0; j < 10; j++) {
      if (rxValue == zap[j].buttonCode) {
        if (lastCode != zap[j].buttonCode) {
          lastCode = zap[j].buttonCode;
          if (Blynk.connected()) {
            Blynk.virtualWrite(zap[j].blynkVirtPin, zap[j].blynkVirtPinValue);
          }
          client.publish(zap[j].mqttMsg, (byte*)zap[j].mqttPld, strlen(zap[j].mqttPld), true);  // ??? what is 3rd parameter???
          txCode(lastCode, 186, 3);
        }
      } 
    }
    rf.resetAvailable();
  } // endof: if (rx.available())...

}

// Transmit Zap RF code by bit-bang method.
// Assume code is 24-bit value
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
        digitalWrite(RF_TX_PIN, HIGH);
        delayMicroseconds(dlyh);
        digitalWrite(RF_TX_PIN, LOW);
        delayMicroseconds(dlyl);
      } else {
        digitalWrite(RF_TX_PIN, HIGH);
        delayMicroseconds(dlyl);
        digitalWrite(RF_TX_PIN, LOW);
        delayMicroseconds(dlyh);
      }
      digit >>= 1;
    } while (digit);
    
    digitalWrite(RF_TX_PIN, HIGH);
    delayMicroseconds(dlyl);
    digitalWrite(RF_TX_PIN, LOW);
    delayMicroseconds(dlyh);
    delay(5);
  } while (--rpt);
}
