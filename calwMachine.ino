/*
   -- New project --
   
   This source code of graphical user interface 
   has been generated automatically by RemoteXY editor.
   To compile this code using RemoteXY library 3.1.13 or later version 
   download by link http://remotexy.com/en/library/
   To connect using RemoteXY mobile app by link http://remotexy.com/en/download/                   
     - for ANDROID 4.15.01 or later version;
     - for iOS 1.12.1 or later version;
    
   This source code is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.    
*/

//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// you can enable debug logging to Serial at 115200
//#define REMOTEXY__DEBUGLOG    

// RemoteXY select connection mode and include library 
#define REMOTEXY_MODE__ESP32CORE_BLE

#include <BLEDevice.h>

// RemoteXY connection settings 
#define REMOTEXY_BLUETOOTH_NAME "Claw"
#define REMOTEXY_ACCESS_PASSWORD "1234"


#include <RemoteXY.h>

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t RemoteXY_CONF[] =   // 93 bytes
  { 255,6,0,2,0,86,0,19,0,0,0,65,100,118,101,110,116,115,105,0,
  31,2,106,200,200,84,1,1,5,0,5,0,2,60,60,6,12,60,60,32,
  93,26,31,5,0,66,61,61,135,13,60,60,32,204,26,31,67,10,171,83,
  20,67,8,63,10,86,2,26,3,7,129,89,33,82,23,35,13,131,2,26,
  1,65,50,34,34,88,51,24,24,0,2,31,0 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  int8_t joystick_01_x; // from -100 to 100
  int8_t joystick_01_y; // from -100 to 100
  int8_t joystick_02_x; // from -100 to 100
  int8_t joystick_02_y; // from -100 to 100
  uint8_t selectorSwitch_01; // from 0 to 3
  uint8_t button_01; // =1 if button pressed, else =0

    // output variables
  int16_t value_01; // -32768 .. +32767

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;   
#pragma pack(pop)
 
/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////
#include <AccelStepper.h>
uint32_t claw_pos = 3600;
uint32_t claw_pos_k1 = 3600;
unsigned long last_update =0;
unsigned long claw_update =0;
#define claw_pin 16
#define door_pin 4
#define door_button_pin 23

#define MAX_SPEED 1000.0
#define ACCELERATION 1000.0
#define START_LEFT 400.0
#define START_RIGHT 400.0
#define STEPS_PER_LENGTH 28.85
//#define STEPS_PER_LENGTH 22.6
#define BASE_WIDTH 385.0
#define ANALOG_READ_MAX 1024
const int JOY_MID=100;
const int JOY1_X_PIN = 34;
const int JOY1_Y_PIN = 35;

const int JOY2_X_PIN = 15;
const int JOY2_Y_PIN = 2;


AccelStepper stepperRight(AccelStepper::HALF4WIRE,26,14,27,12); // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5
AccelStepper stepperLeft(AccelStepper::HALF4WIRE,25,32,33,13); // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5
AccelStepper stepperFront(AccelStepper::HALF4WIRE,17,18,5,19); // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

class input_axis
{
  public:
  input_axis(unsigned int pin):
  m_pin{pin}
  {

  }
  void init(void)
  {
    pinMode(m_pin,INPUT);
    int temp = analogRead(m_pin);
    delay(10);
    temp += analogRead(m_pin);
    delay(10);
    temp += analogRead(m_pin);
    temp = temp/3;
    Serial.print(m_pin);
    Serial.print("\t");
    Serial.println(temp);
    m_min = temp-JOY_MID;
    m_max = temp+JOY_MID;
    m_mid = temp;    
  }
  int8_t value(void)
  {
    int temp = analogRead(m_pin);
    if (temp< m_min)
    {
      m_min = temp;
      return (int8_t)-100;
    }
    else if (temp> m_max)
    {
      m_max = temp;
      return (int8_t)100;
    }
    else
    {
      if (temp< (m_mid-JOY_MID))
      {
        return (int8_t) map(temp,m_min,m_mid-JOY_MID,-100,0);
      }
      else if (temp> m_mid+JOY_MID)
      {
        return (int8_t) map(temp,m_mid+JOY_MID,m_max,0,100);
      }
      else
      {
        return (int8_t)0;
      }
    }
  }
  const unsigned int m_pin;
  int m_max = ANALOG_READ_MAX/2;
  int m_min = ANALOG_READ_MAX/2;
  int m_mid = ANALOG_READ_MAX/2;
  
};

class triangleClass
{
  public:
    triangleClass(float baseWidth,float eng1StepPerLength,float eng2StepPerLength, AccelStepper& Eng1, AccelStepper& Eng2):
    m_baseWidth{baseWidth},
    m_eng1StepPerLength{eng1StepPerLength},
    m_eng2StepPerLength{eng2StepPerLength},
    m_Eng1{Eng1},
    m_Eng2{Eng2}
    {
      
    }
    void calcLengthFromPos(const float x,const float y, float &length1, float &length2)
    {
      length1 = sqrt(x*x + y*y);
      length2 = sqrt(sq(m_baseWidth-x)+sq(y));
    }
    void calcPosFromLengths(const float length1,const float length2,float &x,float &y)
    {
      x = (sq(length1)+sq(m_baseWidth)-sq(length2))/(2*m_baseWidth);
      y = sqrt(sq(length1) - sq(x));
    }

    void setPos(const float x, const float y)
    {
      float l1,l2;
      m_pos_x = x;
      m_pos_y = y;
      calcLengthFromPos(x,y,l1,l2);
      m_Eng1.setCurrentPosition((long)l1*m_eng1StepPerLength);
      m_Eng2.setCurrentPosition((long)l2*m_eng2StepPerLength);
    }

    void updatePosFromLength()
    {
      float x,y;
      calcPosFromLengths(m_Eng1.currentPosition()/m_eng1StepPerLength,m_Eng2.currentPosition()/m_eng2StepPerLength,x,y);
      m_pos_x = x;
      m_pos_y = y;
      //Serial.print(x);
      //Serial.print(" ");
      //Serial.println(y);      
    }

    void setVelocity(int8_t dX,int8_t dY)
    {
      float x,y,v;
      float newLength1,newLength2;
      v = sqrt(sq((float)dX)+sq((float)dY));
      if (v>0.01)
      {
          x = (float)dX/v;
          y = (float)dY/v;
      }
      else
      {
        x=0.0;
        y=0.0;
      }
      v=v/1.2;


      updatePosFromLength();
     // Serial.print(m_pos_x);
      //Serial.print(" ");
      //Serial.println(x);

      calcLengthFromPos(constrain(m_pos_x+10*x,10,m_baseWidth-10),m_pos_y+10*y,newLength1,newLength2);
      //calcLengthFromPos(m_pos_x,m_pos_y,newLength1,newLength2);
//      Serial.print(m_Eng1.currentPosition()/m_eng1StepPerLength);
//      Serial.print(" ");
//      Serial.print(newLength1);
//      Serial.print(" ");
//      Serial.print(m_Eng2.currentPosition()/m_eng2StepPerLength);
//      Serial.print(" ");
//      Serial.println(newLength2);
      m_Eng1.setSpeed((newLength1-m_Eng1.currentPosition()/m_eng1StepPerLength)*v);
      m_Eng2.setSpeed((newLength2-m_Eng2.currentPosition()/m_eng2StepPerLength)*v);
      //Serial.print((newLength1-m_Eng1.currentPosition()/m_eng1StepPerLength)*v);
     // Serial.print(" ");
      //Serial.println((newLength2-m_Eng2.currentPosition()/m_eng2StepPerLength)*v); 
    }
    void runSpeed()
    {
      m_Eng1.runSpeed();
      m_Eng2.runSpeed();
    }
    void setDefault()
    {
      defaultLength1 = m_Eng1.currentPosition();
      defaultLength2 = m_Eng2.currentPosition();
    }
    void moveDefault()
    {

      if (m_Eng1.speed()==0)
      {
        //Serial.println("set speed 1");
        m_Eng1.setSpeed(1);
      }
      if (m_Eng2.speed()==0)
      {
        m_Eng2.setSpeed(1);
      }
      m_Eng1.moveTo(defaultLength1);
      m_Eng2.moveTo(defaultLength2);
      m_Eng1.run();
      m_Eng2.run();
      
    }
  private:
    const float m_baseWidth;
    const float m_eng1StepPerLength;
    const float m_eng2StepPerLength;
    long defaultLength1;
    long defaultLength2;
    float m_pos_x;
    float m_pos_y;
    AccelStepper& m_Eng1;
    AccelStepper& m_Eng2;
};

triangleClass triangle{BASE_WIDTH,STEPS_PER_LENGTH,STEPS_PER_LENGTH,stepperLeft,stepperRight};
input_axis joy1_x{JOY1_X_PIN};
input_axis joy1_y{JOY1_Y_PIN};
input_axis joy2_x{JOY2_X_PIN};
input_axis joy2_y{JOY2_Y_PIN};

void setup() 
{
  RemoteXY_Init (); 
  ledcAttach(claw_pin,100,14);  
  ledcAttach(door_pin,100,14);
  pinMode(door_button_pin,INPUT_PULLUP);
  Serial.begin(115200);
  stepperRight.setMaxSpeed(MAX_SPEED);
  stepperRight.setAcceleration(ACCELERATION);
  stepperRight.setCurrentPosition(START_RIGHT * STEPS_PER_LENGTH);
  
  stepperLeft.setMaxSpeed(MAX_SPEED);
  stepperLeft.setAcceleration(ACCELERATION);
  stepperLeft.setCurrentPosition((long) START_LEFT * STEPS_PER_LENGTH);
  
  stepperFront.setMaxSpeed(MAX_SPEED/2);
  stepperFront.setAcceleration(ACCELERATION);
  // TODO you setup code

  triangle.updatePosFromLength();
  triangle.setDefault();
  joy1_x.init();
  joy1_y.init();
  joy2_x.init();
  joy2_y.init();
}

void loop() 
{ 
  RemoteXY_Handler ();
  if (millis()-last_update > 100)
  {
    last_update = millis();
    
    
    //stepperRight.setSpeed((float)RemoteXY.joystick_01_x*10);
    //stepperLeft.setSpeed((float)RemoteXY.joystick_01_y*10);

    if ((RemoteXY.selectorSwitch_01 != 2) or ((RemoteXY.connect_flag==false)))
    {
      triangle.setVelocity(joy1_x.value(),joy1_y.value());
      stepperFront.setSpeed((float)joy2_y.value()*-5);
    }
    else
    {
      triangle.setVelocity(-RemoteXY.joystick_01_x,-RemoteXY.joystick_01_y);
      stepperFront.setSpeed((float)RemoteXY.joystick_02_y*-5);
      joy1_x.value();
      joy1_y.value();
    }
    RemoteXY.value_01 = (int16_t) joy1_x.value();
    

    

  }
  if ((millis()-claw_update)>10)
  {
    if (RemoteXY.selectorSwitch_01 != 2)
    {
      if (abs(joy2_x.value())>10)
      {
        claw_pos = constrain(claw_pos+joy2_x.value()/4,720,3500);
      }
    }
    else
    {
      if (abs(RemoteXY.joystick_02_x)>10)
      {
        claw_pos = constrain(claw_pos+RemoteXY.joystick_02_x/4,720,3500);
      }
    }
    
    if (claw_pos != claw_pos_k1)
    {
      stepperFront.disableOutputs ();
      stepperRight.disableOutputs ();
      stepperLeft.disableOutputs ();
      delay(1);
      claw_pos_k1=claw_pos;
      ledcWrite(claw_pin,(uint32_t )claw_pos);
      claw_update = millis();
    }
    if ((millis()-claw_update)>500)
    {
      ledcWrite(claw_pin,(uint32_t )0);
      delay(1);
      stepperFront.enableOutputs ();
      stepperRight.enableOutputs ();
      stepperLeft.enableOutputs ();
    }
  }
  if ((millis()-claw_update)>500)
  {
  triangle.runSpeed();
  stepperFront.runSpeed();
  }
  if ((digitalRead(door_button_pin)==LOW)or (RemoteXY.button_01 == true))
  {
    Serial.println("doors");
    claw_update = millis();
    while ((millis()-claw_update)<1600)
    {
      ledcWrite(door_pin,(uint32_t )2000 + (millis()-claw_update));
      delay(2);
      RemoteXY_Handler ();
      
    }
    ledcWrite(door_pin,0);
    while((millis()-claw_update)<8000)
    {
      RemoteXY_Handler ();
    }
    while ((millis()-claw_update)<9600)
    {
      ledcWrite(door_pin,(uint32_t )3600 - (millis()-8000-claw_update));
      delay(2);
      
      //RemoteXY_Handler ();
    }   
    ledcWrite(door_pin,(uint32_t )2000);
    delay(20);
    ledcWrite(door_pin,0); 
  }
  //stepperRight.runSpeed();
  //stepperLeft.runSpeed();
  
  // TODO you loop code
  // use the RemoteXY structure for data transfer
  // do not call delay(), use instead RemoteXY_delay() 


}
