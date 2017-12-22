//Include libraries for FAT32 file and SD Card access
#include <ArduinoPins.h>
#include <FatStructs.h>
#include <Sd2Card.h>
#include <SdFat.h>
#include <SdFatmainpage.h>
#include <SdFatUtil.h>
#include <SdInfo.h>
#include <ctype.h>

//Create the variables to be used by SdFat Library
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

char name[] = "Logger.txt";     //Create an array that contains the name of our file.
char contents[256];           //This will be a data buffer for writing contents to the file.
char in_char=0;
int THERM_PIN1=4;

int index=0;
int therm1;

void setup(void) 
{                
  //Initialize FAT System
  Serial.begin(9600);
  pinMode(8, OUTPUT);       //Pin 8 must be set as an output for the SD communication to work.   
  card.init();               //Initialize the SD card and configure the I/O pins.
  volume.init(card);         //Initialize a volume on the SD card.
  root.openRoot(volume);     //Open the root directory in the volume.
}

void loop(void) 
{
  /*
    // read the analog input
    float a1 = analogRead(THERM_PIN1);
    Serial.print("analogread = ");
    Serial.print(a1);
    Serial.print("\n");

    // calculate voltage
    float voltage = a1 / 1024 * 5.0;
    Serial.print("voltage = ");
    Serial.print(voltage);
    Serial.print("\n");

    // calculate resistance
    float resistance = (10000 * voltage) / (5.0 - voltage);
    Serial.print("resistance = ");
    Serial.print(resistance);
    Serial.print("\n");
  */
  file.open(root, name, O_CREAT | O_APPEND | O_WRITE);    //Open or create the file 'name' in 'root' for writing to the end of the file
  sprintf(contents, "Millis");    //Copy the letters 'Millis: ' followed by the integer value of the millis() function into the 'contents' array.
  file.print(contents);    //Write the 'contents' array to the end of the file.
  file.print("Millis");    //Write the 'contents' array to the end of the file.
  file.close();            //Close the file.
  delay(1000);
    Serial.print("Hello\n");    //Print the current character  
    file.open(root, name, O_READ);    //Open the file in read mode.
    in_char=file.read();              //Get the first byte in the file.
    //Keep reading characters from the file until we get an error or reach the end of the file. (This will output the entire contents of the file).
    while(in_char >=0){            //If the value of the character is less than 0 we've reached the end of the file.
        Serial.print(in_char);    //Print the current character
        in_char=file.read();      //Get the next character
    }
    file.close();    //Close the file
    delay(3000);     //Wait 1 second before repeating the process.
}
