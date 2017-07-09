/*
 * Find more information about this project https://conejoninja.github.io/home/devices/food01/
 * 
 * 
 * The MIT License (MIT)
 * 
 * Copyright 2017 Daniel Esteban - conejo@conejo.me
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
 * 
 * 
 * There are several libraries used in the code, described in the section below that may have different licenses.
 */


/*
 * LIBRARIES
 * All libraries could be downloaded with the Arduino IDE searching by the name
 * 
 * RTC by Makuna : https://github.com/Makuna/Rtc
 * WifiManager : https://github.com/tzapu/WiFiManager
 * Adafruit DHT11 : https://github.com/adafruit/DHT-sensor-library
 *   requires Adafruit Unified sensor library : https://github.com/adafruit/Adafruit_Sensor
 * ArduinoJson : https://github.com/bblanchon/ArduinoJson
 * PubSubClient : https://github.com/knolleary/pubsubclient
 * 
 */

/* CONNECTIONS 
 *  
 * Circuit at circuit.png 
 */

/* LOAD CONFIGURATION */
#include "config.h"
// See config-sample.h

/* START LOAD LIBRARIES */
#if defined(ESP8266)
#include <pgmspace.h>
#else
#include <avr/pgmspace.h>
#endif

#include <Wire.h> // must be included here so that Arduino library object file references work
#include <RtcDS1307.h>
RtcDS1307<TwoWire> Rtc(Wire);
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Need to modify PubSubClient.h for some reason it doesn't fully work with this
#define MQTT_MAX_PACKET_SIZE 8192
#include <PubSubClient.h>
#include <ArduinoJson.h>
/* END LOAD LIBRARIES */

/* DEFINES AND CONFIGURATION */
#define countof(a) (sizeof(a) / sizeof(a[0]))

const char APName[] = "DULICOMIDA3000";
const char* device_id = "food01";
const char* device_name = "Dulicomida 3000";
const char* listen_channel = "food01-call";

#define DHTPIN        14         
#define ENCODERAPIN   12
#define ENCODERBPIN   13
#define MOTORAPIN     2
#define MOTORBPIN     3

#define MOTORENABLED  1 // Motor is using the serial pins, set to 0 to disable it and be able to debug via Serial
#define DHTENABLED    1

#define MAXLONGROTATION   120 // SET NUMBER OF MAX ROTATION FOR FOOD
#define MAXSHORTROTATION  30 // SET NUMBER OF MAX ROTATION FOR FOOD

// Uncomment the type of sensor in use
#define DHTTYPE           DHT11     // DHT 11 
//#define DHTTYPE           DHT22     // DHT 22 (AM2302)
//#define DHTTYPE           DHT21     // DHT 21 (AM2301)

DHT_Unified dht(DHTPIN, DHTTYPE);
uint32_t delayMS;

// ROTARY ENCODER
volatile long pos = 0;   
volatile boolean CW = true;

boolean isConnected = true;
WiFiClient espClient;
PubSubClient client(espClient);


// Calculate how much food has been dispensed
void ICACHE_RAM_ATTR interruptEncoder() {
  volatile boolean a = digitalRead(ENCODERAPIN); 
  volatile boolean b = digitalRead(ENCODERBPIN); 
  if(a) {
    CW = (a != b);
    if (CW)  {
      pos++;
    } else { 
      pos--;
    }
  }
}



void setup() {
  //Serial.begin(115200); // Disable if the motor is connected
  Serial.println("[SETUP]");
  
  
  
  /**** START SETUP CLOCK ****/
  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.print(" ");
  Serial.println(__TIME__);
  
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  Serial.println();
  
  if (!Rtc.IsDateTimeValid()) {
    // Common Cuases:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing  
    Serial.println("RTC lost confidence in the DateTime!");  
    Rtc.SetDateTime(compiled);
  }
  
  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }
  
  RtcDateTime now = Rtc.GetDateTime();
  Serial.print("NOW: ");
  printDateTime(now);
  Serial.println();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
  
  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.SetSquareWavePin(DS1307SquareWaveOut_Low);
  
  
  // RESET ALARMS 
  // Uncomment to reset the alarms (or do it via MQTT call!)
  //setMemory(1, 0);
  //setMemory(2, 0);
  //setMemory(3, 2000);
  //setMemory(4, 700);
  /**** END SETUP CLOCK ****/
  
  
  
  /**** START SETUP DHTXX ****/ 
  dht.begin();
  /**** END SETUP DHTXX ****/
  
  
  /**** START SETUP ROTARY ENCODER ****/
  pinMode(ENCODERAPIN, INPUT);
  pinMode(ENCODERBPIN, INPUT);
  attachInterrupt(ENCODERAPIN, interruptEncoder, RISING);
  /**** END SETUP ROTARY ENCODER ****/
  
  if(MOTORENABLED==1) {
    /**** START SETUP DC MOTOR ****/
    pinMode(MOTORAPIN, OUTPUT);
    pinMode(MOTORBPIN, OUTPUT);
    digitalWrite(MOTORAPIN, LOW);
    digitalWrite(MOTORBPIN, LOW);
    /**** END SETUP DC MOTOR ****/  
  }
  
  
  
  /**** START SETUP WIFI ****/
  // WiFiManager is great
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  //wifiManager.resetSettings(); // reset saved settings
  wifiManager.setTimeout(180);
  
  if(!wifiManager.autoConnect(APName)) {
    isConnected = false;
    Serial.println("TIMEOUT WIFI");
  }
  
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  /**** END SETUP WIFI ****/


  if(isConnected) {
    /**** START MQTT STUFF ****/
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    
    if(!client.connected()) {
      reconnect();
    }

    if(client.connected()) {
      Serial.println("CLIENT LOOP");


      // RUN THIS FOR A FEW SECONDS TO SEE IF THERE'S ANY MESSAGE
      int k = 1000;
      while(k>0) {
        client.loop();
        k--;
        delay(1);
      }

      if(DHTENABLED==1) {
        getTemp(); // get sensor data and publish it
      }
      
    }
    /**** END MQTT STUFF ****/
  }    

  uint32 deepSleepNext = checkTime();
  delay(1000);
  Serial.println("[DISCONNECT]");
  client.disconnect();
  digitalWrite(MOTORAPIN, LOW);
  digitalWrite(MOTORBPIN, LOW);
  ESP.deepSleep(deepSleepNext*1000000);

}

// This is called if there's any message in the device_id-call channel
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("[CALLBACK]");
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  StaticJsonBuffer<400> jsonBuffer;

  if((char)(payload[0])=='[') {
    JsonArray& root = jsonBuffer.parseArray(payload);
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }
    int k = root.size();
    for(int i=0;i<k;i++) {
      processCall(root[i]);
    }
  } else if (length>0) {
    JsonObject& root = jsonBuffer.parseObject(payload);
    if (!root.success()) {
      Serial.println("parseObject() failed");
      return;
    }
    processCall(root);
  } else {
    return;
  }
  client.publish(listen_channel, "", true);
}

void processCall(JsonObject& root) {
  Serial.println("[PROCESS CALL]");
  if(root.containsKey("name")) {
    if(root["name"]=="food") {
      doFood(0);
    } else if(root["name"]=="ping") {
      if(root.containsKey("params")) {
        JsonObject& p = root["params"];
        if(p.containsKey("s")) {
          doPing(p["s"]);
        }
      }
    } else if(root["name"]=="setmem") {
      if(root.containsKey("params")) {
        JsonObject& p = root["params"];
        if(p.containsKey("id") && p.containsKey("value")) {
          Serial.println("[SET MEM]");
          int id = atoi(p["id"]);
          Serial.println(id);
          int val = atoi(p["value"]);
          Serial.println(val);
          setMemory(id, val);
        }
      }
    } else if(root["name"]=="getmem") {
      
      StaticJsonBuffer<400> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();
      JsonArray& out = root.createNestedArray("");
      
      JsonObject& m1 = jsonBuffer.createObject();
      JsonObject& m2 = jsonBuffer.createObject();
      JsonObject& m3 = jsonBuffer.createObject();
      JsonObject& m4 = jsonBuffer.createObject();
  
      m1["id"] = "m1";
      m1["value"] = getMemory(1);
      m2["id"] = "m2";
      m2["value"] = getMemory(2);
      m3["id"] = "m3";
      m3["value"] = getMemory(3);
      m4["id"] = "m4";
      m4["value"] = getMemory(4);
      
      out.add(m1);
      out.add(m2);
      out.add(m3);
      out.add(m4);
      
      String output;
      out.printTo(output);   
      char msg[120];
      output.toCharArray(msg, (output.length() + 1));     

      Serial.print("[GET MEM] ");
      Serial.println(msg);
      
      client.publish(device_id, msg);

      
    }
  }
}

// Connection to the MQTT server
void reconnect() {
  Serial.println("[RECONNECT]");
  uint8 c = 0;
  while (c<6 && !client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("HomeFood01", mqtt_user, mqtt_pass)) {
      Serial.println("connected");

      client.subscribe(listen_channel);

      /* Calculate buffer size https://bblanchon.github.io/ArduinoJson/assistant/
       *  Removed "type":"number" , since it's the default type, we could save some resources here
       {
         "id":"food01",
         "name":"Dulicomida 3000",
         "version":"1.0.0",
         "out":[
            {"id":"t1","name":"temperature"},
            {"id":"h1","name":"humidity"},
            {"id":"m1","name":"alarm1"},
            {"id":"m2","name":"alarm2"},
            {"id":"m3","name":"memory1"},
            {"id":"m4","name":"memory2"}
         ],
         "methods":[
            {"name":"food"},
            {"name":"ping"},
            {"name":"setmem","params":[{"name":"id"},{"name":"value"}]},
            {"name":"getmem"}
         ]
       }
       */


      StaticJsonBuffer<1000> jsonBuffer;
      JsonObject& root = jsonBuffer.createObject();
      root["id"] = device_id;
      root["name"] = device_name;
      root["version"] = "1.0.0";

      JsonArray& out = root.createNestedArray("out");
      JsonObject& out_temp = jsonBuffer.createObject();
      out_temp["id"] = "t1";
      out_temp["name"] = "temperature";
      //out_temp["type"] = "number";
      out.add(out_temp);
      JsonObject& out_humd = jsonBuffer.createObject();
      out_humd["id"] = "h1";
      out_humd["name"] = "humidity";
      //out_humd["type"] = "number";
      out.add(out_humd);
      JsonObject& out_mem1 = jsonBuffer.createObject();
      out_mem1["id"] = "m1";
      out_mem1["name"] = "alarm1";
      out.add(out_mem1);
      JsonObject& out_mem2 = jsonBuffer.createObject();
      out_mem2["id"] = "m2";
      out_mem2["name"] = "alarm2";
      out.add(out_mem2);
      JsonObject& out_mem3 = jsonBuffer.createObject();
      out_mem3["id"] = "m3";
      out_mem3["name"] = "memory1";
      out.add(out_mem3);
      JsonObject& out_mem4 = jsonBuffer.createObject();
      out_mem4["id"] = "m4";
      out_mem4["name"] = "memory2";
      out.add(out_mem4);

      JsonArray& methods = root.createNestedArray("methods");
      JsonObject& m1 = jsonBuffer.createObject();
      m1["name"] = "food";
      methods.add(m1);
      
      JsonObject& m2 = jsonBuffer.createObject();
      m2["name"] = "ping";
      JsonArray& params2 = m2.createNestedArray("params");
      JsonObject& p2 = jsonBuffer.createObject();
      p2["name"] = "s";
      p2["type"] = "text";
      params2.add(p2);
      methods.add(m2);
      
      JsonObject& m3 = jsonBuffer.createObject();
      m3["name"] = "getmem";
      methods.add(m3);
      
      JsonObject& m4 = jsonBuffer.createObject();
      m4["name"] = "setmem";
      JsonArray& params4 = m4.createNestedArray("params");
      JsonObject& p41 = jsonBuffer.createObject();
      p41["name"] = "id";
      params4.add(p41);
      JsonObject& p42 = jsonBuffer.createObject();
      p42["name"] = "value";
      params4.add(p42);
      methods.add(m4);

      String output;
      root.printTo(output);

      char msg[420];
      output.toCharArray(msg, (output.length() + 1));
      Serial.println(msg);
      boolean sent = client.publish("discovery", msg);
      Serial.print("DISCOVER SENT : ");
      Serial.println(sent);
      delay(100);      
      break;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
    c++;
  }
}
/**** END MQTT STUFF ****/


void doFood(uint8 alarm) {
  Serial.println("[DO FOOD]");
  Serial.println(alarm);
  
  uint8 tries = 0; // AVOID GETTING STUCK IF MOTOR IS JAMMED
  pos = 0;

  int maxRotation = MAXSHORTROTATION;
  if(alarm==1) {
     maxRotation = MAXLONGROTATION;
  }
  if(MOTORENABLED==1) {
    for(tries=0;tries<70;tries++) {
      doMotor();
      delay(10);
      if(pos>=maxRotation || pos<(-maxRotation)) {
        break;  
      }
    }
  }


  char* alertMsg;
  uint8 alertPriority;
  if(pos<maxRotation && pos>(-maxRotation)) {
    // Send alert, it's jammed (didn't move enough)
    // Don't modify memory value, so next wake up will try to dispense food again.
    alertMsg = "Food01 is jammed. No food served.";
    alertPriority = 3;
  } else {
    // SET NEXT EXECUTION IN A DAY
    if(alarm==1 || alarm==2) {
      uint32 mNext = 0;
      uint8 hourNext = 20;
      uint8 minNext = 0;
      if(alarm==1) {
        mNext = getMemory(3);
      } else {
        mNext = getMemory(4);
      }
      minNext = mNext%100;
      hourNext = ((mNext-minNext)/100);
      if(minNext>59) {
        minNext = 0;
        hourNext++;
      }
      hourNext = hourNext%24;
      RtcDateTime now = Rtc.GetDateTime();
      RtcDateTime next = RtcDateTime(now.Year(), now.Month(), now.Day(), hourNext, minNext, 0);
      Serial.println("[NEXT ALARM SET]");
      printDateTime(next);
      uint32 totalNext = next.TotalSeconds();
      totalNext += 86390; // 24*60*60-10
      Serial.print("TOTALNEXT ");
      Serial.print(totalNext);
      Serial.print("     (");
      Serial.print(alarm);
      Serial.println(")");
      setMemory(alarm, totalNext);
    }
    alertMsg = "Food served correctly.";
    alertPriority = 0;   
  }
  event(alertMsg, alertPriority);
}

// Enable the motor
void doMotor() {
  Serial.println("[DO MOTOR]");

  int n = 0;
  for(n=0;n<10;n++) {
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, HIGH);    
    delay (40);
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, LOW);    
    delay (8);
  }

  digitalWrite(MOTORAPIN, HIGH);
  digitalWrite(MOTORBPIN, LOW);    
  delay (140);
  for(n=0;n<6;n++) {
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, HIGH);    
    delay (40);
    digitalWrite(MOTORAPIN, LOW);     
    digitalWrite(MOTORBPIN, LOW);    
    delay (8);
  }

  digitalWrite(MOTORAPIN, HIGH);
  digitalWrite(MOTORBPIN, LOW);    
  delay (90);
  digitalWrite(MOTORAPIN, LOW);
  digitalWrite(MOTORBPIN, LOW);    
}


void getTemp() {
  Serial.println("[GET TEMP]");
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  delayMS = 2100;//sensor.min_delay / 1000;  
  uint8 n = 5; // number of samples we take
  uint8 nT = 0;
  uint8 nH = 0;
  float t = 0;
  float h = 0;

  // We take some samples and calculate the average
  while(n>0) {
    sensors_event_t event;  
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      Serial.println("Error reading temperature!");
    } else {
      Serial.print("Temperature: ");
      Serial.print(event.temperature);
      t += event.temperature;
      nT++;
    
      Serial.println(" *C");
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      Serial.println("Error reading humidity!");
    } else {
      Serial.print("Humidity: ");
      Serial.print(event.relative_humidity);
      h += event.relative_humidity;
      nH++;
      Serial.println("%");
    }  
    n--;
    delay(delayMS);
  }

  StaticJsonBuffer<250> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  JsonArray& out = root.createNestedArray("");

  JsonObject& tdata = jsonBuffer.createObject();
  JsonObject& hdata = jsonBuffer.createObject();
  if(nT>0 || nH>0) {
    if(nT>0) {
      tdata["id"] = "t1";
      tdata["unit"] = "C";
      tdata["value"] = t/nT;
      out.add(tdata);
    }
    if(nH>0) {
      hdata["id"] = "h1";
      hdata["unit"] = "%";
      hdata["value"] = h/nH;
      out.add(hdata);
    }
    
    String output;
    out.printTo(output);   
    char msg[120];
    output.toCharArray(msg, (output.length() + 1));      
      
    client.publish(device_id, msg);
  } else {
    event("DHT11 of food01 is not working properly", 1); // alert if we couldn't take any valid data
  }
}

// Check if any alarm is triggered
// Calculate the # of seconds for the next wake up
uint32 checkTime() {
  Serial.println("[CHECK TIME]");
  RtcDateTime now = Rtc.GetDateTime();
  uint32 total = now.TotalSeconds();

  if (!Rtc.IsDateTimeValid()) {
    Serial.println("[CHECK TIME] DATE TIME IS NOT VALID");
    event("Clock of food01 is not working properly", 2);
    return 960; //900 secs = 15min 
  }
  printDateTime(now); 
  
  uint8 minNext = now.Minute();
  if(minNext<15) {
    minNext = 0;
  } else if(minNext>=15 && minNext<30) {
    minNext = 15;
  } else if(minNext>=30 && minNext<45) {
    minNext = 30;
  } else if(minNext>=45) {
    minNext = 45;
  }
  
  RtcDateTime next = RtcDateTime(now.Year(), now.Month(), now.Day(), now.Hour(), minNext, 0);
  printDateTime(next);
  uint32 totalNext = next.TotalSeconds();
  totalNext += 960;  // 16min
  next = RtcDateTime(totalNext);
  
  uint32 memExec = getMemory(1);
  Serial.print("TOTAL NEXT ");
  Serial.println(totalNext);
  Serial.print("TOTAL ");
  Serial.println(total);
  Serial.print("MEMEXEC[1] ");
  Serial.println(memExec);
  if(memExec<=total) {
    doFood(1);
  } else {
    memExec = getMemory(2);
    Serial.print("TOTAL ");
    Serial.println(total);
    Serial.print("MEMEXEC[2] ");
    Serial.println(memExec);
    if(memExec<=total) {
      doFood(2);
    }
  }
  
  return (totalNext - total);
}

void doPing(const char* message) {
  event(message, 0);
}

void event(const char* message, uint8 priority) {
  Serial.println("[EVENT]");
  Serial.println(message);
  StaticJsonBuffer<120> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = device_id;
  root["message"] = message;
  root["priority"] = priority;

  String output;
  root.printTo(output);   
  char msg[120];
  output.toCharArray(msg, (output.length() + 1));      
  
  boolean sent = client.publish("events", msg);
  Serial.print("[EVENT SENT] ");
  Serial.println(sent);  
}

void printDateTime(const RtcDateTime& dt) {
  Serial.println("[PRINT DATE TIME]");
  char datestring[20]; 
  snprintf_P(datestring, 
  countof(datestring),
  PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
  dt.Month(),
  dt.Day(),
  dt.Year(),
  dt.Hour(),
  dt.Minute(),
  dt.Second() );
  Serial.println(datestring);
}

// Save data in memory of DS1307
void setMemory(uint8 alarm, uint32_t dataMemInt) {
  Serial.println("[SET MEMORY]");
  char dataMem[10];
  itoa(dataMemInt, dataMem, 10);
  Rtc.SetMemory(2*(alarm-1), 13*alarm);
  Serial.println(dataMemInt);
  Serial.println(dataMem);
  uint8_t written = Rtc.SetMemory(13*alarm, (const uint8_t*)dataMem, sizeof(dataMem) );
  Rtc.SetMemory(1+2*(alarm-1), written);
}

// Get values from memory
uint32 getMemory(uint8 alarm) {
  Serial.println("[GET MEMORY]");
  uint8_t address = Rtc.GetMemory(2*(alarm-1));
  if (address != (13*alarm)) {
    Serial.println("address didn't match");
  } else {
    uint8_t count = Rtc.GetMemory(1+2*(alarm-1));
    uint8_t buff[10];
    String bf;
    uint8_t gotten = Rtc.GetMemory(address, buff, count);
    
    if (gotten != count ) {
      //Serial.print("something didn't match, count = ");
      //Serial.print(count, DEC);
      //Serial.print(", gotten = ");
      //Serial.print(gotten, DEC);
      //Serial.println();
    }
    
    while(gotten > 0) {
      bf += (char)buff[count - gotten];
      gotten--;
    }

    return bf.toInt();
  }
  return 0;
}


void loop() { }

