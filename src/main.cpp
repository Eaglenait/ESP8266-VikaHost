#include <Arduino.h>
#include <Fs.h>
#include "ArduinoJson.h"

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebSockets.h>
#include <WiFiClient.h>
#include "IPAddress.h"

#include <WiFiUdp.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>

File fsUploadFile;
ESP8266WebServer server(80);

websockets::WebsocketsClient socket;

const unsigned int localUDPPort = 45454;
const IPAddress multicastGroupAddress = IPAddress(224,0,0,251);
const int GREEN = 12;

WiFiUDP UDP;
char incomingPacket[255];

//for pin handling 
bool greenValue = false;

Ticker t;

void handleNotFound()
{
  Serial.println("on handle not found");
  server.send(404, "text/html", "404 Not Found");
}

//Handles a pin request
void handlePin() {
  Serial.println("Pin handled");

  File configJson = SPIFFS.open("/config.json", "r");
  StaticJsonDocument<650> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, configJson);
  if (error) {
    Serial.println("Error while parsing actions");
  }

  JsonArray actions = jsonDoc["a"].as<JsonArray>();
  for(JsonVariant action : actions) {
    String url = action["url"].as<String>();
    Serial.println(url);
  }

  int action = -1;

  //handle url args
  if(server.hasArg("a")) { 
    action = server.arg("a").toInt();
  } else {  
    Serial.println("invalid reequest, not handleable");
    return;
  }
  
  switch (action)
  {
  default:
  case 0:
    /* Handle action 0 */
    greenValue = !greenValue;
    analogWrite(GREEN, greenValue ? 255 : 0);
    break;
  
  case -1:
    return;
  }

  server.send(200, "text/html", "pin handled");
}

void handleFileUpload() {
  Serial.println("on file upload");
  HTTPUpload &upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    //create if no exists
    fsUploadFile = SPIFFS.open("/config.json", "w");
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (fsUploadFile) {
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) {     // If the file was successfully created
      fsUploadFile.close(); // Close the file again
      Serial.print("handleFileUpload Size: ");
      Serial.println(upload.totalSize);
      server.send(303);
    }
    else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

void handleRoot() {
  Serial.println("on root");
  File file = SPIFFS.open("/root.html", "r");
  server.streamFile(file, "text/html");
}

void StartAsAP() {
  //Access point configuration
  IPAddress apAddress = IPAddress(192, 168, 0, 1);
  IPAddress apGateWay = IPAddress(192, 168, 0, 1);
  IPAddress apMask    = IPAddress(255, 255, 255, 0);

  //start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apAddress, apGateWay, apMask);
  WiFi.softAP("vika", "VikaHostPassword");

  Serial.println("Started as AP");
}

void StartAsClient() {
  if (!SPIFFS.exists("/config.json")) {
    Serial.println("config.json does not exists");
    StartAsAP();
  } else {
    Serial.println("json config found");
  }

  File configJson = SPIFFS.open("/config.json", "r");
  StaticJsonDocument<650> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, configJson);

  if(error) {
    Serial.println("ERROR: bad config or deserialization error");
    StartAsAP();
    return;
  } else {
    Serial.println("config parsed");
  }

  const char *ssid     = jsonDoc["ws"].as<const char *>();
  const char *password = jsonDoc["wp"].as<const char *>();

  configJson.close();

  WiFi.mode(WIFI_STA);
  //WiFi.begin(ssid, password);
  WiFi.begin("EFOR_PRIVATE", "Efor69Consu!tants");

  Serial.println("Starting WiFi connection with: ");
  Serial.print(ssid);
  Serial.print(' ');
  Serial.println(password);

  int wifiTimeOut = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeOut != 500) { 
    delay(20);
    Serial.print(".");
    wifiTimeOut++;
  }
    
  Serial.println("Started as client");
  Serial.print("wifi connected with ip ");
  Serial.println(WiFi.localIP().toString());
}

void udpRead() {
  int packetSize = UDP.parsePacket();
  if(packetSize > 0) {
    Serial.printf("Received %d bytes from %s, port %d ", packetSize, UDP.remoteIP().toString().c_str(), UDP.remotePort());

    int len = UDP.read(incomingPacket, 255);
    if(len) {
      incomingPacket[len] = 0;
    }
    Serial.printf("UDP packet contents : %s\n", incomingPacket);
    Serial.println();

    //response / register to vika server
    Serial.println("Registering...");
    bool socketConnection = socket.connect(UDP.remoteIP().toString(), 80, "/");
    if(socketConnection) {
      Serial.println("  websocket connected");
      socket.send("heeyy");
    } else {
      Serial.println("  error while connecting websocket");
    }

    Serial.println("end reg");
  }
}

void setup()
{
  //tmp
  pinMode(GREEN, OUTPUT);

  Serial.begin(115200);
  SPIFFS.begin();

  //Will try to connect to given SSID, on failure starts as Access Point
  StartAsClient();
  Serial.println();

  server.on("/", handleNotFound);
  server.on("/handlePin", handlePin);
  server.on("/upload", handleFileUpload);
  server.onFileUpload(handleFileUpload);
  server.onNotFound(handleNotFound);

  server.serveStatic("/getConfig", SPIFFS, "/config.json");

  server.begin();

  analogWrite(GREEN, 0);

  if(!MDNS.begin("esp-" + WiFi.localIP().toString())) {
    Serial.println("Error setting up mDNS responder");
  }
  MDNS.addService("vika", "tcp", 4545);

  UDP.begin(localUDPPort);
}

void loop() {
  server.handleClient();
  MDNS.update();
  socket.poll();
  udpRead();
 }