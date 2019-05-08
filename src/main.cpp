#include <Arduino.h>
#include <ESP8266WebServer.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266mDNS.h>
#include <Fs.h>

#include "VikaConfigReader.h"
/*
  Serveur web ou l'on peut envoyer une config qui sera stockée dans le FS
*/
File fsUploadFile;
ESP8266WebServer server(80);

void handleNotFound(){
  server.send(404, "text/html", "404 Not Found");
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    Serial.print("handleFileUpload Name: "); Serial.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");            // Open the file for writing in SPIFFS (create if it doesn't exist)
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE) {
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize); // Write the received bytes to the file
  } else if(upload.status == UPLOAD_FILE_END) {
    if(fsUploadFile) {                                    // If the file was successfully created
      fsUploadFile.close();                               // Close the file again
      Serial.print("handleFileUpload Size: "); Serial.println(upload.totalSize);
      server.sendHeader("Location","/success.html");      // Redirect the client to the success page
      server.send(303);
    } else {
      server.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
  
void handleRoot() {
  File file = SPIFFS.open("/root.html", "r");
  server.streamFile(file, "text/html");
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  SPIFFS.begin();

  //TODO : IF CONFIG READ SSID+PASSWD = FAIL
  WiFi.softAP("esp", "pass");
  IPAddress myIp = WiFi.softAPIP();
  Serial.print("IP ");
  Serial.println(myIp);

  server.on("/", handleRoot);
  server.onFileUpload(handleFileUpload);

  server.begin();
  Serial.println("Server Started");
} 

void loop() {
  server.handleClient();
}