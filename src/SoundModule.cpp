#include "SoundModule.h"

#define FLASHBLOCK_TYPE_MODULE_TYPE   0xEF
FlashStorage* SoundModule::pStorage = NULL;

SoundModule::SoundModule(uint8_t serialPin)
{
  serial = new SoftwareSerialTX(serialPin);
  serial->begin(9600);
  this->serialPin = serialPin;

  FlashBlock fbs;
  uint8_t moduleType;
  if (pStorage->getBlock(fbs, FLASHBLOCK_TYPE_MODULE_TYPE, serialPin))
  {
    moduleType = fbs.getData(0);
    if (moduleType == MODULE_MP3_TF_16P || moduleType == MODULE_JQ6500)
    {
      Serial.printf("%d: module type is %d\n", serialPin, moduleType);
    }
    else
    {
      Serial.printf("%d: invalid module type %d, using default\n", serialPin, moduleType);
      moduleType = MODULE_MP3_TF_16P;
    }
  }
  else
  {
    Serial.printf("%d: can't read module type, using default\n", serialPin);
    moduleType = MODULE_MP3_TF_16P;
  }
  mp3.setModuleType(moduleType);
  mp3.begin(*serial);
}

void SoundModule::init(FlashStorage* pStorage)
{
  SoundModule::pStorage = pStorage;
}

void SoundModule::process(uint8_t cmd, uint8_t param1, uint8_t param2) 
{
    sendToMp3(cmd, param1, param2);
}

void SoundModule::setModuleType(uint8_t type) 
{
  mp3.setModuleType(type);
  Serial.printf("%d: saving module type %d\n", serialPin, type);

  FlashWriteBlock fwb(FLASHBLOCK_TYPE_MODULE_TYPE, serialPin);
  fwb.setData(type, 0);
  if (!pStorage->write(&fwb))
  {
    Serial.printf("%d: error saving module type %d\n", serialPin, type);
    // todo error handling
  }
  else
  {
    Serial.printf("%d: module type %d saved\n", serialPin, type);
  }
}

void SoundModule::sendToMp3(uint8_t cmd, uint8_t param1, uint8_t param2)
{
    switch (cmd)
    {
        case PLAYER_NEXT:
        case PLAYER_PREV:
        case PLAYER_VOLUME_INCREASE:
        case PLAYER_VOLUME_DECREASE:
        case PLAYER_STANDBY:
        case PLAYER_RESET:
        case PLAYER_PLAYBACK:
        case PLAYER_PAUSE:
        case PLAYER_STOP_ADVERTISEMENT:
        case PLAYER_STOP:
        case PLAYER_NORMAL:
        case PLAYER_SET_RANDOM:
            mp3.sendCmd(cmd);
            break;
        case PLAYER_SET_VOLUME:
        case PLAYER_EQ:
        case PLAYER_SOURCE:
        case PLAYER_REPEAT:
        case PLAYER_SET_DAC:
        case PLAYER_SET_REPEAT_CURRENT:
            mp3.sendCmd(cmd, param1);
            break;
        case PLAYER_PLAY_TRACK:
        case PLAYER_SINGLE_REPEAT:
        case PLAYER_PLAY_MP3:
        case PLAYER_ADVERTISEMENT:
        case PLAYER_FOLDER_REPEAT:
            mp3.sendCmd(cmd, uint16_t((param1 << 5) + param2 + 1));
            break;
        case PLAYER_PLAY_FOLDER:
            mp3.sendCmd(cmd, param1 + 1, (uint16_t)(param2 + 1));
            break;
        case PLAYER_ADJUST_VOLUME:
            mp3.sendCmd(cmd, param1, param2);
            break;
        case PLAYER_PLAY_FOLDER_TRACK:
            mp3.sendCmd(0x14, uint16_t((((uint16_t)(param1 + 1)) << 12) | (param2)));
            break;
        case PLAYER_SET_MODULE_TYPE:
            setModuleType(param1);
            break;
    }
}
