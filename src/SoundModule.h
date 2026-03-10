#pragma once
#include "MultiPlayer.h"
#include "SoftwareSerialTX.h"
#include "PicoFlashStorage.h"

using namespace PicoFlashStorage;

class SoundModule 
{
public:
    SoundModule(uint8_t serialPin);
    void process(uint8_t cmd, uint8_t param1, uint8_t param2);
    void setModuleType(uint8_t type);
    static void init(FlashStorage* pStorage);

private:
    MultiPlayer mp3;
    uint8_t serialPin;
    SoftwareSerialTX* serial;
    static FlashStorage* pStorage;
    void sendToMp3(uint8_t cmd, uint8_t param1, uint8_t param2);
};
