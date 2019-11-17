#include <EEPROM.h>
#include <LiquidCrystal.h>
#include "tachometer.h"

struct EEPROMStruct EEPROMData;

volatile uint16_t rotations, oldRotations; //Thanks to bus size, only allows 65k RPM. Boring
uint16_t idle, RPM, MAXRPM;
volatile bool updateDisplay;
uint32_t oldMillis;

//Is this a constructor? on an embedded system? eww.
LiquidCrystal lcd(12, 11, 10, 9, 8, 6); //apparently pin 7 can cause hangs thanks to the arduino bootloader

void setup() {

	pinMode(PHOTODIODE_PIN, INPUT);
  
	rotations=0;
	idle=0;
	updateDisplay=0;
	oldMillis = 0;


	lcd.begin(LCD_WIDTH, 2);
	lcd.print("booting");
	lcd.noCursor();

	lcd.createChar(0, hourGlass1);
	lcd.createChar(1, hourGlass2);

	readDataFromEEPROM();

	cli();
	attachInterrupt(digitalPinToInterrupt(PHOTODIODE_PIN), rpmTrigger, RISING);

	TCCR1A = 0;// set entire TCCR1A register to 0
	TCCR1B = 0;// same for TCCR1B
	TCNT1  = 0;//initialize counter value to 0
	OCR1A = OCR1A_val;
	// turn on CTC mode
	TCCR1B |= (1 << WGM12);
	// Set CS10 and CS12 bits for 1024 prescaler
	TCCR1B |= (1 << CS12) | (1 << CS10);  
	// enable timer compare interrupt
	TIMSK1 |= (1 << OCIE1A);

  	sei();

}


int numDigits(int x){
	int n = 0;
	while (x > 10){
		n++;
		x = x/10;
	}
	return (n + 1);
}

float getBatteryVoltage(){
	float voltage = (analogRead(VOLTAGE_PIN) * VOLTAGE_DIVIDER_CONSTANT * AREF)/ 1024;
	return voltage;
}

//these helper functions are arguably hacky, but straightforward
void writeDataToEEPROM(){  
  int i; char* x;
	for(x = (char*) &EEPROMData, i = 0; i < sizeof(struct EEPROMStruct); ++i, ++x){ //walk through the struct (however big it is) one byte at a time
		EEPROM.write(EEPROM_BASE_ADDRESS + i, *x); //write the one-byte portion of the data to eeprom
	}
}

void readDataFromEEPROM(){
  int i; char* x;
	for(x = (char*) &EEPROMData, i = 0; i < sizeof(struct EEPROMStruct); ++i, ++x){ //walk through the struct (however big it is) one byte at a time
		*x = EEPROM.read(EEPROM_BASE_ADDRESS + i);
	}
}

//save hours to EEPROM, indicate somehow on the LCD
void idleMode(){
	writeDataToEEPROM();
}

//everything else is globals for now because ISRs need to access it. 
void lcdUpdate(float voltage){
	static byte hglass = 0;

	lcd.clear();

	lcd.print("RPM ");
	lcd.print(RPM, DEC);

	lcd.setCursor(LCD_WIDTH - (numDigits(EEPROMData.totalHours) + 2), 0);
	lcd.write(hglass);
	lcd.write(" ");
	lcd.print(EEPROMData.totalHours, DEC);

	lcd.setCursor(0, 1);
	lcd.print("MAX ");
	lcd.print(MAXRPM, DEC);

	//4 digits of float plus "VOL "
	lcd.setCursor(LCD_WIDTH - (4 + 4), 1);
	lcd.print("VOL ");
	lcd.print(voltage, 4);

	if (!idle){
		hglass ^= 1;
	}
}

void loop() {
	//recommend a sleep here to avoid polling
	if (updateDisplay){
		if (idle > SECONDS_TO_IDLE_MODE){
			idleMode();
		}

		lcdUpdate(getBatteryVoltage());

		updateDisplay = 0;
	}

}

SIGNAL(TIMER1_COMPA_vect){
	if (rotations == 0){
		idle++;
	} else {
		idle = 0;
		oldRotations = rotations;
	}

	unsigned long elapsedTime = (millis() - oldMillis);
  oldMillis += elapsedTime; 
	RPM = ((rotations / PULSES_PER_ROTATION) * ((uint32_t) 1000 * 60)) / (elapsedTime);
	rotations = 0; //reset for next go around. Hopefully don't miss any between the last line and this one (math is hard)
	if (RPM > MAXRPM){
		MAXRPM = RPM;
	}

	EEPROMData.totalMillis += elapsedTime;
	EEPROMData.totalHours = floor(EEPROMData.totalMillis / ((uint32_t) 1000 * 60 * 60));

	updateDisplay = 1;
}	

void rpmTrigger(){
	rotations++;
}
