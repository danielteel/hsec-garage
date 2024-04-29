#include <Arduino.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <Esp.h>
#include "camera.h"
#include "encro.h"
WiFiManager wifiManager;
WiFiClient Client;
WiFiClient Messaging;

const char* keyString = "4c97d02ae05b748dcb67234065ddf4b8f832a17826cf44a4f90a91349da78cba";

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
        
        const char* stringToEncrypt = "Garage";
        uint32_t encryptedLength;
        uint8_t* encrypted=encrypt((const uint8_t*)stringToEncrypt, strlen(stringToEncrypt), encryptedLength, keyString);
        Messaging.write((unsigned char)73);
        Messaging.write((unsigned char)31);
        Messaging.write((unsigned char)0);
        Messaging.write((unsigned char)(encryptedLength>>16) & 0xFF);
        Messaging.write((unsigned char)(encryptedLength>>8) & 0xFF);
        Messaging.write((unsigned char)encryptedLength & 0xFF);
        Messaging.write(encrypted, encryptedLength);
        delete[] encrypted;
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
            uint32_t encryptedLength;
            uint8_t* encrypted=encrypt((const uint8_t*)capture.jpgBuff, capture.jpgBuffLen, encryptedLength, keyString);
            Messaging.write((unsigned char)73);
            Messaging.write((unsigned char)31);
            Messaging.write((unsigned char)1);
            Messaging.write((unsigned char)(encryptedLength>>16) & 0xFF);
            Messaging.write((unsigned char)(encryptedLength>>8) & 0xFF);
            Messaging.write((unsigned char)encryptedLength & 0xFF);
            Messaging.write(encrypted, encryptedLength);
            delete[] encrypted;
            cameraCaptureCleanup(capture);
        }
        else{
            Serial.println("failed to capture ");
        }
    }
    delay(250);
}
