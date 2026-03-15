# PicoSound-ATTINY

This firmware runs on a Raspberry Pi Pico and controls up to 6 sound modules
It is part of the MobaLedLib project. It receives LED data in groups of three byte values
parses the data and controls the JQ6500 or MP3-TF16P sound modules via serial line.
It provides visual feedback via the onboard RGB LED.
Sound module types are stored in flash memory. 

---

Diese Firmware läuft auf einem Raspberry Pi Pico und steuert bis zu 6 Soundmodule.
Sie ist Teil des MobaLedLib-Projekts. Sie empfängt LED-Daten in Gruppen von jeweils drei Byte-Werten,
analysiert die Daten und steuert die Soundmodule JQ6500 oder MP3-TF16P über die serielle Schnittstelle.
Sie liefert visuelles Feedback über die integrierte RGB-LED.
Die Soundmodultypen sind im Flash-Speicher gespeichert.
