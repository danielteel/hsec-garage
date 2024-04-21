#include <Arduino.h>
#include <WiFiManager.h> 
#include <WiFi.h>
#include <Esp.h>
#include "camera.h"
WiFiManager wifiManager;
WiFiClient Client;


void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");

  WiFiManagerParameter deviceNameParameter("name", "Device Name", "garage", 31);
  wifiManager.addParameter(&deviceNameParameter);

  WiFiManagerParameter hsecIPParameter("hsecIP", "HSEC IP Address", "192.168.1.14", 31);
  wifiManager.addParameter(&hsecIPParameter);

  wifiManager.setConfigPortalTimeout(60);
  wifiManager.setConnectTimeout(60);
  wifiManager.autoConnect("HSEC Dev Setup", "powerboner69");

  Serial.println("Auto connect returned...");
  cameraSetup();
  Serial.println("camera setup...");
}

void loop() {
  
    if (Client.connect("192.168.50.178", 3000)){
      CAMERA_CAPTURE capture;
      if (cameraCapture(capture)){
          //do stuff with capture.jpgBuff / jpgBuffLen
          Client.println("POST / HTTP/1.0");
          Client.println("Host: 192.168.50.178:3000");
          Client.println("Content-Length: "+String(capture.jpgBuffLen));
          Client.println("Content-Type: image/jpeg");
          Client.println("Connection: close");
          Client.println();
          Client.write(capture.jpgBuff, capture.jpgBuffLen);
          cameraCaptureCleanup(capture);
          Serial.print("captured ");
      }else{
        Serial.print("failed to capture ");
      }
      
      Serial.print(ESP.getFreeHeap());
      Serial.print(" ");
      Serial.println(ESP.getFreePsram());
      Client.stop();
    }
    delay(2000);
}
