//include required header files for DS3234 clock, SPI Interface, and SD Card
#include <SD.h>
#include <ArduinoPins.h>
#include <ctype.h>
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
#define RTCPIN 10        
    //Define Output Pin for SDCARD
#define SDPIN 9        
    //Define Analog Input Pin for Thermistor Reading 
#define THERMPIN 4
    //Define Analog Input Pin for Clock Wakeup Reading 
#define WAKEUPPIN 1

// Create a RTC instance, using the chip select pin it's connected to
RTC_DS3234 RTC(RTCPIN);

void setup () {
    Serial.begin(9600);
    delay(5000);
    Wire.begin();
    SPI.begin();
    RTC.begin();
    RTC.adjust(DateTime(__DATE__, __TIME__));
    
    pinMode(THERMPIN,INPUT); // setup Thermistor Pin
    pinMode(ENABLEPIN,INPUT); // setup PowerBooster Pin
    pinMode(RTCPIN,OUTPUT); // setup RTC Chip
    
        //calculate alarm time by adding 30 seconds to current time
    DateTime now = RTC.now();
    DateTime alarmtime = (now.unixtime() + 30);
    setAlarmTime(alarmtime);

    const int len = 32;
    static char buf[len];
    Serial.print("Alarm time:     ");
    Serial.println(alarmtime.toString(buf,len));
        
        //reset flags 
        //8F=write to location for control/status flags
        //B00000000=Ocillator Stop Flag 0, No Batt Backed 32 KHz Output, Keep Temp CONV Rate at 64 sec (may change later), disable 32 KHz output, temp Not Busy, alarm 2 not tripped, alarm 1 not tripped
    RTCWrite(0x8F,B00000000);
  
        //set control register 
        //8E=write to location for control register
        //B01100101=Oscillator always on, SQW on, Convert Temp off, SQW freq@ 1Hz, Interrupt enabled, Alarm 2 off, Alarm 1 on
    RTCWrite(0x8E,B01100001);

        // setup SDCARD
    SDCardStart();
}
void loop () {
    const int len = 32;
    static char buf[len];

    DateTime now = RTC.now();
     
    Serial.print("Current time:     ");
    Serial.println(now.toString(buf,len));
    
    int wakeup = analogRead(WAKEUPPIN);
    Serial.print("Analog pin 2 =    ");
    Serial.println(wakeup);
    
    int enableval = analogRead(ENABLEPIN);
    Serial.print("Analog pin 1 =    ");
    Serial.println(enableval);
 
    char flagval = 0x00;
    flagval = RTCRead(0x0F);
    Serial.print("Flag Value =      ");
    Serial.println(flagval,BIN);
   
    Serial.println();
    
    SDCardWrite(enableval);

    delay(5000);
}


//Routine to Write data to RTC Register VIA Arduino SPI
void RTCWrite(char reg, char val){
    noInterrupts();            //make sure transfew doesn't get interrupted
    digitalWrite(RTCPIN, LOW);     //enable SPI read/write for chip
    SPI.transfer(reg);         //define memory register location
    SPI.transfer(bin2bcd(val));         //write value
    digitalWrite(RTCPIN, HIGH);    //disable SPI read/write for chip
    delay(10);                 //delay 10 ms to make sure chip is off
    interrupts();              //resume normal operation
}

//Routine to Read data From RTC Register VIA Arduino SPI
char RTCRead(char reg){
    noInterrupts();                     //make sure transfew doesn't get interrupted
    char val = 0x00;                    //initialize return variable 
    digitalWrite(RTCPIN, LOW);              //enable SPI read/write for chip
    SPI.transfer(reg);                  //define memory register location
    val = bcd2bin(SPI.transfer(-1));    //retrieve value
    digitalWrite(RTCPIN, HIGH);             //disable SPI read/write for chip
    delay(10);                          //delay 10 ms to make sure chip is off
    interrupts();                       //resume normal operation
    return val;
}

void setAlarmTime(DateTime alarmtime){
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
        //8A=write to location for alarm day
        //binary & day with 0x3F required to turn alarm day "on" (not dayofWeek)
    RTCWrite(0x8A,alarmtime.day() & 0x3F);
}

void SDCardStart(){
    pinMode(10,OUTPUT);
    if (!SD.begin(SDPIN)) {
      Serial.println("initialization failed!");
      return;
    }
    Serial.println("initialization done.");     
}

void SDCardWrite(int val){
  pinMode(9,OUTPUT);
  SD.begin(9);  
  File dataFile = SD.open("datalog2.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.println(val);
    dataFile.close();
    // print to the serial port too:
    Serial.println(val);
  }  
  // if the file isn't open, pop up an error:
  else {
    Serial.println("error opening datalog2.txt");
  } 
}
