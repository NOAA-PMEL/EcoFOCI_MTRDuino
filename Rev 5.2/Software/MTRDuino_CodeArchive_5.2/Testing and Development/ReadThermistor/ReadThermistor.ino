#include <ArduinoPins.h>
#include <ctype.h>

int THERM_PIN1=4;  //What analog pin is thermistor coltage divider connected to?
float VREF = 4.096; //Votage Reference for Thermistor Measurement (Defined by LM4040 Precision Regulator)
void setup(void) 
{                
  //Initialize FAT System
  Serial.begin(9600);
  analogReference(EXTERNAL);
}

void loop(void) 
{
    // read the analog input
    float a1 = analogRead(THERM_PIN1);  //Read value on analog pin for thermistor voltage divider
    Serial.print("analogread = ");
    Serial.print(a1,0);
    Serial.print("\n");

    // calculate voltage
    float voltage = a1 / 1023.0 * VREF;
    Serial.print("voltage = ");
    Serial.print(voltage,3);
    Serial.print("\n");

    // calculate resistance
    float thermistor_res = 10000 * 1/(VREF/voltage-1);  //Calculate value of thermistor resistance based on voltage divider circuit
    Serial.print("resistance = ");
    Serial.print(thermistor_res,3);
    Serial.print("\n");
    delay(1000);     //Wait 1 second before repeating the process.
}
