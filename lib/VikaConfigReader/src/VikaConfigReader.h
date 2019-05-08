#include <ArduinoJson.h>
#include "VikaConfig.h"
#include <FS.h>

#define JSON_SIZE 650

/*
{
    "ws":"wifi ssid",
    "wp":"wifi password",
    "a":[
        {                        a[0]
            "url":"url d'accès",
            "io":[1,2,3],        io à déclencher
            "v":["1","2"],       verbes compatibles (sans synonymes)
            "loc":"cuisine",     localisation
            "obj":"lumiere"      nom d'objet      
        }
    ]
}
*/

class VikaConfigReader
{
private:
public:
    VikaConfig config;

    void ReadConfig();
};

bool VikaConfigReader::ReadConfig() {
    StaticJsonDocument<JSON_SIZE> jsonDoc;
    if(SPIFFS.exists("/config.json")) {
        return false;
    }

    File config = SPIFFS.open("/config.json", "r");
    DeserializationError error = deserializeJson(jsonDoc, json);
    if(error) {
        return false;        
    }

    config.wifiSSID = jsonDoc["ws"];
    config.wifiPassword = jsonDoc["wp"];

}