#pragma once

#include "Arduino.h"

// Equalizer modes
#define PLAYER_EQ_NORMAL 0
#define PLAYER_EQ_POP 1
#define PLAYER_EQ_ROCK 2
#define PLAYER_EQ_JAZZ 3
#define PLAYER_EQ_CLASSIC 4
#define PLAYER_EQ_BASS 5

// Sources
#define PLAYER_DEVICE_U_DISK 1
#define PLAYER_DEVICE_SD 2
#define PLAYER_DEVICE_AUX 3
#define PLAYER_DEVICE_SLEEP 4
#define PLAYER_DEVICE_FLASH 5

#define PLAYER_RECEIVED_LENGTH 10
#define PLAYER_SEND_LENGTH 10

//#define _DEBUG

// Commands with no parameters
#define PLAYER_NO_COMMAND 0x00        // Do not do anything
#define PLAYER_NEXT 0x01							// Play next track - No parameters
#define PLAYER_PREV 0x02							// Play previous track - No parameters
#define PLAYER_VOLUME_INCREASE 0x04		// Increase volume - No parameters
#define PLAYER_VOLUME_DECREASE 0x05		// Decrease volume - No parameters
#define PLAYER_STANDBY 0x0a						// Activate sleep mode
#define PLAYER_RESET 0x0c							// Reset device - no parameters
#define PLAYER_PLAYBACK 0x0d					// Play - no parameters
#define PLAYER_PAUSE 0x0e							// Pause - no parameters
#define PLAYER_STOP_ADVERTISEMENT 0x15 // Stop advertisement and return to track
#define PLAYER_STOP 0x16							// Stop playing - affects tracks and advertisement
#define PLAYER_NORMAL 0x0b						// Return to normal mode or maybe unused
#define PLAYER_SET_RANDOM	0x18				// Start random playback. All files should be used, main folder, mp3 folder, 00-99 folders

// Commands with one 8-bit parameter
#define PLAYER_SET_VOLUME 0x06				// Set volume - 0-30
#define PLAYER_EQ 0x07								// Set Equalizer, see constants above
#define PLAYER_SOURCE 0x09						// See constants above - default is PLAYER_DEVICE_SD
#define PLAYER_REPEAT 0x11						// Set all repeat - 0/1
#define PLAYER_SET_DAC 0x1a						// Set DAC 0|1 off-on
#define PLAYER_SET_REPEAT_CURRENT 0x19 // Set repeat playback of current track 0|1 off-on

// Commands with one 16-bit parameter
#define PLAYER_PLAY_TRACK 0x03				// Play track from main folder, 16-bit parameter
#define PLAYER_SINGLE_REPEAT 0x08			// Set single repeat - 1 = No, 2 = Yes
#define PLAYER_PLAY_MP3 0x12					// Play from folder mp3, track 16-bit parameter, filename 0000, 0001... 16 bit
#define PLAYER_ADVERTISEMENT 0x13			// Insert advertisement from folder ADVERT, filename 0000... 16 bit
#define PLAYER_FOLDER_REPEAT 0x17			// Repeat track from folder folder 01-99, 16 bit

// Commands with two 8-bit parameters
#define PLAYER_PLAY_FOLDER 0x0f				// Play track from folder, 2 parameters
#define PLAYER_ADJUST_VOLUME 0x10			// Set audio amplification setting 0|1 off-on, 0-31 gain

// Commands with 4+12bit parameters
#define PLAYER_PLAY_FOLDER_TRACK 0x14	// Play from folder arg1, track arg2, high 4 bits=folder, low 12 bits = track 0000...

// With reduced number of tracks (1-256)and module number (1-3)
#define PLAYER_PLAY_TRACK_ON 0x1d        // Like PLAYER_PLAY_TRACK with module number
#define PLAYER_PLAY_MP3_ON 0x1c          // Like PLAYER_PLAY_MP3 with module number
#define PLAYER_PLAY_TRACK_REPEAT_ON 0x1b // Play Track on module with repeat

// Pseudo commands processed by Attiny
#define PLAYER_SET_MODULE_TYPE   0x1e  // Change module type
#define PLAYER_SET_ACTIVE_MODULE 0x1f  // Change active module

#define Stack_Header 0
#define Stack_Version 1
#define Stack_Length 2
#define Stack_Command 3
#define Stack_ACK 4
#define Stack_Parameter 5
#define Stack_CheckSum 7
#define Stack_End 9

#define MODULE_MP3_TF_16P 0
#define MODULE_JQ6500 1

class MultiPlayer {
  Stream* _serial;
  uint8_t _currentType = MODULE_MP3_TF_16P;
  unsigned long _timeOutTimer;
  bool _isSending = false;
  uint8_t _sending[PLAYER_SEND_LENGTH] = {
    0x7E,   // Startbyte
    0xff,   // Version
    06,     // Number of bytes to follow
    00,     // Command
    00,     // Feedback 0=No, 1=Yes
    00,     // Param1 or high data byte
    00,     // Param2 or low data byte
    00,     // Checksum
    00,     // Filler?
    0xEF};  // End byte
  uint8_t _sendJQ[6] = {
    0x7e, // Start byte
    0,    // Number of bytes to follow
    0,    // Command
    0,    // Argument 1 or high data byte
    0,    // Argument 2 or low data byte
    0xEF  // End byte 0xef
  };

  uint16_t calculateCheckSum(uint8_t *buffer);
  void uint16ToArray(uint16_t value,uint8_t *array);
  void disableACK();

  void sendStack();
  void sendStack(uint8_t command);
  void sendStack(uint8_t command, uint16_t argument);
  void sendStack(uint8_t command, uint8_t argumentHigh, uint8_t argumentLow);

  public:
  
  bool begin(Stream& stream);

  void setModuleType(uint8_t moduleType);

  void sendCmd(uint8_t command);
  void sendCmd(uint8_t command, uint8_t argument);
  void sendCmd(uint8_t command, uint16_t argument);
  void sendCmd(uint8_t command, uint8_t argumentHigh, uint8_t argumentLow);

  void sendJQ(uint8_t command);
  void sendJQ(uint8_t command, uint8_t argument);
  void sendJQ(uint8_t command, uint16_t argument);

protected:
  uint8_t mapJQ6500Command(uint8_t command);
};
