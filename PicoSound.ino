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

const uint DEFAULT_RECONNECT_CYCLES = 2;

const char* bootMessage = "MobaLedLib Pico Sound V0.02";

#include "WS281xProcessor.h"

#define MP3_DEBUG
#define S_DEBUG

bool on = true;
WS281xProcessor* pWs281xProcessor = NULL;
CLEDController* pController0;
#define NUM_LEDS 20  
CRGB leds[NUM_LEDS];           // Define the array of leds

int updatesPerSec = 0;
unsigned long lastSec = millis();
unsigned long lastDebug = millis();

bool isOnline = false;
uint8_t reconnectCycles = DEFAULT_RECONNECT_CYCLES;

volatile bool dataAvailable = false;
volatile bool dataChanged = false;
volatile bool errorDetected  = false;
int blinkSleep = 1000;

uint8_t ledData[NUM_LEDS_TO_EMULATE*3];
uint8_t ledDataPrevious[NUM_LEDS_TO_EMULATE*3];
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

#define  Switchable_RGB_Heartbeat_Color(LED, InCh, MinBrightness, MaxBrightness, Color, Duration) \
             New_HSV_Group()                                          \
             APatternT1(LED,224,InCh,1,Color,Color,0,PM_HSV,0 ms,1)  \
             APatternT1(LED, 194,InCh,1,MinBrightness,MaxBrightness,0,PM_HSV|PF_EASEINOUT,Duration,1)       

#define PinA4 HB_INPUT+2
#define MAX_BRIGHT 20
MobaLedLib_Configuration()
{
  Switchable_RGB_Heartbeat_Color(0, HB_INPUT, 20, 70, 170, 1000)
  Blink3(0, C_RED, HB_INPUT+1, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  Blink3(0, C_YELLOW, HB_INPUT + 2, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)
  Blink3(0, C_BLUE, HB_INPUT + 3, 0.5 Sek, 0.5 Sek, 5, MAX_BRIGHT, 0)

  
  //BlueLight1(0, C3, HB_INPUT + 1)

  EndCfg // End of the configuration
};

MobaLedLib_Create(leds); // Define the MobaLedLib instance

void setSignal(uint8_t signal)
{
  if (signal > 3) return;
  MobaLedLib.Set_Input(HB_INPUT, 0);
  MobaLedLib.Set_Input(HB_INPUT + 1, 0);
  MobaLedLib.Set_Input(HB_INPUT + 2, 0);
  MobaLedLib.Set_Input(HB_INPUT + 3, 0);
  MobaLedLib.Set_Input(HB_INPUT + signal, 1);
}

void setup()
{
    Serial.begin(115200);
    // only enable for debugging purpose to see trace output of boot code
    //while (!Serial) {} 
    Serial.println(bootMessage);

    for (int i=0;i<16;i++)
    {
      pinMode(i, INPUT);
    }
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    memset(previousCommand, sizeof(previousCommand), 0);
    memset(ledDataPrevious, sizeof(ledDataPrevious), 0);
    for (uint8_t i=0; i<NUM_LEDS_TO_EMULATE; i++)
    {
      activeModule[i] = i*3;
    }
    
    pWs281xProcessor = new WS281xProcessor();
    pWs281xProcessor->init(DATA_IN_PIN, SIDESET_PIN, DATA_OUT_PIN, NUM_LEDS_TO_EMULATE, NUM_LEDS_TO_SKIP, GRB, true);

    FastLED.addLeds<NEOPIXEL, STATUSLED_PIN>(leds, NUM_LEDS); // Initialize the FastLED library
    FastLED.addLeds<NEOPIXEL, 16>(leds, NUM_LEDS); // Initialize the FastLED library
    FastLED.setDither(DISABLE_DITHER);       // avoid sending slightly modified brightness values

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

  auto lastDataMillis = millis() + 1000;   // at start wait 1secs
  
  int status = -1;
  while (true) 
  {
    if (errorDetected)
    {
      Serial.println("*** errorDetected ***");
      errorDetected = false;
    }
    if ((millis()-lastDataMillis)>50)
    {
      if (status!=1)
      {
        status = 1;
        isOnline = false;
        pWs281xProcessor->reset();
        if (errorDetected)
        {
          setSignal(3);  // blink blue
        }
        else
        {
          setSignal(1);  // blink red
        }
      }
    }
    else if (pWs281xProcessor->notEnoughData())   // highest priority for status signaling
    {
      Serial.println("*** notEnoughData ***");
      isOnline = false;
      if (status!=0)
      {
        status = 0;
        setSignal(2);  // blink yellow
      }
    }
    MobaLedLib.Update();
    FastLED.show();                       // Show the LEDs (send the leds[] array to the LED stripe)
    pWs281xProcessor->setRepeaterLEDColor(leds[0].r, leds[0].g, leds[0].b);

    std::array<RGBLED,NUM_LEDS_TO_EMULATE> leds;
    pWs281xProcessor->getLEDs(&leds[0]);
    if (dataAvailable && ! errorDetected)
    {
      dataAvailable = false;
      //Serial.println("+");
      if (!isOnline) 
      {
        isOnline = true;
        reconnectCycles = DEFAULT_RECONNECT_CYCLES;
        lastDataMillis = millis() + 200;   // after reconnect wait 200msecs for next data
      }
      if (millis()>=lastDataMillis) 
      {
          lastDataMillis = millis();
          updatesPerSec++;
          //
          uint8_t cnt = 0;
          for (auto it = leds.begin(); it != leds.end(); it++) {
            //Serial.printf("[%7u] LED %u: ", time_us_32() / 1000, std::distance(leds.begin(), it));
            //print_led_state(*it, pWs2811Processor->getResetCnt());
            ledData[cnt*3] = (*it).colors.r;
            ledData[cnt*3+1] = (*it).colors.g;
            ledData[cnt*3+2] = (*it).colors.b;
            cnt++;
          }
          bool dataSame = memcmp(ledData, ledDataPrevious, sizeof(ledData))==0;
          
          if (reconnectCycles>0)
          {
              if (dataSame) 
              {
                  reconnectCycles--;
                  dataSame = false;
              }
              else
              {
                  memcpy(ledDataPrevious, ledData, sizeof(ledData));
                  reconnectCycles = DEFAULT_RECONNECT_CYCLES;       // data changed, start triggering again
              }
          }
          if (reconnectCycles==0)
          {
              // connected, getting valid data
              if (status!=2)
              {
                  status=2;
                  setSignal(0);   // green heartbeat
              }
              if (!dataSame)
              {
                memcpy(ledDataPrevious, ledData, sizeof(ledData));
                DebugOutputLedData();
                dataChanged = true;
              }
          }
       }
    }
    if ((millis()-lastSec)>=1000)
    {
      lastSec+=1000;
      
#define SHOW_SECONDS_OUTPUT      
#ifdef SHOW_SECONDS_OUTPUT
      if ((millis()-lastDebug)>=10000)
      {
        lastDebug = lastDebug + 10000;
        Serial.print(bootMessage);
        DebugOutputLedData();
        Serial.printf("% d updates per second\r\n", updatesPerSec);
      }
#endif      
      updatesPerSec = 0;
    }
  }
}

void DebugOutputLedData()
{
  Serial.print(" LED DATA: ");
  for (int i=0;i<NUM_LEDS_TO_EMULATE*3;i++)
  {
    Serial.printf("%2x ", ledData[i]);
  }
  Serial.println();
}

void WS281xProcessor_ReceiveError()
{
  errorDetected = true;
}

void WS281xProcessor_DataReceived()
{
  dataAvailable = true;
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
