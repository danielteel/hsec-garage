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

const uint8_t packetMagicBytes[]={73, 31};

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

    delay(500);
    pinMode(14, OUTPUT);
    digitalWrite(14, LOW);

    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(handshakeNumber, nullptr, 0, encryptedLength, keyString);
    if (encrypted){
        uint32_t decryptedHandshake;
        uint32_t decryptedLength;
        uint8_t* decrypted=decrypt(decryptedHandshake, encrypted, encryptedLength, decryptedLength, keyString);
        delete[] encrypted;
        
        if (decryptedHandshake==handshakeNumber && decryptedLength==0){
            Serial.println("Everything looks good");
        }else{
            if (decryptedHandshake!=handshakeNumber){
                Serial.println("Handshakes bad");
            }
            if (decryptedLength!=0){
                Serial.println("decrypted length bad");
            }
        }
            
        if (decrypted) delete[] decrypted;
    }else{
        Serial.println("failed to encrypt");
    }
}

void onPacket(uint8_t type, uint8_t* data, uint32_t dataLength){
    if (type==3){
        Serial.println("recieved garage door press command");
        digitalWrite(14, HIGH);
        delay(250);
        digitalWrite(14, LOW);
    }
}

void sendInitialHandshake(){
    uint32_t encryptedLength;
    const uint8_t handshakeMessage=0;
    uint8_t* encrypted=encrypt(handshakeNumber, &handshakeMessage, 0, encryptedLength, keyString);
    if (encrypted){
        Messaging.write(packetMagicBytes, 2);
        Messaging.write((uint8_t*)&encryptedLength, 4);
        Messaging.write(encrypted, encryptedLength);
        delete[] encrypted;
    }else{
        Serial.println("failed to encrypt in sendInitialHandshake()");
    }
}

void sendPacket(const void* data, uint32_t dataLength){
    uint32_t encryptedLength;
    uint8_t* encrypted=encrypt(handshakeNumber, (const uint8_t*)data, dataLength, encryptedLength, keyString);
    if (encrypted){
        Messaging.write(packetMagicBytes, 2);
        Messaging.write((uint8_t*)&encryptedLength, 4);
        Messaging.write(encrypted, encryptedLength);
        delete[] encrypted;
        handshakeNumber++;
    }else{
        Serial.println("failed to encrypt in sendPacket()");
    }
}

typedef enum {
    MAGIC1,
    MAGIC2,
    LEN1,
    LEN2,
    LEN3,
    LEN4,
    PAYLOAD
} PACKETWRITESTATE;

PACKETWRITESTATE packetState=PACKETWRITESTATE::MAGIC1;

uint8_t packetType=0;
uint32_t packetLength=0;
uint8_t* packetPayload = nullptr;
uint32_t packetPayloadWriteIndex = 0;
bool haveRecievedServerHandshakeNumber=false;


void onError(const char* errorMsg){
    if (errorMsg){
        Serial.print("Error: ");
        Serial.println(errorMsg);
    }else{
        Serial.println("Error occured");
    }
    Messaging.stop();
    if (packetPayload){
        delete[] packetPayload;
        packetPayload=nullptr;
    }
    packetState=PACKETWRITESTATE::MAGIC1;
    packetType=0;
    packetLength=0;
    packetPayloadWriteIndex=0;
    haveRecievedServerHandshakeNumber=false;
    serverHandshakeNumber=0;
}

void dataRecieved(uint8_t byte){
    switch (packetState){
        case PACKETWRITESTATE::MAGIC1:
            if (byte!=packetMagicBytes[0]){
                onError("magic1 byte is incorrect");
                return;
            }
            packetState=PACKETWRITESTATE::MAGIC2;
            break;
        case PACKETWRITESTATE::MAGIC2:
            if (byte!=packetMagicBytes[1]){
                onError("magic2 byte is incorrect");
                return;
            }
            packetState=PACKETWRITESTATE::LEN1;
            break;
        case PACKETWRITESTATE::LEN1:
            memmove(((uint8_t*)&packetLength)+0, &byte, 1);
            packetState=PACKETWRITESTATE::LEN2;
            break;
        case PACKETWRITESTATE::LEN2:
            memmove(((uint8_t*)&packetLength)+1, &byte, 1);
            packetState=PACKETWRITESTATE::LEN3;
            break;
        case PACKETWRITESTATE::LEN3:
            memmove(((uint8_t*)&packetLength)+2, &byte, 1);
            packetState=PACKETWRITESTATE::LEN4;
            break;
        case PACKETWRITESTATE::LEN4:
            memmove(((uint8_t*)&packetLength)+3, &byte, 1);
            if (packetPayload){
                delete[] packetPayload;
                packetPayload=nullptr;
            }
            packetPayload=new uint8_t[packetLength];//need to clean this up on an error
            packetState=PACKETWRITESTATE::PAYLOAD;
            packetPayloadWriteIndex=0;
            break;
        case PACKETWRITESTATE::PAYLOAD:

            packetPayload[packetPayloadWriteIndex]=byte;

            packetPayloadWriteIndex++;

            if (packetPayloadWriteIndex>=packetLength){
                uint32_t decryptedLength;
                uint32_t recvdServerHandshakeNumber;
                uint8_t* decrypted = decrypt(recvdServerHandshakeNumber, packetPayload, packetLength, decryptedLength, keyString);
                delete[] packetPayload;
                packetPayload=nullptr;
                if (decrypted){
                    if (!haveRecievedServerHandshakeNumber){
                        serverHandshakeNumber=recvdServerHandshakeNumber;
                        haveRecievedServerHandshakeNumber=true;
                    }

                    //Send off decrypted packet for processing
                    if (recvdServerHandshakeNumber==serverHandshakeNumber){
                        serverHandshakeNumber++;
                        onPacket(packetType, decrypted, decryptedLength);
                    }else{
                        onError("incorrect handshake number recieved");
                        Serial.printf("Recvd: %u  Expected: %u\n", recvdServerHandshakeNumber, serverHandshakeNumber);
                        delete[] decrypted;
                        return;
                    }
                    
                    delete[] decrypted;
                }else{
                    Serial.println("failed to decrypt packet");
                }
                packetState=PACKETWRITESTATE::MAGIC1;
            }
            break;
    }
}

void loop(){
    static uint32_t lastCaptureTime=0;
    static uint32_t lastConnectAttemptTime=0;

    uint32_t currentTime = millis();

    if (!Messaging.connected() && ((currentTime-lastConnectAttemptTime)>=2000 || currentTime<lastConnectAttemptTime)){ 
        lastConnectAttemptTime=currentTime;
        if (packetPayload){
            delete[] packetPayload;
            packetPayload=nullptr;
        }
        if (Messaging.connect("192.168.50.178", 4004)){
            sendInitialHandshake();
            sendPacket("Garage", 6);
        }
    }else if (Messaging.connected()){
        while (Messaging.connected() && Messaging.available()){
            byte message;
            Messaging.readBytes(&message, 1);
            dataRecieved(message);
        }

        if ((currentTime-lastCaptureTime)>=2000 || currentTime<lastCaptureTime){
            CAMERA_CAPTURE capture;
            if (cameraCapture(capture)){
                if (Messaging.connected()) sendPacket(capture.jpgBuff, capture.jpgBuffLen);
                cameraCaptureCleanup(capture);
            }else{
                Serial.println("failed to capture ");
            }
            lastCaptureTime=currentTime;
        }
    }
}
