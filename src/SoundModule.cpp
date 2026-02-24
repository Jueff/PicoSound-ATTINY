#include "SoundModule.h"

SoundModule::SoundModule(uint8_t serialPin)
{
    serial = new SoftwareSerialTX(serialPin);
    serial->begin(9600);
    mp3.begin(*serial);
}

void SoundModule::process(uint8_t cmd, uint8_t param1, uint8_t param2) 
{
    sendToMp3(cmd, param1, param2);
}

void SoundModule::setModuleType(uint8_t type) 
{
  mp3.setModuleType(type);
  //EEPROM.write(param1-1, param2-1);
}

void SoundModule::sendToMp3(uint8_t cmd, uint8_t param1, uint8_t param2)
{
    switch (cmd)
    {
        case DFPLAYER_NEXT:
        case DFPLAYER_PREV:
        case DFPLAYER_VOLUME_INCREASE:
        case DFPLAYER_VOLUME_DECREASE:
        case DFPLAYER_STANDBY:
        case DFPLAYER_RESET:
        case DFPLAYER_PLAYBACK:
        case DFPLAYER_PAUSE:
        case DFPLAYER_STOP_ADVERTISEMENT:
        case DFPLAYER_STOP:
        case DFPLAYER_NORMAL:
        case DFPLAYER_SET_RANDOM:
            mp3.sendCmd(cmd);
            break;
        case DFPLAYER_SET_VOLUME:
        case DFPLAYER_EQ:
        case DFPLAYER_SOURCE:
        case DFPLAYER_REPEAT:
        case DFPLAYER_SET_DAC:
        case DFPLAYER_SET_REPEAT_CURRENT:
            mp3.sendCmd(cmd, param1);
            break;
        case DFPLAYER_PLAY_TRACK:
        case DFPLAYER_SINGLE_REPEAT:
        case DFPLAYER_PLAY_MP3:
        case DFPLAYER_ADVERTISEMENT:
        case DFPLAYER_FOLDER_REPEAT:
            mp3.sendCmd(cmd, uint16_t((param1 << 5) + param2 + 1));
            break;
        case DFPLAYER_PLAY_FOLDER:
            mp3.sendCmd(cmd, param1 + 1, (uint16_t)(param2 + 1));
            break;
        case DFPLAYER_ADJUST_VOLUME:
            mp3.sendCmd(cmd, param1, param2);
            break;
        case DFPLAYER_PLAY_FOLDER_TRACK:
            mp3.sendCmd(0x14, uint16_t((((uint16_t)(param1 + 1)) << 12) | (param2)));
            break;
        case DFPLAYER_SET_MODULE_TYPE:
            mp3.setModuleType(param1);
            break;
    }
}
