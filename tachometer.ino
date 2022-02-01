#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <math.h>
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

int isValidInterrupt(){
	static int lastMillis = millis();
	if ((millis() - lastMillis) >= MIN_INTERRUPT_INTERVAL){
		lastMillis = millis();
		return 1;
	}
	return 0;
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

int string_start_match(char* string, const char* string_to_match, int len){
	char* i = string;
  const char* j = string_to_match;
	for(int k = 0; k < len; i++, j++, k++){
		if (*i != *j) return 0;
	}
	return 1;
} 

void enable_gpvtg(){
	Serial.print(ENABLE_GPVTG_STRING);
}

struct nmea_gpvtg parse_nmea_gpvtg(char* string){
	struct nmea_gpvtg ret = {.TMG=NAN, .MTMG=NAN, .spd_kts=NAN, .spd_km=NAN, .cksum = 0};
	if (string_start_match(string, (const char*) "$GPVTG", 5)){
    char* i = string;
		for (int j = 0, commas=0; commas < 9; j++, i++){
			if (*i == ','){
				switch(commas){
					case 1:
						ret.TMG = atof(i);
						break;
					case 3: 
						ret.MTMG = atof(i);
						break;
					case 5:
						ret.spd_kts = atof(i);
						break;
					case 7: ret.spd_km = atof(i);
						break;
					case 2: case 4: case 6: case 8:
					default:
						continue;
				}
				commas++;
			}
		}
	}
	return ret;
}
			
		

//everything else is globals for now because ISRs need to access it. 
void lcdUpdate(float voltage, float spd){
	static byte hglass = 0;

	lcd.clear();

	lcd.print("RPM ");
	lcd.print(RPM, DEC);

	lcd.setCursor(LCD_WIDTH - (4 + 3), 0);
	lcd.print("K/H");
	lcd.print(spd, 4);

	//4 digits of float plus "V"
	lcd.setCursor(0, 1);
	lcd.print(voltage, 4);
	lcd.print("V");
	
	lcd.setCursor(LCD_WIDTH - (numDigits(EEPROMData.totalHours) + 2), 0);
	lcd.write(hglass);
	lcd.write(" ");
	lcd.print(EEPROMData.totalHours, DEC);
	
	if (!idle){
		hglass ^= 1;
	}
}

void loop() {
	//recommend a sleep here to avoid polling
	struct nmea_gpvtg gpt_info, temp;
	char nmea_string[80];
	int nmea_iterator = 0;
	while(Serial.available()){
		char rbyte = (char) Serial.read();
		if (rbyte == '$') nmea_iterator = 0;
		if (rbyte == '\r') temp = parse_nmea_gpvtg(nmea_string);
		nmea_string[nmea_iterator] = rbyte;
	}
	//if we got new data, update the main struct
	if (temp.spd_km == NAN){
		gpt_info = temp;
		temp = (nmea_gpvtg) {NAN, NAN, NAN, NAN};
		//update the display every time we get GPS data. This means there's no averaging so we'll see how it goes
		updateDisplay = 1;
	}
	if (updateDisplay){
		if (idle > SECONDS_TO_IDLE_MODE){
			idleMode();
		}

		lcdUpdate(getBatteryVoltage(), gpt_info.spd_km == NAN ? 0 : gpt_info.spd_km);

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
	if (isValidInterrupt()) rotations++;
}
