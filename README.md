# PicoSound-ATTINY

This firmware runs on a Raspberry Pi Pico and controls up to 6 sound modules
It is part of the MobaLedLib project. It receives LED data in groups of three byte values
parses the data and controls the JQ6500 or MP3-TF16P sound modules via serial line.
It provides visual feedback via the onboard RGB LED.
Sound module types are stored in flash memory. The solution supports
status indication using FastLED and MobaLedLib.
