#include <Arduino.h>
#include "LedToSound.h"
#include <mutex>


LedToSound::LedToSound(SoundModule* modules[NUM_MODULES])
{
  for (uint8_t i = 0; i < NUM_MODULES; i++)
    soundModules[i] = modules[i];
  activeModule = 0;
  previousCommand = 0xFF;
}

void LedToSound::processLedData(uint8_t ledData1, uint8_t ledData2, uint8_t ledData3)
{
  uint8_t cmd = ledData1;
  uint8_t param1 = ledData2;
  uint8_t param2 = ledData3;

  // Mapping wie im Originalcode
  if (cmd > 63) cmd = (cmd - 64) / 6; else cmd = 0;
  if (param1 > 63) param1 = (param1 - 64) / 6; else param1 = 0;
  if (param2 > 63) param2 = (param2 - 64) / 6; else param2 = 0;

  if (previousCommand == cmd) return;
  previousCommand = cmd;
  handleCommand(cmd, param1, param2);
}

void LedToSound::handleCommand(uint8_t cmd, uint8_t param1, uint8_t param2)
{
  uint8_t nr;
  switch (cmd)
  {
  case DFPLAYER_SET_ACTIVE_MODULE:
    if ((param1 > 0) && (param1 <= NUM_MODULES)) {
      activeModule = param1 - 1;
    }
    break;
  case DFPLAYER_SET_MODULE_TYPE:
    if ((param1 > 0) && (param2 > 0) && (param1 <= NUM_MODULES) && (param2 <= 2)) {
      enqueueCommand(param1 - 1, DFPLAYER_SET_MODULE_TYPE, param2 - 1, 0);
    }
    break;
  case DFPLAYER_PLAY_TRACK_ON:
  case DFPLAYER_PLAY_MP3_ON:
    nr = param1 >> 3;
    param1 = param1 & 0x7;
    if (nr < 1 || nr > NUM_MODULES) return;
    activeModule = nr - 1;
    enqueueCommand(activeModule, DFPLAYER_PLAY_TRACK, param1, param2);
    break;
  case DFPLAYER_PLAY_TRACK_REPEAT_ON:
    nr = param1 >> 3;
    param1 = param1 & 0x7;
    if (nr < 1 || nr > NUM_MODULES) return;
    activeModule = nr - 1;
    enqueueCommand(activeModule, DFPLAYER_PLAY_TRACK, param1, param2);
    enqueueCommand(activeModule, DFPLAYER_SET_REPEAT_CURRENT, 1, 0);
    break;
  case 0:
    // Do nothing
    break;
  default:
    enqueueCommand(activeModule, cmd, param1, param2);
  }
}

void LedToSound::enqueueCommand(uint8_t module, uint8_t cmd, uint8_t param1, uint8_t param2)
{
  if (module >= NUM_MODULES) return;
  spin_lock_t* lock = spin_lock_instance(spinlock_id);

  spin_lock_unsafe_blocking(lock);
  commandQueue.push({ module, cmd, param1, param2 });
  spin_unlock_unsafe(lock);
}

void LedToSound::processQueue()
{
  spin_lock_t* lock = spin_lock_instance(spinlock_id);
  spin_lock_unsafe_blocking(lock);
  while (!commandQueue.empty()) 
  {
    SoundCommand cmd = commandQueue.front();
    commandQueue.pop();
    spin_unlock_unsafe(lock); // Queue verlassen, Verarbeitung außerhalb
    soundModules[cmd.module]->process(cmd.cmd, cmd.param1, cmd.param2);
    spin_lock_unsafe_blocking(lock); // Für nächsten Zugriff wieder sperren
  }
  spin_unlock_unsafe(lock);
}