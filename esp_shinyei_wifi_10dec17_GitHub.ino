//#######################################################################
//
// Copyright:
//
// For all parts regarding the additions made by RIVM the GPL 4 license conditions,
// quoted below apply!
//
//     This program is free software: you can redistribute it and/or modify
//     it under the terms of the GNU General Public License as published by
//     the Free Software Foundation,  version 4 of the License, or
//     any later version.
//
//     This program is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU General Public License for more details.
//
//     You should have received a copy of the GNU General Public License
//     along with this program. If not, see <http://www.gnu.org/licenses/>.
//
//#######################################################################


//==============================================
// Code for NodeMCU v1.0 module (ESP8266)
// Waag Society, Making Sense
// author: Dave Gonner & Emma Pareschi
// version 11 May 2016
//==============================================
// RIVM, aanpassingen Joost Wesseling
// Version 25 september 2016
// Version 28 september 2016
// - Aangepast voor test-data.
// Version 24 November 2016
// - Aangepast voor voor metingen aan vuurwerk.
//==============================================


#include <FS.h>                   // Make sure ESP library 2.1.0 or higher is installed
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <PubSubClient.h>         //https://github.com/knolleary/pubsubclient

// DEFAULT MQTT SETTINGS, will be overwritten by values from config.json
// We only use some variables in the interface of the Access Point formed
// by the ESP. For communicating we do NOT use MQTT, but employ the
// services of Dweet.io .

char mqtt_server[41] = "VUURWERK 2016/2017\0";
char mqtt_portStr[7] = "12345\0";
char mqtt_username[21] = "not used\0";
char mqtt_password[21] = "not used\0";
char mqtt_topic[21] = "20\0";             // Default for test-data
int mqtt_port = atoi(mqtt_portStr);

// Button and indicators of activities
#define BUTTON_PIN 16              // D0, button to enter wifi manager
#define RED_LED_PIN 0   // D2
#define BLUE_LED_PIN 2      // D6

#define IIMAX 256

#define MAX_ITER 6

// Do we want red/blue led's blinking (1=yes)?
int DoBlinkStatus = 2;
long  TotCount = 0;

String InfluxThing = "vuurwerk";       

// Needed for the ESP
WiFiManager wifiManager;
WiFiClient espClient;
PubSubClient mqttClient(espClient);
String readStr;
long chipid;
bool shouldSaveConfig = false;    //flag for saving data

// Parameters to be transmitted ...
String id = "01234567890123456789";
String val0 = "-999.999";
String val1 = "-999.999";
String val2 = "-999.999";
String val3 = "-999.999";
String val4 = "-999.999";
String val5 = "-999.999";
String val6 = "-999.999";
String val7 = "-999.999";
String val8 = "-999.999";
String val9 = "-999.999";

float x0 = 0.0, x1 = 0.0, x2 = 0.0, x3 = 0.0, x4 = 0.0, x5 = 0.0, x6 = 0.0, x7 = 0.0, x8 = 0.0;

long rssi ;                 // WiFi strength

#define LOG_INTERVAL 925    // mills between entries (reduce to take more/faster data)

//#######################################################################
// Dust Sensor settings
#define DUST_PM10_PIN 13      // Dust sensor PM10 pin
unsigned long starttime = 0;
unsigned long triggerOnP2;
unsigned long triggerOffP2;
unsigned long pulseLengthP2;
unsigned long durationP2;
boolean valP2 = HIGH;
boolean triggerP2 = false;
float ratioP2 = 0;
unsigned long sampletime_ms = 20000;

// For averaging the dust measurements:
int NAvg = 0;
int MaxIter = 2;
float SumDust = 0.0;

long NumMelding = 0;

//----------------------------------------------
//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

//#######################################################################
// Taken from Github
// Used to store configuration data
//
void saveConfigJson() {
  //save the custom parameters to FS
  Serial.println("saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_portStr;
  json["mqtt_username"] = mqtt_username;
  json["mqtt_password"] = mqtt_password;
  json["mqtt_topic"] = mqtt_topic;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }

  json.printTo(Serial);
  Serial.println();
  json.printTo(configFile);
  configFile.close();
  //end save
}

//#######################################################################
// Mostly taken from Github
// Used to setup the ESP and create a WiFi Access Point
//

void setup() {
  int i;

  Serial.begin(9600);
  Serial.println();

  // We gebruiken de input op de niet-aangesloten analoge ingang als seed voor de random functie
  pinMode(A0, INPUT);

  pinMode(BUTTON_PIN, INPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  digitalWrite(BLUE_LED_PIN, HIGH); // off
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(RED_LED_PIN, HIGH); // off

  Serial.println("===============================================");
  Serial.println("  Code for NodeMCU v1.0 module (ESP8266) ");
  Serial.println("  Waag Society, Making Sense ");
  Serial.println("  Author: Dave Gonner & Emma Pareschi ");
  Serial.println("  Version 11 May 2016 ");
  Serial.println("===============================================");
  Serial.println("  Adapted for use by RIVM ");
  Serial.println("  Version 04 December 2016 ");  
  Serial.println("===============================================");
  Serial.println("  Updated for use 2017/2018 by RIVM ");
  Serial.println("  Version for Shinyei PPD42 ");
  Serial.println("  Version 10 December 2017 ");
  Serial.print  ("  DoBlinkStatus = ");
  Serial.println( DoBlinkStatus );
  Serial.print  ("  Navg = ");
  Serial.println( NAvg );

  Serial.println("===============================================");

  Serial.println("ESP8266 0.1  ...");
  Serial.println("mounting FS...");

  //
  // The code in the remainder of Setup() was obtained from Github and only marginally modified.
  //
  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_portStr, json["mqtt_port"]);
          mqtt_port = atoi(mqtt_portStr);
          strcpy(mqtt_username, json["mqtt_username"]);
          strcpy(mqtt_password, json["mqtt_password"]);
          strcpy(mqtt_topic, json["mqtt_topic"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    } else {
      Serial.println("/config.json does not exist, creating");
      saveConfigJson(); // saving the hardcoded default values
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read

  wifiManager.setSaveConfigCallback(saveConfigCallback);

  boolean startConfigPortal = false;
  if ( digitalRead(BUTTON_PIN) == LOW ) {
    Serial.println("startConfigPortal = true");
    // startConfigPortal = true;
    Serial.println("DISABLED startConfigPortal = true");
  }

  WiFi.mode(WIFI_STA);
  if (WiFi.SSID()) {
    Serial.println("Using saved credentials");
    ETS_UART_INTR_DISABLE();
    wifi_station_disconnect();
    ETS_UART_INTR_ENABLE();
    WiFi.begin();
  } else {
    Serial.println("No saved credentials");
    startConfigPortal = true;
  }

  WiFi.waitForConnectResult();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Failed to connect Wifi");
    startConfigPortal = true;
  }

  if (startConfigPortal) {
    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_portStr, 6);
    WiFiManagerParameter custom_mqtt_username("username", "mqtt username", mqtt_username, 20);
    WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 20);

    // Informatie in de interface:
    sprintf(mqtt_topic, "%ld", ESP.getChipId());
    sprintf(mqtt_server, "VUURWERK 2016/2017\0");

    WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", mqtt_topic, 20);
    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);

    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_topic);

    // If the user requests it, start the wifimanager
    digitalWrite(RED_LED_PIN, LOW); // off
    digitalWrite(BLUE_LED_PIN, LOW); // on
    wifiManager.startConfigPortal("SENSOR_RIVM");
    digitalWrite(BLUE_LED_PIN, HIGH); // off

    if (shouldSaveConfig) {
      // read the updated parameters
      strcpy(mqtt_server, custom_mqtt_server.getValue());
      strcpy(mqtt_portStr, custom_mqtt_port.getValue());
      mqtt_port = atoi(mqtt_portStr);
      strcpy(mqtt_username, custom_mqtt_username.getValue());
      strcpy(mqtt_password, custom_mqtt_password.getValue());
      strcpy(mqtt_topic, custom_mqtt_topic.getValue());

      saveConfigJson();
      shouldSaveConfig = false;
    }
  }

  Serial.println("Wifi connected...");
  Serial.print("Wifi SSID = ");
  Serial.println(WiFi.SSID());

  // Blink uitbundig met rood als we verbonden zijn ....
  for (i = 0 ; i < 15 ; i++) {
    digitalWrite(RED_LED_PIN, LOW); // off
    delay(100);
    digitalWrite(RED_LED_PIN, HIGH); // off
    delay(100);
  }

  mqttClient.setServer(mqtt_server, mqtt_port);
  chipid = ESP.getChipId();

  //-----------------------------------
  // Some setup for the Shinyei ...

  starttime = millis();
  pinMode(DUST_PM10_PIN, INPUT);

  randomSeed(analogRead(A0));

}

//#######################################################################
//
// Make a data packet and upload to server ...
//
void DoInfluxdbPost() {

  int i, j, ii;
  char  sret[IIMAX];
  char  *sok = "HTTP/1.1 204 No Content";

  WiFiClient client;
  const char* host = "#HOST_NAME_ASK_RIVM#";

  //---------------------------------------
  // The actual data package to the server:
  String PostData = InfluxThing + ",id=" + String( chipid ) ;
  PostData += " Temp=";
  PostData += x0;
  PostData += ",Pres=";
  PostData += x1;
  PostData += ",Hum=";
  PostData += x2;
  PostData += ",PM25=";
  PostData += x3;
  PostData += ",PM10=";
  PostData += x4;
  PostData += ",RSSI=";
  PostData += x5;
  PostData += ",ERROR=";
  PostData += x6;
  PostData += ",ECOUNT=";
  PostData += x7;
  PostData += ",SMPLS=";
  PostData += x8;

  // Both ON
  if (DoBlinkStatus > 1) {
    Serial.println("\nInfluxdbPost");
    digitalWrite(RED_LED_PIN, LOW); // on
    digitalWrite(BLUE_LED_PIN, LOW); // on
  }

  Serial.println("\n\n");
  Serial.println(PostData);
  Serial.println("\n\n");

  Serial.println("Ready for InfluxdbPost");

  int iret = client.connect(host, 8086);

  // Problem connecting to WiFi:  RED ON
  if (iret < 0) {
    Serial.println("\nNO client.connect ??" );

    if (DoBlinkStatus > 0) {
      digitalWrite(RED_LED_PIN, LOW);
    }

    Serial.println("\n=====================================================\n");
    Serial.print("FAILED CONNECT InfluxdbPost, code = ");
    Serial.println( iret);
    Serial.println("TIMED_OUT -1 ");
    Serial.println("INVALID_SERVER -2 ");
    Serial.println("TRUNCATED -3 ");
    Serial.println("INVALID_RESPONSE -4 ");
    Serial.println("=====================================================\n");
    Serial.println(" ");
    delay(20);
    return;
  }

  //#######################################################################
  //
  // IF YOU WANT TO SEND YOUR DATA TO THE RIVM DATA PORTAL PLEASE CONTACT
  //
  // SAMENMETEN@RIVM.NL
  //
  // FOR CREDENTIALS AND IMPLEMENTATION !!!
  //
  client.println("POST /write?db=#DBNAME#&u=#UNAME#&p=#PASSWRD# HTTP/1.1");
  client.println("Host: #HOST_NAME_ASK_RIVM#:8086");
  client.println("Cache-Control: no-cache");
  client.println("Content-Type: application/x-www-form-urlencoded");
  client.print("Content-Length: ");
  client.println(PostData.length());
  client.println();
  client.println(PostData);
  
  ii = 0;
  while (client.available()) {
    char c = client.read();
    if (ii < IIMAX) {
      sret[ii++] = c;
    }
    Serial.print(c);
  }

  sret[ii] = '\n';

  int sdif = 0;
  for (i = 0 ; i < strlen(sok) ; i++) {
    if (sret[i] != *(sok + i)) {
      sdif++;
    }
  }

  //----------------------------------
  // Respons InFluxDB not OK
  if (sdif > 0) {
    iret = -100 * sdif;
    Serial.print("\nDId not get EXPECTED respons from InFluxDB ");
    Serial.println(iret);
    Serial.print("[");
    Serial.print(sret);
    Serial.println("]");

    Serial.println(PostData);

    digitalWrite(RED_LED_PIN, HIGH); // off

    if (DoBlinkStatus > 0) {
      // BLUE BLINK 15
      for (i = 0 ; i < 15 ; i++) {
        digitalWrite(BLUE_LED_PIN, LOW); // off
        delay(450);
        digitalWrite(BLUE_LED_PIN, HIGH); // off
        delay(150);
      }
      // BLUE ON
      digitalWrite(BLUE_LED_PIN, LOW); // off
    }
  }

  //----------------------------------
  // UNDEFINED issue, BLUE BLINK 15
  if (iret == 0 && DoBlinkStatus > 0) {
    Serial.print("Did not get ANY respons from InFluxDB ");
    digitalWrite(RED_LED_PIN, HIGH); // off
    for (i = 0 ; i < 15 ; i++) {
      digitalWrite(BLUE_LED_PIN, LOW); // off
      delay(150);
      digitalWrite(BLUE_LED_PIN, HIGH); // off
      delay(150);
    }
  }

  if (iret > 0 && DoBlinkStatus > 1) {
    // Communication OK, BLINK RED/BLUE 15
    Serial.print("\nEverything seems OK ... ");
    for (i = 0 ; i < 15 ; i++) {
      digitalWrite(BLUE_LED_PIN, LOW); // off
      digitalWrite(RED_LED_PIN, HIGH); // off
      delay(150);
      digitalWrite(BLUE_LED_PIN, HIGH); // off
      digitalWrite(RED_LED_PIN, LOW); // off
      delay(150);
    }
    digitalWrite(RED_LED_PIN, HIGH); // on
    digitalWrite(BLUE_LED_PIN, HIGH); // on
  }

  if (iret > 0) {
    digitalWrite(RED_LED_PIN, HIGH); // off
    digitalWrite(BLUE_LED_PIN, HIGH); // off
  }

  Serial.print("\nSDif, IRet =  ");
  Serial.print(sdif);
  Serial.print(",  ");
  Serial.print(iret);
}



//#######################################################################
//
// The main loop that is continually called/executed.
//
void loop() {
  int aux;
  int i;
  int secs = 0;

  //#####################################################################
  // This code can be found all over the internet, sometimes with 
  // additional lines to read pulse-length associated with smaller particles.
  // We choose to use only the output for the larger particles.
  // The pulse of the Shinyei is sampled on input defined in DUST_PM10_PIN .
  // We use the Shinyei PPD42, only sampling the output for the larger particles (PM10)
  //
  // Examples:
  // https://github.com/MattSchroyer/DustDuino/blob/master/DustDuino.ino
  // https://gist.github.com/proffalken/
  // https://github.com/dustduino/DustDuinoSerial/blob/master/DustDuinoSerial/DustDuinoSerial.ino 
  // And many more .... 
  
  valP2 = digitalRead(DUST_PM10_PIN);

  if (valP2 == LOW && triggerP2 == false) {
    triggerP2 = true;
    triggerOnP2 = micros();
  }

  if (valP2 == HIGH && triggerP2 == true) {
    triggerOffP2 = micros();
    pulseLengthP2 = triggerOffP2 - triggerOnP2;
    durationP2 = durationP2 + pulseLengthP2;
    triggerP2 = false;
  }

  if ((millis() - starttime) > sampletime_ms) {
    ratioP2 = durationP2 / (sampletime_ms * 10.0);

    SumDust += ratioP2;          // Store in val3
    durationP2 = 0;

    //---------------------------------------
    // Some output...
    Serial.print(NAvg);
    Serial.print(" : ID = ");
    Serial.print(mqtt_topic);
    Serial.print(" : ESP = ");
    Serial.print(chipid);
    Serial.print(" : WiFi = ");
    Serial.print(WiFi.RSSI());
    Serial.print(" : SumDust = ");
    Serial.println(SumDust);

    starttime = millis();
    NAvg++;

    if (DoBlinkStatus == 1) {
      digitalWrite(RED_LED_PIN, LOW); // off
      delay(250);
      if (WiFi.RSSI() < 0) {
        digitalWrite(RED_LED_PIN, HIGH); // off
      }
    }

    //--------------------------------------------------------
    // Average the data and periodically send it to a server:
    //
    if (NAvg > MaxIter) {
      rssi = WiFi.RSSI(); // RSSI = wifi signal strength

      //--------------------------------------------------------
      // Create data package:
      x0 = -1.0; x1 = -1.0;   x2 = -1.0;  x3 = -1.0;    // All unused.
      x4 = SumDust / ((float) MaxIter);                 // Dust measured (proxy)
      x5 = rssi;                                        // WiFi signal
      x6 = -1.0;  x7 = -1.0;                            // Unused
      x8 = x8 + 1.0;                                    // Counter

      if (x8 > 1000.0)
        x8 = 0.0;

      // Send the data ...
      delay (1000);

      //--------------------------------------------------------
      // To the server?
      //
      if (NAvg == MAX_ITER) {           

        delay(10);
        DoInfluxdbPost();   // Send to server
        TotCount++;

        Serial.print("\n\nTotCount = ");
        Serial.println(TotCount);

        if (TotCount > 1000000)
          TotCount = 1000;

        if (TotCount > 10 && DoBlinkStatus > 1) {   // Switch off led's ?
          DoBlinkStatus = 1;
          Serial.print("\nDoBlinkStatus reduced to 1 ");
        }

        Serial.println();
        NAvg = 0;
        SumDust = 0.0;
      }
    }
  }
}

