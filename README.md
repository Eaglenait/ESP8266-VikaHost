# ESP8266-VikaHost

## À quoi ça sert

- Serveur REST pour ESP8266
- Nocode only config
- Post to set pin state
- Get to get pin state
- Starts as Access point if bad config
- config upload while running doesn't work
- mdns service discovery (defaultname esp-currentip)

(default service vika tcp:4545)

## URLs:

```
/getConfig
  returns the json config to be parsed by your client
  
/handlePin?a=0 -> POST
  Will toggle the pin (if toggle type)

/handlePin?a=0&v=255 -> POST
  Will set the pin to the value v (if analog type)
  will try to handle the action you pass as arg (here action 0)
  actions are in the config file
  answer json
  {"value":"val"}
  Reads value of pin after setting it

/handlePin?a=0 -> GET
  Returns pin state as json : {"value":"val"}
```

## JSON config

### Example
```javascript
{
  "ws": "",              // WiFi SSID
	"wp": "",              // WiFi passwd
	"a": [                 // List containing actions
    {
      "desc": "",        // Action Description
			"type": "",        // Action type {toggle,analog(0-255)}
      // optional 
			"vrb": [],         // Verb that can used to control this action
			"obj": [],         // Object names
			"adj": [],                      
			"loc": "chambre",  // Localisation
			"pin": 4           // Which pin on the esp to drive (and autoconfigure)
    }
	]
}
```

Max number of actions is defined in 
https://github.com/Eaglenait/ESP8266-VikaHost/blob/c241fee319308037770e286fe534e402c5968103/src/main.cpp#L18
  
Max Length of config is set here
https://github.com/Eaglenait/ESP8266-VikaHost/blob/c241fee319308037770e286fe534e402c5968103/src/main.cpp#L19

