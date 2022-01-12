#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <SoftwareSerial.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


//#define co2Serial Serial1
#define mqtt_server "192.168.0.233"
#define mqtt_user ""  
#define mqtt_password ""

const char* ssid = "TEST";
const char* password = "test";


U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ 16, /* clock=*/ 5, /* data=*/ 4);
SoftwareSerial co2Serial(13,15);

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long previousMillis=0 ;
unsigned long interval = 3000L;

int readCO2()
{

  byte cmd[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
  // command to ask for data
  byte response[9]; // for answer

  co2Serial.write(cmd, 9); //request PPM CO2

  // The serial stream can get out of sync. The response starts with 0xff, try to resync.
  while (co2Serial.available() > 0 && (unsigned char)co2Serial.peek() != 0xFF)
  {
    co2Serial.read();
  }

  memset(response, 0, 9);
  co2Serial.readBytes(response, 9);

  if (response[1] != 0x86)
  {
    Serial.println("Invalid response from co2 sensor!");
    return -1;
  }

  byte crc = 0;
  for (int i = 1; i < 8; i++)
  {
    crc += response[i];
  }
  crc = 255 - crc + 1;

  if (response[8] == crc)
  {
    int responseHigh = (int)response[2];
    int responseLow = (int)response[3];
    int ppm = (256 * responseHigh) + responseLow;
    return ppm;
  }
  else
  {
    Serial.println("CRC error!");
    return -1;
  }
}



void i2c_scanner (void)
{
  byte error, address;
  int nDevices;
  Serial.println("\nI2C Scanner"); 
  Serial.println("Scanning...");
 
  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  {
 
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
 
    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");
 
      nDevices++;
    }
    else if (error==4) 
    {
      Serial.print("Unknow error at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");
}

String ipToString(IPAddress ip){
  String s="";
  for (int i=0; i<4; i++)
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  return s;
}

void configModeCallback (WiFiManager *myWiFiManager) {
  char msg[64] = {0} ; 
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());

  u8g2.firstPage();
  u8g2.setFont(u8g2_font_helvR10_tf); // set the target font to calculate the pixel width
  u8g2.setFontMode(0);    // enable transparent mode, which is faster
  strcpy(msg,"AP: ") ; 
  strcat(msg,myWiFiManager->getConfigPortalSSID().c_str()) ;
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 16, msg);     // draw the scolling text

  strcpy(msg,ipToString(WiFi.softAPIP()).c_str()) ; 
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 32, msg);     // draw the scolling text
  u8g2.nextPage();
}

void reconnect() {
  //Boucle jusqu'à obtenur une reconnexion
  while (!client.connected()) {
    Serial.print("Connexion au serveur MQTT...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("OK");
    } else {
      Serial.print("KO, erreur : ");
      Serial.print(client.state());
      Serial.println(" On attend 5 secondes avant de recommencer");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length,DEC));
}

void setup() {
  char msg[64] = {0} ; 
  Serial.begin(115200);
  co2Serial.begin(9600);

  u8g2.begin();

  u8g2.firstPage();
  u8g2.setFont(u8g2_font_logisoso32_tf); // set the target font to calculate the pixel width
  u8g2.setFontMode(0);    // enable transparent mode, which is faster
  strcpy(msg,"CO2") ; 
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 32, msg);     // draw the scolling text
  u8g2.nextPage();

  delay(2000) ; 

  i2c_scanner();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  client.setServer(mqtt_server, 1883);    //Configuration de la connexion au serveur MQTT
  client.setCallback(callback);

  u8g2.firstPage();
  u8g2.setFont(u8g2_font_helvR10_tf); // set the target font to calculate the pixel width
  u8g2.setFontMode(0);    // enable transparent mode, which is faster
  strcpy(msg,"AP: ") ; 
  strcat(msg,WiFi.SSID().c_str()) ;
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 16, msg);     // draw the scolling text
  strcpy(msg,ipToString(WiFi.localIP()).c_str()) ; 
  u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 32, msg);     // draw the scolling text
  u8g2.nextPage();


    // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char msg[64] = {0} ; 
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));

    u8g2.firstPage();
    u8g2.setFont(u8g2_font_helvR08_tf); // set the target font to calculate the pixel width
    u8g2.setFontMode(0);    // enable transparent mode, which is faster
    u8g2.drawUTF8(0 , 8, "UPLOAD : ");     // draw the scolling text

    u8g2.setFont(u8g2_font_logisoso18_tf); // set the target font to calculate the pixel width
    u8g2.setFontMode(0);    // enable transparent mode, which is faster
    strcpy(msg,String((progress / (total / 100))).c_str()) ;
    strcat(msg," %") ; 
    
    u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 30, msg);     // draw the scolling text
    u8g2.nextPage();

  
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());


  previousMillis = millis() ; 

}

void loop() {
  int CO2  ; 
  char msg[64] = {0} ; 


  if (!client.connected()) 
  {
    Serial.print("Reconnect wifi ");                      
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();


  if( millis() - previousMillis >= interval) 
  {
    previousMillis = millis();  

    CO2 = readCO2();                            // Request CO2 (as ppm)
    
    Serial.print("CO2 (ppm): ");                      
    Serial.println(CO2);                                


    u8g2.firstPage();
    u8g2.setFont(u8g2_font_helvR08_tf); // set the target font to calculate the pixel width
    u8g2.setFontMode(0);    // enable transparent mode, which is faster
    u8g2.drawUTF8(0 , 8, "CO2 : ");     // draw the scolling text

    u8g2.setFont(u8g2_font_logisoso18_tf); // set the target font to calculate the pixel width
    u8g2.setFontMode(0);    // enable transparent mode, which is faster
    strcpy(msg,String(CO2).c_str()) ;
    strcat(msg," ppm") ; 
    
    u8g2.drawUTF8((u8g2.getWidth() - u8g2.getUTF8Width(msg)) /2 , 30, msg);     // draw the scolling text
    u8g2.nextPage();

    strcpy(msg,"{\"idx\": 111, \"nvalue\" : ");
    strcat(msg,String(CO2).c_str()) ;
    strcat(msg," ,\"svalue\" : \"\"}");

    client.publish("domoticz/in", msg, true); 
   } 
}