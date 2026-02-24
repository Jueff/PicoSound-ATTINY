#pragma once
#include "WS281xProcessor.h"



class LEDReceiver {
public:
  enum class Status {
    Unknown     = -1,
    Error       = 0,
    DataMissing = 1,
    Offline     = 2,
    Online      = 3
  };

  void setSignal(uint8_t signal)
  {
    Serial.printf("Set signal %d\r\n", signal);
  }

  LEDReceiver(uint8_t* ledData, uint8_t numLedsToEmulate, uint8_t numLedsToSkip, uint8_t dataInPin, uint8_t dataOutPin)
    : ledData(ledData)
  {
    ledDataLen = numLedsToEmulate * 3;
    ledDataPrevious = (uint8_t*)malloc(ledDataLen);
    if (ledDataPrevious) {
      memset(ledDataPrevious, 0, ledDataLen);
    }
    pWs281xProcessor = new WS281xProcessor();
    currentStatus = Status::Unknown;
    reconnectCycles = DEFAULT_RECONNECT_CYCLES;;
    isOnline = false;
    lastSec = millis();
    lastDebug = millis();
    updatesPerSec = 0;
    dataChanged = false;
    errorDetected = false;
    dataAvailable = false;

    // Initialisierung des WS281xProcessors
    pWs281xProcessor->registerReceiveErrorCallback([this]() { this->WS281xProcessor_ReceiveError(); });
    pWs281xProcessor->registerDataReceivedCallback([this]() { this->WS281xProcessor_DataReceived(); });
    pWs281xProcessor->init(dataInPin, SIDESET_PIN, dataOutPin, numLedsToEmulate, numLedsToSkip, GRB, true);
  }

  ~LEDReceiver() {
    if (ledDataPrevious) {
      free(ledDataPrevious);
      ledDataPrevious = nullptr;
    }
    if (pWs281xProcessor) {
      delete pWs281xProcessor;
      pWs281xProcessor = nullptr;
    }
  }

  // Setter for reconnectCycles
  void setReconnectCycles(uint8_t value) 
  {
    reconnectCycles = value;
  }

  // Getter for Status
  Status getStatus() const 
  {
    return currentStatus;
  }

  bool hasDataChanged()
  {
    auto result = dataChanged;
    dataChanged = false;
    return result;
  }
  void loop() 
  {
    //Serial.printf("Data available: %d, errorDetected: %d\r\n", dataAvailable, errorDetected); 
    if (errorDetected) 
    {
      Serial.println("*** errorDetected ***");
      errorDetected = false;
      currentStatus = Status::Error;
    }
    if ((millis() - lastDataMillis) > 50) 
    {
      auto newStatus = errorDetected ? Status::Error : Status::Offline;
      if (currentStatus != newStatus)
      {
        currentStatus = newStatus;
        isOnline = false;
        pWs281xProcessor->reset();
/*        if (errorDetected)
        {
          setSignal(3);  // blink blue
        }
        else 
        {
          setSignal(1);  // blink red
        }*/
      }
    }
    else if (pWs281xProcessor->notEnoughData()) 
    {
      Serial.println("*** notEnoughData ***");
      isOnline = false;
      if (currentStatus != Status::DataMissing)
      {
        currentStatus = Status::DataMissing;
        //setSignal(2);  // blink yellow
      }
    }
    pWs281xProcessor->setRepeaterLEDColor(ledData[0], ledData[1], ledData[2]); //TODO

    std::array<RGBLED, NUM_LEDS_TO_EMULATE> localLeds;
    pWs281xProcessor->getLEDs(&localLeds[0]);
    if (dataAvailable && !errorDetected) 
    {
      dataAvailable = false;
      if (!isOnline) 
      {
        isOnline = true;
        reconnectCycles = defaultReconnectCycles;
        lastDataMillis = millis() + 200;
      }
      if (millis() >= lastDataMillis) 
      {
        lastDataMillis = millis();
        updatesPerSec++;
        uint8_t cnt = 0;
        for (auto it = localLeds.begin(); it != localLeds.end(); it++) 
        {
          ledData[cnt * 3] = (*it).colors.r;
          ledData[cnt * 3 + 1] = (*it).colors.g;
          ledData[cnt * 3 + 2] = (*it).colors.b;
          cnt++;
        }
        bool dataSame = memcmp(ledData, ledDataPrevious, ledDataLen) == 0;

        if (reconnectCycles > 0) 
        {
          if (dataSame)
          {
            reconnectCycles--;
            dataSame = false;
          }
          else
          {
            memcpy(ledDataPrevious, ledData, ledDataLen);
            reconnectCycles = defaultReconnectCycles;
          }
        }
        if (reconnectCycles == 0)
        {
          if (currentStatus != Status::Online) 
          {
            currentStatus = Status::Online;
            setSignal(0);   // green heartbeat
          }
          if (!dataSame) 
          {
            memcpy(ledDataPrevious, ledData, ledDataLen);
            DebugOutputLedData();
            dataChanged = true;
          }
        }
      }
    }
    if ((millis() - lastSec) >= 1000) 
    {
      lastSec += 1000;
#ifdef SHOW_SECONDS_OUTPUT
      if ((millis() - lastDebug) >= 10000) 
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
  void DebugOutputLedData()
  {
    Serial.print(" LED DATA: ");
    for (int i = 0; i < NUM_LEDS_TO_EMULATE * 3; i++)
    {
      Serial.printf("%2x ", ledData[i]);
    }
    Serial.println();
  }

  // Signalisiert einen Fehler beim Empfang
  void WS281xProcessor_ReceiveError() 
  {
    errorDetected = true;
  }

  // Signalisiert, dass neue Daten empfangen wurden
  void WS281xProcessor_DataReceived() 
  {
    dataAvailable = true;
  }


private:
  const uint DEFAULT_RECONNECT_CYCLES = 2;
  uint8_t* ledData;
  size_t ledDataLen;
  uint8_t* ledDataPrevious;
  uint8_t defaultReconnectCycles;
  WS281xProcessor* pWs281xProcessor;

  // State Variablen
  Status currentStatus;
  bool isOnline;
  bool dataChanged;
  bool dataAvailable;
  bool errorDetected;
  uint8_t reconnectCycles;
  unsigned long lastSec;
  unsigned long lastDebug;
  unsigned long lastDataMillis;
  int updatesPerSec;
};
