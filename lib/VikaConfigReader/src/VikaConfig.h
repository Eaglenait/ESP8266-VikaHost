#include <Arduino.h>

class VikaConfig
{
    private:
        char* _wifiSSID;
        char* _wifiPassword;

    public:
        const char* wifiSSID() const {return _wifiSSID; }
        void wifiSSID(const String ssid) { strcpy(_wifiSSID, ssid.c_str); }

        const char* wifiPassword() const {return _wifiPassword; }
        void wifiPassword(const String password) { strcpy(_wifiPassword, password.c_str); }
};
