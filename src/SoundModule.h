#pragma once
#include "DFPlayerMini.h"
#include "SoftwareSerialTX.h"

class SoundModule 
{
public:
    SoundModule(uint8_t serialPin);
    void process(uint8_t cmd, uint8_t param1, uint8_t param2);
    void setModuleType(uint8_t type);

private:
    DFPlayerMini mp3;
    SoftwareSerialTX* serial;
    void sendToMp3(uint8_t cmd, uint8_t param1, uint8_t param2);
};
