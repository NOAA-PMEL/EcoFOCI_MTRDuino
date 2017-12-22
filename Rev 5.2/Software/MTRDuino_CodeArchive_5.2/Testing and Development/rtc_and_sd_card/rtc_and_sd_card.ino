/*
  SD card read/write
 
 This example shows how to read and write data to and from an SD card file 	
 The circuit:
 * SD card attached to SPI bus as follows:
 ** MOSI - pin 11
 ** MISO - pin 12
 ** CLK - pin 13
 ** CS - pin 4
 
 created   Nov 2010
 by David A. Mellis
 modified 9 Apr 2012
 by Tom Igoe
 
 This example code is in the public domain.
 	 
 */
 
#include <SD.h>
#include <ArduinoPins.h>
#include <ctype.h>
#include <Wire.h>
#include "RTClib.h"

RTC_DS1307 RTC;

File myFile;

void setup()
{
  delay(5000);

 // Open serial communications and wait for port to open:
  Serial.begin(9600);
   while (!Serial) {
    ; // wait for serial port to connect. Needed for Leonardo only
  }
    Wire.begin();
      RTC.begin();
    if (! RTC.isrunning()) {
      Serial.println("RTC is NOT running!");
      //following line sets the RTC to the date & time this sketch was compiled
    }
      RTC.adjust(DateTime(__DATE__, __TIME__));
      
  Serial.print("Initializing SD card...");
  // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
  // Note that even if it's not used as the CS pin, the hardware SS pin 
  // (10 on most Arduino boards, 53 on the Mega) must be left as an output 
  // or the SD library functions will not work. 
   pinMode(10, OUTPUT);
   
  if (!SD.begin(4)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
  
  // open the file. note that only one file can be open at a time,
  // so you have to close this one before opening another.
  myFile = SD.open("test.txt", FILE_WRITE);
  
  // if the file opened okay, write to it:
  if (myFile) {
    Serial.print("Writing to test.txt...");
    myFile.println("testing 1, 2, 3.");
    DateTime now = RTC.now();
    myFile.print(now.year(), DEC);
    myFile.print('/');
    myFile.print(now.month(), DEC);
    myFile.print('/');
    myFile.print(now.day(), DEC);
    myFile.print(' ');
    myFile.print(now.hour(), DEC);
    myFile.print(':');
    myFile.print(now.minute(), DEC);
    myFile.print(':');
    myFile.print(now.second(), DEC);
    myFile.println();
    myFile.print(" since 1970 = ");
    myFile.print(now.unixtime());
    myFile.print("s = ");
    myFile.print(now.unixtime() / 86400L);
    myFile.println("d");
    // calculate a date which is 7 days and 30 seconds into the future
    DateTime future (now.unixtime() + 7 * 86400L + 30);
    myFile.print(" now + 7d + 30s: ");
    myFile.print(future.year(), DEC);
    myFile.print('/');
    myFile.print(future.month(), DEC);
    myFile.print('/');
    myFile.print(future.day(), DEC);
    myFile.print(' ');
    myFile.print(future.hour(), DEC);
    myFile.print(':');
    myFile.print(future.minute(), DEC);
    myFile.print(':');
    myFile.print(future.second(), DEC);
    myFile.println();
    myFile.println();
	// close the file:
    myFile.close();
    Serial.println("done.");
  } else {
    // if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
  
  // re-open the file for reading:
  myFile = SD.open("test.txt");
  if (myFile) {
    Serial.println("test.txt:");
    
    // read from the file until there's nothing else in it:
    while (myFile.available()) {
    	Serial.write(myFile.read());
    }
    // close the file:
    myFile.close();
  } else {
  	// if the file didn't open, print an error:
    Serial.println("error opening test.txt");
  }
  
}

void loop()
{
	// nothing happens after setup
}


