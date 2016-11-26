/* Program for controlling Pool PH 
Version 1.3 18/07/2015: Added Writing SP & Caibration Constants on EEPROM*
Version 2.0 13/11/2016: Master for implementing Raspberry Pi communication*
Version 2.1 20/11/2016: Draft for implementing Raspberry Pi communication*/

//LCD Display Initialisation
#include <LiquidCrystal.h>
LiquidCrystal lcd(12,11,5,4,3,2);

//IC2 Setup
#include <Wire.h>     // for I2C
#define i2caddr 0x50    // device address for left-hand chip on our breadboard
byte d=0; // data to store in or read from t5e EEPROM

//PHValue is the calculated PH Value
float PHValue=0;

//Initilaisation of Reference Value, CalValL is the Raw reading (0-1023) when PHREF is being used 
//a is the slope and b is the offset: y= a x + b  
float PHRef=10.0;
int CalVal1=228;// Raw Value for PH Ref
int CalVal2=512;// Raw Value for PH 7
float a=0;
float b=0; 
float SetPoint; 
int SetPointInt=760;// Default Value for SetPoint
int MSB; // Most significant Byte value in EEPROM
int LSB; // Less significant Byte value in EEPROM
float AnalogValue=0;

//Initialisation of time variable used for Pulse Duration SP (min): 5 min (équivalent to 100 ml)
unsigned long PulseSP=5;
//Initialisation of time variable used for Period Duration (min)
unsigned long Period=60;
//Initialisation of variable used for masquing time Setting (sec)
unsigned long MaskTime=1200;
//Initialisation of time variable used for masquing Duration SP (msec)
unsigned long DelaySP=Period*60000-PulseSP*60000;

//Initialisation of time variable used for time elapsed since power-up (msec)
unsigned long Time;
//Initialisation of time variable used for Pulse Start Time reference (msec)
unsigned long PulseTime=0;
//Initialisation of time variable used for Pulse Duration (msec)
unsigned long PulseWidth=0;
//Initialisation of time variable used for masquing Start Time reference  (msec)
unsigned long DelayTime=0;
//Initialisation of time variable used for masquing Duration (msec)
unsigned long DelayWidth=0;

//Initialisation of I/O Variables
int DI_State1 =0;
int DI_State2 =0;

//Initialisation of Flag1 used for SP Comparison .
int Flag1 =0;
//Initialisation of Flag2 used for driving pump
int Flag2 =0;
//Initialisation of Flag3 used for Waiting period
int Flag3 =0;
//Initialisation of Flag4 used for Calibration Mode
int Flag4 =0;
int message = 0;

//Init for Power-down sensing
int Reset_Flag =0;
int PW_Down_Flag =0;
int PW_Up_Flag=0;

void setup()
{
Wire.begin();// wake up the I2C
lcd.begin (16,2);

//Initialisation of PH Setpoint
SetPoint=float(SetPointInt)/100;
d=readData(0);
if (d==255)
{
MSB=readData(1);
LSB=readData(2);
SetPointInt=256*MSB+LSB;
SetPoint=float(SetPointInt)/100;
}

//Initialisation of CalVal1
d=readData(6);
if (d==255)
{
MSB=readData(7);
LSB=readData(8);
CalVal1=MSB*256+LSB;
}

//Initialisation of CalVal2
d=readData(3);
if (d==255)
{
int MSB=readData(4);
int LSB=readData(5);
CalVal2=MSB*256+LSB;
}

//Calculation of PH Slope and Offset
a=(PHRef-7)/(float(CalVal1)-float(CalVal2));
b= 7-float(CalVal2)*a;

//Initilisation de MaskTime en msec
MaskTime=MaskTime*1000;
//Initialisation of time variable used for Pulse Duration SP en msec
PulseSP=PulseSP*60000;

//DI for decreasing SP
pinMode(6,INPUT);
//DI for increasing SP
pinMode(7,INPUT);
//DO for driving dosing pump
pinMode(8,OUTPUT);
Serial.begin(9600);

}
void(* resetFunc) (void) = 0; //declare reset function @ address 0


void loop() {  
//Reading of PH Value
AnalogValue=Analog();
PHValue=AnalogValue*a+b;

if (PHValue > 6 && PW_Down_Flag ==0)
{
PW_Up_Flag=1;
}

if (PHValue <1.5 && PW_Up_Flag ==1)
{
PW_Down_Flag=1;
}

if (PHValue > 6 && PW_Down_Flag ==1)
{
Reset_Flag =1;
resetFunc();  //call reset

}

DI_State1=readButton(1);
DI_State2=readButton(2);
Time = millis();
  
// Setting of SP Value
if (DI_State1 ==1)
{
delay(100);
DI_State2=readButton(2);
if (DI_State2 !=1)
{
SetPointInt=SetPointInt-5;
MSB=SetPointInt/256;
LSB=SetPointInt-MSB*256;
SetPoint=float(SetPointInt)/100;
writeData(0,255);
writeData(1,MSB);
writeData(2,LSB);
}
}
if (DI_State2 ==1)
{
delay(100);
DI_State1=readButton(1);
if (DI_State1 !=1)
{
SetPointInt=SetPointInt+5;
MSB=SetPointInt/256;
LSB=SetPointInt-MSB*256;
SetPoint=float(SetPointInt)/100;
writeData(0,255);
writeData(1,MSB);
writeData(2,LSB);
}
}  

//Calibration mode
if (DI_State1 ==1 && DI_State2 ==1)
{
Flag4 =1; // Calibation Mode Set
}
if (Flag4==1)
{
lcd.setCursor (0,0);
lcd.print ("Calibration 7 PH"); 
lcd.setCursor(0,1);
lcd.print ("- ABORT  + CONT.");

// Waiting until no button is pressed
while (DI_State1 !=0 || DI_State2 !=0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
}

// Waiting until a button is pressed
while (DI_State1 ==0 && DI_State2 ==0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
}

if (DI_State1 ==1) // Calibration Aborted
{ 
Flag4 =0;// Calibration Flag is reset
}
else
{
// Waiting until no button is pressed
while (DI_State1 !=0 || DI_State2 !=0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
AnalogValue=Analog();
PHValue=AnalogValue*a+b;
lcd.setCursor (0,0);
lcd.print ("- ABORT  + CONT.");  
lcd.setCursor(0,1);
lcd.print ("PH:"); 
lcd.print (PHValue,2); 
lcd.print (" Raw:");
lcd.print (AnalogValue,0); 
lcd.print ("  ");
}
// Waiting until a button is pressed
while (DI_State1 ==0 && DI_State2 ==0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
AnalogValue=Analog();
PHValue=AnalogValue*a+b;
lcd.setCursor (0,0);
lcd.print ("- ABORT  + CONT.");  
lcd.setCursor(0,1);
lcd.print ("PH:"); 
lcd.print (PHValue,2); 
lcd.print (" Raw:");
lcd.print (AnalogValue,0); 
lcd.print ("  ");
}
if (DI_State1 ==1) // Calibration Aborted
{ 
Flag4 =0;
}
else
{ 
CalVal2=int(AnalogValue);
MSB= CalVal2/256;
LSB= CalVal2-MSB*256;
writeData(3,255);
writeData(4,MSB);
writeData(5,LSB);
a=(PHRef-7)/(float(CalVal1)-float(CalVal2));
b= 7-float(CalVal2)*a;

lcd.setCursor (0,0);
lcd.print ("Calibration REF PH");  
lcd.setCursor(0,1);
lcd.print ("- ABORT  + CONT."); 

// Waiting until no button is pressed
while (DI_State1 !=0 || DI_State2 !=0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
}
// Waiting until a button is pressed
while (DI_State1 ==0 && DI_State2 ==0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
}

if (DI_State1 ==1) // Calibration Aborted
{ 
Flag4 =0;
}
else 
{
// Waiting until no button is pressed
while (DI_State1 !=0 || DI_State2 !=0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
AnalogValue=Analog();
PHValue=AnalogValue*a+b;
lcd.setCursor (0,0);
lcd.print ("- ABORT  + CONT.");  
lcd.setCursor(0,1);
lcd.print ("PH:"); 
lcd.print (PHValue,2); 
lcd.print (" Raw:");
lcd.print (AnalogValue,0); 
lcd.print ("  ");
}
// Waiting until a button is pressed
while (DI_State1 ==0 && DI_State2 ==0)
{
DI_State1=readButton(1);
DI_State2=readButton(2);
AnalogValue=Analog();
PHValue=AnalogValue*a+b;
AnalogValue=Analog();
PHValue=AnalogValue*a+b;
lcd.setCursor (0,0);
lcd.print ("- ABORT  + CONT.");  
lcd.setCursor(0,1);
lcd.print ("PH:"); 
lcd.print (PHValue,2); 
lcd.print (" Raw:");
lcd.print (AnalogValue,0); 
lcd.print ("  ");
}
if (DI_State1 ==1) // Calibration Aborted
{ 
Flag4 =0;
}
else
{ 
CalVal1=int(AnalogValue);
MSB= CalVal1/256;
LSB= CalVal1-MSB*256;
writeData(6,255);
writeData(7,MSB);
writeData(8,LSB);
a=(PHRef-7)/(float(CalVal1)-float(CalVal2));
b= 7-float(CalVal2)*a;
}
Flag4=0;
}
}
}
//lcd.clear();
// Waiting until no button is pressed
//while (DI_State1 !=0 || DI_State2 !=0)
//{
//DI_State1=readButton(1);
//DI_State2=readButton(2);
//}
}

// SP Comparison Flag 1 is increased if Wating period is not active and pump is not driven and Powered up for the duration of the masking period
if (Time>MaskTime && PHValue>SetPoint && Flag2==0 && Flag3==0)
  {         
  Flag1=Flag1+1;
  }
else
  {         
  Flag1=0;
  }
  
// Display Values on LCD
lcd.setCursor (0,0);
  lcd.print ("SP PH: ");  
  lcd.print(SetPoint,2);
  lcd.print ("     ");
  lcd.setCursor(0,1);
  if (PW_Down_Flag ==0)
  { 
  lcd.print ("PV PH: ");
  lcd.print(PHValue,2); 
  lcd.print ("       ");
  }
else
  { 
   lcd.print ("NO POWER          ");
  } 
// Sending Data to Raspberry Pi
if (Serial.available())  {
    message = Serial.read()-'0';  // on soustrait le caractère 0, qui vaut 48 en ASCII

    if (message == 5){
      Serial.println(SetPoint,2);
      Serial.println(PHValue,2);
      
     
    }
     if (message == 1){
      Serial.println("PH");
      
     
    }
  }
 
// Driving of dosing pump

// Setting of Flag 2: If PV > SP 3 consecutive times then Flag2 is set
if (Flag1==3)
  {         
  Flag2=1;
  PulseTime=Time;
  }
  
// Flag 2 is Reset after Pulse Duration SP and Flag 3 is set for the duration of the masquing delay variable
PulseWidth= Time-PulseTime;
if (Flag2==1 && PulseWidth>=PulseSP)
  {         
  Flag2=0;
  DelayTime=Time;
  Flag3=1;
  }
  
// Flag 3 is Reset after masquing delay elapsed
DelayWidth=Time-DelayTime;
if (Flag3==1 and DelayWidth>=DelaySP)
  {         
  Flag3=0;
  }
  
// The pump is driven 
  
if (Flag2==1)
  {         
  digitalWrite(8,HIGH);
  }
else
 {         
  digitalWrite(8,LOW);
  }

// Wait before looping program
  delay(100);
}

// Function Read PHValue
float Analog()
{  
int potPin = A2;
int LoopVal=256;
int ADValue=0;
float Var=0; 
int Loop=0;
do 
  {
  ADValue = analogRead(potPin);
  delay(1);
  Var=Var+ADValue;
  Loop=Loop+1;
  }
while (Loop<LoopVal);
  ADValue= Var/LoopVal;
  return ADValue;
}  

// writes a byte of data in memory location addr
void writeData(unsigned int addr, byte data) 
{
  Wire.beginTransmission(i2caddr);
  // set the pointer position
  Wire.write((int)(addr >> 8));
  Wire.write((int)(addr & 0xFF));
  Wire.write(data);
  Wire.endTransmission();
  delay(10);
}

// reads a byte of data from memory location addr
byte readData(unsigned int addr) 
{
  byte result;
  Wire.beginTransmission(i2caddr);
  // set the pointer position
  Wire.write((int)(addr >> 8));
  Wire.write((int)(addr & 0xFF));
  Wire.endTransmission();
  Wire.requestFrom(i2caddr,1); // get the byte of data
  result = Wire.read();
  return result;
}
  //
  
 uint8_t readButton(int Di)
  {
 int i=0;
 if (Di==1)
 {
 if ((PIND & 0x40)>0){        //If the button was pressed
 delay(50); }        //Debounce the read value
 if ((PIND & 0x40)>0){        //Verify that the value is the same that what was read
 i=1; }            //If it is still 1 its because we had a button press
 else{                    //If the value is different the press is invalid
 i=0;}
 }
 if (Di==2)
 {
 if ((PIND & 0x80)>0){        //If the button was pressed
 delay(50); }        //Debounce the read value
 if ((PIND & 0x80)>0){        //Verify that the value is the same that what was read
 i=1; }            //If it is still 1 its because we had a button press
 else{                    //If the value is different the press is invalid
 i=0;}
 }
 return i;
 }
 
