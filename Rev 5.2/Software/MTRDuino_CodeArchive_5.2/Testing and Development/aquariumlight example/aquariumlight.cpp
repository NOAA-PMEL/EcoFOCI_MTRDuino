#include <SPI.h>
#include <EEPROM.h>
#include "pins_arduino.h"

// RTC registry addresses
#define HOURS_WRITE 0x82
#define HOURS 0x02
#define MINUTES_WRITE 0x81
#define MINUTES 0x01
#define SECONDS_WRITE 0x80
#define SECONDS 0x00
#define ALARM2_HOURS_WRITE 0x8C
#define ALARM2_HOURS 0x0C
#define ALARM2_MINUTES_WRITE 0x8B
#define ALARM2_MINUTES 0x0B
#define ALARM1_HOURS_WRITE 0x89
#define ALARM1_HOURS 0x09
#define ALARM1_MINUTES_WRITE 0x88
#define ALARM1_MINUTES 0x08
#define ALARM1_SECONDS_WRITE 0x87
#define ALARM1_SECONDS 0x07
#define ALARM1_DATE_WRITE 0x8A
#define ALARM1_DATE 0x0A
#define CONTROL_WRITE 0x8E
#define CONTROL 0x0E
#define STATUS_WRITE 0x8F
#define STATUS 0x0F

// define pin connected to the RTC alarm (!INT) pin
#define ALARM 2
// define the pin which is connected to the LED driver
#define LED 3

// this sets the maximum PWM level for brightness
int max_brightness = 255;
// this sets the timing period, ie. for how long the lights are on (in hours)
char timing_period = 11;
// variable so we know if we are going to increase or decrease the PWM value
bool direction_up = true;
// this variable sets how many interrupts (they come in once a second) it takes to change the PWM value
int pwm_speed = 3;

// these variables not for the user to modify
int brightness = 0;
char incomingByte = 0x00;
int interrupt_cycle = 0;
bool enabled = true;

void setup() {

	noInterrupts();

// define IO
	pinMode(ALARM, INPUT);
	pinMode(LED, OUTPUT);
	digitalWrite(ALARM, HIGH);
	digitalWrite(LED, LOW);

	pwm_speed = EEPROM.read(0x01);												
	timing_period = EEPROM.read(0x02);
// check if the eeprom was empty or not										
	if(pwm_speed == 0xff) { pwm_speed = 0; }
	if(timing_period == 0xff) { timing_period = 11; }

// start UART
	Serial.begin(9600);
// set SPI mode 3
	SPI.setDataMode(SPI_MODE3);
// set SPI clock to system clock / 16 ie. 1MHz
	SPI.setClockDivider(SPI_CLOCK_DIV16);
// enable SPI
	SPI.begin();

	if(EEPROM.read(0x00) == 0) {
		enabled = false;
		enableAlarm1(false);
		enableAlarm2(false);
		brightness = 0;
		invAnalogWrite(LED, brightness);
	}
	else {
		enabled = true;
		enableAlarm1(true);
		enableAlarm2(true);
// set A2M4 bit (needed for alarm)
		writeRTC(0x8D, 0x80);
// disable both alarm flags, leave other bits untouched
		writeRTC(STATUS_WRITE, (readRTC(STATUS)&0xFC));

// check the time and see if we should put the lights on or off
		checkTime();
	}											

// attach RTC alarm to interrupt on pin 2
	attachInterrupt(0, changePWM, FALLING);
	interrupts();
  
}
void loop() {
  
	if (Serial.available() > 0) {

// read the incoming byte:
		incomingByte = Serial.read();
		switch(incomingByte) {

// if we get the symbol "t", we want to read the temperature from the RTC
			case 0x74:					

				Serial.flush();
				initTempConv();
// wait for the conversion to finish
				delay(250);

				Serial.print("Temperature: ");
				Serial.print(readRTC(0x11), DEC);
				Serial.print(".");

// figure out the decimals, does not work, needs fixing...
				switch (readRTC(0x12)) {
					case 0x00:
						Serial.print("0");
						break;
					case 0x40:
						Serial.print("25");
						break;
					case 0x80:
						Serial.print("50");
						break;
					case 0xC0:
						Serial.print("75");
						break;
				}
				Serial.println("C");
				break;

// if we receive symbol "h" which has to be followed by an ascii number value for the hours (eg. 01 or 13)
			case 0x68:

				incomingByte = hoursToBCD(readAsciiHours());
				checkAndWrite(HOURS_WRITE, incomingByte);
				printTime();
				checkTime();
				break;

// if we receive symbol "m" which has to be followed by a value for the minutes register (the amount of minutes straight in hex, like 0x40 is 40 minutes etc.)
			case 0x6D:

				incomingByte = minutesToBCD(readAsciiMinutes());
				checkAndWrite(MINUTES_WRITE, incomingByte);
				printTime();
				checkTime();
				break;

// if we receive symbol "s" which has to be followed by a value for the seconds register (the amount of seconds straight in hex)
			case 0x73:

				incomingByte = minutesToBCD(readAsciiMinutes());
				checkAndWrite(SECONDS_WRITE, incomingByte);
				printTime();
				break;

// if we receive symbol "a" which has to be followed by an int value for the alarm2 hours (ie. convert decimal to hex)
			case 0x61:

				incomingByte = hoursToBCD(readAsciiHours());
				checkAndWrite(ALARM2_HOURS_WRITE, incomingByte);
				printAlarm2();
				checkTime();
				break;

// if we receive symbol "b" which has to be followed by a value for the alarm2 minutes register (the amount of minutes straight in hex)
			case 0x62:

				incomingByte = minutesToBCD(readAsciiMinutes());
				checkAndWrite(ALARM2_MINUTES_WRITE, incomingByte);
				printAlarm2();
				checkTime();
				break;

// if we receive symbol "c" which has to be followed by the timing_period value as ascii
			case 0x63:
				
				incomingByte = readAsciiHours();
				if(incomingByte > 0) { 
					timing_period = incomingByte;
					EEPROM.write(0x02, timing_period);
					checkTime();
				}
				printTp();
				break;

// if we receive symbol "d" which has to be followed by the pwm_speed value as two-number ascii (eg "01")
			case 0x64:

				incomingByte = readAsciiMinutes();
				pwm_speed = incomingByte;
				EEPROM.write(0x01, pwm_speed);
				printPwmSpeed();
				break;

// if we get symbol "e", disable or enable the alarms
			case 0x65:
				if(enabled == true) {
					enableAlarm1(false);
					enableAlarm2(false);
					Serial.println("Disabled");
					enabled = false;
					EEPROM.write(0x00, 0);
					brightness = 0;
					invAnalogWrite(LED, brightness);
				}
				else {
					enableAlarm1(true);
					enableAlarm2(true);
					Serial.println("Enabled");
					enabled = true;
					EEPROM.write(0x00, 1);
					checkTime();
				}
				break;

// if we receive symbol "p" which prints the current time and current alarm2 values to serial port
			case 0x70:

				printTime();
				printAlarm2();
				printAlarm1();
				printTp();
				printPwmSpeed();
				if(enabled = true) {
					Serial.println("Enabled: yes");
				}
				else {
					Serial.println("Enabled: no");
				}
				break;

// if we get LF ('\n') just do nothing and ignore it
			case 0x0A:
				break;

			default:

				Serial.flush();
// return input if it makes no sense and print some help
				Serial.println("Incorrect input");
				Serial.println("Correct input  (replace 00 with value):");
				Serial.println("h00, m00, s00 to change the time");
				Serial.println("a00, b00 to change the time of alarm");
				Serial.println("c00 to change the timing period");
				Serial.println("d00 to change the dimming speed");
				Serial.println("p to print current values");
				Serial.println("t to print temperature");
				Serial.println("e to enable or disable");
		}
	}

}
void changePWM() {

// disable both alarm flags, leave other bits untouched
	writeRTC(STATUS_WRITE, (readRTC(STATUS)&0xFC));

	switch (direction_up) {
		case true:
			if(brightness == 0) {
// pwm_speed variable does not affect the first cycle
				setAlarm1_every_second(true);
				enableAlarm1(true);
				brightness++;
				invAnalogWrite(LED, brightness);
			}
			else {
// if not enough interrupts have not occurred after last pwm change, just increase the interrupt_cycle timer
				if(interrupt_cycle == pwm_speed) {
					brightness++;
					interrupt_cycle = 0;
				
					if(brightness == max_brightness) {
						invAnalogWrite(LED, brightness);
						direction_up = false;
						setAlarm1_every_second(false);
// we are at full brightness, set alarm1 to timing_period ahead of alarm2
						setAlarm1();
					}
					else { invAnalogWrite(LED, brightness); }
				}
				else { interrupt_cycle++; }
			}
			break;
		case false:
			if(brightness == max_brightness) {
				setAlarm1_every_second(true);
				enableAlarm1(true);
				brightness--;
				invAnalogWrite(LED, brightness);
			}
			else {
				if(interrupt_cycle == pwm_speed) {
					brightness--;
					interrupt_cycle = 0;

					if(brightness == 0) {
						direction_up = true;
						enableAlarm1(false);
						invAnalogWrite(LED, brightness);
					}
					else { invAnalogWrite(LED, brightness); }
				}
				else { interrupt_cycle++; }
			}
			break;
	}
}
void checkAndWrite(char reg, char val) {

	if(val >= 0x00) {
		writeRTC(reg, val);
	}
	else {
		Serial.println("Incorrect input");
	}
	Serial.flush();

}
// check the time and if we should put the lights on or off
void checkTime() {

	int lighting_time = 0;
	int time = 0;
	lighting_time = convertTime(readRTC(ALARM2_HOURS), readRTC(ALARM2_MINUTES)) + (timing_period * 60);
	time = convertTime(readRTC(HOURS), readRTC(MINUTES));

	if(lighting_time >= 1440) {
  		lighting_time -= 1440;
	}

// if the time is inside the lighting period, fire up the lights
	if(time < lighting_time && time > convertTime(readRTC(ALARM2_HOURS), readRTC(ALARM2_MINUTES))) {
	
// make sure we are going to dim the lights next
		direction_up = false;
		brightness = max_brightness;
		invAnalogWrite(LED, brightness); 

// set control bits, enable both of the alarms
		writeRTC(CONTROL_WRITE, 0x07);
// set alarm1 to timing_period ahead of alarm2
		setAlarm1();
// set A1M4 bit to enable hours, minutes, seconds match for alarm1
		setAlarm1_every_second(false);

	}
	else {

		direction_up = true;
		brightness = 0;
		invAnalogWrite(LED, brightness);
// disable alarm1
		enableAlarm1(false);

	}

}
char convertAsciiNumber(char character) {

// deduct 0x30 from the ascii character to get a number
	return (character - 0x30);

}
// converts hours from BCD to int
char convertHours(char hours) {			

	char temp = 0x00;

// check tens of hours
	if(((hours >> 4)&1) == 1) {
		temp += 10;
	}
// check 20h
	if(((hours >> 5)&1) == 1) {
		temp += 20;
	}

// add all the remaining hours
	temp += hours&0x0F;
	return temp;

}
// converts time from register value to an int which contains amount of minutes passed
int convertTime(char hours, char minutes) {

	int time = 0;

// check tens of hours
	if(((hours >> 4)&1) == 1) {
		time += 600;
	}
// check 20h
	if(((hours >> 5)&1) == 1) {
		time += 1200;
	}
// add all the remaining hours
	time += (hours&0x0F)*60;
	
	time += (minutes >> 4)*10;
	time += minutes&0x0F;

	return time;
}
void enableAlarm1(bool val) {

	switch (val) {
		case true:
			writeRTC(CONTROL_WRITE, (readRTC(CONTROL)|0x01));
			break;
		case false:
// disable alarm1
			writeRTC(CONTROL_WRITE, (readRTC(CONTROL)&0xFE));
			break;
	}
}
void enableAlarm2(bool val) {

	switch (val) {
		case true:
			writeRTC(CONTROL_WRITE, (readRTC(CONTROL)|0x02));
			break;
		case false:
// disable alarm2
			writeRTC(CONTROL_WRITE, (readRTC(CONTROL)&0xFD));
			break;
	}
}
// converts hours as int to hours as BCD
char hoursToBCD(char hours) {

	char temp = 0x00;

	if(hours >= 20) {
  // switch 20h bit
		temp = temp|0x20;
		hours -= 20;
	}
	else if(hours >= 10) {
		temp = temp|0x10;
		hours -= 10;
	}

// add rest of the hours
	temp += hours;
	return temp;

}
void initTempConv() {

	writeRTC(CONTROL_WRITE, (readRTC(CONTROL)|0x20));

}
void invAnalogWrite(int pin, int value) {

	analogWrite(pin, ~value);  

}
char minutesToBCD(char minutes) {

	char temp = 0x00;

// take off tens of minutes and add to temp until we have 10min or less
	while(minutes >= 10) {
		minutes -= 10;
		temp++;
	}
// shift to make them tens of minutes in bcd
	temp = temp << 4;
// add rest of the minutes
	if(minutes > 0) {
		temp += minutes;
	}

	return temp;

}
void printAlarm1() {

	Serial.print("Current alarm1: ");
	Serial.print(convertHours(readRTC(ALARM1_HOURS)), DEC);
	Serial.print(":");
// while printing the hours, ignore the config bit
	Serial.print((readRTC(ALARM1_MINUTES)&0x7F), HEX);
	Serial.print("\n");

}
void printAlarm2() {

	Serial.print("Current alarm: ");
	Serial.print(convertHours(readRTC(ALARM2_HOURS)), DEC);
	Serial.print(":");
	Serial.print(readRTC(ALARM2_MINUTES), HEX);
	Serial.print("\n");

}
void printTime() {

	Serial.print("Current time: ");
	Serial.print(convertHours(readRTC(HOURS)), DEC);
	Serial.print(":");
	Serial.print(readRTC(MINUTES), HEX);
	Serial.print(":");
	Serial.print(readRTC(SECONDS), HEX);
	Serial.print("\n");

}
void printPwmSpeed() {

	Serial.print("Current PWM speed: ");
	Serial.print(pwm_speed, DEC);

}
void printTp() {

	Serial.print("Current timing period: ");
	Serial.print(timing_period, DEC);

}
char readAsciiHours() {

	char number1 = 0x00;
	char number2 = 0x00;
	char number = 0x00;

// convert user input to an int value
	number1 = convertAsciiNumber(waitForByte());
	number2 = convertAsciiNumber(waitForByte());

	number = 10*number1 + number2;

// check if the input makes any sense
	if(number >= 24 || number < 0) {
		Serial.println("Incorrect input");
		number = 0x00;
	}

	return number;

}
// this works for seconds as well
char readAsciiMinutes() {

	char number1 = 0x00;
	char number2 = 0x00;
	char number = 0x00;

// convert user input to an int value
	number1 = convertAsciiNumber(waitForByte());
	number2 = convertAsciiNumber(waitForByte());

	number = 10*number1 + number2;

// check if the input makes any sense
	if(number > 60 || number < 0) {
		Serial.println("Incorrect input");
		number = 0x00;
	}

	return number;

}
char readRTC(char reg) {

	noInterrupts();

	char temp = 0x00;
	digitalWrite(SS, LOW);
	SPI.transfer(reg);
// dont write anything, just put out clock signal and read
	temp = SPI.transfer(0x00);
	digitalWrite(10, HIGH);

	interrupts();
	return temp;

}
// set alarm1 to timing_period ahead of alarm2
void setAlarm1() {

	char hours = 0x00;

// calculate hours for timer1 hours
	hours = (convertHours(readRTC(ALARM2_HOURS)) + timing_period);

// if hours more than 24, deduct 24 for correct time
	if(hours >= 24) {
		hours -= 24;
	}

// make sure we dont touch the A1M4 bit
	writeRTC(ALARM1_HOURS_WRITE, ((readRTC(ALARM1_HOURS)&0x80)|hoursToBCD(hours)));
// set alarm1 minutes to same as alarm2, just the hours are alarm2_hours + timing period
	writeRTC(ALARM1_MINUTES_WRITE, ((readRTC(ALARM1_MINUTES)&0x80)|readRTC(ALARM2_MINUTES)));
// set alarm1 seconds to 0
	writeRTC(ALARM1_SECONDS_WRITE, ((readRTC(ALARM1_SECONDS)&0x80)));
	hours = 0x00;

}
void setAlarm1_every_second(bool val) {

	switch (val) {
		case true:

// change A1M1 bit via bitmask to make alarm1 give alarms every second
			writeRTC(ALARM1_SECONDS_WRITE, (readRTC(ALARM1_SECONDS)|0x80));
// change A1M2 bit via bitmask
			writeRTC(ALARM1_MINUTES_WRITE, (readRTC(ALARM1_MINUTES)|0x80));
// change A1M3 bit via bitmask
			writeRTC(ALARM1_HOURS_WRITE, (readRTC(ALARM1_HOURS)|0x80));
// change A1M4 bit via bitmask
			writeRTC(ALARM1_DATE_WRITE, (readRTC(ALARM1_DATE)|0x80));
			break;

		case false:

// change A1M1 bit via bitmask to make alarm1 give alarms when hours, seconds, and minutes match
			writeRTC(ALARM1_SECONDS_WRITE, (readRTC(ALARM1_SECONDS)&0x7F));
// change A1M2 bit via bitmask
			writeRTC(ALARM1_MINUTES_WRITE, (readRTC(ALARM1_MINUTES)&0x7F));
// change A1M3 bit via bitmask
			writeRTC(ALARM1_HOURS_WRITE, (readRTC(ALARM1_HOURS)&0x7F));
			break;
	}


}
char waitForByte() {

	char temp = 0x00;
// wait for the UART to fill up
	while(Serial.available() == 0) { }
	temp = Serial.read();
	return temp;

}
void writeRTC(char reg, char val) {

	noInterrupts();	

	digitalWrite(SS, LOW);
// select register
	SPI.transfer(reg);
// print value to register
	SPI.transfer(val);
	digitalWrite(SS, HIGH);

	interrupts();
  
}
