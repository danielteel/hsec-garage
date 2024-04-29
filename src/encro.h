#pragma once
#include <stdint.h>

uint8_t* encrypt(const uint8_t* data, uint16_t dataLength, uint16_t& encryptedLength, const char* keyString);
uint8_t* decrypt(const uint8_t* data, uint16_t dataLength, uint16_t& decryptedLength, const char* keyString);