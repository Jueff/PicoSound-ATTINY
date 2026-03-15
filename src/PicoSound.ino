/*
 * SPDX-FileCopyrightText: 2025-2026 Juergen Winkler <MobaLedLib@gmx.at>
 * SPDX-License-Identifier: BSL-1.1
 *
 * Description:
 *  This firmware runs on a Raspberry Pi Pico and controls up to 6 sound modules
 *  It is part of the MobaLedLib project. It receives LED data in groups of three byte values
 *  parses the data and controls the JQ6500 or MP3-TF16P sound modules via serial line.
 *  It provides visual feedback via the onboard RGB LED.
 *  Sound module types are stored in flash memory. The solution supports
 *  status indication using FastLED and MobaLedLib.
*/

#include "pico/multicore.h"
#include "MobaLedLib.h"
#include "PicoFlashStorage.h"

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
const char* bootMessage = "MobaLedLib Pico 6x Sound ATTiny V0.02";

#include "LEDReceiver.h"
#include "SoundModule.h"
#include "LedToSound.h"

bool dataChanged = false;
LEDReceiver* pLEDReceiver;
#define NUM_LEDS 20  
CRGB leds[NUM_LEDS];           // Define the array of leds

uint8_t lastSignal = 0xff;

uint8_t ledData[NUM_LEDS_TO_EMULATE*3];
uint8_t activeModule[NUM_LEDS_TO_EMULATE];

#define MAX_CHANNEL NUM_LEDS_TO_EMULATE*3
LedToSound* ledToSound[NUM_LEDS_TO_EMULATE];
SoundModule* soundHandlers[MAX_CHANNEL];


#define SECTORS_TO_USE 4
using namespace PicoFlashStorage;
FlashStorage* pStorage;
#define HB_INPUT 0
#define MAX_SIGNAL 4
#define MAX_BRIGHT 20

MobaLedLib_Configuration()
{
  //BlueLight1(0, C3, HB_INPUT + 2)
  Blink3(0, C_BLUE,                 HB_INPUT + 0, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  Blink3(0, C_YELLOW,               HB_INPUT + 1, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  Blink3(0, C_RED,                  HB_INPUT + 2, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  APatternT1(0, 193,                HB_INPUT + 3, 1, 5, MAX_BRIGHT, 0, PF_EASEINOUT, 1 Sec, 1)
  PatternT4(0, _NStru(C1, 4, 1), HB_INPUT + 4, _Cx2LedCnt(C1), 0, 255, 0, 0, 24 ms, 74 ms, 24 ms, 512 ms, _Cx2P_DBLFL(C1))

  //Switchable_RGB_Heartbeat_Color(0, HB_INPUT + 3, 20, 70, 170, 1000)

  EndCfg // End of the configuration
};

MobaLedLib_Create(leds); // Define the MobaLedLib instance

void turnInputsOff()
{
  for (int i = 0; i <= MAX_SIGNAL; i++)
  {
    MobaLedLib.Set_Input(HB_INPUT + i, 0);
  }
  lastSignal = -1;
  MobaLedLib.Update();
}

void setSignal(LEDReceiver::State signal)
{
  /*
    Error       = 0,
    DataMissing = 1,
    Offline     = 2,
    Online      = 3,
    FlashError  = 4,
  */
  uint8_t newSignal = (uint8_t)signal;
  if (newSignal > MAX_SIGNAL) return;
  if (newSignal == lastSignal) return;
  turnInputsOff();
  lastSignal = newSignal;
  Serial.printf("Set signal %d\r\n", newSignal);
  MobaLedLib.Set_Input(HB_INPUT + newSignal, 1);
}

void setup()
{
  Serial.begin(115200);
  // only enable for debugging purpose to see trace output of boot code
  // while (!Serial) {}
  Serial.println(bootMessage);


  for (int i = 0; i < 30; i++)
  {
    pinMode(i, INPUT);
  }

  Serial.println("Initialize LED Receiver");
  pLEDReceiver = new LEDReceiver(&ledData[0], NUM_LEDS_TO_EMULATE, NUM_LEDS_TO_SKIP, DATA_IN_PIN, DATA_OUT_PIN);

  Serial.println("Initialize FastLED"); 
          
  FastLED.addLeds<NEOPIXEL, STATUSLED_PIN>(leds, NUM_LEDS); // Initialize the FastLED library
  FastLED.addLeds<NEOPIXEL, 16>(leds, NUM_LEDS); // Initialize the FastLED library
  FastLED.setDither(DISABLE_DITHER);       // avoid sending slightly modified brightness values

  turnInputsOff();

  Serial.println("Initialize sound handlers");
  setupSoundModules();

  Serial.println("Initialize core1");
  multicore_launch_core1(core1_entry);
}

void setupSoundModules()
{
  uint16_t baseSectorNumber = (PICO_FLASH_SIZE_BYTES / FLASH_SECTOR_SIZE) - SECTORS_TO_USE;
  pStorage = new FlashStorage(baseSectorNumber, SECTORS_TO_USE, (uint8_t*)"MLLSRS10");

  if (!pStorage->isValid())
  {
    ShowCriticalError("can't use flash storage");
  }
  SoundModule::init(pStorage);

  for (uint8_t i=0; i<NUM_LEDS_TO_EMULATE; i++)
  {
    activeModule[i] = i*3;
  }
    
  for (uint8_t i = 0; i < MAX_CHANNEL; i++) {
    soundHandlers[i] = new SoundModule(i);
  }

  for (uint8_t i = 0; i < NUM_LEDS_TO_EMULATE; i++)
  {
    ledToSound[i] = new LedToSound(&soundHandlers[i * 3]);
  }
}

void core1_entry() 
{
  // process the data queue of the sound handlers in an infinite loop, so that the main loop is not blocked by waiting for the sound module to be ready
  while(true)
  {
    for (int ledIndex = 0; ledIndex < NUM_LEDS_TO_EMULATE; ledIndex++)
    {
      ledToSound[ledIndex]->processQueue();
    }
    delay(10);
  }
}

void loop()
{
    pLEDReceiver->loop();
    if (pLEDReceiver->hasDataChanged())
    {
      for (int ledIndex = 0; ledIndex < NUM_LEDS_TO_EMULATE; ledIndex++)
      {
        ledToSound[ledIndex]->processLedData(ledData[ledIndex * 3], ledData[ledIndex * 3 + 1], ledData[ledIndex * 3 + 2]);
      }
    }
    setSignal(pLEDReceiver->getState());
    MobaLedLib.Update();
    FastLED.show();                       // Show the LEDs (send the leds[] array to the LED stripe)
}

void ShowCriticalError(const char* message)
{
  auto lastTick = millis();
  do
  {
    if ((millis() - lastTick) > 1000)
    {
      lastTick = millis();
      Serial.println(message);
    }
    MobaLedLib.Update();
    FastLED.show(); // Show the LEDs (send the leds[] array to the LED stripe)
    delay(10);
  } while (true);
}
