# SSTCInterrupter
Used for atmega328 in special circuit which has PS2 port, audio jack and optical output to drive SSTC from safe distance.  

- Uses Pulser library to switch LED in optical cable which drives SSTC (https://github.com/Cooble/Pulser)
- Uses SdFat library to read specialy formatted MIDI songs and play them using Pulser (https://github.com/greiman/SdFat)
- Uses PS2Keyboard library to convert keyboard button presses to musical notes and play them using Pulser (https://github.com/PaulStoffregen/PS2Keyboard)
- 3 indicator LEDS
- One control button to switch between different modes and different songs
