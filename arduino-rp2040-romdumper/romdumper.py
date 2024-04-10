from datetime import datetime
import serial
import time
import sys

# Reference: https://github.com/Promolife/sup_console_programmator/

COM_PORT = "COM16"

print("================")
print("K5L flash dumper")
print("================")

ser = serial.Serial(port=COM_PORT, baudrate=115200, bytesize=8, parity='N', stopbits=1, timeout=0.5)

addrStart = 0x000000
addrEnd = 0x800000
totalSize = addrEnd - addrStart
progress = 0x0000

print("Address is by 16-bit data.")
print("Example: Addr for 8-bit data is 0x70000")
print("         Addr for 16-bit data is 0x38000")
print("================")
print("Start address: ", hex(addrStart))
print("End address: ", hex(addrEnd))
assert (addrEnd > addrStart) , "Start addr should not be bigger than or equal end addr!" 
print("Size dumped: ", hex(addrEnd-addrStart))
print("================")

addrStartBytes = bytes.fromhex(str.format('{:06X}', addrStart))
addrEndBytes = bytes.fromhex(str.format('{:06X}', addrEnd))

dumpCmd = b'#RDR' + addrEndBytes + addrStartBytes + b'$'

fileName = "romdump_" + datetime.now().strftime("%d%m%Y_%H%M%S") + ".bin"
f = open(fileName,"wb")
f.seek(0)

print("Dump in progress...")

ser.write(dumpCmd)

startTime = datetime.now()

while(totalSize >= 0x200):
    buffer = ser.read(0x400)
    f.write(buffer)
    totalSize -= 0x200
    progress += 0x200
    if((progress % 0x8000) == 0):
        print("64kbytes read and dumped, progress: ", progress * 2)

if(totalSize > 0):
    buffer = ser.read(totalSize * 2)
    f.write(buffer)

f.close()

stopTime = datetime.now()

print("================")
print("Dump successful!")
print("================")
print("File written: ", fileName)
print("Start time: ", startTime)
print("Stop time: ", stopTime)
print("time taken: ", stopTime - startTime)
