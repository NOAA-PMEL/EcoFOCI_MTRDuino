//include required header files for Arduino funtions, SPI functionality, DS3234 clock, and SD Card
#include <Wire.h>
#include <ArduinoPins.h>
#include <ctype.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>
#include <RTC_DS3234.h>

// Avoid spurious warnings
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];}))
#define BUFF_MAX 256


    
#define RTCPIN 10        //Define Output Pin for RTC 
#define SDPIN 9          //Define Output Pin for SDCARD
#define POWERPIN 12      //Define Digital Output Pin for Power Valve 
#define WAKEUPPIN 1      //Define Analog Input Pin for Clock Wakeup Reading 
#define THERMPIN 0       //Define Analog Input Pin for Thermistor Reading 
#define CREG B00000101   //Define Control Register byte for DS3234 RTC 
const float VREF = 4.096;      //Define Voltage Reference from LM4040 Diode
    
RTC_DS3234 RTC(RTCPIN);    // Create a RTC instance, using the chip select pin it's connected to
int alarminterval = 30;
      
void setup () {    
    Wire.begin();                  // Start Arduino 
    analogReference(EXTERNAL);     // set analog reference to 4.096V from LM4040 voltage regulator diode
    pinMode(THERMPIN,INPUT);       // setup Thermistor Pin
    pinMode(RTCPIN,OUTPUT);        // setup RTC Chip
    pinMode(POWERPIN,OUTPUT);      // setup Power Valve Pin
    SPI.begin();                   // Start SPI functionality to talk to SD Card and RTC 
    SPI.setBitOrder(MSBFIRST);     // Set bitorder for SPI (required for setting registers on DS3234 RTC)

    digitalWrite(POWERPIN, HIGH);  
    RTC.begin();
    
    //If there is no battery connected, go to Setup mode.
    //This will establish a serial connection and reset the RTC    
    /*
    //if(nobattery){
      Serial.begin(9600);   //Establish Serial Connection
      while(!Serial){}      //Required for Arduino Micro to wait for Serial Connection to establish
      delay(5000);    //Wait a few seconds to allow user to open the Serial Window (Ctrl+Shift+M)
      
      RTC.adjust(DateTime(__DATE__, __TIME__));    //Sync RTC on Arduino with compiler's clock (will set Arduino RTC to same time as computer used to load code) 
      DateTime now = RTC.now();  //Get time from RTC
      DateTime alarmtime = (now.unixtime() + alarminterval);  //calculate alarm time by adding 30 seconds to current time
      setAlarmTime(alarmtime);  //Write to registers on RTC to set alarm
  
      SDCardCheck();  // initialize SDCARD, print status in Serial Window
      
      //display Current Time and Alarm Time:
      const int len = 32;
      static char buf[len];
      Serial.print("Current Time:     ");
      Serial.println(now.toString(buf,len));
      Serial.print("Alarm time set to:     ");
      Serial.println(alarmtime.toString(buf,len));
    //}
    */
}

void loop () {   
    DateTime now = RTC.now();          //Get time from RTC
    char flagval = RTCRead(0x0F);      //Read Flag Byte from DS3234 RTC Register
    char alarm1status = flagval % 2;   //Get status of flag for alarm 1 (uses last bit of the sequence, so use mod function to see if number is odd or even)
    float therm_val = analogRead(THERMPIN);  //read value from analog input for thermistor
    float therm_voltage = therm_val / 1023.0 * VREF;
    float therm_resistance = 10000.0/(VREF/therm_voltage-1.0);  //Calculate value of thermistor resistance based on voltage divider circuit
          
    const int len = 32;
    static char buf[len];
    Serial.print("Current time:     ");
    Serial.println(now.toString(buf,len));
    Serial.print("Thermistor Pin Voltage Reading =    ");
    Serial.println(therm_resistance);
    Serial.print("Flag Value =      ");
    Serial.println(flagval,BIN);
    Serial.print("Alarm 1 Status =      ");
    Serial.println(flagval,BIN);

    
    //If alarm has tripped, log value and reset Alarm
      
      //Drive output to power valve high so arduino doesn't shut off when alarm is reset
      digitalWrite(POWERPIN, HIGH);       
      
      DateTime alarmtime = (now.unixtime() + alarminterval);
      setAlarmTime(alarmtime);
      
      Serial.print("Current Time:     ");
      Serial.println(now.toString(buf,len));
      Serial.print("Alarm time set to:     ");
      Serial.println(alarmtime.toString(buf,len));

      SDCardWrite(therm_resistance,now.unixtime());
      //Drive output to power valve low (go to sleep and wake up on RTC alarm only)
      
      digitalWrite(POWERPIN, LOW);       
  
      Serial.println();


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
    RTCWrite(0x8E,CREG);
}

void SDCardCheck(){
    pinMode(10,OUTPUT);
    if (!SD.begin(SDPIN)) {
        Serial.println("SD Card Initialization failed!  Check connections and if SD Card is Present");
      return;
    }
        Serial.println("SD Card Initialized successfully");
}

void SDCardWrite(int val,long timestamp){
  pinMode(9,OUTPUT);
  SD.begin(9);  
  File dataFile = SD.open("newtest.txt", FILE_WRITE);

  // if the file is available, write to it:
  if (dataFile) {
    dataFile.print(val);
    dataFile.print(", ");
    dataFile.println(timestamp);
    dataFile.close();
    // print to the serial port too:
        Serial.println(val);
  }  
  // if the file isn't open, pop up an error:
  else {
        Serial.println("error opening data file");
  } 
}
