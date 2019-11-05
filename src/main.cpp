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

#define MAX_ACTIONS     2
#define JSON_FILE_SIZE  743
#define RED_PIN         15
#define BLUE_PIN        12
#define GREEN_PIN       13

//New vars
//  Sleep time
//  Deep sleep hour 
//  Deep sleep duration

enum ActionType { Toggle, Analog, Measure, Unknown };

class Action
{
public:
  ActionType type;
  int pin;
  int state;
};

class VikaConfig
{
public:
  String wifiSSID;
  String wifiPassword;
  Action actions[MAX_ACTIONS];
};

VikaConfig *config = new VikaConfig();

void handleNotFound()
{
  Serial.println("on handle not found");
  server.send(404, "text/html", "404 Not Found");
}

//Handles a pin request
void handlePin()
{
  Serial.println("handeling pin");

  int action = -1;

  Serial.println("");

  //handle url args
  if (server.hasArg("a"))
  {
    action = server.arg("a").toInt();
  }
  else
  {
    Serial.println("ree: invalid request, not handleable");
    return;
  }

  String reply = "{\"value\":";
  if (action >= 0 && action <= MAX_ACTIONS)
  {
    int state = config->actions[action].state;
    int pin   = config->actions[action].pin;

    switch (config->actions[action].type)
    {
    case Toggle:
      if (server.method() == HTTP_POST)
      {
        if (state == 0)
        {
          digitalWrite(pin, HIGH);
          config->actions[action].state = 255;
          reply += "255}";
        }
        else
        {
          digitalWrite(pin, LOW);
          config->actions[action].state = 0;
          reply += "0}";
        }
      }
      else if (server.method() == HTTP_GET)
      {
        reply += state;
        reply += '}';
      }
      break;

    case Analog:
      if (server.method() == HTTP_POST)
      {
        if (server.hasArg("v"))
        {
          uint value = server.arg("v").toInt();
          if (value > 0 || value < 255)
          {
            analogWrite(pin, value);
            reply += value;
            reply += '}';
          }
          else
          {
            server.send(400, "text/plain", "bad argument in request");
          }
        }
        else
        {
          server.send(400, "text/plain", "value argument missing in request");
        }
      }
      else if (server.method() == HTTP_GET)
      {
            reply += state;
            reply += '}';
      }
      break;

    default:
    case Measure:
    case Unknown:
      reply += state;
      reply += '}';
      break;
    }
  }

  server.send(200, "application/json", reply);
}

int validateConfig(File &f)
{
  StaticJsonDocument<JSON_FILE_SIZE> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, f);
  if (error)
  {
    Serial.println("validation config : Error while parsing actions");
    return 1;
  }

  String ssid = jsonDoc["ws"].as<String>();
  String password = jsonDoc["wp"].as<String>();

  if (ssid || password)
  {
    Serial.println("validation config: wifi or password missing");
    return 2;
  }

  JsonArray actions = jsonDoc["a"].as<JsonArray>();
  if (actions.size() <= 0)
  {
    Serial.println("validation config: no actions");
    return 3;
  }

  //TODO : action validation ?

  return 0;
}

void handleFileUpload()
{
  if (server.method() != HTTPMethod::HTTP_POST)
  {
    server.send(400, "text/plain", "only post");
    return;
  }

  Serial.println("on file upload");

  server.send(200, "text/plain", "handling");
  delay(100);

  HTTPUpload &upload = server.upload();
  File f;

  if (upload.status == UPLOAD_FILE_START)
  {
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
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    Serial.println("upload file write");
    if (fsUploadFile)
    {
      fsUploadFile = SPIFFS.open("/config.json", "w");

      size_t written = fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
      Serial.println("config written " + written);
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    Serial.println("upload file end");
    if (fsUploadFile)
    {                       // If the file was successfully created
      fsUploadFile.close(); // Close the file again
      Serial.print("handleFileUpload Size: ");
      Serial.println(upload.totalSize);
      server.send(303);
    }
    else
    {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}

short parseConfig()
{
  if (!SPIFFS.exists("/config.json"))
  {
    Serial.println("config.json does not exists");
    return 3;
  }

  File configJson = SPIFFS.open("/config.json", "r");
  StaticJsonDocument<JSON_FILE_SIZE> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, configJson);

  if (error)
  {
    Serial.println("ERROR: bad config or deserialization error:");
    Serial.println(error.c_str());
    return 3;
  }
  else
  {
    Serial.println("config parsed");
  }

  String ws = jsonDoc["ws"].as<const char *>();
  String wp = jsonDoc["wp"].as<const char *>();
  config->wifiSSID = ws;
  config->wifiPassword = wp;

  JsonArray actions = jsonDoc["a"].as<JsonArray>();
  if (actions.isNull())
  {
    return 2;
  }

  Serial.println("actions found : " + actions.size());
  for (size_t i = 0; i < actions.size(); i++)
  {
    config->actions[i].pin = actions[i]["pin"].as<int>();
    String type = actions[i]["type"].as<const char *>();
    if (type == "Toggle")
      config->actions[i].type = ActionType::Toggle;
    else if (type == "Measure")
      config->actions[i].type = ActionType::Measure;
    else if (type == "Analog")
      config->actions[i].type = ActionType::Analog;
    else
      config->actions[i].type = ActionType::Unknown;
  }

  return 0;
}

void handleRoot()
{
  Serial.println("on root");
  File file = SPIFFS.open("/root.html", "r");
  server.streamFile(file, "text/html");
}

void StartAsAP()
{
  //Access point configuration
  IPAddress apAddress = IPAddress(192, 168, 0, 1);
  IPAddress apGateWay = IPAddress(192, 168, 0, 1);
  IPAddress apMask = IPAddress(255, 255, 255, 0);

  //start in AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apAddress, apGateWay, apMask);
  WiFi.softAP("vika", "VikaHostPassword");

  Serial.println("Started as AP");
  Serial.println(WiFi.localIP());
}

void StartAsClient()
{
  String ssid = config->wifiSSID;
  String password = config->wifiPassword;

  Serial.println("Starting WiFi connection with: ");
  Serial.print(ssid);
  Serial.print(' ');
  Serial.println(password);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int wifiTimeOut = 0;
  while (WiFi.status() != WL_CONNECTED && wifiTimeOut != 500)
  {
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

  //parse config into memory
  short parseResult = parseConfig();
  switch (parseResult)
  {
  case 0:
    Serial.println("returns 0");
    StartAsClient();
    break;
  case 1:
    Serial.println("returns 1");
    StartAsAP();
    break;
  case 2:
    while (true)
    {
      Serial.println("bad config");
      delay(1000);
    }
    break;
  }

  size_t actions = sizeof(config->actions) / sizeof(config->actions[0]);
  for (size_t i = 0; i < actions; i++)
  {
    int pin = config->actions[i].pin;
    ActionType type = config->actions[i].type;

    //setup pins
    switch (type)
    {
    case Unknown:
    case Analog:
    case Toggle:
      //Hardware specific
      pinMode(pin, OUTPUT);
      digitalWrite(pin, LOW);
      break;
    case Measure:
      //Hardware specific
      pinMode(pin, INPUT);
      break;
    }
  }

  //useless pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);

  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);

  //http server config
  server.on("/", handleNotFound);
  server.on("/handlePin", handlePin);
  server.on("/upload", HTTPMethod::HTTP_POST, []() { server.send(200); }, handleFileUpload);
  server.onFileUpload(handleFileUpload);
  server.onNotFound(handleNotFound);

  server.serveStatic("/getConfig", SPIFFS, "/config.json");

  server.begin();

  if (!MDNS.begin("esp-" + WiFi.localIP().toString()))
  {
    Serial.println("Error setting up mDNS responder");
  }
  MDNS.addService("vika", "tcp", 4545);
}

void loop()
{
  //powerstate
  //
  //wait n min in light sleep
  //wake up - wait n min
  //poll date
  //if date close to night -> go into deep sleep for night duration
  //                          should be configurable 
  //else complete wait
  //prepare deepsleep (clean sleep)
  //what req has to be sent to be considered network alive

  server.handleClient();
  delay(50);
  MDNS.update();
}
