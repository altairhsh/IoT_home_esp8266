#include <ESP8266WiFi.h>
#include <DHT.h>
#include "Account.h"
#include "ThingSpeak.h"
#include "PMS.h"

#define DHTPIN  4
#define DHTTYPE DHT22

#define  VER          20171007

//#define DEBUG

#ifdef DEBUG
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINT(x)  Serial.print(x)
#else
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINT(x)
#endif

DHT dht(DHTPIN, DHTTYPE);
PMS pms(Serial);
PMS::DATA data;
WiFiClient client;

int http_get(String url, char* host) {
  WiFiClient http_client;
  const int httpPort = 80;

  DEBUG_PRINT("connecting to ");
  DEBUG_PRINTLN(host);

  if(!http_client.connect(host, httpPort)) {
    DEBUG_PRINTLN("connection failed");
    return 0;
  }
  
  DEBUG_PRINT("Requesting URL: ");
  DEBUG_PRINTLN(url);
  
  http_client.print(String("GET ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
  unsigned long timeout = millis();

  while(http_client.available() == 0) {
    if(millis() - timeout > 5000) {
      DEBUG_PRINTLN(">>> Client Timeout !");
      http_client.stop();
      return 0;
    }
  }

  while(http_client.available()) {
    String line = http_client.readStringUntil('\r');
    DEBUG_PRINT(line);
  }

  DEBUG_PRINTLN();
  DEBUG_PRINTLN("closing connection");

  return 1;
}

void thingSpeakClient(int pm1_0, int pm2_5, int pm10_0, float temperature, float humidity) {
  ThingSpeak.setField(1, pm1_0);
  ThingSpeak.setField(2, pm2_5);
  ThingSpeak.setField(3, pm10_0);
  ThingSpeak.setField(4, VER);
  ThingSpeak.setField(5, temperature);
  ThingSpeak.setField(6, humidity);

  ThingSpeak.writeFields(ThingSpeak_ChannelNumber, ThingSpeak_WriteAPIKey);
}

void blink_LED(unsigned int period, int duration) {
  unsigned int i;
  for(i=0;i<period;i++) {
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    delay(duration);                       // wait for a second
    digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage LOW
    delay(duration);                       // wait for a second
  }
}

void setup() {
  // put your setup code here, to run once:
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.begin(9600);

  DEBUG_PRINTLN();
  DEBUG_PRINTLN();
  DEBUG_PRINT("Connecting to ");
  DEBUG_PRINTLN(WIFI_ssid);

  WiFi.begin(WIFI_ssid, WIFI_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("WiFi connected");
  DEBUG_PRINTLN("IP address: ");
  DEBUG_PRINTLN(WiFi.localIP());  

  dht.begin();
  ThingSpeak.begin(client);
  pms.passiveMode();
}

void loop() {
  unsigned long timeout;
  int pm1_0         = 0;
  int pm2_5         = 0;
  int pm10_0        = 0;
  float humidity    = 0;
  float temperature = 0;

  DEBUG_PRINTLN("Room monitor : Start"); 
  timeout = millis();
  pms.wakeUp();
  delay(30000);
  blink_LED(1, 1000);
  
  DEBUG_PRINTLN("Read DHT22 : Start"); 
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  while(isnan(humidity) || isnan(temperature)) {
    DEBUG_PRINTLN("Failed to read from DHT sensor!");
    blink_LED(3, 250);
    delay(2000);
    
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();    
  }
  DEBUG_PRINT("Temperature :");
  DEBUG_PRINTLN(temperature);
  DEBUG_PRINT("Humidity :");
  DEBUG_PRINTLN(humidity);
  
  DEBUG_PRINTLN("Read DHT22 : Done"); 

  DEBUG_PRINTLN("Read PMS7003 : Start"); 

  pms.requestRead();

  while(!pms.read(data, 10000)) {
    DEBUG_PRINTLN("Failed to read from dust sensor!");
    blink_LED(4, 250);
    delay(2000);
    
    pms.requestRead();
  }

  DEBUG_PRINT("PM1.0 :");
  DEBUG_PRINTLN(data.PM_AE_UG_1_0);
  DEBUG_PRINT("PM2.5 :");
  DEBUG_PRINTLN(data.PM_AE_UG_2_5);
  DEBUG_PRINT("PM10.0 :");
  DEBUG_PRINTLN(data.PM_AE_UG_10_0);
  
  DEBUG_PRINTLN("Read PMS7003 : Done"); 

  DEBUG_PRINTLN("Update Thingspeak : Start"); 
  thingSpeakClient((int)data.PM_AE_UG_1_0, (int)data.PM_AE_UG_2_5, (int)data.PM_AE_UG_10_0, temperature, humidity);
  DEBUG_PRINTLN("Update Thingspeak : Done"); 
  
  DEBUG_PRINTLN("Room monitor : Done"); 
  blink_LED(2, 1000);
  pms.sleep();

  while(1)
  {
    delay(30000); // ThingSpeak will only accept updates every 10 minutes.
    if(millis() - timeout > 600000) {
      break;
    }
  }
}
