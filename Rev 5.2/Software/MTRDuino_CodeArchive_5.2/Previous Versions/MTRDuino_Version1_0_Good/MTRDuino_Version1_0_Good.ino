

//include required header files for Arduino funtions, SPI functionality, DS3234 clock, and SD Card
#include <ctype.h>
#include <Wire.h>
#include <ArduinoPins.h>
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
    
#define RTCPIN 11        //Define Output Pin for RTC 
#define SDPIN 10          //Define Output Pin for SDCARD
#define POWERPIN 12      //Define Digital Output Pin for Power Valve 
#define THERMPIN 4       //Define Analog Input Pin for Thermistor Reading 
#define CREG B00000101   //Define Control Register byte for DS3234 RTC 
const float VREF = 4.096;      //Define Voltage Reference   
RTC_DS3234 RTC(RTCPIN);    // Create a RTC instance, using the chip select pin it's connected to

//*********** SET LOGGING VARIABLES HERE: ***********
const int sampleinterval = 60;
char filename[] = "dataworking.txt";
const int numSamples = 10;
//*********** SET LOGGING VARIABLES HERE: ***********

void setup () {    
    Wire.begin();                  // Start Arduino 
    pinMode(POWERPIN,OUTPUT);      // setup Power Valve Pin
    digitalWrite(POWERPIN, HIGH);  // Set Power Pin High so Arduino stays on after RTC alarm is reset
    pinMode(THERMPIN,INPUT);       // setup Thermistor Pin
    pinMode(RTCPIN,OUTPUT);        // setup Real Time Clock Chip
    SPI.begin();                   // Start SPI functionality to talk to SD Card and RTC 
    SPI.setBitOrder(MSBFIRST);     // Set bitorder for SPI (required for setting registers on DS3234 RTC)
    RTC.begin();                   // Start Real Time Clock
    analogReference(EXTERNAL);     // set analog reference to 4.096V from LM4040 voltage regulator diode
    DateTime now = RTC.now();          //Get time from RTC
    DateTime alarmtime = (now.unixtime() + sampleinterval);
    setAlarmTime(alarmtime);
    SD.begin(SDPIN);  
    float therm_val = 0;
    for (int j = 0;j < numSamples;j++){  //Take a number of samples and everage them to reduce noise
      delay(10);
      therm_val += analogRead(THERMPIN) ;  //read value from analog input for thermistor
    }
    float therm_voltage = therm_val / 1023.0 * VREF / numSamples;  //divide accumulated thermistor reading by 1024 (10bit ADC), multiply by 4.096V reference, and average
    float therm_resistance = 10000.0 / (VREF/therm_voltage-1.0);  //Calculate value of thermistor resistance based on voltage divider circuit
    SDCardWrite(therm_resistance,now);
    
    //Drive output to power pin low (go to sleep and wake up on RTC alarm only)
    digitalWrite(POWERPIN, LOW);       
    delay(2000);
}

void loop () {   
      //*******This Section of code will only run if arduino is plugged into a serial port (it will have shut off already otherwise);
      
      //Measure value and reset Alarm  
      //Do this every time we change settings (when the loop runs) just to be safe.
      DateTime now = RTC.now();          //Get time from RTC
      DateTime alarmtime = (now.unixtime() + sampleinterval);
      setAlarmTime(alarmtime);
      const int len = 32;       //buffer length for RTC display on Serial comm
      static char buf[len];     //string for RTC display on Serial comm
      float therm_val = analogRead(THERMPIN);  //read value from analog input for thermistor
      float therm_voltage = therm_val / 1023.0 * VREF;
      float therm_resistance = 10000.0*therm_voltage/(VREF-therm_voltage);  //Calculate value of thermistor resistance based on voltage divider circuit
    
      Serial.begin(9600);    // Turn the Serial Protocol ON
      while(!Serial){}       //Wait till serial connection is established

      
      //"OPTIONS" Menu
      Serial.println("'S' to display Status");
      Serial.println("'T' to sync time");
      Serial.println("'A' to set first alarm date");
      Serial.println();
      
      byte byteRead;   //variable to read Serial input from user
      boolean waiting=true;  //variable to wait for user input

      while(waiting==true){        //Run loop to set unit status as long as serial connection exists
        byteRead = Serial.read();   //read serial input (keystroke from user)
        switch (byteRead){   
          case  84 : //if user enters 'T', sync the RTC
            //code
            RTC.adjust(DateTime(__DATE__, __TIME__));
            Serial.println("**Time Synchronized with System Clock!");
            Serial.println();
            waiting=false;
          case  83 : //if user enters 'S', display logger "Status"
            now = RTC.now();
            Serial.print("Current RTC Time:   ");
            Serial.println(now.toString(buf,len));
            Serial.print("First Alarm Time:   ");
            Serial.println(alarmtime.toString(buf,len));
            Serial.print("SD Card Status:    ");
            SDCardCheck();
            Serial.print("Thermistor Value:   ");
            Serial.println(therm_voltage,2);
            Serial.print("Sample Interval:    ");
            Serial.print(sampleinterval);
            Serial.println(" seconds");
            Serial.print("Measurements Per Sample:   ");
            Serial.println(numSamples);
            Serial.println("  (Must change Sample Interval and Measurements Per Sample in code if needed)");
            Serial.println();
            waiting=false;
            break;
          case  65 : //'A'
            Serial.println("Enter Alarm Date:");
            Serial.println("MMDDYY");
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
              Serial.print(DD); 
              Serial.println(YY);            
            }
            else{
             Serial.println("Input Error");
            }
            waiting2=true;     //reset variable waiting for user input
            if(checkdigit==true){
              Serial.println("Enter Alarm Time:");
              Serial.println("hhmmss");
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
              Serial.print(mm); 
              Serial.println(ss); 
            }
              else{
               Serial.println("Input Error");
              }
            }
            if(checkdigit==true){      //if both inputs were six digits, then reset the alarm with the right values
              int YEAR = (int)YY + 1900;
              alarmtime = DateTime(ss,mm,hh,DD,MM,YEAR);
              setAlarmTime(alarmtime);
              Serial.println("Alarm Set Successfully!");
              Serial.println();
            }
            waiting=false;
            break;
        }
      }
      delay(2000);   //wait a few seconds before running the loop again
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

//simple routine to check if SD card is functioning properly
void SDCardCheck(){
    pinMode(10,OUTPUT);    //prevent errors
    if (!SD.exists(filename)) {
        Serial.println();
        Serial.println("  **SD Card Initialization failed!  Check connections and if SD Card is Present");
      return;
    }
        Serial.print(" SD Card Initialized successfully.  Saving records to ");
        Serial.println(filename);
        
}

//routine to write a sensor value and a formatted time/date string to SD Card
void SDCardWrite(int val,DateTime timestamp){
  pinMode(10,OUTPUT);    //prevent errors
  File dataFile = SD.open(filename, FILE_WRITE);
  if (dataFile) {   // if the file is available, write to it:
    dataFile.print(val);
    dataFile.print(", ");
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
