#include "pico/multicore.h"
#include "MobaLedLib.h"
#include "SoundModule.h"
#include "LedToSound.h"

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
LedToSound* ledToSound[NUM_LEDS_TO_EMULATE];
SoundModule* soundHandlers[MAX_CHANNEL];


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

    Serial.println("Initialize sound handlers");
    for (uint8_t i = 0; i < MAX_CHANNEL; i++) {
      soundHandlers[i] = new SoundModule(i);
    }

    for (uint8_t i = 0; i < NUM_LEDS_TO_EMULATE; i++)
    {
      ledToSound[i] = new LedToSound(&soundHandlers[i * 3]);
    }

    Serial.println("Initialize core1");
    multicore_launch_core1(core1_entry);
}


void core1_entry() 
{
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
