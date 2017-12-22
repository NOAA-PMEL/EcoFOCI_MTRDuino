//include required header files for Arduino funtions, SPI functionality, DS3234 clock, and SD Card
#include <ctype.h>
#include <Wire.h>
#include <ArduinoPins.h>
#include <SPI.h>
#include <SD.h>
#include <RTClib.h>0
#include <RTC_DS3234.h>

//*********** SET LOGGING VARIABLES HERE: ***********
const int sampleinterval = 60;
//char filename[] = "data.txt";
const int numSamples = 100;
//*********** SET LOGGING VARIABLES HERE: ***********

// Avoid spurious warnings
#undef PROGMEM
#define PROGMEM __attribute__(( section(".progmem.data") ))
#undef PSTR
#define PSTR(s) (__extension__({static prog_char __c[] PROGMEM = (s); &__c[0];}))
#define BUFF_MAX 256
    
#define RTCPIN 11        //Define Output Pin for RTC 
#define SDPIN 10          //Define Output Pin for SDCARD
#define POWERPIN 12      //Define Digital Output Pin for Power Valve 
#define ads1100write 0x49     //Define ADC control register
const float VREF = 4.096;      //Define Voltage Reference   
RTC_DS3234 RTC(RTCPIN);    // Create a RTC instance, using the chip select pin it's connected to
int measurements[numSamples];


void setup () {    
    Wire.begin();                  // Start Arduino 
    pinMode(POWERPIN,OUTPUT);      // setup Power Valve Pin
    digitalWrite(POWERPIN, HIGH);  // Set Power Pin High so Arduino stays on after RTC alarm is reset
    pinMode(RTCPIN,OUTPUT);        // setup Real Time Clock Chip
    SPI.begin();                   // Start SPI functionality to talk to SD Card and RTC 
    SPI.setBitOrder(MSBFIRST);     // Set bitorder for SPI (required for setting registers on DS3234 RTC)
    RTC.begin();                   // Start Real Time Clock
    DateTime now = RTC.now();          //Get time from RTC
    DateTime alarmtime = (now.unixtime() + sampleinterval);
    setAlarmTime(alarmtime);
    ADCSetup(2);  //Set gain to 2 on ADC (more precise)
      for (int j = 0;j < numSamples;j++){  //Take a number of samples and everage them to reduce noise
        measurements[j] = ADCRead();
        delay(200);  //short 10 ms delay before next reading
      }
    SDCardWrite(now);
    
    //Drive output to power pin low (go to sleep and wake up on RTC alarm only)
    digitalWrite(POWERPIN, LOW);       
    delay(2000);
}

void loop () {   
      
}

int ADCRead(){
  int adcVal = 0;
  byte b1 = 0;
  byte b2 = 0;
  byte controlRegister =0;
  Wire.requestFrom(ads1100write, 3);  //Take sample from ADC
      while(Wire.available()){ // ensure all the data comes in
        b1 = Wire.read(); // high byte * B11111111
        b2 = Wire.read();
        adcVal=(b1<<8)+b2;
        //adcVal = ((unsigned int)adcVal << 8);
        controlRegister = Wire.read();
      }
  return adcVal;
}

void ADCSetup(int gain){
  Wire.beginTransmission(ads1100write);
  if(gain==1){
    Wire.write(B10001100); //set ADC gain to 2, 15 Samples per Second (16-bit Resolution)
  }
  else if(gain==2){
    Wire.write(B10001101); //set ADC gain to 2, 15 Samples per Second (16-bit Resolution)
  }
  else if(gain==3){
    Wire.write(B10001110); //set ADC gain to 2, 15 Samples per Second (16-bit Resolution)
  }
  else if(gain==4){
    Wire.write(B10001111); //set ADC gain to 2, 15 Samples per Second (16-bit Resolution)
  }
  //Wire.write(0x87); //set ADC gain to 8, 60 Samples per Second (14-bit Resolution)
  //Wire.write(0x83); //set ADC gain to 8, 240 Samples per Second (12-bit Resolution)
  Wire.endTransmission();
  delay(10);
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


//routine to write a sensor value and a formatted time/date string to SD Card
void SDCardWrite(DateTime timestamp){
  pinMode(10,OUTPUT);    //prevent errors
  SD.begin(SDPIN);  
  File dataFile = SD.open("data.txt", FILE_WRITE);
  if (dataFile) {   // if the file is available, write to it:
    //therm_voltage = adcVal * VREF / 32768 / 2;  //voltage is adc reading * reference voltage / adc possible values / adc gain
    for (int j = 0;j < numSamples;j++){  //Take a number of samples and everage them to reduce noise
      dataFile.print(measurements[j]);
      dataFile.print(",");
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
