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
    //Define Analog Output Pin for Power Valve 
#define POWERPIN A0
    //Define Analog Input Pin for Clock Wakeup Reading 
#define WAKEUPPIN 1    
    //Define Analog Input Pin for Thermistor Reading 
#define THERMPIN 4

// Create a RTC instance, using the chip select pin it's connected to
RTC_DS3234 RTC(RTCPIN);

int numalarms;

void setup () {    
    Wire.begin();
    pinMode(THERMPIN,INPUT); // setup Thermistor Pin
    pinMode(RTCPIN,OUTPUT); // setup RTC Chip
    SPI.begin();
    RTC.begin();
        
    DateTime now = RTC.now();
    
    //Try to Establish Serial Connection
    Serial.begin(9600);
    delay(5000);

    //If there is a serial connection, Reset clock and setup alarm.  Otherwise, go to datalog mode
    if(Serial){      
      RTC.adjust(DateTime(__DATE__, __TIME__));
      now = RTC.now();

      //calculate alarm time by adding 30 seconds to current time
      DateTime alarmtime = (now.unixtime() + 30);
      setAlarmTime(alarmtime);
  
      const int len = 32;
      static char buf[len];
      Serial.print("Current Time:     ");
      Serial.println(now.toString(buf,len));
      Serial.print("Alarm time set to:     ");
      Serial.println(alarmtime.toString(buf,len));
          

    }  
        // setup SDCARD
    SDCardStart();
    numalarms=0;

}
void loop () {
  pinMode(POWERPIN,OUTPUT); // setup Power Valve Pin
        
  pinMode(RTCPIN,OUTPUT); // setup RTC Chip
    SPI.begin();
    
    const int len = 32;
    static char buf[len];
    DateTime now = RTC.now();
    
    char flagval = 0x00;
    flagval = RTCRead(0x0F);
    char alarm1status = flagval % 2;
    
    if(Serial){         
      Serial.print("Current time:     ");
      Serial.println(now.toString(buf,len));
      int enableval = analogRead(WAKEUPPIN);
      Serial.print("Analog pin 1 Voltage Reading =    ");
      Serial.println(enableval);
      Serial.print("Flag Value =      ");
      Serial.println(flagval,BIN);
      Serial.print("Alarm 1 Status =      ");
      Serial.println(flagval,BIN);
    }
    
    //If alarm has tripped, log value and reset Alarm
    if (alarm1status==1){
      
      //Drive output to power valve high so arduino doesn't shut off when alarm is reset
      analogWrite(POWERPIN, 255);       
      
      numalarms++;
      RTCWrite(0x8E,B01100000);
      DateTime alarmtime = (now.unixtime() + 30);
      setAlarmTime(alarmtime);
      
      if(Serial){    
        const int len = 32;
        static char buf[len];      
        Serial.print("Current Time:     ");
        Serial.println(now.toString(buf,len));
        Serial.print("Alarm time set to:     ");
        Serial.println(alarmtime.toString(buf,len));
      }
      SDCardWrite(numalarms,now.unixtime());
      //Drive output to power valve low (go to sleep and wake up on RTC alarm only)
      
      analogWrite(POWERPIN, 0);       
      
    }
    if(Serial){    
      Serial.println();
    }
    
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
    
        //Set Alarm #2 to zll zeroes (disable)
    RTCWrite(0x8B,0);
    RTCWrite(0x8C,0);
    RTCWrite(0x8D,0);
   
        //reset flags 
        //8F=write to location for control/status flags
        //B00000000=Ocillator Stop Flag 0, No Batt Backed 32 KHz Output, Keep Temp CONV Rate at 64 sec (may change later), disable 32 KHz output, temp Not Busy, alarm 2 not tripped, alarm 1 not tripped
    RTCWrite(0x8F,B00000000);
    
        //set control register 
        //8E=write to location for control register
        //B01100101=Oscillator always on, SQW on, Convert Temp off, SQW freq@ 1Hz, Interrupt enabled, Alarm 2 off, Alarm 1 on
    RTCWrite(0x8E,B01100001);
}

void SDCardStart(){
    pinMode(10,OUTPUT);
    if (!SD.begin(SDPIN)) {
          if(Serial){Serial.println("initialization failed!");}
      return;
    }
        if(Serial){Serial.println("SD Card Initialized");}  
}

void SDCardWrite(int val,int timestamp){
  pinMode(9,OUTPUT);
  SD.begin(9);  
  File dataFile = SD.open("datayay.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.print(val);
    dataFile.print(", ");
    dataFile.println(timestamp);
    dataFile.close();
    // print to the serial port too:
        if(Serial){Serial.println(val);}
  }  
  // if the file isn't open, pop up an error:
  else {
        if(Serial){Serial.println("error opening data file");}
  } 
}
