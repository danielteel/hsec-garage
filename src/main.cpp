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

void onPacket(uint8_t type, uint8_t* data, uint32_t dataLength){
    if (type==3){
        digitalWrite(14, HIGH);
        delay(250);
        digitalWrite(14, LOW);
    }
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

typedef enum {
    MAGIC1,
    MAGIC2,
    TYPE,
    LENHI,
    LENMID,
    LENLO,
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
            if (byte!=73){
                onError("magic1 byte is incorrect");
                return;
            }
            packetState=PACKETWRITESTATE::MAGIC2;
            break;
        case PACKETWRITESTATE::MAGIC2:
            if (byte!=31){
                onError("magic2 byte is incorrect");
                return;
            }
            packetState=PACKETWRITESTATE::TYPE;
            break;
        case PACKETWRITESTATE::TYPE:
            if (!haveRecievedServerHandshakeNumber && byte!=0){
                onError("first packet needs to be initial handshake packet");
                return;
            }
            packetType=byte;
            packetState=PACKETWRITESTATE::LENHI;
            break;
        case PACKETWRITESTATE::LENHI:
            packetLength=(uint32_t)byte<<16;
            packetState=PACKETWRITESTATE::LENMID;
            break;
        case PACKETWRITESTATE::LENMID:
            packetLength|=(uint32_t)byte<<8;
            packetState=PACKETWRITESTATE::LENLO;
            break;
        case PACKETWRITESTATE::LENLO:
            packetLength|=byte;
            if (packetPayload){
                delete[] packetPayload;
                packetPayload=nullptr;
            }
            packetPayload=new uint8_t[packetLength];//need to clean this up on an error
            packetState=PACKETWRITESTATE::PAYLOAD;
            break;
        case PACKETWRITESTATE::PAYLOAD:
            packetPayload[packetPayloadWriteIndex]=byte;
            packetPayloadWriteIndex++;
            if (packetPayloadWriteIndex>=packetLength){
                uint32_t decryptedLength;
                uint8_t* decrypted = decrypt(packetPayload, packetLength, decryptedLength, keyString);
                delete[] packetPayload;
                packetPayload=nullptr;
                if (packetType==0){
                    if (haveRecievedServerHandshakeNumber){
                        //Error, we already recieved the handshake number, this can only happen at the beginning of each connection
                        onError("handshake packet was already recieved");
                        delete[] decrypted;
                        return;
                    }else{
                        if (decryptedLength!=4){
                            //Error, handshake packet needs to be 4 bytes
                            onError("initial recieved handshake packet needs to be 4 bytes long");
                            delete[] decrypted;
                            return;
                        }else{
                            serverHandshakeNumber=(decrypted[0]<<24) | (decrypted[1]<<16) | (decrypted[2]<<8) | decrypted[3];
                            haveRecievedServerHandshakeNumber=true;
                        }
                    }

                }else{
                    //Send off decrypted packet for processing
                    uint32_t recvdServerHandshakeNumber=(decrypted[0]<<24) | (decrypted[1]<<16) | (decrypted[2]<<8) | decrypted[3];
                    if (recvdServerHandshakeNumber==serverHandshakeNumber){
                        serverHandshakeNumber++;
                        onPacket(packetType, decrypted+4, decryptedLength-4);
                    }else{
                        onError("incorrect handshake number recieved");
                        delete[] decrypted;
                        return;
                    }
                }
                delete[] decrypted;
                packetState=PACKETWRITESTATE::MAGIC1;
            }
            break;
    }
}

void loop(){
    static uint32_t lastCaptureTime=0;

    if (!Messaging.connected()){
        Messaging.connect("192.168.50.178", 4004);
        sendInitialHandshake();
        sendPacket(1, "Garage", 6);
    }else{
        while (Messaging.available()){
            byte message;
            Messaging.readBytes(&message, 1);
            dataRecieved(message);
        }

        uint32_t currentTime = millis();
        if ((currentTime-lastCaptureTime)>=2000 || currentTime<lastCaptureTime){
            CAMERA_CAPTURE capture;
            if (cameraCapture(capture)){
                sendPacket(2, capture.jpgBuff, capture.jpgBuffLen);
                cameraCaptureCleanup(capture);
            }
            else{
                Serial.println("failed to capture ");
            }
            lastCaptureTime=currentTime;
        }
    }
}
