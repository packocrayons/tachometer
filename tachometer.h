#define PHOTODIODE_PIN 2
//1 second interrupt fire
#define OCR1A_val 15624
#define SECONDS_TO_IDLE_MODE 5
#define PULSES_PER_ROTATION 2
#define EEPROM_BASE_ADDRESS 0
#define LCD_WIDTH 20

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
