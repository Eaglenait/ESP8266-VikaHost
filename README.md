# ESP8266-VikaHost

Quoi ça sert
-Serveur REST pour ESP8266
-Nocode only config
-Post to set pin state
-Get to get pin state
-Starts as Access point if bad config
-config upload while running doesn't work
-mdns service discovery (defaultname esp-currentip)
                        (default service vika tcp:4545)

Urls:
/getConfig 
  returns the json config to be parsed by your client
  
/handlePin?a=0 -> POST  -> will toggle the pin (if toggle type)
/handlePin?a=0&v=255 -> POST -> Will set the pin to the value v (if analog type) 
  will try to handle the action you pass as arg (here action 0)
  actions are in the config file
answer json
  {"value":"val"}
  Reads value of pin after setting it
  
/handlePin?a=0 -> GET
  returns pin state as json : {"value":"val"}
  
Config JSON
ws = wifi ssid
wp = wifi password
a = actions
each actions has
  desc = description
  type = type of signal that is awaited
          can be analog (accepts 0-255 value) or Toggle (on off)
  (optional config that can be parsed in other clients)
  vrb = verb that can be used to control that action
  obj = object names
  loc = localisation
  pin = what pin on the esp to drive (and autoconfigure)
  
Max number of actions is defined in https://github.com/Eaglenait/ESP8266-VikaHost/blob/c241fee319308037770e286fe534e402c5968103/src/main.cpp#L18
  
