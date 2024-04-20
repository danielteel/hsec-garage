#include <Arduino.h>
#include <WiFiManager.h> 
#include "camera.h"
WiFiManager wifiManager;



void setup() {
  WiFiManagerParameter deviceNameParameter("name", "Device Name", "garage", 31);
  wifiManager.addParameter(&deviceNameParameter);

  WiFiManagerParameter hsecIPParameter("hsecIP", "HSEC IP Address", "192.168.1.14", 31);
  wifiManager.addParameter(&hsecIPParameter);

  wifiManager.setConfigPortalTimeout(60);
  wifiManager.setConnectTimeout(60);
  wifiManager.autoConnect("DnD", "powerboner69");

  cameraSetup();
}

void loop() {
    CAMERA_CAPTURE capture;
    if (cameraCapture(capture)){
        //do stuff with capture.jpgBuff / jpgBuffLen
        cameraCaptureCleanup(capture);
    }
    
    delay(2000);
}
