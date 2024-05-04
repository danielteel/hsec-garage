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

uint32_t handshakeNumber=0;
uint32_t serverHandshakeNumber=0;

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

    handshakeNumber=esp_random();

    Serial.println("Auto connect returned...");
    cameraSetup();
    Serial.println("camera setup...");

    delay(2000);
    pinMode(14, OUTPUT);
    digitalWrite(14, LOW);
}

void sendInitialHandshake(){
    Messaging.write((unsigned char)73);
    Messaging.write((unsigned char)31);
    Messaging.write((unsigned char)0);
    uint32_t encryptedLength;
    uint8_t handshakeBytes[4];
    handshakeBytes[0]=(handshakeNumber>>24) & 0xFF;
    handshakeBytes[1]=(handshakeNumber>>16) & 0xFF;
    handshakeBytes[2]=(handshakeNumber>>8) & 0xFF;
    handshakeBytes[3]=handshakeNumber & 0xFF;
    uint8_t* encrypted=encrypt((const uint8_t*)&handshakeBytes, 4, encryptedLength, keyString);
    Messaging.write((unsigned char)(encryptedLength>>16) & 0xFF);
    Messaging.write((unsigned char)(encryptedLength>>8) & 0xFF);
    Messaging.write((unsigned char)encryptedLength & 0xFF);
    Messaging.write(encrypted, encryptedLength);
}

void sendPacket(uint8_t type, const void* data, uint32_t dataLength){
    uint8_t* buffer = new uint8_t[dataLength+4];
    uint8_t handshakeBytes[4];
    handshakeBytes[0]=(handshakeNumber>>24) & 0xFF;
    handshakeBytes[1]=(handshakeNumber>>16) & 0xFF;
    handshakeBytes[2]=(handshakeNumber>>8) & 0xFF;
    handshakeBytes[3]=handshakeNumber & 0xFF;
    memmove(buffer, (const uint8_t*)&handshakeBytes, 4);
    memmove(buffer+4, data, dataLength);

    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt((const uint8_t*)buffer, dataLength+4, encryptedLength, keyString);
    Messaging.write((uint8_t)73);
    Messaging.write((uint8_t)31);
    Messaging.write(type);
    Messaging.write((uint8_t)(encryptedLength>>16) & 0xFF);
    Messaging.write((uint8_t)(encryptedLength>>8) & 0xFF);
    Messaging.write((uint8_t)encryptedLength & 0xFF);
    Messaging.write(encrypted, encryptedLength);

    delete[] encrypted;
    delete[] buffer;

    handshakeNumber++;
}

void loop(){
    if (!Messaging.connected()){
        Messaging.connect("192.168.50.178", 4004);
        sendInitialHandshake();
        sendPacket(1, "Garage", 6);
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
            sendPacket(2, capture.jpgBuff, capture.jpgBuffLen);
            cameraCaptureCleanup(capture);
        }
        else{
            Serial.println("failed to capture ");
        }
    }
    delay(2000);
}
