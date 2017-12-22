// **** INCLUDES *****
#include "LowPower.h"

void setup()
{
    // No setup is required for this library
  pinMode(13, OUTPUT);     
}

void loop() 
{
    // Enter power down state for 8 s with ADC and BOD module disabled
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);  
    
digitalWrite(13, HIGH);
delay(3000);
 
// Turn the LED off and sleep for 5 seconds
digitalWrite(13, LOW);
delay(3000);
}
