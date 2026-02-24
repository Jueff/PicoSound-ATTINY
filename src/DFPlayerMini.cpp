#define MP3_DEBUG
#include "DFPlayerMini.h"

uint16_t DFPlayerMini::calculateCheckSum(uint8_t *buffer){
  uint16_t sum = 0;
  for (int i=Stack_Version; i<Stack_CheckSum; i++) {
    sum += buffer[i];
  }
  return -sum;
}

void DFPlayerMini::disableACK(){
  _sending[Stack_ACK] = 0x00;
}

bool DFPlayerMini::begin(Stream &stream){
  _serial = &stream;
  
  disableACK();
  
  //reset();
  //waitAvailable(2000);
  //delay(200);

  return true;
}

void DFPlayerMini::sendStack() {
  #ifdef MP3_DEBUG
    Serial.printf("MP3-TF-16P sendStack\n");
  #endif
  _serial->write(_sending, DFPLAYER_SEND_LENGTH);
  _timeOutTimer = millis();
  _isSending = _sending[Stack_ACK];
  
  delay(10);
}

void DFPlayerMini::sendStack(uint8_t command){
  sendStack(command, 0);
}

void DFPlayerMini::sendStack(uint8_t command, uint16_t argument){
  #ifdef MP3_DEBUG
    Serial.printf("MP3-TF-16p command: %d larg %d\n", command, argument);
  #endif
  _sending[Stack_Command] = command;
  uint16ToArray(argument, _sending+Stack_Parameter);
  uint16ToArray(calculateCheckSum(_sending), _sending+Stack_CheckSum);
  sendStack();
}

void DFPlayerMini::sendStack(uint8_t command, uint8_t argumentHigh, uint8_t argumentLow){
  #ifdef MP3_DEBUG
    Serial.printf("MP3-TF-16p command: %d msb: %d lsb: %d\n", command, argumentHigh, argumentLow);
  #endif
  uint16_t buffer = argumentHigh;
  buffer <<= 8;
  sendStack(command, buffer | argumentLow);
}

void DFPlayerMini::uint16ToArray(uint16_t value, uint8_t *array){
  *array = (uint8_t)(value>>8);
  *(array+1) = (uint8_t)(value);
}

void DFPlayerMini::setModuleType(uint8_t moduleType) {
  _currentType = moduleType;
#ifdef MP3_DEBUG
  Serial.printf("Module type changed to: %d\n", _currentType);
#endif
}

void DFPlayerMini::sendCmd(uint8_t command) {
  #ifdef MP3_DEBUG
    Serial.printf("Module type: %d\n", _currentType);
  #endif
  switch (_currentType) {
    case MODULE_MP3_TF_16P:
      sendStack(command);
      break;
    case MODULE_JQ6500:
      sendJQ(command);
      break;
  }
}

void DFPlayerMini::sendCmd(uint8_t command, uint8_t argument) {
  #ifdef MP3_DEBUG
    Serial.printf("Module type: %d\n", _currentType);
  #endif
  switch (_currentType) {
    case MODULE_MP3_TF_16P:
      sendStack(command, argument);
      break;
    case MODULE_JQ6500:
      sendJQ(command, argument);
      break;
  }
}

void DFPlayerMini::sendCmd(uint8_t command, uint16_t argument) {
  #ifdef MP3_DEBUG
    Serial.printf("Module type: %d\n", _currentType);
  #endif
  switch (_currentType) {
    case MODULE_MP3_TF_16P:
      sendStack(command, argument);
      break;
    case MODULE_JQ6500:
      sendJQ(command, argument);
      break;
  }
}

void DFPlayerMini::sendCmd(uint8_t command, uint8_t argumentHigh, uint8_t argumentLow) {
  #ifdef MP3_DEBUG
    Serial.printf("Module type: %d\n", _currentType);
  #endif
  switch (_currentType) {
    case MODULE_MP3_TF_16P:
      sendStack(command, argumentHigh, argumentLow);
      break;
    /*case MODULE_JQ6500:
      sendJQ(command, argumentHigh, argumentLow);
      break;*/
  }
  
}

void DFPlayerMini::sendJQ(uint8_t command) {
  #ifdef MP3_DEBUG
    Serial.printf("JQ6500 cmd %d\n", command);
  #endif
  _sendJQ[1] = 2;
  _sendJQ[2] = command;
  _sendJQ[3] = 0xef;
  _serial->write(_sendJQ, 4);
}

uint8_t DFPlayerMini::mapJQ6500Command(uint8_t command)
{
  if (command == DFPLAYER_PLAY_MP3)
  {
    // map to normal play command
    command = DFPLAYER_PLAY_TRACK;
  }
  return command;
}

void DFPlayerMini::sendJQ(uint8_t command, uint8_t argument) {
  #ifdef MP3_DEBUG
    Serial.printf("JQ6500 cmd %d arg %d\n", command, argument);
  #endif
  command = mapJQ6500Command(command);
  _sendJQ[1] = 3;
  _sendJQ[2] = command;
  _sendJQ[3] = argument;
  _sendJQ[4] = 0xef;
  _serial->write(_sendJQ, 5);
}

void DFPlayerMini::sendJQ(uint8_t command, uint16_t argument) {
  #ifdef MP3_DEBUG
    Serial.printf("JQ6500 cmd %d larg %d\n", command, argument);
  #endif
  command = mapJQ6500Command(command);
  _sendJQ[1] = 4;
  _sendJQ[2] = command;
  _sendJQ[3] = (uint8_t) (argument>>8);
  _sendJQ[4] = (uint8_t) argument;
  _sendJQ[5] = 0xef;
  _serial->write(_sendJQ, 6);
}
