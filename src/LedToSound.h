#pragma once
#include <queue>  
#include "SoundModule.h"

struct SoundCommand {
  uint8_t module;   // Zielmodul (Index)
  uint8_t cmd;
  uint8_t param1;
  uint8_t param2;
};

class LedToSound {
public:
  static constexpr uint8_t NUM_MODULES = 3;
  LedToSound(SoundModule* modules[NUM_MODULES]);
  void processLedData(uint8_t ledData1, uint8_t ledData2, uint8_t ledData3); // core0
  void processQueue(); // core1

private:
  SoundModule* soundModules[NUM_MODULES];
  std::queue<SoundCommand> commandQueue;
  uint8_t activeModule;
  uint8_t previousCommand;
  static constexpr uint spinlock_id = 0; // RP2040: Spinlock 0 verwenden
  void handleCommand(uint8_t cmd, uint8_t param1, uint8_t param2);
  void enqueueCommand(uint8_t module, uint8_t cmd, uint8_t param1, uint8_t param2);
};