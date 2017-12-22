// Date and time functions using a DS1307 RTC connected via I2C and Wire lib

#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <RTC_DS3234.h>

// Avoid spurious warnings
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];}))
#define BUFF_MAX 256

    //Define Output Pin for RTC 
#define cs 10        
    //Define Analog Input Pin for Thermistor Reading 
#define THERMPIN 2

// Create a RTC instance, using the chip select pin it's connected to
RTC_DS3234 RTC(cs);

void setup () {
    Serial.begin(57600);
    Wire.begin();
    SPI.begin();
    RTC.begin();
    RTC.adjust(DateTime(__DATE__, __TIME__));
    
        //adjust clock settings
    pinMode(cs,OUTPUT); // setup RTC Chip
    pinMode(THERMPIN,INPUT); // setup Thermistor Pin
       
        //set control register 
        //8E=write to location for control register
        //B01100101=Oscillator always on, SQW on, Convert Temp off, SQW freq@ 1Hz, Interrupt enabled, Alarm 2 off, Alarm 1 on
    RTCWrite(0x8E,B01100101);
    
        //calculate alarm time by adding 45 seconds to current time
    DateTime now = RTC.now();
    DateTime alarmtime = (now.unixtime() + 45);
    
        //set alarm time: seconds 
        //87=write to location for alarm seconds
        //binary & second with 0x7F required to turn alarm second "on"
    RTCWrite(0x87,alarmtime.second() & 0x7F);
  
        //set alarm time: minutes 
        //88=write to location for alarm minutes
        //binary & minute with 0x7F required to turn alarm minute "on"
    RTCWrite(0x88,alarmtime.minute() & 0x7F);
    
        //set alarm time: hour 
        //89=write to location for alarm hour
        //binary & hour with 0x7F required to turn alarm hour "on"
    RTCWrite(0x89,alarmtime.hour() & 0x7F);
  
        //set alarm time: day 
        //87=write to location for alarm day
        //binary & day with B01001111 required to turn alarm day "on" (not dayofWeek)
    RTCWrite(0x87,alarmtime.day() & B01001111);
        
        //reset flags 
        //8F=write to location for control/status flags
        //B00000000=Ocillator Stop Flag 0, No Batt Backed 32 KHz Output, Keep Temp CONV Rate at 64 sec (may change later), disable 32 KHz output, temp Not Busy, alarm 2 not tripped, alarm 1 not tripped
    RTCWrite(0x8E,B00000000);
  
}

void loop () {
    const int len = 32;
    static char buf[len];

    DateTime now = RTC.now();
    
    
    Serial.print("Current time:     ");
    Serial.println(now.toString(buf,len));
    
    //Serial.print(" since midnight 1/1/1970 = ");
    //Serial.print(now.unixtime());
    //Serial.print("sseconds, = ");
    //Serial.print(now.unixtime() / 86400L);
    //Serial.println("days");
    
    // calculate a date which is 7 days and 30 seconds into the future
    DateTime future (now.unixtime() + 7 * 86400L + 30 );
    
    //Serial.print(" now + 7d + 30s: ");
    //Serial.println(future.toString(buf,len));
    
    int thermval = analogRead(THERMPIN);
    Serial.print("Analog pin 2 =    ");
    Serial.println(thermval);
    Serial.println();

    char flagval = 0x00;
    flagval = RTCRead(0x0F);
    Serial.print("Flag Value =      ");
    Serial.println(flagval,DEC);
    Serial.println();
    
  
    delay(500);
}

//Routine to Write data to RTC Register VIA Arduino SPI
void RTCWrite(char reg, char val){
    noInterrupts();  
    digitalWrite(cs, LOW);     //enable SPI read/write for chip
    SPI.transfer(reg);         //define memory register location
    SPI.transfer(val);         //write value
    digitalWrite(cs, HIGH);    //disable SPI read/write for chip
    delay(10);                 //delay 10 ms to make sure chip is off
    interrupts();
}

//Routine to Read data From RTC Register VIA Arduino SPI
char RTCRead(char reg){
    noInterrupts();
    char val = 0x00;
    digitalWrite(cs, LOW);    //enable SPI read/write for chip
    SPI.transfer(reg);        //define memory register location
    val = bcd2bin(SPI.transfer(-1));   //retrieve value
    digitalWrite(cs, HIGH);   //disable SPI read/write for chip
    delay(10);                //delay 10 ms to make sure chip is off
    interrupts();
    return val;
}

