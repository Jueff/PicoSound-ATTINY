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
#include "MLLSoundTiny.h"
#include <AceButton.h>  

using namespace ace_button;

 // stringification helpers: convert numeric macro to string
#ifndef STRINGIFY2
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#endif

// fallback in case APP_VERSION is not set
#ifndef APP_VERSION
#define APP_VERSION 0.0
#endif

const uint DATA_IN_PIN = 15;
const uint DATA_OUT_PIN = 14;
const uint STATUSLED_PIN = 13;
const uint DEBUG_PIN = 8;

const uint NUM_LEDS_TO_EMULATE = 2;                       // number of LEDs to be read from the data stream
const uint NUM_LEDS_TO_SKIP = NUM_LEDS_TO_EMULATE - 1;    // number of LEDs to skip in the data stream, folloing data are forwared to DOut
const uint NUM_SOUND_CONTROLLERS = 2;                     // number of sound controllers, each controller can handle 3 channels, so with 1 controller up to 3 sound channels can be controlled
const uint NUM_LEDS = 1;                                  // number of FASTLed output LEDs, only one LED used for status indication and setup mode
const char* bootMessage = "MobaLedLib Pico 6x Sound ATTiny V" STRINGIFY(APP_VERSION);

#include "LEDReceiver.h"
#include "SoundModule.h"
#include "LedToSound.h"

// LED receiver members
LEDReceiver* pLEDReceiver;

uint8_t ledData[NUM_LEDS_TO_EMULATE * 3];

// sound members
#define MAX_CHANNEL NUM_SOUND_CONTROLLERS*3
LedToSound* ledToSound[NUM_SOUND_CONTROLLERS];
SoundModule* soundHandlers[MAX_CHANNEL];

// debug 
AceButton button(DEBUG_PIN);
uint8_t LogLevel;
uint8_t logState = 0;
#define MLLAPP_LOG(level,...) if (LogLevel>=level) Serial.printf(__VA_ARGS__);

// flash storage
#define SECTORS_TO_USE 4
using namespace PicoFlashStorage;
FlashStorage* pStorage;

// Status signal members and defines
uint8_t lastSignal = 0xff;
const uint8_t HB_INPUT = 0;
const uint8_t MAX_SIGNAL = 4;
const uint8_t MAX_BRIGHT = 20;

// MobaLedLib
CRGB leds[NUM_LEDS];

MobaLedLib_Configuration()
{
  Blink3(0, C_BLUE, HB_INPUT + 0, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)               // blue flashing
  Blink3(0, C_YELLOW, HB_INPUT + 1, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)             // yellow flashing
  Blink3(0, C_RED, HB_INPUT + 2, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)                // red flashing
  // APatternT1(0, 193, HB_INPUT + 3, 1, 5, MAX_BRIGHT, 0, PF_EASEINOUT, 1 Sec, 1)  // green fading
  ConstRGB(0, HB_INPUT + 3, 0, 0, 0, 0, 0, 0)                                       // led off
  PatternT4(0, _NStru(C1, 4, 1), HB_INPUT + 4, _Cx2LedCnt(C1), 0, 255, 0, 0, 24 ms, 74 ms, 24 ms, 512 ms, _Cx2P_DBLFL(C1))  // red warning flashlight
  EndCfg // End of the configuration
};
MobaLedLib_Create(leds); // Define the MobaLedLib instance

void turnInputsOff()
{
  for (int i = 0; i <= MAX_SIGNAL; i++)
  {
    MobaLedLib.Set_Input(HB_INPUT + i, 0);
  }
  lastSignal = 0xff;
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
  MLLAPP_LOG(3, "Set signal %d\r\n", newSignal);
  MobaLedLib.Set_Input(HB_INPUT + newSignal, 1);
}

void setup()
{
  Serial.begin(115200);
  for (int i = 0; i < 30; i++)
  {
    pinMode(i, INPUT);
  }
  pinMode(DEBUG_PIN, INPUT_PULLUP);

  // check if debug button is pressed during startup, if yes, wait 3 seconds for usb serial is connected and enable all logs
  if (digitalRead(DEBUG_PIN) == LOW)
  {
    // wait three seconds for the usb serial to attach
    auto startTime = millis();
    while ((millis() - startTime) < 3000)
    {
      // only enable for debugging purpose to see trace output of boot code
      if (Serial) break;
      delay(10);
    }
    MLLSoundTiny::LogLevel = 8;
    FlashStorage::LogLevel = 8;
    LogLevel = 8;
    logState = 3;
    MLLAPP_LOG(1, "debug mode: All logs enabled\n");
  }
  else
  {
    MLLSoundTiny::LogLevel = 0;
    FlashStorage::LogLevel = 0;
    LogLevel = 0;
  }

  MLLAPP_LOG(1, bootMessage);

  MLLAPP_LOG(3, "Initialize LED Receiver");
  pLEDReceiver = new LEDReceiver(&ledData[0], NUM_LEDS_TO_EMULATE, NUM_LEDS_TO_SKIP, DATA_IN_PIN, DATA_OUT_PIN);

  MLLAPP_LOG(3, "Initialize FastLED");

  FastLED.addLeds<NEOPIXEL, STATUSLED_PIN>(leds, NUM_LEDS); // Initialize the FastLED library
  FastLED.addLeds<NEOPIXEL, 16>(leds, NUM_LEDS); // Initialize the FastLED library
  FastLED.setDither(DISABLE_DITHER);       // avoid sending slightly modified brightness values

  turnInputsOff();

  MLLAPP_LOG(3, "Initialize sound handlers");
  uint16_t baseSectorNumber = (PICO_FLASH_SIZE_BYTES / FLASH_SECTOR_SIZE) - SECTORS_TO_USE;
  pStorage = new FlashStorage(baseSectorNumber, SECTORS_TO_USE, (uint8_t*)"MLLSRS10");
  if (!pStorage->isValid())
  {
    showCriticalError("can't use flash storage");
  }

  setupSoundModules(pStorage, 0);

  MLLAPP_LOG(3, "Initialize core1");
  multicore_launch_core1(core1_entry);

  MLLAPP_LOG(3, "Activating buttons");
  ButtonConfig* buttonConfig = ButtonConfig::getSystemButtonConfig();
  buttonConfig->setEventHandler(handleButton);
  buttonConfig->setFeature(ButtonConfig::kFeatureLongPress);
}

void setupSoundModules(FlashStorage* pStorage, uint8_t basePin)
{
  SoundModule::init(pStorage);

  for (uint8_t i = 0; i < MAX_CHANNEL; i++) {
    soundHandlers[i] = new SoundModule(i+basePin);
  }

  for (uint8_t i = 0; i < NUM_SOUND_CONTROLLERS; i++)
  {
    ledToSound[i] = new LedToSound(&soundHandlers[i * 3]);
  }
}

void core1_entry()
{
  // process the data queue of the sound handlers in an infinite loop, so that the main loop is not blocked by waiting for the sound module to be ready
  while (true)
  {
    for (int index = 0; index < NUM_SOUND_CONTROLLERS; index++)
    {
      ledToSound[index]->processQueue();
    }
    delay(10);
  }
}

void loop()
{
  pLEDReceiver->loop();

  updateSound(0);
  setSignal(pLEDReceiver->getState());
  MobaLedLib.Update();
  FastLED.show();                       // Show the LEDs (send the leds[] array to the LED stripe)
  button.check();
}
void updateSound(uint8_t ledOffset)
{
  if (pLEDReceiver->hasDataChanged())
  {
    if (LogLevel>0)
    {
      pLEDReceiver->DebugOutputLedData();
    }
    for (int index = 0; index < NUM_SOUND_CONTROLLERS; index++)
    {
      ledToSound[index]->processLedData(ledData[ledOffset + index * 3], ledData[ledOffset + index * 3 + 1], ledData[ledOffset + index * 3 + 2]);
    }
  }
}

void showCriticalError(const char* message)
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

void handleButton(AceButton* button, uint8_t eventType, uint8_t buttonState) 
{
  switch (eventType) 
  {
  case AceButton::kEventPressed:
    shortPress();
    break;
  case AceButton::kEventLongPressed:
    longPress();
    break;
  }
}

void shortPress() 
{
  switch (logState)
  {
  case 0:
    LogLevel = 3;
    MLLAPP_LOG(3, "Log for main application enabled\n");
    MLLSoundTiny::LogLevel = 0;
    FlashStorage::LogLevel = 0;
    logState = 1;
    break;
  case 1:
    MLLAPP_LOG(3, "Log for MLLSoundTiny enabled\n");
    MLLSoundTiny::LogLevel = 4;
    logState = 2;
    break;
  case 2:
    MLLAPP_LOG(3, "Log for FlashStorage enabled\n");
    FlashStorage::LogLevel = 4;
    logState = 0;
    break;
  }
}

void longPress() 
{
  MLLAPP_LOG(3, "logs turned off\n");
  MLLSoundTiny::LogLevel = 0;
  FlashStorage::LogLevel = 0;
  LogLevel = 0;
  logState = 0;
}