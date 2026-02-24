#include "pico/multicore.h"
#include "MobaLedLib.h"

// cool tool: https://godbolt.org/

//#define SOUNDSERVO_ALPHA_HARDWARE

#ifdef SOUNDSERVO_ALPHA_HARDWARE
  const uint DATA_IN_PIN = 27;
  const uint DATA_OUT_PIN = 26;
  const uint STATUSLED_PIN = 16;
#else
  const uint DATA_IN_PIN = 15;
  const uint DATA_OUT_PIN = 14;
  const uint STATUSLED_PIN = 13;
#endif

const uint NUM_LEDS_TO_EMULATE = 2;
const uint NUM_LEDS_TO_SKIP = NUM_LEDS_TO_EMULATE-1;

const char* bootMessage = "MobaLedLib Pico Sound V0.02";

#include "LEDReceiver.h"

#define MP3_DEBUG
#define S_DEBUG

bool on = true;
bool dataChanged = false;
CLEDController* pController0;
LEDReceiver* pLEDReceiver;
#define NUM_LEDS 20  
CRGB leds[NUM_LEDS];           // Define the array of leds

int updatesPerSec = 0;
unsigned long lastSec = millis();
unsigned long lastDebug = millis();

bool isOnline = false;
uint8_t lastSignal = 0xff;

volatile bool dataAvailable = false;

uint8_t ledData[NUM_LEDS_TO_EMULATE*3];
uint8_t previousCommand[NUM_LEDS_TO_EMULATE];
uint8_t activeModule[NUM_LEDS_TO_EMULATE];

#define MAX_CHANNEL NUM_LEDS_TO_EMULATE*3

#include "DFPlayerMini.h"
#include "SoftwareSerialTX.h"


DFPlayerMini MP3[MAX_CHANNEL];

#ifndef PICO_DEFAULT_LED_PIN
#define PICO_DEFAULT_LED_PIN 25
#endif

#define HB_INPUT 0

#define PinA4 HB_INPUT+2
#define MAX_BRIGHT 20
MobaLedLib_Configuration()
{
  //BlueLight1(0, C3, HB_INPUT + 2)
  Blink3(0, C_BLUE,                 HB_INPUT + 0, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  Blink3(0, C_YELLOW,               HB_INPUT + 1, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  Blink3(0, C_RED,                  HB_INPUT + 2, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  APatternT1(0, 193,                HB_INPUT + 3, 1, 5, MAX_BRIGHT, 0, PF_EASEINOUT, 1 Sec, 1)

  //Switchable_RGB_Heartbeat_Color(0, HB_INPUT + 3, 20, 70, 170, 1000)

  EndCfg // End of the configuration
};

MobaLedLib_Create(leds); // Define the MobaLedLib instance

void turnInputsOff()
{
  for (int i = 0; i < 4; i++)
  {
    MobaLedLib.Set_Input(HB_INPUT + i, 0);
  }
  MobaLedLib.Update();
}

void setSignal(LEDReceiver::State signal)
{
  /*
    Error       = 0,
    DataMissing = 1,
    Offline     = 2,
    Online      = 3
  */
  uint8_t newSignal = (uint8_t)signal;
  if (newSignal > 3) return;
  if (newSignal == lastSignal) return;
  lastSignal = newSignal;
  Serial.printf("Set signal %d\r\n", newSignal);
  turnInputsOff();
  MobaLedLib.Set_Input(HB_INPUT + newSignal, 1);

}

void setup()
{
    Serial.begin(115200);
    // only enable for debugging purpose to see trace output of boot code
    while (!Serial) {} 
    Serial.println(bootMessage);

    for (int i=0;i<16;i++)
    {
      pinMode(i, INPUT);
    }
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    memset(previousCommand, sizeof(previousCommand), 0);
    for (uint8_t i=0; i<NUM_LEDS_TO_EMULATE; i++)
    {
      activeModule[i] = i*3;
    }
    Serial.println("Initialize LED Receiver");
    pLEDReceiver = new LEDReceiver(&ledData[0], NUM_LEDS_TO_EMULATE, NUM_LEDS_TO_SKIP, DATA_IN_PIN, DATA_OUT_PIN);
    Serial.println("Initialize FastLED"); 
          
    FastLED.addLeds<NEOPIXEL, STATUSLED_PIN>(leds, NUM_LEDS); // Initialize the FastLED library
    FastLED.addLeds<NEOPIXEL, 16>(leds, NUM_LEDS); // Initialize the FastLED library
    FastLED.setDither(DISABLE_DITHER);       // avoid sending slightly modified brightness values

    turnInputsOff();
    Serial.println("Initialize core1");
    multicore_launch_core1(core1_entry);
}


void core1_entry() 
{
  for (uint8_t i=0; i < MAX_CHANNEL; i++)
  {
    auto serial = new SoftwareSerialTX(i);
    serial->begin(9600);
    MP3[i].begin(*serial);
  }
  while(true)
  {
    if (!dataChanged)
    {
      delay(1);
      continue;
    }
    //todo move data sending to a queue
    
    dataChanged = false;        
    for (int ledIndex = 0; ledIndex < NUM_LEDS_TO_EMULATE; ledIndex++)
    {
      uint8_t cmd = ledData[ledIndex*3];
      uint8_t param1 = ledData[ledIndex*3+1];
      uint8_t param2 = ledData[ledIndex*3+2];
      uint8_t nr;
      
      if (cmd>63)
      {
        cmd = (cmd-64)/6;
      } 
      else 
      {
        cmd = 0;
      }
      if (param1>63) 
      {
       param1 = (param1-64)/6;
      } 
      else 
      {
       param1=0;
      }
      if (param2>63) 
      {
       param2 = (param2-64)/6;
      }
      else
      {
       param2 = 0;
      }
      if (previousCommand[ledIndex] == cmd) continue;   // command not changed
        
      previousCommand[ledIndex] = cmd;  
      Serial.printf("Command: %d, Param1: %d, Param2: %d\r\n", cmd, param1, param2);
      switch (cmd) 
      {
        case DFPLAYER_SET_ACTIVE_MODULE:
          if ( (param1>0) && (param1<=MAX_CHANNEL) ) 
          {
            #if defined(S_DEBUG) || defined(MP3_DEBUG)
              Serial.printf("Change active module to %d\r\n", param1-1);
            #endif
            activeModule[ledIndex] = param1-1 + ledIndex*3;
          }
          break;
        case DFPLAYER_SET_MODULE_TYPE:
          if ( (param1>0) && (param2>0) && (param1<=MAX_CHANNEL) && (param2<=2) ) 
          {
            #if defined(S_DEBUG) || defined(MP3_DEBUG)
              Serial.printf("Set module type on position %d to %d\r\n", param1-1 + ledIndex*3, param2-1);
            #endif
            MP3[param1-1].setModuleType(param2-1);
            //EEPROM.write(param1-1, param2-1);
          }
          break;
        case DFPLAYER_PLAY_TRACK_ON:
          nr = param1>>3;
          param1 = param1 & 0x7;
          if (nr>0) 
          {
            activeModule[ledIndex] = nr-1 + ledIndex*3;
            #if defined(S_DEBUG) || defined(MP3_DEBUG)
              Serial.printf("module %d play track p1: %d p2: %d\r\n", activeModule[ledIndex], param1, param2);
            #endif
          }
          SendToMp3(activeModule[ledIndex], DFPLAYER_PLAY_TRACK, param1, param2);
          break;
        case DFPLAYER_PLAY_MP3_ON:
          nr = param1>>3;
          param1 = param1 & 0x7;
          if (nr>0) 
          {
            activeModule[ledIndex] = nr-1 + ledIndex*3;
            #if defined(S_DEBUG) || defined(MP3_DEBUG)
              Serial.printf("module %d play mp3 p1: %d p2: %d\r\n", activeModule[ledIndex], param1, param2);
            #endif
            SendToMp3(activeModule[ledIndex], DFPLAYER_PLAY_MP3, param1, param2);
          }
          break;
        case DFPLAYER_PLAY_TRACK_REPEAT_ON:
          nr = param1>>3;
          param1 = param1 & 0x7;
          if (nr>0) 
          {
            activeModule[ledIndex] = nr-1 + ledIndex*3;
          }
          SendToMp3(activeModule[ledIndex], DFPLAYER_PLAY_TRACK, param1, param2);
          SendToMp3(activeModule[ledIndex], DFPLAYER_SET_REPEAT_CURRENT, 1, 0);
          break;        
        case 0:
          // Do nothing - preparation for next command
          break;
        default:
          #ifdef S_DEBUG
            Serial.printf("Execute command %d on ledIndex %d\r\n", cmd, ledIndex);
          #endif
          SendToMp3(activeModule[ledIndex], cmd, param1, param2);
      }
    }
  }
}

void loop()
{
    pLEDReceiver->loop();
    dataChanged = pLEDReceiver->hasDataChanged();
    setSignal(pLEDReceiver->getState());
    MobaLedLib.Update();
    FastLED.show();                       // Show the LEDs (send the leds[] array to the LED stripe)
}

void SendToMp3(uint8_t moduleIndex, uint8_t cmd, uint8_t param1, uint8_t param2) {
  #ifdef MP3_DEBUG
    Serial.printf("module %d command: %d/%d/%d\r\n", moduleIndex, cmd, param1, param2);
  #endif
  switch (cmd) {
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
      MP3[moduleIndex].sendCmd(cmd);
      break;
		// 8-bit parameter
		case DFPLAYER_SET_VOLUME:
		case DFPLAYER_EQ:
		case DFPLAYER_SOURCE:
		case DFPLAYER_REPEAT:
		case DFPLAYER_SET_DAC:
		case DFPLAYER_SET_REPEAT_CURRENT:
      MP3[moduleIndex].sendCmd(cmd, param1);
			break;
		// 16 bit parameter
		case DFPLAYER_PLAY_TRACK:	// Play track (param1<<5+param2)+1, 0-1023 => 0001-1024.mp3/wav
		case DFPLAYER_SINGLE_REPEAT:	// Repeat (param1<<5+param2)+1, 0-1023 => 0001-1024.mp3/wav
		case DFPLAYER_PLAY_MP3:	// Play mp3/track (param1<<5+param2)+1, 0-1023 => 0001-1024.mp3/wav
		case DFPLAYER_ADVERTISEMENT: // Interrupt playback with ADVERT/track (param1<<5+param2)+1, 0-1023=>0001-1024.mp3/wav
		case DFPLAYER_FOLDER_REPEAT:	// Repeat files from folder (param1<<5+param2)+1, 0-98 => 01-99
      #ifdef MP3_DEBUG
        Serial.printf("Track: %d\n", uint16_t((param1<<5)+param2+1));
      #endif
			MP3[moduleIndex].sendCmd(cmd, uint16_t((param1<<5)+param2+1));
			break;
		// two 8 bit parameters
		case DFPLAYER_PLAY_FOLDER:			// folder, file
			MP3[moduleIndex].sendCmd(cmd, param1+1, (uint16_t) (param2+1));  // Folder 0-31 => "01"-"32", track 00-31 => 01-32.mp3/wav
			break;
		case DFPLAYER_ADJUST_VOLUME:		// enable 0|1, gain 0-31
			MP3[moduleIndex].sendCmd(cmd, param1, param2);
			break;
		// 4+12 bit parameter
		case DFPLAYER_PLAY_FOLDER_TRACK:
			MP3[moduleIndex].sendCmd(0x14, uint16_t((((uint16_t)(param1+1)) << 12) | (param2)));
			break;
  }
}
