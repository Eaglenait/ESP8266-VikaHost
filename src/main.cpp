#include <Arduino.h>
#include <Fs.h>
#include "ArduinoJson.h"
#include "ArduinoJson/Deserialization/DeserializationError.hpp"

#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ArduinoWebSockets.h>
#include <WiFiClient.h>
#include "IPAddress.h"

#include <WiFiUdp.h>
#include <ESP8266mDNS.h>

File fsUploadFile;
ESP8266WebServer server(80);

const int PIN   = 14;
const int JSON_FILE_SIZE = 743;

enum ActionType { Toggle, Analog, Measure, Unknown};

class Action {
public:
  ActionType type;  
  int pin;
  int state = 0;
};

class VikaConfig {
public:
  String wifiSSID;
  String wifiPassword;
  Action actions[2];
};

VikaConfig *config = new VikaConfig();

void handleNotFound()
{
  Serial.println("on handle not found");
  server.send(404, "text/html", "404 Not Found");
}

//Handles a pin request
void handlePin() {
  Serial.println("handeling ping");

  File configJson = SPIFFS.open("/config.json", "r");
  StaticJsonDocument<JSON_FILE_SIZE> jsonDoc;
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
    Serial.println("ree: invalid request, not handleable");
    return;
  }
  
  switch (action)
  {
  default:
  case 0:
    /* Handle action 0 */
    break;
  
  case -1:
    return;
  }

  server.send(200, "text/html", "pin handled");
}

int validateConfig(File &f) {
  StaticJsonDocument<JSON_FILE_SIZE> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, f);
  if (error) {
    Serial.println("validation config : Error while parsing actions");
    return 1;
  }

  String ssid = jsonDoc["ws"].as<String>();
  String password = jsonDoc["wp"].as<String>();

  if(ssid || password) {
    Serial.println("validation config: wifi or password missing");
    return 2;
  }

  JsonArray actions = jsonDoc["a"].as<JsonArray>();
  if(actions.size() <= 0) {
    Serial.println("validation config: no actions");
    return 3;
  }

  //TODO : action validation ?

  return 0;
}

void handleFileUpload() {
  if(server.method() != HTTPMethod::HTTP_POST) {
    server.send(400, "text/plain", "only post");
    return;
  }
  
  Serial.println("on file upload");

  server.send(200,"text/plain", "handling");
  delay(100);

  HTTPUpload &upload = server.upload();
  File f;

  if (upload.status == UPLOAD_FILE_START) {
    Serial.println("upload file start");
    f.read(upload.buf, upload.currentSize);

    int validationCode = validateConfig(f);

    switch (validationCode)
    {
    case 1:
      server.send(400, "text/plain", "Bad config");
      return;

    case 2:
      server.send(400, "text/plain", "wifi or password missing");
      return;

    case 3:
      server.send(400, "text/plain", "no actions");
      return;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    Serial.println("upload file write");
    if (fsUploadFile) {
      fsUploadFile = SPIFFS.open("/config.json", "w");
      
      size_t written = fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
      Serial.println("config written " + written);
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    Serial.println("upload file end");
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

short parseConfig() {
  if (!SPIFFS.exists("/config.json")) {
    Serial.println("config.json does not exists");
    return 3;
  } else {
    Serial.println("json config found");
  }

  File configJson = SPIFFS.open("/config.json", "r");
  StaticJsonDocument<JSON_FILE_SIZE> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, configJson);

  if(error) {
    Serial.println("ERROR: bad config or deserialization error:");
    Serial.println(error.c_str());
    return 3;
  } else {
    Serial.println("config parsed");
  }

  String ws = jsonDoc["ws"].as<const char *>();
  String wp = jsonDoc["wp"].as<const char *>();
  config->wifiSSID = ws;
  config->wifiPassword = wp;

  JsonArray actions = jsonDoc["a"].as<JsonArray>();
  if(actions.isNull()) {
    return 2;
  } 

  for(size_t i = 0; i < actions.size(); i++) {
    config->actions[i].pin = actions[i]["pin"].as<int>();
    String type = actions[i]["type"].as<const char *>();
    if(type == "Toggle")
      config->actions[i].type = ActionType::Toggle;
    else if(type == "Measure")
      config->actions[i].type = ActionType::Measure;
    else if(type == "Analog")
      config->actions[i].type = ActionType::Analog;
    else
      config->actions[i].type = ActionType::Unknown;
  }

  return 0;
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
  Serial.println(WiFi.localIP());
}

void StartAsClient() {
  String ssid     = config->wifiSSID;
  String password = config->wifiPassword;

  Serial.println("Starting WiFi connection with: ");
  Serial.print(ssid);
  Serial.print(' ');
  Serial.println(password);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

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

void setup()
{
  Serial.begin(115200);
  SPIFFS.begin();

  short parseResult = parseConfig();
  switch(parseResult) {
  case 0:
    Serial.println("returns 0");
    StartAsClient();
    break;
  case 1:
    Serial.println("returns 1");
    StartAsAP();
    break;
  case 2:
    while (true) {
      Serial.println("bad config");
      delay(1000);
    }
    break;
  }

  //Will try to connect to given SSID, on failure starts as Access Point
  size_t actions = sizeof(config->actions) / sizeof(config->actions[0]);
  for(size_t i = 0; i < actions; i++) {
    int pin = config->actions[i].pin;
    ActionType type = config->actions[i].type;

    switch(type) {
    case Unknown:
    case Analog:
    case Toggle:
      pinMode(pin, OUTPUT);
      break;
    case Measure:
      pinMode(pin, INPUT);
      break;
    }
  }

  server.on("/", handleNotFound);
  server.on("/handlePin", handlePin);
  server.on("/upload", HTTPMethod::HTTP_POST, [](){server.send(200);}, handleFileUpload);
  server.onFileUpload(handleFileUpload);
  server.onNotFound(handleNotFound);

  server.serveStatic("/getConfig", SPIFFS, "/config.json");

  server.begin();

  if(!MDNS.begin("esp-" + WiFi.localIP().toString())) {
    Serial.println("Error setting up mDNS responder");
  }
  MDNS.addService("vika", "tcp", 4545);
}

void loop() {
  server.handleClient();
  delay(50);
  MDNS.update();
 }