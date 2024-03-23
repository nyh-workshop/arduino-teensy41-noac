// Ref: https://forum.pjrc.com/index.php?threads/reading-multiple-gpio-pins-on-the-teensy-4-0-atomically.58377/page-2
#define IMXRT_GPIO6_DIRECT (*(volatile uint32_t *)0x42000000)
#define IMXRT_GPIO9_DIRECT (*(volatile uint32_t *)0x4200C000)
#define IMXRT_GPIO7_DIRECT (*(volatile uint32_t *)0x42004000)

// It works but had to use 912MHz!! :o

// note: the system starts at 0x7FFFC, but because the data lines are 16-bit,
// it will pick up 0x3FFFE instead!

uint16_t buffer[65536];
uint16_t memory1[65536];

constexpr uint8_t NOAC_OE_PIN = 30;
constexpr uint8_t NOAC_CE_PIN = 31;
constexpr uint8_t NOAC_RST_PIN = 25;
constexpr uint32_t NOAC_RST_ADDR = 0x3FFFE;

struct addrBits {
  uint32_t part0 : 3;
  uint32_t part1 : 1;
  uint32_t part2 : 5;
  uint32_t part3 : 3;
  uint32_t part4 : 4;
  uint32_t part5 : 2;
  uint32_t part6 : 2;
};

union addrPort {
  struct addrBits addrBitsSet;
  uint32_t port;
};

constexpr uint32_t GPIO7_PINMASK = 0b00110000000011110001110000000111;
constexpr uint32_t GPIO9_PINMASK = 0b10000000000000000000000111110000;
constexpr uint32_t GPIO6_PINMASK = 0b00000000000000000011000000001100;

inline uint32_t getAddrTest() {
  // tested at 40.0ns

  // these pins are input only!
  // gpio7 = GPIO7_DR -> A0-A2, A9-A17
  //uint32_t data_gpio7 = IMXRT_GPIO7_DIRECT & 0b00110000000011110001110000001111;
  uint32_t data_gpio7 = GPIO7_PSR & GPIO7_PINMASK;
  // gpio9 = GPIO9_DR -> A3, A4-A8
  //uint32_t data_gpio9 = IMXRT_GPIO9_DIRECT & 0b00000000000000000000000111110000;
  uint32_t data_gpio9 = GPIO9_PSR & GPIO9_PINMASK;
  // gpio6 = GPIO6_DR -> A18-A21
  //uint32_t data_gpio6 = IMXRT_GPIO6_DIRECT & 0b00000000000000000011000000001100;
  uint32_t data_gpio6 = GPIO6_PSR & GPIO6_PINMASK;

  struct addrBits s0;
  s0.part0 = data_gpio7 & 0x0007;
  s0.part1 = data_gpio9 >> 31;
  s0.part2 = data_gpio9 >> 4;
  s0.part3 = data_gpio7 >> 10;
  s0.part4 = data_gpio7 >> 16;
  s0.part5 = data_gpio7 >> 28;
  s0.part6 = data_gpio6 >> 12;

  union addrPort ap = { s0 };
  return ap.port;
}

inline void enableData() {
  GPIO6_GDIR |= 0xffff0000;
}

inline void disableData() {
  GPIO6_GDIR &= 0x0000ffff;
}

inline void putData(uint32_t data) {
  GPIO6_DR = data << 16;  
}

inline void setGPIO6789edgeSel() {
  GPIO6_EDGE_SEL = GPIO6_PINMASK;
  GPIO7_EDGE_SEL = GPIO7_PINMASK;
  GPIO9_EDGE_SEL = GPIO9_PINMASK;
  
  GPIO6_IMR = GPIO6_PINMASK;
  GPIO7_IMR = GPIO7_PINMASK;
  GPIO9_IMR = GPIO9_PINMASK;
}

void chgIntrGPIO6789()
{
  volatile uint32_t addr6502 = getAddrTest();

  if(!((GPIO8_PSR & 0xC00000) >> 22))
  {
    enableData();
    putData(decode6502(addr6502));
  }
  else {
    disableData();
  }
  //Serial.printf("Pin Change detected! Value: %08x\n", chgAddr);
  GPIO6_ISR = GPIO6_PINMASK;
  GPIO7_ISR = GPIO7_PINMASK;
  GPIO9_ISR = GPIO9_PINMASK;
}

char fileName[16];

// https://forum.pjrc.com/index.php?threads/how-to-trigger-software-reset-on-teensy-4-1.69849/
void resetTeensy41()
{
  SCB_AIRCR = 0x05FA0004;
}

void setup() {
  // put your setup code here, to run once:
  pinMode(NOAC_RST_PIN, OUTPUT);

  pinMode(NOAC_OE_PIN, INPUT_PULLUP);
  pinMode(NOAC_CE_PIN, INPUT_PULLUP);

  // Reset pin:
  //pinMode(28, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(28), resetTeensy41, FALLING);

  // Address pins:
  // teensy pins 2-13:
  for (uint32_t i = 2; i < 14; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  pinMode(24, INPUT_PULLUP);
  pinMode(25, INPUT_PULLUP);
  pinMode(29, INPUT_PULLUP);

  // teensy pins 32-37:
  for (uint32_t i = 32; i < 38; i++) {
    pinMode(i, INPUT_PULLUP);
  }

  pinMode(13, OUTPUT);

  // Data pins:
  pinMode(19, OUTPUT);
  pinMode(18, OUTPUT);
  pinMode(14, OUTPUT);
  pinMode(15, OUTPUT);
  pinMode(40, OUTPUT);
  pinMode(41, OUTPUT);
  pinMode(17, OUTPUT);
  pinMode(16, OUTPUT);
  pinMode(22, OUTPUT);
  pinMode(23, OUTPUT);
  pinMode(20, OUTPUT);
  pinMode(21, OUTPUT);
  pinMode(38, OUTPUT);
  pinMode(39, OUTPUT);
  pinMode(26, OUTPUT);
  pinMode(27, OUTPUT);

  Serial.begin(115200);

  Serial.println("Downloading binary:");
  
  int fileSize = Ymodem_Receive((unsigned char*)buffer, 128 * 1024, fileName);

  if (fileSize <= 0)
  {
    Serial.printf("Error downloading binary! Error no.: %d\n", fileSize);
  }
  else {
    Serial.println("Receive done!");
    Serial.printf("File name: %s\nFile size: %d\n", fileName, fileSize);
    hexDump(const_cast<char *>("First 128 bytes: "), buffer, 128);
  }

  memcpy(memory1,  buffer, 65536*2);

  Serial.printf("Reset vector: %04x\n", memory1[0xFFFE]);

  setGPIO6789edgeSel();
  attachInterruptVector(IRQ_GPIO6789, &chgIntrGPIO6789);
  NVIC_ENABLE_IRQ(IRQ_GPIO6789);

  // NOAC_RST_PIN is connected to the NPN transistor base.
  // It is active high:
  digitalWrite(NOAC_RST_PIN, LOW);
  delay(500);
  digitalWrite(NOAC_RST_PIN, HIGH);
  delay(500);
  digitalWrite(NOAC_RST_PIN, LOW);

  Serial.printf("Done setting up!\n");

  delay(100);
}

inline uint16_t decode6502(uint32_t aAddr)
{
    return memory1[aAddr & 0x0000ffff];
}

// inline void oneBusNOAC_op()
// {
//   addr6502 = getAddrTest();
//   if(!digitalReadFast(NOAC_CE_PIN) && !digitalReadFast(NOAC_OE_PIN))
//   {
//     enableData();
//     putData(decode6502(addr6502));
//   }
//   else {
//     disableData();
//   }
// }

uint32_t addr = 0x0000;

void loop() {
  //addr = getAddrTest();
  //Serial.printf("Addr: 0x%08x\n", addr);
  //delay(1000);
}