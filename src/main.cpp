#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <Esp.h>
#include "camera.h"
WiFiManager wifiManager;
WiFiClient Client;
WiFiClient Messaging;

void setup(){
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

    delay(2000);
    pinMode(14, OUTPUT);
    digitalWrite(14, LOW);
}

void loop(){
    if (!Messaging.connected()){
        Messaging.connect("192.168.50.178", 4004);
    }else{
        while (Messaging.available()){
            byte message;
            Messaging.readBytes(&message, 1);
            if (message=='1'){
                digitalWrite(14, HIGH);
                delay(1000);
                digitalWrite(14, LOW);
                Serial.println("Garage button pressed");
            }else{
                Serial.println("Unknown command");
            }
        }
        CAMERA_CAPTURE capture;
        if (cameraCapture(capture)){
            Serial.println("captured ");
            Messaging.write(73);
            Messaging.write(31);
            Messaging.write(1);
            Messaging.write((capture.jpgBuffLen & 0xFF0000)>>16);
            Messaging.write((capture.jpgBuffLen & 0xFF00)>>8);
            Messaging.write( capture.jpgBuffLen & 0xFF);
            Messaging.write(capture.jpgBuff, capture.jpgBuffLen);
            cameraCaptureCleanup(capture);
        }
        else{
            Serial.println("failed to capture ");
        }
    }
    delay(2000);
}
