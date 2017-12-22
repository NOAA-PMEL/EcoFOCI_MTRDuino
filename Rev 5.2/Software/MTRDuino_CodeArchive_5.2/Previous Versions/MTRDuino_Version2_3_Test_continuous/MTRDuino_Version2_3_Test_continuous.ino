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
  int measurements[numSamples+1];                            //Array to store measurements for all samples
  while(true){
    wakeUpTime = RTC.now();
    takeMeasurements(measurements);                            //take measurements and store them to array
    storeMeasurements(wakeUpTime, measurements);               //Write all data to the SD Card
    delay(250);
    //shutdownUnit();                                            //go to sleep and wake up on RTC alarm (unless connected to USB)
    }
  Serial.begin(115200);
  while(!Serial){}
}

void loop () {   
  //*******This Section of code will only run 
  //*******IF the arduino is plugged into a serial/USB port (i.e. to set up the instrument)
  //*******It will have shut off already if not plugged in (i.e in sampling mode)

  //Do this every time we change settings (when the loop runs) just to be safe.
  //"OPTIONS" Menu
  Serial.println(F("'S' to display Status"));
  Serial.println(F("'D' to display SD Card Info"));
  Serial.println(F("'T' to set unit time and date"));
  Serial.println(F("'F' to set first sample time and date"));
  Serial.println(F("'I' to change sample interval"));
  Serial.println(F("'R' to turn on Run mode (first sample in 5 minutes)\n"));

  byte byteRead;                //variable to read Serial input from user
  boolean waiting=true;         //variable to wait for user input

  while(waiting==true){         //Run loop to set unit status as long as serial connection exists
    byteRead = Serial.read();   //read serial input (keystroke from user)
    switch (byteRead){   
    case 116:
      //break intentionally ommited
    case  84 :                //if user enters 'T', sync the RTC
      userSetRTC();
      waiting=false;
      break;
    case 115:
      //break intentionally ommited
    case  83 :                //if user enters 'S', display logger "Status"
      displayStatus();
      waiting=false;
      break;
    case 114:
      //break intentionally ommited
    case  82 :                //if user enters 'R', to turn on Run mode (first sample in 2 mintues)
      runMode();         
      waiting=false;
      break;
    case 102:
      //break intentionally ommited
    case  70 :                //if user enters 'F', set the first sample time and date
      userSetFirstSample();
      waiting=false;
      break;
    case 105:
      //break intentionally ommited
    case  73 :                //if user enters 'I', set the sample interval
      userSetSampleInterval();
      waiting=false;
      break;
    case 100:
      //break intentionally ommited
    case  68 :                //if user enters 'I', set the sample interval
      sdCardInfo();
      waiting=false;
      break;
    }
  }
  delay(1000);   //wait 1 second before running the loop again
}

void initializePins(){
  pinMode(POWERPIN,OUTPUT);       // setup Power Valve Pin
  digitalWrite(POWERPIN, HIGH);   // Set Power Pin High so Arduino stays on after RTC alarm is reset
  pinMode(TEMPSWITCH,OUTPUT);     // setup Temperature switch Pin
  digitalWrite(TEMPSWITCH, HIGH);  // Set temperature switch pin low initially (current flowing through thermistor)
  pinMode(RTCPIN,OUTPUT);         // setup Real Time Clock Chip
  pinMode(SDPIN,OUTPUT);          // setup Real Time Clock Chip
  pinMode(10,OUTPUT);             // setup Real Time Clock Chip
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
  Wire.beginTransmission(ads1100write);          //Send Write command to ADC
  Wire.write(B10001101);                         //set ADC gain to 2, 15 Samples per Second (16-bit Resolution)
  Wire.endTransmission();                        //End Write command to ADC
}

DateTime setNextAlarm(){
  unsigned long sampleInterval = readSampleInterval();       //get sample interval if one is stored on SD Card
  SPI.setDataMode(SPI_MODE1);                                //Set SPI Data Mode to 1 for RTc
  DateTime wakeUpTime = RTC.now();                                  //Get time from RTC
  alarmtime = (wakeUpTime.unixtime() + sampleInterval);             //Determine next alarm time based on sample interval 
  setRTCAlarm(alarmtime);                                    //Set the alarm
  return wakeUpTime;
}

void setRTCAlarm(DateTime alarmtime){
  SPI.setDataMode(SPI_MODE1);                   //Set SPI Data Mode to 1 for RTC
  RTCWrite(0x87,alarmtime.second() & 0x7F);     //set alarm time: seconds     //87=write to location for alarm seconds    //binary & second with 0x7F required to turn alarm second "on"
  RTCWrite(0x88,alarmtime.minute() & 0x7F);     //set alarm time: minutes     //88=write to location for alarm minutes    //binary & minute with 0x7F required to turn alarm minute "on"
  RTCWrite(0x89,alarmtime.hour() & 0x7F);       //set alarm time: hour        //89=write to location for alarm hour       //binary & hour with 0x7F required to turn alarm hour "on"
  RTCWrite(0x8A,alarmtime.day() & 0x3F);        //set alarm time: day         //8A=write to location for alarm day        //binary & day with 0x3F required to turn alarm day "on" (not dayofWeek) 
  RTCWrite(0x8B,0);                             //Set Alarm #2 to zll zeroes (disable)
  RTCWrite(0x8C,0);                             //Set Alarm #2 to zll zeroes (disable)
  RTCWrite(0x8D,0);                             //Set Alarm #2 to zll zeroes (disable)
  RTCWrite(0x8F,B00000000);                     //reset flags                //8F=write to location for control/status flags    //B00000000=Ocillator Stop Flag 0, No Batt Backed 32 KHz Output, Keep Temp CONV Rate at 64 sec (may change later), disable 32 KHz output, temp Not Busy, alarm 2 not tripped, alarm 1 not tripped
  RTCWrite(0x8E,B00000101);                     //set control register       //8E=write to location for control register        //B01100101=Oscillator always on, SQW on, Convert Temp off, SQW freq@ 1Hz, Interrupt enabled, Alarm 2 off, Alarm 1 on
}

void RTCWrite(char reg, char val){              //Function to Write data to RTC Register
  digitalWrite(RTCPIN, LOW);                  //enable SPI read/write for chip
  SPI.transfer(reg);                          //define memory register location
  SPI.transfer(bin2bcd(val));                 //write value
  digitalWrite(RTCPIN, HIGH);                 //disable SPI read/write for chip
}

unsigned long readSampleInterval(){
  SPI.setDataMode(SPI_MODE0);                   //Set SPI Data Mode to 0
  File sampleIntervalFile = SD.open("sampint.txt", FILE_READ);
  unsigned long sampleInterval=0;
  if (sampleIntervalFile) {                                                           // if the file is available, write to it:
    byte fileLen=0;
    while(sampleIntervalFile.available()&&fileLen<100){                               //read up to 100 characters
      char readChar=sampleIntervalFile.read();                                        //keep reading characters
      if(readChar>47&&readChar<58){                                                   //if there is a digit in the next slot
        sampleInterval=sampleInterval*10+readChar-'0';                                //multiply the previous value by 10 and add the new digit value
      }
    }
    sampleIntervalFile.close();
  }
  if (sampleInterval>4){
    return sampleInterval;
  }
  else{
    return 3600;  
  }
}

void takeMeasurements(int measurements[]){
  for (int j = 0;j < numSamples;j++){                        //Loop for number of samples
    measurements[j] = ADCRead();                           //Take measurements and store in an array
  }
  digitalWrite(TEMPSWITCH, LOW);                            //
  delay(500);
  measurements[numSamples] = ADCRead();
  digitalWrite(TEMPSWITCH, HIGH);                            //
}

int ADCRead(){
  int adcVal=0;
  Wire.requestFrom(ads1100write, 3);           //Take sample from ADC
  while(Wire.available()){                     //ensure all the data comes in
    adcVal = Wire.read();                      //read high byte
    adcVal = ((unsigned int)adcVal << 8);      //shift high byte to correct location
    adcVal += Wire.read();                     //read low byte
    byte controlRegister = Wire.read();        //read control register
  }
  return adcVal;
}

void storeMeasurements(DateTime timestamp, int measurements[]){               //function to write a sensor value and a formatted time/date string to SD Card
  SPI.setDataMode(SPI_MODE0);                       //Set SPI Data Mode to 1 for RTC
  File dataFile = SD.open("mtrdata.txt", FILE_WRITE);
  if (dataFile) {                                   // if the file is available, write to it:
    dataFile.print(timestamp.month());
    dataFile.print(F("/"));
    dataFile.print(timestamp.day());
    dataFile.print(F("/"));
    dataFile.print(timestamp.year());
    dataFile.print(F(" "));
    dataFile.print(timestamp.hour());
    dataFile.print(F(":"));
    dataFile.print(timestamp.minute());
    dataFile.print(F(":"));
    dataFile.print(timestamp.second());
    dataFile.print(F(", "));
    for (int j = 0;j <= numSamples;j++){             //Take a number of samples and everage them to reduce noise
      dataFile.print(measurements[j]);
      dataFile.print(F(", "));
    }    
    dataFile.println();
    dataFile.close();
  }  
  // if the file isn't open, do nothing
}

void shutdownUnit(){
  digitalWrite(POWERPIN, LOW);                               //Drive output to power pin low 
  delay(5000);                                               //Delay to allow voltages to settle and Arduino to go to sleep  
}

void userSetSampleInterval(){
  unsigned long sampleInterval = userInputSampleInterval();
  SPI.setDataMode(SPI_MODE0);                   //Set SPI Data Mode to 0
  SD.remove("sampint.txt");
  File sampleIntervalFile = SD.open("sampint.txt", FILE_WRITE);
  if(sampleIntervalFile){
    sampleIntervalFile.print(F("sample interval = "));
    sampleIntervalFile.print(sampleInterval);
    sampleIntervalFile.print(F(" seconds\n"));
    Serial.print(F("**Sample interval set to "));
    Serial.print(sampleInterval);
    Serial.println(F(" seconds.**\n"));
    sampleIntervalFile.close();
  }
  else{
    Serial.println(F("**Sample interval adjustment FAILED.  Check SDCard and try again.\n**"));
  }  
}

void SDCardCheck(){                             //function to check if SD card is functioning properly
  SPI.setDataMode(SPI_MODE0);                   //Set SPI Data Mode to 1 for RTC
  Sd2Card card;
  if (!card.init(SPI_HALF_SPEED, SDPIN)) {
    Serial.println(F("**Initialization FAILED! Check if card is inserted and formatted correctly!** "));
    return;
  } 
  else {
    Serial.println(F("SD Card initialized successfully. Card is present and communicating properly.")); 
  }        
}

void userSetRTC(){  
  Serial.println(F("Input Desired Date and Time for Internal Clock:"));
  userInputDateTime(true);
}

void userSetFirstSample(){
  Serial.println(F("Input Desired Date and Time for First Sample:"));
  userInputDateTime(false);
}

unsigned long userInputSampleInterval(){
  Serial.println(F("Enter Desired Sample Interval (in seconds): "));
  char inputbuffer[10];
  boolean verifyInput = true;                      //variable to check if user input is actually digits
  boolean waitingForInput = true;                 //another variable to wait for user input
  byte readPosition = 0;
  char readChar = 0;
  unsigned long sampleInterval=0;                                        
  while(waitingForInput==true){
    if(Serial.available()){
      readChar = Serial.read();
      Serial.print(char(readChar));
      inputbuffer[readPosition]=readChar;
      readPosition++;    
      if(readChar==127||readChar==8){
        readPosition=readPosition-2;
      }
      if(readPosition>9||readChar==13){
        waitingForInput=false;
      }      
    } 
  }
      
  for(int i = 0; i<readPosition-1; i++){
    if(inputbuffer[i]<48||inputbuffer[i]>57){                                         //if there is a digit in the next slot
      verifyInput=false;
    }
    else{
      sampleInterval=sampleInterval*10+inputbuffer[i]-'0';                                //multiply the previous value by 10 and add the new digit value
    }
  }
  if(sampleInterval>2419200){
      verifyInput=false;
  }    
  
  if(verifyInput==true){
    unsigned long sampleIntervalHours=sampleInterval/(unsigned long)3600;
    unsigned long sampleIntervalMinutes=(sampleInterval%(unsigned long)3600)/(unsigned long)60;
    unsigned long sampleIntervalSeconds=sampleInterval%(unsigned long)60;
    
    Serial.print(F("\nSample Interval will be set to ")); 
    Serial.print(sampleInterval);
    Serial.print(F(" seconds. (")); 

    Serial.print(sampleIntervalHours);
    Serial.print(F("h,")); 
    Serial.print(sampleIntervalMinutes); 
    Serial.print(F("m,")); 
    Serial.print(sampleIntervalSeconds);
    Serial.println(F("s)\n"));  
    delay(1000);
    return sampleInterval;
  }
  else{
    Serial.println(F("/n**Input Error**/n"));
    return sampleInterval;
  }
}


void userInputDateTime(boolean RTCorFirstSample){
  Serial.println(F("Enter Date:"));
  Serial.println(F("MMDDYY"));
  char inputbuffer[7];
  boolean verifyInput = true;                      //variable to check if user input is actually digits
  boolean waitingForInput = true;                 //another variable to wait for user input
  byte readPosition = 0;
  char readChar = 0;
  byte MM;                                        //variables to store time and date values
  byte DD;
  byte YY;
  byte hh;
  byte mm;
  byte ss;
  while(waitingForInput==true){
    if(Serial.available()){
      readChar = Serial.read();
      Serial.print(char(readChar));
      inputbuffer[readPosition]=readChar;
      readPosition++;
      if(readChar==127||readChar==8){
        readPosition=readPosition-2;
      }
      if(readPosition>6||readChar==13){
        waitingForInput=false;
      }      
    } 
  }
  if (readPosition==7&&readChar==13){                            //check if user has put in 6 digits correctly
    for(int i = 0; i<6; i++){
      if(inputbuffer[i]<48||inputbuffer[i]>57){
        verifyInput=false;
      }
    }
  }
  else{
    verifyInput=false;    
  }
  if(verifyInput==true){
    MM=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);  //parse input data into vaiables
    DD=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);
    YY=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);
  }
  if(MM>12||DD>31){
    verifyInput=false;
  }
  if(verifyInput==true){
    Serial.println();  
    Serial.print(F("Date will be set to ")); 
    Serial.print(MM);
    Serial.print(F("/")); 
    Serial.print(DD); 
    Serial.print(F("/")); 
    Serial.println(YY);
    Serial.println();  
  }
  else{
    Serial.println();  
    Serial.println(F("**Input Error**"));
    Serial.println();  
    return;
  }
  
  delay(1000);
  waitingForInput=true;     //reset variable waiting for user input
  
  Serial.println(F("Enter Time:"));
  Serial.println(F("hhmmss"));
  for(int i=0;i<8;i++){
    inputbuffer[i] = 0;
  }
  readPosition = 0;
  readChar = 0;
  while(waitingForInput==true){
    if(Serial.available()){
      readChar = Serial.read();
      Serial.print(char(readChar));
      inputbuffer[readPosition]=readChar;
      readPosition++;
      if(readChar==127||readChar==8){
        readPosition=readPosition-2;
      }
      if(readPosition>6||readChar==13){
        waitingForInput=false;
      }      
    } 
  }
  if (readPosition==7&&readChar==13){                            //check if user has put in 6 digits and carriage return correctly
    for(int i = 0; i<6; i++){
      if(inputbuffer[i]<48||inputbuffer[i]>57){
        verifyInput=false;
      }
    }
  }
  else{
    verifyInput=false;    
  }
  if(verifyInput==true){
    hh=10*(inputbuffer[0]-48)+(inputbuffer[1]-48);  //parse input data into vaiables
    mm=10*(inputbuffer[2]-48)+(inputbuffer[3]-48);
    ss=10*(inputbuffer[4]-48)+(inputbuffer[5]-48);
  }
  if(hh>23||mm>60||ss>60){
    verifyInput=false;    
  }
  if(verifyInput==true){
    Serial.println();
    Serial.print(F("Time will be set to ")); 
    Serial.print(hh); 
    Serial.print(F(":")); 
    if(mm<10){
      Serial.print(F("0")); 
    }
    Serial.print(mm); 
    Serial.print(F(":")); 
    if(ss<10){
      Serial.print(F("0")); 
    }
    Serial.println(ss); 
    Serial.println();
  }
  else{
    Serial.println();  
    Serial.println(F("**Input Error**"));
    Serial.println();  
    return;
  }
  
  delay(1000);
  
  if(verifyInput==true){      //if both inputs were six digits, then reset the alarm with the right values
    SPI.setDataMode(SPI_MODE1);                       //Set SPI Data Mode to 1 for RTc
    if(RTCorFirstSample){
      DateTime settime = DateTime(YY,MM,DD,hh,mm,ss);
      RTC.adjust(settime);
      Serial.println(F("Internal Clock set successfully!"));
    }
    else{
      alarmtime = DateTime(YY,MM,DD,hh,mm,ss);
      setRTCAlarm(alarmtime);  
      Serial.println(F("First Sample set successfully!"));  
    }
    Serial.println();
  }
}

void runMode(){
  SPI.setDataMode(SPI_MODE1);                       //Set SPI Data Mode to 1 for RTc
  DateTime now = RTC.now();
  alarmtime = now.unixtime() + 300;
  setRTCAlarm(alarmtime);  
  Serial.println(F("Unit set to RUN MODE: first sample in 5 minutes"));
  Serial.println(F("Check status and disconnect from USB cable\n"));
  delay(2000);
}

void displayStatus(){
  const int len = 32;       //buffer length for RTC display on Serial comm
  static char buf[len];     //string for RTC display on Serial comm
  SPI.setDataMode(SPI_MODE1);                       //Set SPI Data Mode to 1 for RTc
  DateTime now = RTC.now();
  Serial.print(F("Unit Time      :  "));
  Serial.println(now.toString(buf,len));
  Serial.print(F("SD Card Status :  "));
  SDCardCheck();
  Serial.print(F("First Sample   :  "));
  Serial.println(alarmtime.toString(buf,len));
  unsigned long sampleInterval = readSampleInterval();
  Serial.print(F("Sample Interval:  "));
  Serial.print(sampleInterval);
  Serial.println(F(" seconds"));
  Serial.print(F(" -Unit will sample every ["));
  unsigned long sampleIntervalHours=sampleInterval/(unsigned long)3600;
  unsigned long sampleIntervalMinutes=(sampleInterval%(unsigned long)3600)/(unsigned long)60;
  unsigned long sampleIntervalSeconds=sampleInterval%(unsigned long)60;
  Serial.print(sampleIntervalHours);
  Serial.print(F("h,")); 
  Serial.print(sampleIntervalMinutes); 
  Serial.print(F("m,")); 
  Serial.print(sampleIntervalSeconds);
  Serial.print(F("s] starting  "));    
  Serial.println(alarmtime.toString(buf,len));
  Serial.println();
}

void sdCardInfo(){
  Sd2Card card;
  SdVolume volume;
  SdFile root;
  
  if (!card.init(SPI_HALF_SPEED, SDPIN)) {
    Serial.println(F("**Card Initialization FAILED! Check if card is inserted!** "));
    return;
  } 
  else {
    //Serial.println(F("SD Card initialized successfully.  Card is present and communicating properly.")); 
    if (!volume.init(card)) {
      Serial.println(F("**Could not find file partition. Make sure you've formatted the card!**"));
      return;
    }
  }        
  
  uint32_t volumesize;
  volumesize = volume.blocksPerCluster();    // clusters are collections of blocks
  volumesize *= volume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                         // SD card blocks are always 512 bytes
  volumesize /= 1024;                        // convert to kbytes
  volumesize /= 1024;                        // convert to mbytes
  Serial.print(F("SDCard size (Mbytes): "));
  Serial.println(volumesize);
  Serial.println(F("Files on SD Card (name, date and size in bytes): "));
  root.openRoot(volume);
  root.ls(LS_R | LS_DATE | LS_SIZE);         // list all files in the card with date and size
  Serial.println();
}

