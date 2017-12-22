//include required header files for Arduino funtions, SPI functionality, DS3234 clock, and SD Card
#include <ctype.h>
#include <Wire.h>
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

#define RTCPIN 4                  //Define Output Pin for RTC 
#define SDPIN 10                  //Define Output Pin for SDCARD
#define POWERPIN 12               //Define Digital Output Pin for Power Switch 
#define TEMPSWITCH 13             //Define Output Pin for Temperature Switch
#define ads1100write 0x49         //Define ADC control register
RTC_DS3234 RTC(RTCPIN);           //Create a RTC instance, using the chip select pin it's connected to
DateTime alarmtime;               //Global variable for alarmtime
const int numSamples = 5;         //Global Variable for number of samples to take each time the unit wakes up 

void setup(){    
  initializePins();                                          //Set output pins, initialize digital signals
  initializeICs();                                           //Initialize RTC, SDCard, and ADC
  DateTime wakeUpTime = setNextAlarm();                      //Set the next alarm and return the time when unit woke up
  delay(500);
  int measurements[numSamples+1];                            //Array to store measurements for all samples
  takeMeasurements(measurements);                            //take measurements and store them to array
  storeMeasurements(wakeUpTime, measurements);               //Write all data to the SD Card
  shutdownUnit();                                            //go to sleep and wake up on RTC alarm (unless connected to USB)
  Serial.begin(9600);
  while(!Serial){}
}

void loop () {   
  //*******This Section of code will only run 
  //*******IF the arduino is plugged into a serial/USB port (i.e. to set up the instrument)
  //*******It will have shut off already if not plugged in (i.e in sampling mode)

  //"OPTIONS" Menu
  Serial.println(F("'S' to display status"));
  Serial.println(F("'D' to display sdcard info"));
  Serial.println(F("'T' to set time and date"));
  Serial.println(F("'F' to set first sample time and date"));
  Serial.println(F("'I' to change sample interval\n"));

  byte byteRead;                //variable to read Serial input from user
  boolean waiting=true;         //variable to wait for user input

  while(waiting==true){         //Run loop to set unit status as long as serial connection exists
    byteRead = Serial.read();   //read serial input (keystroke from user)
    switch (byteRead){   
    case 116:                 //break intentionally ommited  //if user enters 't', execute code for 'T'
    case  84 :                //if user enters 'T', sync the RTC
      userSetRTC();
      waiting=false;
      break;
    case 115:                 //break intentionally ommited  //if user enters 's', execute code for 'S'
    case  83 :                //if user enters 'S', display logger "Status"
      displayStatus();
      waiting=false;
      break;
    case 102:                 //break intentionally ommited  //if user enters 'f', execute code for 'F'
    case  70 :                //if user enters 'F', set the first sample time and date
      userSetFirstSample();
      waiting=false;
      break;
    case 105:                 //break intentionally ommited  //if user enters 'i', execute code for 'I'
    case  73 :                //if user enters 'I', set the sample interval
      userSetSampleInterval();
      waiting=false;
      break;
    case 100:                 //break intentionally ommited  //if user enters 'd', execute code for 'D'
    case  68 :                //if user enters 'D', display sdcard info
      sdCardInfo();
      waiting=false;
      break;
    }
  }
  delay(1000);                 //wait 1 second before running the loop again
}

void initializePins(){
  pinMode(POWERPIN,OUTPUT);       // setup Power Valve Pin
  digitalWrite(POWERPIN, HIGH);   // Set Power Pin High so Arduino stays on after RTC alarm is reset
  pinMode(TEMPSWITCH,OUTPUT);     // setup Temperature switch Pin
  digitalWrite(TEMPSWITCH, HIGH); // Set temperature switch pin low initially (current flowing through thermistor)
  pinMode(RTCPIN,OUTPUT);         // setup Real Time Clock Chip
  pinMode(SDPIN,OUTPUT);          // setup SDCard Pin
  //pinMode(10,OUTPUT);           // setup Pin 10 as output for SD Card library //omitted because SDPIN is already defined as pin 10
}

void initializeICs(){
  Wire.begin();                   // Start I2C Comms 
  SPI.begin();                    // Start SPI functionality to talk to SD Card and RTC 
  SPI.setBitOrder(MSBFIRST);      // Set bitorder for SPI (required for setting registers on DS3234 RTC)
  SPI.setDataMode(SPI_MODE1);     // Set SPI Data Mode to 1 for RTC
  RTC.begin();                    // Start Real Time Clock Library 
  SPI.setDataMode(SPI_MODE0);     // Set SPI Data Mode to 0 for SD Card
  SD.begin(SDPIN);                // Start SD Card Communication
  ADCSetup();                     // Set up the ADC for conversions    
}

void ADCSetup(){
  Wire.beginTransmission(ads1100write);          // Send Write command to ADC
  Wire.write(B10001110);                         // set ADC gain to 4, 8 Samples per Second (16-bit Resolution)
  Wire.endTransmission();                        // End Write command to ADC
}

DateTime setNextAlarm(){
  unsigned long sampleInterval = readSampleInterval();       //get sample interval if one is stored on SD Card
  SPI.setDataMode(SPI_MODE1);                                //Set SPI Data Mode to 1 for RTC
  DateTime wakeUpTime = RTC.now();                             //Get time from RTC
  alarmtime = (wakeUpTime.unixtime() + sampleInterval);      //Determine next alarm time based on sample interval 
  setRTCAlarm(alarmtime);                                    //Set the alarm
  return wakeUpTime;                                         //return the current time so other functions remember when the unit woke up
}

void setRTCAlarm(DateTime alarmtime){
  SPI.setDataMode(SPI_MODE1);                   //Set SPI Data Mode to 1 for RTC
  RTCWrite(0x87,alarmtime.second() & 0x7F);     //set alarm time: seconds     //87=write to location for alarm seconds    //binary & second with 0x7F required to turn alarm second "on"
  RTCWrite(0x88,alarmtime.minute() & 0x7F);     //set alarm time: minutes     //88=write to location for alarm minutes    //binary & minute with 0x7F required to turn alarm minute "on"
  RTCWrite(0x89,alarmtime.hour() & 0x7F);       //set alarm time: hour        //89=write to location for alarm hour       //binary & hour with 0x7F required to turn alarm hour "on"
  RTCWrite(0x8A,alarmtime.day() & 0x3F);        //set alarm time: day         //8A=write to location for alarm day        //binary & day with 0x3F required to turn alarm day "on" (not dayofWeek) 
  RTCWrite(0x8B,0);                             //Set Alarm #2 to all zeroes (disable)
  RTCWrite(0x8C,0);                             //Set Alarm #2 to all zeroes (disable)
  RTCWrite(0x8D,0);                             //Set Alarm #2 to all zeroes (disable)
  RTCWrite(0x8F,B00000000);                     //reset flags                //8F=write to location for control/status flags    //B00000000=Ocillator Stop Flag 0, No Batt Backed 32 KHz Output, Keep Temp CONV Rate at 64 sec (may change later), disable 32 KHz output, temp Not Busy, alarm 2 not tripped, alarm 1 not tripped
  RTCWrite(0x8E,B00000101);                     //set control register       //8E=write to location for control register        //B01100101=Oscillator always on, SQW on, Convert Temp off, SQW freq@ 1Hz, Interrupt enabled, Alarm 2 off, Alarm 1 on
}

void RTCWrite(char reg, char val){              //Function to Write data to RTC Register
  digitalWrite(RTCPIN, LOW);                    //enable SPI read/write for chip
  SPI.transfer(reg);                            //define memory register location
  SPI.transfer(bin2bcd(val));                   //write value in correct bit format
  digitalWrite(RTCPIN, HIGH);                   //disable SPI read/write for chip
}

unsigned long readSampleInterval(){
  SPI.setDataMode(SPI_MODE0);                                                         //Set SPI Data Mode to 0 for SD Card
  File sampleIntervalFile = SD.open("sampint.txt", FILE_READ);                        //Open sample interval file 
  unsigned long sampleInterval=0;                                                     //initialize sampleInterval variable
  if (sampleIntervalFile) {                                                           //if the file is available:
    byte fileLen=0;                                                                   //initialize file Length variable
    while(sampleIntervalFile.available()&&fileLen<100){                               //while there is still data in the file and the numebr of characters read is less than 100
      char readChar=sampleIntervalFile.read();                                        //read the next character in the file
      if(readChar>47&&readChar<58){                                                   //if there is a digit in the next slot
        sampleInterval=sampleInterval*10+readChar-'0';                                //multiply the previous value by 10 and add the new digit value
      }
    }
    sampleIntervalFile.close();                                                       //close the file
  }
  if (sampleInterval>4){                                                              //if sample interval is greater than 4
    return sampleInterval;                                                            //use the sample interval found in the file
  }
  else{                                                                               //if sample interval is less than 4 (unit needs a few seconds to sample and shut down before ready for the next sample)
    return 3600;                                                                      //use the default sample interval of 1 hour (3600 seconds)
  }
}

void takeMeasurements(int measurements[]){
  for (int j = 0;j < numSamples;j++){                      //Loop for the defined number of samples
    measurements[j] = ADCRead();                           //Take a measurement from the thermistor and store in next slot in the array
  }
  digitalWrite(TEMPSWITCH, LOW);                           //Send current through the reference resistor
  delay(500);                                              //wait 500 milliseconds for the voltage levels to stabilize
  measurements[numSamples] = ADCRead();                    //Take a measurement from the reference resistor and store it in the last slot of the array
}

int ADCRead(){
  int adcVal=0;                                //initialize ADC value
  Wire.requestFrom(ads1100write, 3);           //Request 3 bytes from ADC
  while(Wire.available()){                     //ensure all the data comes in
    adcVal = Wire.read();                      //read first byte (ADC val high byte)
    adcVal = ((unsigned int)adcVal << 8);      //shift high byte to correct location
    adcVal += Wire.read();                     //read second byte (ADC val low byte)
    byte controlRegister = Wire.read();        //read third byte (ADC control register value)
  }
  return adcVal;                               //return the adc value 
}

void storeMeasurements(DateTime timestamp, int measurements[]){           //function to write a sensor value and a formatted time/date string to SD Card
  SPI.setDataMode(SPI_MODE0);                                             //Set SPI Data Mode to 0 for SDCard
  File dataFile = SD.open("mtrdata.txt", FILE_WRITE);                     //Open file for mtr data with write access
  if (dataFile) {                                                         //if the file is available, write to it:
    dataFile.print(timestamp.month());                                    //write the month (stamp from when unit woke up)
    dataFile.print(F("/"));                                               //write '/'
    dataFile.print(timestamp.day());                                      //write the day (stamp from when unit woke up)
    dataFile.print(F("/"));                                               //write '/'  
    dataFile.print(timestamp.year());                                     //write the year (stamp from when unit woke up)
    dataFile.print(F(" "));                                               //write ' '
    dataFile.print(timestamp.hour());                                     //write the hour (stamp from when unit woke up)
    dataFile.print(F(":"));                                               //write ':'
    dataFile.print(timestamp.minute());                                   //write the minute (stamp from when unit woke up)
    dataFile.print(F(":"));                                               //write ':'
    dataFile.print(timestamp.second());                                   //write the second (stamp from when unit woke up)
    dataFile.print(F(", "));                                              //write ','
    for (int j = 0;j <= numSamples;j++){                                  //for the total number of stored samples
      dataFile.print(measurements[j]);                                    //write the measured value
      dataFile.print(F(", "));                                            //write ','
    }    
    dataFile.println();                                                   //write a line return at end of sapmle set
    dataFile.close();                                                     //close the file
  }  
  // if the file isn't open, do nothing
}

void shutdownUnit(){
  digitalWrite(POWERPIN, LOW);                                            //Drive output to power pin low 
  delay(5000);                                                            //Delay to allow voltages to settle and Arduino to go to sleep  
}

void userSetSampleInterval(){
  unsigned long sampleInterval = userInputSampleInterval();               //initialize and get sample interval from user input
  SPI.setDataMode(SPI_MODE0);                                             //Set SPI Data Mode to 0 for sdcard
  if(SD.exists("sampint.txt")){                                           //if sampint.txt exists on the sdcard
    SD.remove("sampint.txt");                                             //delete the file so we can make a new one
  }  
  File sampleIntervalFile = SD.open("sampint.txt", FILE_WRITE);           //create and open sampint.txt with write access
  if(sampleIntervalFile){                                                 //if the file exists
    sampleIntervalFile.print(F("sample interval = "));                    //print 'sample interval = ' to the file (for users who may open the file to know what this is)
    sampleIntervalFile.print(sampleInterval);                             //write the sample interval collected from the user to the file
    sampleIntervalFile.print(F(" sec\n"));                                //write 'sec' to file
    Serial.print(F("*Sample interval set to "));                          //print confirmation to serial monitor to tell user that the sample interval has been set
    Serial.print(sampleInterval);                                         //print sampleInterval to serial monitor
    Serial.println(F(" seconds.*\n"));                                    //print ' seconds.*' and line return
    sampleIntervalFile.close();                                           //close the file
  }
  else{                                                                                            //if file does not open 
    Serial.println(F("**Sample interval adjustment FAILED.  Check SDCard and try again.\n**"));    //print message to serial monitor that sample interval could not be changed
  }  
}

void SDCardCheck(){                                                                                       //function to check if SD card is functioning properly
  SPI.setDataMode(SPI_MODE0);                                                                             //Set SPI Data Mode to 0 for sdcard
  Sd2Card card;                                                                                           //initialize sdcard variable
  if (!card.init(SPI_HALF_SPEED, SDPIN)) {                                                                //try to initialize sdcard, if it fails
    Serial.println(F("**Initialization FAILED! Check if card is inserted and formatted correctly!** "));  //print message to serial monitor that initialization failed
    return;                                                                                               //do nothing else
  } 
  else {                                                                                                  //if card initializes successfully
    Serial.println(F("SD Card initialized successfully. Card is present and communicating properly."));   //print message to serial monitor that card is working properly
  }        
}

void userSetRTC(){                                                          
  Serial.println(F("Input Desired Date and Time for this Unit:"));             //print prompt to serial monitor to ask user for RTC date and time
  userInputDateTime(true);                                                     //collect date and time from user (true tells the function this is for the RTC) 
}

void userSetFirstSample(){
  Serial.println(F("Input Desired Date and Time for First Sample:"));          //print prompt to serial monitor to ask user for first sample date and time
  userInputDateTime(false);                                                    //collect date and time from user (false tells the function this is for the alarm)
}

unsigned long userInputSampleInterval(){
  Serial.println(F("Enter Desired Sample Interval (in seconds): "));     //prompt user to input sample interva;
  char inputbuffer[10];                                                  //buffer for input 
  boolean verifyInput = true;                                            //variable to check if user input is actually digits
  boolean waitingForInput = true;                                        //variable to loop while waiting for user input
  byte readPosition = 0;                                                 //variable for position in input buffer
  char readChar = 0;                                                     //variable for reading individual characters
  unsigned long sampleInterval=0;                                        //initialize sample interval
  while(waitingForInput==true){                                          //while we are sating for input
    if(Serial.available()){                                              //if any data comes in on the serial line
      readChar = Serial.read();                                          //read the next character
      Serial.print(char(readChar));                                      //echo the character to the serial monitor
      inputbuffer[readPosition]=readChar;                                //store the character in the input array
      readPosition++;                                                    //increment to the next position in the array
      if(readChar==127||readChar==8){                                    //if user enters delete or backspace
        readPosition=readPosition-2;                                     //decrement the array by 2 characters (the del or backspace we just read and the previous character)
      }
      if(readPosition>9||readChar==13){                                  //if user enters more than 8 characters or 'ENTER'
        waitingForInput=false;                                           //end this loop and process what the user entered
      }      
    } 
  }
      
  for(int i = 0; i<readPosition-1; i++){                                                          //for all the characters that the user just input 
    if(inputbuffer[i]<48||inputbuffer[i]>57){                                                     //if there is not a digit in the next slot (ascii values)
      verifyInput=false;                                                                          //flag an error
    }
    else{                                                                                         //if there is a digit in the next slot
      sampleInterval=sampleInterval*10+inputbuffer[i]-'0';                                        //multiply the previous value by 10 and add the new digit value
    }                                                                      
  }
  if(sampleInterval>2419200){                                                                     //if sample interval is too large
      verifyInput=false;                                                                          //flag an error
  }    
  
  if(verifyInput==true){                                                                          //if there are no errors in the input 
    Serial.print(F("\nSample Interval will be set to "));                                         //print info to serial monitor to tell user sample interval was set
    Serial.print(sampleInterval);                                                                 //print the sample interval to serial monitor
    Serial.print(F(" seconds. "));                                                                //print ' seconds. (' to serial monitor
    displayHoursMinsSecs(sampleInterval);                                                         //print hours minutes and seconds in sample interval to serial monitor
    Serial.println();                                                                             //print line return
    delay(1000);                                                                                  //delay 1 second 
    return sampleInterval;                                                                        //return the sample interval input by the user
  }
  else{                                                                                           //if there were errors in the input
    Serial.println(F("\n**Input Error**\n"));                                                     //print **input error** on serial monitor
    return sampleInterval;                                                                        //return 0 as sample interval
  }
}


void userInputDateTime(boolean RTCorFirstSample){    //function to collect date and time from user, boolean variable to tell function if this is for unit's time or first sample
  Serial.println(F("Enter Date:"));                  //prompt user to input date
  Serial.println(F("MMDDYY"));                       //give user format for date input
  char inputbuffer[7];                               //buffer for user input
  boolean verifyInput = true;                        //variable to check if user input is actually digits
  boolean waitingForInput = true;                    //variable to wait for user input
  byte readPosition = 0;                             //variable for position in input buffer
  char readChar = 0;                                 //variable for reading individual characters
  byte MM;                                           //variable to store month
  byte DD;                                           //variable to store day
  byte YY;                                           //variable to store year
  byte hh;                                           //variable to store hour
  byte mm;                                           //variable to store minute
  byte ss;                                           //variable to store second
  while(waitingForInput==true){                      //while we are still waiting for user input  
    if(Serial.available()){                          //if there is any data that comes in on the serial port
      readChar = Serial.read();                      //read the next character in the string
      Serial.print(char(readChar));                  //echo the character to the serial monitor
      inputbuffer[readPosition]=readChar;            //place the character in the input array
      readPosition++;                                //increment to the next position in the array
      if(readChar==127||readChar==8){                //if the user enters backspace or delete
        readPosition=readPosition-2;                 //decrement the array by 2 (the backspace or del we just read and the previous character)
      }
      if(readPosition>6||readChar==13){              //if user enters at least 6 characters or 'Enter'
        waitingForInput=false;                       //stop waiting for user input and process the characters
      }      
    } 
  }
  if (readPosition==7&&readChar==13){                //check if user has put in 6 digits and 'Enter' correctly
    for(int i = 0; i<6; i++){                        //for each of the first 6 characters the user entered
      if(inputbuffer[i]<48||inputbuffer[i]>57){      //if the character is not a digit (ascii values)
        verifyInput=false;                           //flag an error
      }
    }
  }
  else{                                              //if user didn't input 6 digits or 'Enter' correctly
    verifyInput=false;                               //flag an error
  }
  if(verifyInput==true){                             //if there are no errors
    MM=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);   //parse first 2 characters and calculate the month
    DD=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);   //parse second 2 characters and calculate the day
    YY=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);   //parse third 2 characters and calculate the year
  }
  if(MM>12||DD>31||MM==0||DD==0){                    //if month or day values are not valid
    verifyInput=false;                               //flag an error
  }
  if(verifyInput==true){                             //if there are no errors
    Serial.println();                                //print new line to serial monitor
    Serial.print(F("Date will be set to "));         //print 'Date will be set to ' to serial monitor
    Serial.print(MM);                                //print Month value to serial monitor
    Serial.print(F("/"));                            //print '/' to serial monitor
    Serial.print(DD);                                //print Day value to serial monitor
    Serial.print(F("/"));                            //print '/' to serial monitor 
    Serial.println(YY);                              //print Year value to serial monitor
    Serial.println();                                //print new line to serial monitor 
  }
  else{                                              //if there are errors
    Serial.println(F("\n**Input Error**\n"));        //print **input error** on serial monitor
    return;
  }
  
  delay(1000);                                        //delay 1 second
  waitingForInput=true;                               //reset variable waiting for user input
  
  Serial.println(F("Enter Time:"));                   //prompt user to input date
  Serial.println(F("hhmmss"));                        //give user format for time
  for(int i=0;i<8;i++){                               //for each character in the input buffer
    inputbuffer[i] = 0;                               //delete any info in the buffer
  }
  readPosition = 0;                                   //reset readPosition variable
  readChar = 0;                                       //reset readChar variable
  while(waitingForInput==true){                       //while we are still waiting for user input
    if(Serial.available()){                           //if there is any data available on the serial port
      readChar = Serial.read();                       //read the next character
      Serial.print(char(readChar));                   //echo the character to the serial monitor
      inputbuffer[readPosition]=readChar;             //place the character in the input buffer
      readPosition++;                                 //increment to the next position in the array 
      if(readChar==127||readChar==8){                 //if the user enters backspace or delete
        readPosition=readPosition-2;                  //decrement the array by 2 (the backspace or del we just read and the previous character)
      }
      if(readPosition>6||readChar==13){               //if user enters at least 6 characters or 'Enter'
        waitingForInput=false;                        //stop waiting for user input and process the characters
      }      
    } 
  }
  if (readPosition==7&&readChar==13){                 //if user has put in 6 characters and 'Enter' correctly
    for(int i = 0; i<6; i++){                         //for each character in the array
      if(inputbuffer[i]<48||inputbuffer[i]>57){       //if the value is not a digit  (ascii values)
        verifyInput=false;                            //flag an error 
      }
    }
  }
  else{                                                //if user did not input 6 characters or 'Enter' correctly
    verifyInput=false;                                 //flag an error
  }
  if(verifyInput==true){                               //if no errors were flagged
    hh=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);     //calculate the value for hours
    mm=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);     //calculate the value for minuutes
    ss=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);     //calculate the value for seconds
  }
  if(hh>23||mm>60||ss>60){                             //if values for hours, minutes, or seconds are not valid
    verifyInput=false;                                 //flag an error
  }
  if(verifyInput==true){                               //if no errors were flagged
    Serial.print(F("\nTime will be set to "));         //print 'Time will be set to ' to serial monitor
    Serial.print(hh,2);                                //print hours value to serial monitor
    Serial.print(F(":"));                              //print ':' to serial monitor 
//    if(mm<10){                                         //if month value is less than 10
//      Serial.print(F("0"));                            //display a zero in the first digit place so format looks right
//    }
    Serial.print(mm,2);                                //print minutes value to serial monitor 
    Serial.print(F(":"));                              //print ':' to serial monitor 
//    if(ss<10){                                         //if seconds value is less than 10
//      Serial.print(F("0"));                            //display a zero in the first digit place so format looks right 
//    }
    Serial.println(ss,2);                              //print seconds value to serial monitor
  }
  else{                                                //if errors were flagged
    Serial.println(F("\n**Input Error**\n"));          //print **input error** on serial monitor
    return;
  }
  
  delay(1000);                                         //delay 1 second
  
  if(verifyInput==true){                                          //if input for time and date were both correct, then reset the clock or alarm with the right values
    SPI.setDataMode(SPI_MODE1);                                   //Set SPI Data Mode to 1 for RTC
    if(RTCorFirstSample){                                         //if input is for the unit's clock
      DateTime settime = DateTime(YY,MM,DD,hh,mm,ss);             //initialize DateTime variable using input data
      RTC.adjust(settime);                                        //set the unit clock to the date and time collected by the user
      Serial.println(F("\nInternal Clock set successfully!\n"));  //tell user clock was set successfully
    }
    else{                                                         //if input is for the alarm
      alarmtime = DateTime(YY,MM,DD,hh,mm,ss);                    //set the alarmtime DateTime variable using input data
      setRTCAlarm(alarmtime);                                     //set the alarm to the date and time collected by the user
      Serial.println(F("\nFirst Sample set successfully!\n"));    //tell user the first sample was set successfully
    }
  }
}

void displayStatus(){
  const int len = 32;                                        //buffer length for RTC display on Serial comm
  static char buf[len];                                      //string for RTC display on Serial comm
  SPI.setDataMode(SPI_MODE1);                                //Set SPI Data Mode to 1 for RTC
  DateTime now = RTC.now();                                  //Get the current time from the RTC
  Serial.print(F("Unit Time      :  "));                     //print 'unit time    :' to serial monitor
  Serial.println(now.toString(buf,len));                     //print current RTC time to  
  Serial.print(F("SD Card Status :  "));                     //print 'sd card status    :' to serial monitor
  SDCardCheck();                                             //check sd card status and print to serial monitor
  Serial.print(F("First Sample   :  "));                     //print 'first sample    :' to serial monitor
  Serial.println(alarmtime.toString(buf,len));               //print the first sample date and time to the serial monitor
  unsigned long sampleInterval = readSampleInterval();       //read the sample interval from the sd card
  Serial.print(F("Sample Interval:  "));                     //print 'sample interval:' to serial monitor
  Serial.print(sampleInterval);                              //print the sample interval to the serial monitor
  Serial.println(F(" seconds"));                             //print 'seconds' to serial monitor
  Serial.print(F(" -Unit will sample every "));              //print dialog to explain programming to user
  displayHoursMinsSecs(sampleInterval);                      //print hours minutes and seconds in sample interval to serial monitor
  Serial.print(F(" starting  "));                            //print dialog to explain programming to user
  Serial.println(alarmtime.toString(buf,len));               //print first sample date and time to serial monitor
  Serial.print(F("--Unplug instrument from USB to begin--"));//print dialog to remind user how to begin program
  Serial.println();                                          //print newline to serial monitor
}

void displayHoursMinsSecs(unsigned long numSeconds){
  unsigned long displayIntervalHours=numSeconds/(unsigned long)3600;                          //calculate the number of whole hours
  unsigned long displayIntervalMinutes=(numSeconds%(unsigned long)3600)/(unsigned long)60;    //calculate the number of whole minutes leftover
  unsigned long displayIntervalSeconds=numSeconds%(unsigned long)60;                          //calculate the number of seconds leftover
  Serial.print(F("("));                                                                       //print '(' to serial monitor
  Serial.print(displayIntervalHours);                                                         //print number of hours to serial monitor
  Serial.print(F("h,"));                                                                      //print 'h,' to serial monitor 
  Serial.print(displayIntervalMinutes);                                                       //print number of minutes to serial monitor
  Serial.print(F("m,"));                                                                      //print 'm,' to serial monitor
  Serial.print(displayIntervalSeconds);                                                       //print number of seconds to serial monitor
  Serial.print(F("s)"));                                                                      //print 's)' to serial monitor   
}

void sdCardInfo(){
  Sd2Card card;                                                                                       //sdcard card variable 
  SdVolume volume;                                                                                    //sdcard volume variable
  SdFile root;                                                                                        //sdcard root variable
  
  if (!card.init(SPI_HALF_SPEED, SDPIN)) {                                                            //attempt to initialize sdcard variable, if it does not work properly
    Serial.println(F("**Card Initialization FAILED! Check if card is inserted!** "));                 //print dialog to serial monitor to tell user the initialization failed
    return;                                                                                           //return and do nothing else
  } 
  else {                                                                                              //if card does initialize successfully
    if (!volume.init(card)) {                                                                         //if the card cannot be indexed properly 
      Serial.println(F("**Could not find file partition. Make sure you've formatted the card!**"));   //print dialog to serial monitor to tell user formatting may be wrong
      return;
    }
  }        
  
  uint32_t volumesize;
  volumesize = volume.blocksPerCluster();                                    //clusters are collections of blocks
  volumesize *= volume.clusterCount();                                       //we'll have a lot of clusters
  volumesize *= 512;                                                         //SD card blocks are always 512 bytes
  volumesize /= 1024;                                                        //convert to kbytes
  volumesize /= 1024;                                                        //convert to mbytes
  Serial.print(F("SDCard size (Mbytes): "));                                 //print 'SDCard size (mbytes): ' to serial monitor
  Serial.println(volumesize);                                                //print sdcard size in Mbytes to serial monitor
  Serial.println(F("Files on SD Card (name, date and size in bytes): "));    //print 'Files on SD Card (name, date and size in bytes): ' to serial monitor
  root.openRoot(volume);                                                     //open the root directory
  root.ls(LS_R | LS_DATE | LS_SIZE);                                         //list all files in the card with date and size
  Serial.println();                                                          //print newline to serial monitor
}

