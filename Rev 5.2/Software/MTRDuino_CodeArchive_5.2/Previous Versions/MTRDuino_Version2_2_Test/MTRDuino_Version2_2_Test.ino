//include required header files for Arduino funtions, SPI functionality, DS3234 clock, and SD Card
#include <ctype.h>
#include <Wire.h>
//#include <ArduinoPins.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>0
#include <RTC_DS3234.h>

//*********** SET LOGGING VARIABLES HERE: ***********
const int sampleinterval = 30;
const int numSamples = 10;
//*********** SET LOGGING VARIABLES HERE: ***********

// Avoid spurious warnings
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];}))
#define BUFF_MAX 256
    
#define RTCPIN 11               //Define Output Pin for RTC 
#define SDPIN 10                //Define Output Pin for SDCARD
#define POWERPIN 12             //Define Digital Output Pin for Power Valve 
#define CREG B00000101          //Define Control Register byte for DS3234 RTC 
#define ads1100write 0x49       //Define ADC control register
RTC_DS3234 RTC(RTCPIN);         //Create a RTC instance, using the chip select pin it's connected to
int measurements[numSamples];   //Create array for storing measurements
DateTime alarmtime;

void setup () {    
    initializePins();                                          //Initial Setup
    DateTime now = RTC.now();                                  //Get time from RTC
    alarmtime = (now.unixtime() + sampleinterval);             //Determine next alarm time based on sample interval 
    setAlarmTime(alarmtime);                                   //Set the alarm
    ADCSetup();                                                //Set up the ADC for conversions
    for (int j = 0;j < numSamples;j++){                        //Loop for number of samples
        measurements[j] = ADCRead();                           //Take measurements and store in an array
    }
    SDCardWrite(now);                                          //Write all data to the SD Card
    digitalWrite(POWERPIN, LOW);                               //Drive output to power pin low (go to sleep and wake up on RTC alarm only)
    delay(5000);                                               //Delay to allow voltages to settle and Arduino to go to sleep
}

void loop () {   
      //*******This Section of code will only run 
      //*******IF the arduino is plugged into a serial port (i.e. to set up the instrument)
      //*******It will have shut off already if not plugged in (i.e in sampling mode)
      
      //Do this every time we change settings (when the loop runs) just to be safe.
      //"OPTIONS" Menu
      Serial.println(F("'S' to display Status"));
      Serial.println(F("'T' to set time"));
      Serial.println(F("'A' to set first alarm time and date"));
      Serial.println(F("'R' to turn on Run mode (first alarm in 2 mintues)"));
      Serial.println();
      
      DateTime now = RTC.now();
      byte byteRead;   //variable to read Serial input from user
      boolean waiting=true;  //variable to wait for user input

      while(waiting==true){        //Run loop to set unit status as long as serial connection exists
        byteRead = Serial.read();   //read serial input (keystroke from user)
        switch (byteRead){   
          case  84 : //if user enters 'T', sync the RTC
            setRTC();
            waiting=false;
            break;
          case  83 : //if user enters 'S', display logger "Status"
            displayStatus();
            waiting=false;
            break;
          case   82: //'R'
            runMode();         
            waiting=false;
            break;
          case  65 : //'A'
            setAlarm();
            waiting=false;
            break;
        }
      }
      delay(2000);   //wait a few seconds before running the loop again
}

void initializePins(){
    pinMode(POWERPIN,OUTPUT);      // setup Power Valve Pin
    digitalWrite(POWERPIN, HIGH);  // Set Power Pin High so Arduino stays on after RTC alarm is reset
    Wire.begin();                  // Start Arduino 
    pinMode(RTCPIN,OUTPUT);        // setup Real Time Clock Chip
    SPI.begin();                   // Start SPI functionality to talk to SD Card and RTC 
    SPI.setBitOrder(MSBFIRST);     // Set bitorder for SPI (required for setting registers on DS3234 RTC)
    RTC.begin();                   // Start Real Time Clock code
}

void ADCSetup(){
  Wire.beginTransmission(ads1100write);
  Wire.write(B10001101); //set ADC gain to 2, 15 Samples per Second (16-bit Resolution)
  Wire.endTransmission();
  delay(500);
}

int ADCRead(){
  byte controlRegister =0;
  int adcVal = 0;
  Wire.requestFrom(ads1100write, 3);  //Take sample from ADC
      while(Wire.available()){ // ensure all the data comes in
        adcVal = Wire.read(); // high byte * B11111111
        adcVal = ((unsigned int)adcVal << 8);
        adcVal += Wire.read(); // low byte
        controlRegister = Wire.read();
      }
      return adcVal;
}

//Routine to Write data to RTC Register VIA Arduino SPI
void RTCWrite(char reg, char val){
    noInterrupts();              //make sure transfer doesn't get interrupted
    digitalWrite(RTCPIN, LOW);   //enable SPI read/write for chip
    SPI.transfer(reg);           //define memory register location
    SPI.transfer(bin2bcd(val));  //write value
    digitalWrite(RTCPIN, HIGH);  //disable SPI read/write for chip
    delay(5);                    //delay 5 ms to make sure chip is off
    interrupts();                //resume normal operation
}

void setAlarmTime(DateTime alarmtime){
  RTCWrite(0x87,alarmtime.second() & 0x7F);  //set alarm time: seconds     //87=write to location for alarm seconds    //binary & second with 0x7F required to turn alarm second "on"
  RTCWrite(0x88,alarmtime.minute() & 0x7F);  //set alarm time: minutes     //88=write to location for alarm minutes    //binary & minute with 0x7F required to turn alarm minute "on"
  RTCWrite(0x89,alarmtime.hour() & 0x7F);    //set alarm time: hour        //89=write to location for alarm hour       //binary & hour with 0x7F required to turn alarm hour "on"
  RTCWrite(0x8A,alarmtime.day() & 0x3F);     //set alarm time: day         //8A=write to location for alarm day        //binary & day with 0x3F required to turn alarm day "on" (not dayofWeek) 
  RTCWrite(0x8B,0);                          //Set Alarm #2 to zll zeroes (disable)
  RTCWrite(0x8C,0);                          //Set Alarm #2 to zll zeroes (disable)
  RTCWrite(0x8D,0);                          //Set Alarm #2 to zll zeroes (disable)
  RTCWrite(0x8F,B00000000);                  //reset flags                //8F=write to location for control/status flags    //B00000000=Ocillator Stop Flag 0, No Batt Backed 32 KHz Output, Keep Temp CONV Rate at 64 sec (may change later), disable 32 KHz output, temp Not Busy, alarm 2 not tripped, alarm 1 not tripped
  RTCWrite(0x8E,B00000101);                  //set control register       //8E=write to location for control register        //B01100101=Oscillator always on, SQW on, Convert Temp off, SQW freq@ 1Hz, Interrupt enabled, Alarm 2 off, Alarm 1 on
}

//simple routine to check if SD card is functioning properly
void SDCardCheck(){
    pinMode(10,OUTPUT);    //prevent errors
    SD.begin(SDPIN);  
    if (!SD.exists("data.txt")) {
        Serial.println();
        Serial.println(F("  **SD Card Initialization failed!  Check connections and if SD Card is Present"));
      return;
    }
        Serial.print(F(" SD Card Initialized successfully.  Saving records to "));
        Serial.println("data.txt");
        
}

//routine to write a sensor value and a formatted time/date string to SD Card
void SDCardWrite(DateTime timestamp){
  pinMode(10,OUTPUT);    //prevent errors
  SD.begin(SDPIN);  
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {   // if the file is available, write to it:
    //therm_voltage = adcVal * VREF / 32768 / 2;  //voltage is adc reading * reference voltage / adc possible values / adc gain
    for (int j = 0;j < numSamples;j++){  //Take a number of samples and everage them to reduce noise
      dataFile.print(measurements[j]);
      dataFile.print(", ");
    }    
    dataFile.print(timestamp.month());
    dataFile.print("/");
    dataFile.print(timestamp.day());
    dataFile.print("/");
    dataFile.print(timestamp.year());
    dataFile.print(", ");
    dataFile.print(timestamp.hour());
    dataFile.print(":");
    dataFile.print(timestamp.minute());
    dataFile.print(":");
    dataFile.println(timestamp.second());
    dataFile.close();
  }  
  // if the file isn't open, do nothing
}

void setRTC(){
    Serial.println(F("Enter Current Date:"));
    Serial.println(F("MMDDYY"));
    char inputbuffer[6];
    boolean checkdigit = true;    //variable to check if user input is actually digits
    boolean waiting2=true;        //another variable to wait for user input
    byte readLen =0;
    byte MM;        //variables to store time and date values
    byte DD;
    byte YY;
    byte hh;
    byte mm;
    byte ss;
    while(waiting2==true){
      readLen = Serial.readBytes(inputbuffer, 6);   //read serial input (6 keystrokes from user)
      if (readLen==6){      //check if user has put in 6 digits correctly
        waiting2=false;
        for(int i = 0; i<6;i++){
          if(inputbuffer[i]<48||inputbuffer[i]>57){
            checkdigit=false;
          }
        }
      }
    }
    if(checkdigit==true){
      MM=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);  //parse input data into vaiables
      DD=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);
      YY=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);
      Serial.print(MM);
      Serial.print(F("/")); 
      Serial.print(DD); 
      Serial.print(F("/")); 
      Serial.println(YY);
      Serial.println();      
    }
    else{
     Serial.println(F("**Input Error"));
    }
    waiting2=true;     //reset variable waiting for user input
    if(checkdigit==true){
      Serial.println(F("Enter Current Time:"));
      Serial.println(F("hhmmss"));
      while(waiting2==true){
        readLen = Serial.readBytes(inputbuffer, 6);   //read serial input (6 keystrokes from user)
        if (readLen==6){      //check if user has put in 6 digits correctly
          waiting2=false;
          for(int i = 0; i<6;i++){
            if(inputbuffer[i]<48||inputbuffer[i]>57){
              checkdigit=false;
            }
          }
        }
      }
      if(checkdigit==true){
        hh=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);    //parse input data into vaiables
        mm=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);
        ss=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);
      Serial.print(hh); 
      Serial.print(F(":")); 
      Serial.print(mm); 
      Serial.print(F(":")); 
      Serial.println(ss); 
      Serial.println();
   }
      else{
       Serial.println(F("**Input Error"));
      }
    }
    if(checkdigit==true){      //if both inputs were six digits, then reset the alarm with the right values
      DateTime settime = DateTime(YY,MM,DD,hh,mm,ss);
      RTC.adjust(settime);
      Serial.println(F("**Time set successfully!**"));
      Serial.println();
      displayStatus();
    }
}

void setAlarm(){
    Serial.println(F("Enter Alarm Date:"));
    Serial.println(F("MMDDYY"));
    char inputbuffer[6];
    boolean checkdigit = true;    //variable to check if user input is actually digits
    boolean waiting2=true;        //another variable to wait for user input
    byte readLen =0;
    byte MM;        //variables to store time and date values
    byte DD;
    byte YY;
    byte hh;
    byte mm;
    byte ss;
    while(waiting2==true){
      readLen = Serial.readBytes(inputbuffer, 6);   //read serial input (6 keystrokes from user)
      if (readLen==6){      //check if user has put in 6 digits correctly
        waiting2=false;
        for(int i = 0; i<6;i++){
          if(inputbuffer[i]<48||inputbuffer[i]>57){
            checkdigit=false;
          }
        }
      }
    }
    if(checkdigit==true){
      MM=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);  //parse input data into vaiables
      DD=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);
      YY=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);
      Serial.print(MM); 
      Serial.print(F("/")); 
      Serial.print(DD); 
      Serial.print(F("/")); 
      Serial.println(YY);  
      Serial.println();      
    }
    else{
     Serial.println(F("**Input Error"));
    }
    waiting2=true;     //reset variable waiting for user input
    if(checkdigit==true){
      Serial.println(F("Enter Alarm Time:"));
      Serial.println(F("hhmmss"));
      while(waiting2==true){
        readLen = Serial.readBytes(inputbuffer, 6);   //read serial input (6 keystrokes from user)
        if (readLen==6){      //check if user has put in 6 digits correctly
          waiting2=false;
          for(int i = 0; i<6;i++){
            if(inputbuffer[i]<48||inputbuffer[i]>57){
              checkdigit=false;
            }
          }
        }
      }
      if(checkdigit==true){
        hh=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);    //parse input data into vaiables
        mm=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);
        ss=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);
      Serial.print(hh); 
      Serial.print(F(":")); 
      Serial.print(mm); 
      Serial.print(F(":")); 
      Serial.println(ss);
      Serial.println(); 
    }
      else{
       Serial.println(F("**Input Error"));
      }
    }
    if(checkdigit==true){      //if both inputs were six digits, then reset the alarm with the right values
      alarmtime = DateTime(YY,MM,DD,hh,mm,ss);
      setAlarmTime(alarmtime);
      Serial.println(F("**Alarm Set Successfully!**"));
      Serial.println();
      displayStatus();
    }
 }
 
 void runMode(){
   DateTime now = RTC.now();
   alarmtime = now.unixtime() + 120;
   setAlarmTime(alarmtime);  
   Serial.println(F("**Unit set to RUN MODE - will start sampling in 2 minutes**"));
   Serial.println(F("**Check status and disconnect from USB cable**"));
   Serial.println();
   displayStatus();
 }
 
 void displayStatus(){
    const int len = 32;       //buffer length for RTC display on Serial comm
    static char buf[len];     //string for RTC display on Serial comm
    DateTime now = RTC.now();
    Serial.print(F("Current RTC Time:   "));
    Serial.println(now.toString(buf,len));
    Serial.print(F("First Alarm Time:   "));
    Serial.println(alarmtime.toString(buf,len));
    Serial.print(F("SD Card Status:    "));
    SDCardCheck();
    Serial.print(F("Sample Interval:    "));
    Serial.print(sampleinterval);
    Serial.println(" seconds");
    Serial.print(F("Measurements Per Sample:   "));
    Serial.println(numSamples);
    Serial.println(F("  (**Must change Sample Interval and Measurements Per Sample in code if needed)"));
    Serial.println();
 }
 
 
 
