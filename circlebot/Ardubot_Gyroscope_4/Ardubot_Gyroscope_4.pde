#define debugMode //uncomment to send back some more data to computer

#include <RunningPercentile.h> //used to elimiate bad signals from radio
#include "ArduBot.h" //used to drive motors in correct directions
#include <AFMotor.h> //should already be included from prevoius
#include "Read_Servo_Signals.h" //outputs the values of the RC Receiver

const int fiveV = A0;
const int GND = A1;
const int RATE = A2;
const int TEMP = A3;
const int STtwo = A4;
const int STone = A5; // Pins on Gyroscope

unsigned long previousTime = 0; //Time since epoch of last loop, in microseconds
unsigned long currentTime = 0; //Time since epoch of current loop, in microseconds
unsigned int looptime; //time last loop took, in microseconds
unsigned long currentSerialTime;
unsigned long previousSerialTime;
unsigned long previousReadTime;

RunningPercentile LX_Med = RunningPercentile(16, 20);
RunningPercentile LY_Med = RunningPercentile(16, 20);
RunningPercentile RX_Med = RunningPercentile(16, 20);

int maxRate = 383;
int offset = 509;
//Needed for gyroscope

void setup(){
  Serial.begin(115200);
  
  delay(50); //The receiver gives out a bad pulse at the beginning, so we wait that out.
  
  #ifdef debugMode
  Serial.print(1);
  #else
  Serial.print(0);
  #endif
  
  pinMode(fiveV, OUTPUT); //gyro pins
  pinMode(GND, OUTPUT);
  pinMode(RATE, INPUT);
  pinMode(TEMP, INPUT);
  pinMode(STtwo, OUTPUT);
  pinMode(STone, OUTPUT);

  digitalWrite(fiveV, HIGH); //DO NOT CHANGE - WILL DESTROY GRYOSCOPE
  digitalWrite(GND, LOW); //DO NOT CHANGE - WILL DESTROY GYROSCOPE
  digitalWrite(STtwo, LOW);
  digitalWrite(STone, LOW);
  
  readReceiverBegin(13, 2, 3);
  //see Read_Servo_Signals.h
}

struct angleRate {long angle; long rate;};

struct angleRate readgyro(boolean reset){
  static long angle; //millidegrees, between 0 and 360,000
  long rate; //millidegrees per decasecond
  int rateReading; //0-1024, raw value from gyroscope
  struct angleRate AR;
  if (reset){angle = 0;}
  rateReading = analogRead(RATE);
  rate = (long (maxRate) * (rateReading - offset)) >> 9;
  angle += (rate * looptime)/100000;
  while (angle < 0){angle += 6283;} //add 2pi until greater or equal to zero
  angle %= 6283; //once >= zero, make sure it's not above 2pi
  AR.angle = angle;
  AR.rate = rate;
  return AR;
}

void loop(){ 
  previousTime = currentTime;
  currentTime = micros();
  looptime = currentTime - previousTime;
  
  static int strafe = 0;
  static int forward = 0;
  
  boolean gyroReset = false;
  
  unsigned int pulseLeftX = joyTimeLX();
  unsigned int pulseLeftY = joyTimeLY();
  unsigned int pulseRightX = joyTimeRX();
    
  LX_Med.add(pulseLeftX);
  LY_Med.add(pulseLeftY);
  RX_Med.add(pulseRightX);
  
  unsigned int pulseLeftX_filt = LX_Med.getPercentile();
  unsigned int pulseLeftY_filt = LY_Med.getPercentile();
  unsigned int pulseRightX_filt = RX_Med.getPercentile();
    
  if (1800 > pulseLeftX_filt && pulseLeftX_filt > 800 && //if there's a bad value, stop everything
      1700 > pulseLeftY_filt && pulseLeftY_filt > 900){
        if((!(1350 > pulseLeftX_filt && pulseLeftX_filt > 1250) ||    //if the joystick is almost exactly in the center,
            !(1350 > pulseLeftY_filt && pulseLeftY_filt > 1250)   )){ //do not move
          strafe = constrain(pulseLeftX_filt, 860, 1730);
          strafe = map(strafe, 860, 1740, -1000, 1000);
          strafe *= -1;
          forward = constrain(pulseLeftY_filt, 990, 1610);
          forward = map(forward, 990, 1610, -1000, 1000);
          forward *= -1;
        }
      else{
        strafe = 0;
        forward = 0;
      }
      previousReadTime = currentTime;
  }
  else if(currentTime -  previousReadTime < 100000L) {} //If we get a bad value, assume it's just an error for 100ms and keep last values
  else{ //if this happens, the controller has been turned off. A watchdog of sorts.
    strafe = 0;
    forward = 0;
    gyroReset = true; //if controller is cycled, reset the gyroscope.
  }

//  while (Serial.available() >= 4 ){
//    maxRate = Serial.read() | (Serial.read() << 8);
//    offset = Serial.read() | (Serial.read() << 8);
//  }
//  Serial.flush();
  
  struct angleRate AR = readgyro(gyroReset);
  struct moveRobotOutputs moveRobotOutputs = moveRobot(strafe, forward, AR.angle);
  
  if(millis() - previousSerialTime > 50){
    Serial.write((unsigned char*)&AR.angle, 4); //Sends angle as binary data
    Serial.write((unsigned char*)&AR.rate, 4); //Sends rate as binary data
    Serial.write((unsigned char*)&strafe, 2);
    Serial.write((unsigned char*)&forward, 2);
    Serial.write((unsigned char*)&moveRobotOutputs.speeds.speed4, 2);
    Serial.write((unsigned char*)&moveRobotOutputs.speeds.speed3, 2);
    Serial.write((unsigned char*)&moveRobotOutputs.speeds.speed1, 2);
    Serial.write((unsigned char*)&moveRobotOutputs.axis.x, 2);
    Serial.write((unsigned char*)&moveRobotOutputs.axis.y, 2);
    Serial.write((unsigned char*)&pulseLeftX_filt, 2);
    Serial.write((unsigned char*)&pulseLeftY_filt, 2);
    Serial.write((unsigned char*)&pulseRightX_filt, 2); 
    Serial.write((unsigned char*)&pulseLeftX, 2);
    Serial.write((unsigned char*)&pulseLeftY, 2);
    Serial.write((unsigned char*)&pulseRightX, 2);
    Serial.write((unsigned char*)&looptime, 2);
    #ifdef debugMode
      Serial.write((unsigned char*)&case0, 2);
      Serial.write((unsigned char*)&case1, 2);
      Serial.write((unsigned char*)&case2, 2);
      Serial.write((unsigned char*)&case3, 2);
      Serial.write((unsigned char*)&error, 2);
    #endif
    previousSerialTime = millis();
  }
}
