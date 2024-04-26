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
            // do stuff with capture.jpgBuff / jpgBuffLen
            //Client.println("POST / HTTP/1.0");
            //Client.println("Host: 192.168.50.178:3000");
            Messaging.println(String(capture.jpgBuffLen));
            Messaging.write(capture.jpgBuff, capture.jpgBuffLen);
            cameraCaptureCleanup(capture);
            Serial.println("captured ");
        }
        else{
            Serial.println("failed to capture ");
        }
    }
    // if (Client.connect("192.168.50.178", 3000)){
    //     CAMERA_CAPTURE capture;
    //     if (cameraCapture(capture)){
    //         // do stuff with capture.jpgBuff / jpgBuffLen
    //         Client.println("POST / HTTP/1.0");
    //         Client.println("Host: 192.168.50.178:3000");
    //         Client.println("Content-Length: " + String(capture.jpgBuffLen));
    //         Client.println("Content-Type: image/jpeg");
    //         Client.println("Connection: close");
    //         Client.println();
    //         Client.write(capture.jpgBuff, capture.jpgBuffLen);
    //         cameraCaptureCleanup(capture);
    //         Serial.println("captured ");
    //     }
    //     else{
    //         Serial.println("failed to capture ");
    //     }
    //     Client.stop();
    // }
    delay(2000);
}
