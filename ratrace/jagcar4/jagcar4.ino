#include <Servo.h>
#include <RunningMedian.h>
// Get the line numbering to behave
byte dummy;
#line 6

// Inputs
const byte startPin = 12;
const byte leftDistPin = A5;
const byte rightDistPin = A4;
const byte leftFeelerPin = 7;
const byte batPin = A0;

// Outputs
const byte jagPin = 9;
const byte steerPin = 8;
const byte rightledpow = A2;
const byte rightledgnd = A3;
const byte leftledpow = 2;
const byte leftledgnd = 3;
const byte pwrledpow = 6;
const byte pwrledgnd = 5;

Servo jag;
Servo steer;
const unsigned int leftDistThresh = 150;
const unsigned int rightDistThresh = 150;

// Returns distance in cm
byte readIR(const byte pin) {
    return 1/(0.0000868056*(float)analogRead(pin)-0.00222222);
}

namespace st { // States for state machine
    enum robotstate_t {
        beginning,
        beforeButton,
        afterButtonWait,
        startrace,
        goingForFirstWall,
        firstStraightaway,
        curve,
        secondStraightaway,
        finalcountdown,
    };
    enum statepos_t {
        begin,
        idle,
        end,
    };
    enum substate_t {
        noSubstate,
        backing,
    };
    enum direction_t {
        dontknow,
        clockwise,
        counterclockwise,
    };
}
    
          
void setup() {
    Serial.begin(115200);

    pinMode(rightledpow, OUTPUT); pinMode(rightledgnd, OUTPUT);
    pinMode( leftledpow, OUTPUT); pinMode( leftledgnd, OUTPUT);
    pinMode(  pwrledpow, OUTPUT); pinMode(  pwrledgnd, OUTPUT);
    digitalWrite(rightledgnd, LOW); digitalWrite(leftledgnd, LOW);
    digitalWrite(pwrledgnd, LOW);
    jag.attach(jagPin);
    steer.attach(steerPin);
    digitalWrite(leftFeelerPin, HIGH);
    digitalWrite(startPin, HIGH);
}

/* 1. Get sensor values
 * 2. Police state machine
 * 3. Run state machine
 * 4. Run outputs
 */
void loop() {
// 1.Get sensor values
    static RunningMedian leftMedian;
    static RunningMedian rightMedian;
    leftMedian.add(analogRead(leftDistPin));
    rightMedian.add(analogRead(rightDistPin));

    unsigned int leftDist = leftMedian.getMedian();//readIR(leftDistPin);//leftMedian.getMedian();
    unsigned int rightDist = rightMedian.getMedian(); //readIR(rightDistPin); //rightMedian.getMedian();
    unsigned int powerlvl = analogRead(batPin); // powerlvl is voltage*512
    bool leftFeeler = !digitalRead(leftFeelerPin);
    bool startButton = !digitalRead(startPin);
    // Display debugging leds
//    digitalWrite(leftledpow, leftDist > leftDistThresh);
//    digitalWrite(rightledpow, rightDist > leftDistThresh);
//    digitalWrite(pwrledpow, powerlvl < 3700);
    
    static unsigned long lastPrintTime = 0;
    if (millis() - lastPrintTime > 100) {
        lastPrintTime = millis();
//        Serial.print(leftDist);
//        Serial.print('\t');
//        Serial.print(rightDist);
//        Serial.print('\t');
//
//        Serial.println();
    }
// 2.Police state machine
    int8_t jagspeed = 0; // -100 to 100
    int8_t turnamt = 0; // -100 to 100

    static st::robotstate_t robotstate = st::beginning;
    static st::robotstate_t nextrobotstate = st::beforeButton;
    st::statepos_t statepos = st::begin;
    static st::substate_t substate = st::noSubstate;
    static st::direction_t direction = st::dontknow;
    
    if (statepos == st::begin) statepos = st::idle;

    static unsigned long stateStartTime = millis();
    if (robotstate != nextrobotstate) {
        if (false/*statepos = st::idle*/) statepos = st::end;
        else {
            statepos = st::begin;
            robotstate = nextrobotstate;
            stateStartTime = millis();
        }
    }
    unsigned long timeIntoState = millis() - stateStartTime;
    unsigned long timePlaceholder = 0;

    switch(robotstate) {
    case st::beforeButton:
        if(startButton) nextrobotstate = st::afterButtonWait;
        break;
    case st::afterButtonWait:
        if(timeIntoState > 1000) nextrobotstate = st::startrace;
        break;
    case st::startrace:
        if (timeIntoState > 500) nextrobotstate = st::goingForFirstWall;
        jagspeed = 100;
        turnamt = 0;
        break;
    case st::goingForFirstWall:
        if (leftDist > 135) {
            direction = st::counterclockwise;
            nextrobotstate = st::firstStraightaway;
            digitalWrite(rightledpow, HIGH);
        }
        if (rightDist > 135) {
            direction = st::clockwise;
            nextrobotstate = st::firstStraightaway;
            digitalWrite(rightledpow, LOW);
        }
        jagspeed = 20;
        turnamt = 0;
        break;
    case st::firstStraightaway:
        if(timeIntoState > 100) nextrobotstate = st::curve;
        jagspeed = 20;
        if (direction == st::clockwise) turnamt = -20;
        else if (direction == st::counterclockwise) turnamt = 20;
        else nextrobotstate = st::curve;
        
        break;
    
    case st::curve: 
        if(timeIntoState > 4500)
            nextrobotstate = st::secondStraightaway;
        if (substate == st::backing) {
            if (millis() - timePlaceholder > 1500) {
                substate = st::noSubstate;
            }
            if (direction == st::clockwise) {
                turnamt = 30;
            }
            else if (direction == st::counterclockwise) {
                turnamt = -30;
            }
            else {
                turnamt = 30;
            }
            jagspeed = -13;
        }
        else if (leftDist > leftDistThresh && rightDist <= rightDistThresh) {
            jagspeed = 24;
            if (direction == st::clockwise)
                turnamt = 10;
            else if (direction == st::counterclockwise)
                turnamt = 50;
        }
        else if (leftDist <= leftDistThresh &&  rightDist > rightDistThresh) {
            jagspeed = 24;
            if (direction == st::clockwise)
                turnamt = -50;
            else if (direction == st::counterclockwise)
                turnamt = -10;
        }
        else if (leftDist >= leftDistThresh && rightDist >= rightDistThresh) {
            // Both sensors see a wall
            jagspeed = -13;
            turnamt = 0;
            substate = st::backing;
            timePlaceholder = millis();
        }
       
        else {
            jagspeed = 30;
            if (direction == st::clockwise) {
                turnamt = 32;
            }
            else if (direction == st::counterclockwise) {
                turnamt = -32;
            }
            else {
                turnamt = 0;
            }
        }

        break;
    case st::secondStraightaway:
        if (timeIntoState > 300) nextrobotstate = st::finalcountdown;
        if (direction == st::clockwise) {
            turnamt = 60;
        }
        else {
            turnamt = -60;
        }
        break;
    case st::finalcountdown:
        if(timeIntoState > 7000)
            nextrobotstate = st::beginning;
        
        if (substate == st::backing) {
            if (millis() - timePlaceholder > 1500) {
                substate = st::noSubstate;
            }
            if (direction == st::clockwise) {
                turnamt = -30;
            }
            else if (direction == st::counterclockwise) {
                turnamt = 30;
            }
            else {
                turnamt = 30;
            }
            jagspeed = -13;
        }
        
        if (leftDist > leftDistThresh && rightDist <= rightDistThresh) {
            jagspeed = 27;
            turnamt = 60;
        }
        else if (leftDist <= leftDistThresh &&  rightDist > rightDistThresh) {
            jagspeed = 27;
            turnamt = -60;
        }
        else if (leftDist >= leftDistThresh && rightDist >= rightDistThresh) {
            // Both sensors see a wall
            jagspeed = -13;
            turnamt = 0;
            substate = st::backing;
            timePlaceholder = millis();
        }
        else {
            jagspeed = 30;
            turnamt = 0;
        }
//       
//        case st::curve:
//            if (timeIntoState > 6500) nextrobotstate = st::secondStraightaway;
//            jagspeed = 24;
//            turnamt = -25;
//            break;
//        case st::secondStraightaway:
//            if (timeIntoState > 7000) nextrobotstate = st::beforeButton;
//            jagspeed = 40;
//            turnamt = 25;
//            break;
    default:
        Serial.print("Undefined state: "); Serial.print(robotstate); Serial.print('\t');
    }


// Jag datasheet says it wants a puse between .67ms and 2.33ms
    jag.writeMicroseconds(map(jagspeed, -100, 100, 670, 2330));
// May have to change the 0,180 to match what the servo wants
    steer.write(map(turnamt, -100, 100, 0, 180));

}
