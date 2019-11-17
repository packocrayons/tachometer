#define PHOTODIODE_PIN 2
//1 second interrupt fire
#define OCR1A_val 15624
//how long until we save to EEPROM
#define SECONDS_TO_IDLE_MODE 3
//the number of blades/interrupts in one rotation
#define PULSES_PER_ROTATION 2
//Start address for eeprom data
#define EEPROM_BASE_ADDRESS 0
#define LCD_WIDTH 20

#define VOLTAGE_PIN 1
//TFW you can only afford 10% resistors
#define VOLTAGE_DIVIDER_CONSTANT 10.845

#define AREF 5

struct EEPROMStruct{
	uint32_t totalMillis;
	int totalHours;
};

byte hourGlass1[] = {
  B11111,
  B11111,
  B01110,
  B00100,
  B00100,
  B01010,
  B10001,
  B11111
};

byte hourGlass2[] = {
  B11111,
  B10001,
  B01010,
  B00100,
  B00100,
  B01110,
  B11111,
  B11111
};
