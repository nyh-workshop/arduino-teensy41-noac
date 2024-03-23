import time
from datetime import datetime

import board
import busio
import digitalio

from adafruit_mcp230xx.mcp23017 import MCP23017

# ROM dump utility
print("ROM dump utility")

# Initialize the I2C bus:
i2c = busio.I2C(board.SCL, board.SDA)

mcp00 = MCP23017(i2c, address=0x20)
mcp01 = MCP23017(i2c, address=0x21)
mcp02 = MCP23017(i2c, address=0x22)
# mcp00 = addr (A0-A15) [A+B]
# mcp01 = addr (A16-A23) [A]; /CE, /WE, /OE [B]
# mcp02 = data (D0-D15) [A+B]

# setting data pins:
mcp02.iodira = 0xff
mcp02.iodirb = 0xff
mcp02.gppua = 0x00
mcp02.gppub = 0x00

# setting address pins (A0-A15):
mcp00.iodira = 0x00
mcp00.iodirb = 0x00
mcp00.gpioa = 0x00
mcp00.gpiob = 0x00
mcp00.gppua = 0x00
mcp00.gppub = 0x00

# setting address pins (A16-A23):
mcp01.iodira = 0x00
mcp01.gpioa = 0x00
mcp01.gppua = 0x00

# setting control pins (/CE, /WE, /OE):
# bit 0 = /CE
# bit 1 = /WE
# bit 2 = /OE
mcp01.iodirb = 0x00
mcp01.gpiob = 0x07
pinCE = mcp01.get_pin(8)
pinWE = mcp01.get_pin(9)
pinOE = mcp01.get_pin(10)

pinCE.switch_to_output(value=True)
pinWE.switch_to_output(value=True)
pinOE.switch_to_output(value=True)

pinCE.pull = digitalio.Pull.UP
pinWE.pull = digitalio.Pull.UP
pinOE.pull = digitalio.Pull.UP

pinCE.value = True
pinWE.value = True
pinOE.value = True

def _CE(en):
    if (en == True):
        pinCE.value = False
    elif (en == False):
        pinCE.value = True
    else:
        raise Exception("True or False only!")

def _OE(en):
    if (en == True):
        pinOE.value = False
    elif (en == False):
        pinOE.value = True
    else:
        raise Exception("True or False only!")

def putAddr(addr):
    mcp00.gpioa = (addr & 0x000000ff)
    mcp00.gpiob = (addr & 0x0000ff00) >> 8
    mcp01.gpioa = (addr & 0x00ff0000) >> 16
    #print("mcp00 gpioa = " + hex(mcp00.gpioa))
    #print("mcp00 gpiob = " + hex(mcp00.gpiob))
    #print("mcp01 gpioa = " + hex(mcp01.gpioa))
                                                                                                                                                                                                                                                                 
def getData():
    dataL = mcp02.gpioa
    dataH = mcp02.gpiob
    return (dataH << 8) | dataL

def readAddr(addr):
    value = None
    putAddr(addr)
    _CE(True)
    #time.sleep(0.01)
    _OE(True)
    #time.sleep(0.01)
    value = getData()
    _CE(False)
    _OE(False)
    return value

def dumpROM():
    for addr in range(0x00000, 0x3fffff,1):
        print("Reading block: ", hex(addr))
        f.write(readAddr(addr).to_bytes(2, byteorder="little"))   

f = open("romdump.hex","wb")
f.seek(0)

startTime = datetime.now()
addr = 0
dumpROM()
stopTime = datetime.now()

print("time taken: ", stopTime-startTime)
f.close()
