//Include libraries for FAT32 file and SD Card access
#include <ArduinoPins.h>
#include <FatStructs.h>
#include <Sd2Card.h>
#include <SdFat.h>
#include <SdFatmainpage.h>
#include <SdFatUtil.h>
#include <SdInfo.h>
#include <ctype.h>
#include <SPI.h>
const int  rtcpin=10; //chip select 
const int  sdcardpin=8; //chip select 

//Create the variables to be used by SdFat Library
Sd2Card card;
SdVolume volume;
SdFile root;
SdFile file;

char name[] = "Logger.txt";     //Create an array that contains the name of our file.
char contents[256];           //This will be a data buffer for writing contents to the file.
char in_char=0;
int index=0;
//=====================================================================================
void setup(void) 
{                
  //Initialize FAT System
  Serial.begin(9600);
  pinMode(sdcardpin, OUTPUT);       //Pin 10 must be set as an output for the SD communication to work.   
  card.init();               //Initialize the SD card and configure the I/O pins.
  volume.init(card);         //Initialize a volume on the SD card.
  root.openRoot(volume);     //Open the root directory in the volume.
  
  RTC_init();
  SetTimeDate(12,31,14,23,59,5); //month(1-12), day(1-31),  year(0-99), hour(0-23), minute(0-59), second(0-59)
  
}
//=====================================================================================
void loop(void) 
{
  file.open(root, name, O_CREAT | O_APPEND | O_WRITE);    //Open or create the file 'name' in 'root' for writing to the end of the file
  //sprintf(contents, "Millis");    //Copy the letters 'Millis: ' followed by the integer value of the millis() function into the 'contents' array.
  //file.print(contents);    //Write the 'contents' array to the end of the file.
  file.print(ReadTimeDate());    //Write the 'contents' array to the end of the file.
  file.close();            //Close the file.
  delay(1000);
    Serial.print("Hello\n");    //Print the current character  
    Serial.print(ReadTimeDate());    //Print the current character  
    file.open(root, name, O_READ);    //Open the file in read mode.
    in_char=file.read();              //Get the first byte in the file.
    //Keep reading characters from the file until we get an error or reach the end of the file. (This will output the entire contents of the file).
    while(in_char >=0){            //If the value of the character is less than 0 we've reached the end of the file.
        Serial.print(in_char);    //Print the current character
        in_char=file.read();      //Get the next character
    }
    file.close();    //Close the file
    delay(1000);     //Wait 1 second before repeating the process.
}
//=====================================================================================
int RTC_init(){ 
	  pinMode(rtcpin,OUTPUT); // chip select
	  // start the SPI library:
	  SPI.begin();
	  SPI.setBitOrder(MSBFIRST); 
	  SPI.setDataMode(SPI_MODE3); // both mode 1 & 3 should work 
	  //set control register 
	  digitalWrite(rtcpin, LOW);  
	  SPI.transfer(0x8E);
	  SPI.transfer(0x60); //60= disable Osciallator and Battery SQ wave @1hz, temp compensation, Alarms disabled
	  digitalWrite(rtcpin, HIGH);
	  delay(10);
}
//=====================================================================================
int SetTimeDate(int mo, int d, int y, int h, int mi, int s){ 
	int TimeDate [7]={s,mi,h,0,d,mo,y};
	for(int i=0; i<=6;i++){
		if(i==3)
			i++;
		int b= TimeDate[i]/10;
		int a= TimeDate[i]-b*10;
		if(i==2){
			if (b==2)
				b=B00000010;
			else if (b==1)
				b=B00000001;
		}	
		TimeDate[i]= a+(b<<4);
		  
		digitalWrite(rtcpin, LOW);
		SPI.transfer(i+0x80); 
		SPI.transfer(TimeDate[i]);        
		digitalWrite(rtcpin, HIGH);
  }
}
//=====================================================================================
String ReadTimeDate(){
	String temp;
	int TimeDate [7]; //second,minute,hour,null,day,month,year		
	for(int i=0; i<=6;i++){
		if(i==3)
			i++;
		digitalWrite(rtcpin, LOW);
		SPI.transfer(i+0x00); 
		unsigned int n = SPI.transfer(0x00);        
		digitalWrite(rtcpin, HIGH);
		int a=n & B00001111;    
		if(i==2){	
			int b=(n & B00110000)>>4; //24 hour mode
			if(b==B00000010)
				b=20;        
			else if(b==B00000001)
				b=10;
			TimeDate[i]=a+b;
		}
		else if(i==4){
			int b=(n & B00110000)>>4;
			TimeDate[i]=a+b*10;
		}
		else if(i==5){
			int b=(n & B00010000)>>4;
			TimeDate[i]=a+b*10;
		}
		else if(i==6){
			int b=(n & B11110000)>>4;
			TimeDate[i]=a+b*10;
		}
		else{	
			int b=(n & B01110000)>>4;
			TimeDate[i]=a+b*10;	
			}
	}
	temp.concat(TimeDate[5]);
	temp.concat("/") ;
	temp.concat(TimeDate[4]);
	temp.concat("/") ;
	temp.concat(TimeDate[6]);
	temp.concat("     ") ;
	temp.concat(TimeDate[2]);
	temp.concat(":") ;
	temp.concat(TimeDate[1]);
	temp.concat(":") ;
	temp.concat(TimeDate[0]);
  return(temp);
}
