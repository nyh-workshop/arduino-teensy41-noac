#include <Adafruit_MCP23X17.h>
#include <Wire.h>

#define WIRE Wire

Adafruit_MCP23X17 mcp000;
Adafruit_MCP23X17 mcp001;

const pin_size_t MCP23017_SDA = 16;
const pin_size_t MCP23017_SCL = 17;
const pin_size_t CE_PIN = 28;
const pin_size_t LED_PIN = 25;
const uint32_t I2C_FREQ = 400000;

inline void CE_enable() {
  digitalWrite(CE_PIN, 0);
}

inline void CE_disable() {
  digitalWrite(CE_PIN, 1);
}

inline void write_K5L_addr(uint32_t aAddr) {
  // MCP000: A0-A15
  // MCP001: A16-A22
  mcp000.writeGPIOAB(0x0000FFFF & aAddr);
  mcp001.writeGPIOAB((aAddr & 0x7f0000) >> 16);
}

uint16_t read_K5L_data(uint32_t aAddr) {
  uint32_t gpioResult = gpio_get_all();
  gpioResult &= 0x0000FFFF;
  return (uint16_t)gpioResult;
}

uint16_t read_K5L(uint32_t aAddr) {
  uint16_t K5L_data = 0x0000;
  write_K5L_addr(aAddr);
  delayMicroseconds(2);
  CE_enable();
  delayMicroseconds(2);
  K5L_data = read_K5L_data(aAddr);
  delayMicroseconds(1);
  CE_disable();
  return K5L_data;
}

void setup() {
  WIRE.setSDA(MCP23017_SDA);
  WIRE.setSCL(MCP23017_SCL);
  WIRE.setClock(I2C_FREQ);
  WIRE.begin();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, 0);

  Serial.begin(115200);
  while (!Serial)
    delay(10);

  if (!mcp000.begin_I2C(0x20)) {
    //Serial.println("MCP23017 Error at address 0x20.");
    while (1) {
      digitalWrite(LED_PIN, 1);
      delay(250);
      digitalWrite(LED_PIN, 0);
      delay(250);
    }
  }

  if (!mcp001.begin_I2C(0x21)) {
    //Serial.println("MCP23017 Error at address 0x21.");
    while (1) {
      digitalWrite(LED_PIN, 1);
      delay(250);
      digitalWrite(LED_PIN, 0);
      delay(250);
    }
  }

  for (uint8_t i = 0; i < 16; i++) {
    mcp000.pinMode(i, OUTPUT);
    mcp001.pinMode(i, OUTPUT);
  }

  pinMode(CE_PIN, OUTPUT);

  CE_disable();

  // GPIOB = msb
  // GPIOA = lsb
  mcp000.writeGPIOAB(0x0000);
  mcp001.writeGPIOAB(0x0000);

  // Set the GP0-15 all inputs for reading K5L's data pins:
  for (uint8_t i = 0; i < 16; i++)
    pinMode(i, INPUT_PULLUP);

  //Serial.println("K5L dumper init!");
  digitalWrite(LED_PIN, 1);
}

uint8_t dumpCmdBuffer[12];
uint16_t buffer[512];
uint16_t K5L_data = 0x0000;
uint32_t addrProgress = 0x00000000;

void loop() {
  if (Serial.available() > 0) {
    Serial.readBytesUntil('$', dumpCmdBuffer, sizeof(dumpCmdBuffer));
    // Reference: https://github.com/Promolife/sup_console_programmator/
    if (dumpCmdBuffer[0] == '#' && dumpCmdBuffer[1] == 'R' && dumpCmdBuffer[2] == 'D' && dumpCmdBuffer[3] == 'R') {
      //Serial.write("good");
      uint32_t endWordAddr = (dumpCmdBuffer[6]) | (dumpCmdBuffer[5] << 8) | (dumpCmdBuffer[4] << 16);
      uint32_t startWordAddr = (dumpCmdBuffer[9]) | (dumpCmdBuffer[8] << 8) | (dumpCmdBuffer[7] << 16);
      int32_t totalSize = endWordAddr - startWordAddr;
      int32_t progress = 0x00000000;

      if (startWordAddr >= endWordAddr)
      {
        Serial.write("ERR1");
      }
      else
      {
        while (totalSize >= 0x200)
        {          
          // Dump 1kbyte block (0x200 words):
          for (uint32_t i = 0; i < 512; i++)
            buffer[i] = read_K5L(startWordAddr + i + progress);

          for (uint32_t i = 0; i < 512; i++) {
            Serial.write(buffer[i] & 0x00ff);
            Serial.write((buffer[i] & 0xff00) >> 8);
          }

          totalSize -= 0x200;
          progress += 0x200;
        }
        if(totalSize > 0)
        {
          for (uint32_t i = 0; i < totalSize; i++)
            buffer[i] = read_K5L(startWordAddr + i + progress);

          for (uint32_t i = 0; i < totalSize; i++) {
            Serial.write(buffer[i] & 0x00ff);
            Serial.write((buffer[i] & 0xff00) >> 8);
          }
        }
        Serial.write("OK");
      }
    } else {
      Serial.write("ERR0");
    }
  }
}