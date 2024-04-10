## Raspberry Pi ROM dumper for K5L2731CAM breakout boards on Famiclone handhelds (Sup 400-in-1)

This is a prototype of the ROM dumper using 3 I/O expanders (MCP23017) and a Raspberry Pi (any models will do). The dumping process will take about a day or so due to the longer delay times in the Python libraries:

### Note:
* Tie the reset pins on the MCP23017 to +3.3V to prevent non-detection and glitching!

![romdump_schematic](romdump_schematic.png)

The SOP44 to DIP converter:

![converter](romdump_converter.png)

### Attention:
* If you cannot fit the extracted breakout board, try slowly filing the four corners and retry fitting.

As a default, it is saved as "romdump.hex". The dumped binaries can be run in EmuVT 1.36.