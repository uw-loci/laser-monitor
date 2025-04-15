#include <SoftwareSerial.h>
//Version History:
//  v10 version is a copy of v9 with improvements:
//    1. updateLEDs() was updated to only sorth through numLasers
//    2. Moved pinBlinking[laser+4] =false and lastSignal[laser] = millis() inside of check function
//  v9 version is a copy of v8 with improvements:
//    1. add "if (laser <= numLasers) { //only update the laserState if it's from a valid laser"
//       to fix problem where numLasers=1, but there are Lasers 2 and 3 in testing sending valid signals
//       then Laser 2 & 3 turn into ghost lasers that never turn off.
//  v7 version is a copy of v6 with improvements:
//    1. currMillis is now initialized globally in beginning and used throughout
//    2. currMillis is used instead of multiple calls to millis() in the same comparison operation
//    3. errorCode changed to 90+laser so within 255 limit of a byte (instead of 880)
//    4. Definitely set all laser lights to an output and drive LOW in setup so known state and voltage
//  v2 version is identical to v1 but now includes turning the onboard LED on the arduino to increase minimum current draw.
//    1. The batteries we use have an autoshutoff feature that will trigger after some time if not enough current is drawn, and
//    we don't draw much when no lasers are active, so here we also turn the onboard LED on at all times to help prevent
//    the autoshutoff from activating. Onboard LED is pin 13.
//-------------------------------------------------------

//Specify total number of control boxes on this network
const int numLasers = 1; //num of actual in use lasers. turns off blinking of other laser channels that don't exist in the room
const int maxLasers = 4; //maximum number of lasers currently able to be monitored by one unit


//Specify pins that will be used by xbee for transmission
const int TX = 2; // XBee's DOUT (TX) is connected to pin 2 (Arduino's Software RX)
const int RX = 3; // XBee's DIN (RX) is connected to pin 3 (Arduino's Software TX)
SoftwareSerial XBee(TX, RX); //create XBee object of class SoftwareSerial

//Create an array to save the state (i.e. on/off) of each laser. Add 1 to skip the 0 index for more intuitive programming
int laserState[maxLasers + 1] = {0, 0, 0, 0, 0};


int in; //declare variable for value that will be read from XBee

const int commPin = 11; //pin number for comm light
boolean allSignalGood = true; //whether reading signal from all lasers. this is for comm pin, if any signal lost, comm pin will flash
boolean pinBlinking[commPin + 1]; //boolean values indicating if each pin should be blinking. again +1 to avoid 0-indexing

//Create an array to save the last timestamp from when a signal was read from each laser
unsigned long lastSignal[maxLasers + 1] = {0, 0, 0, 0, 0}; //again +1 to avoid 0-indexing

unsigned long lastBlink = 0;  //saves the timestamp of the last blink. This is last blink for all LEDs including comm
boolean blinkState = HIGH;  //alternates between HIGH and LOW for all LEDs that should be blinking

unsigned long prevMillis = 0;
const int interval = 200;  //time interval for state updates (ms)
const int commLoss = 2000; //time intervale at which we say communication has been lost with a laser (ms)

unsigned long currMillis = 0; //initialize and set current milliseconds to 0


void setup() {
  XBee.begin(9600); //begin xbee communication
  Serial.begin(9600); //begin serial communication with any console attached (e.g. the PC)

  //initialize the pinBlinking array as false for all pins (so initially none should be on)
  for (int i = 0; i <= commPin; i++) {
    pinBlinking[i] = false; //initialize the array
  }
  //Initialize LED pins. Pins 5-8 are laser LEDs, pin 11 is comm LED and is initialzed after
  for (int i = 5; i < 5 + maxLasers; i++) {
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }

  //Set up redboard pin 11 as the commPin
  pinMode(commPin, OUTPUT);
  digitalWrite(commPin, LOW);

  //Turn LED for pin13 on to increase current from the battery to prevent it from auto-shutting off
  pinMode(13, OUTPUT);
  digitalWrite(13,HIGH);

  //Cycle through slowly all lights. The power supply will turn off if initial current isn't large enough, thus turn all on slowly.
  digitalWrite(5,HIGH);
  delay(1000);
  digitalWrite(6,HIGH);
  delay(1000);
  digitalWrite(7,HIGH);
  delay(1000);
  digitalWrite(8,HIGH);
  delay(1000);
  digitalWrite(commPin,HIGH);
  delay(1000);
  digitalWrite(8,LOW);
  delay(100);
  digitalWrite(7,LOW);
  delay(100);
  digitalWrite(6,LOW);
  delay(100);
  digitalWrite(5,LOW);
  delay(100);
}


void loop() {
  //Run the process to check for an Xbee input signal. Keeps Xbee buffer manageable.
  process();

  //Update LED states every 200ms
  //  Check if difference in timestamps exceeds global time interval variable
  //  notice this is better than a pure delay, because the if statement will be skipped
  //  and process() called many times before next 200ms LED on/off change
  //  currMillis-prevMillis <=0 added for rare millis() rollover case every 50 days
  currMillis = millis();
  if (long(currMillis - prevMillis) > interval || long(currMillis - prevMillis) <= 0) { //added long() around both OR expressions so they can return a negative value
    prevMillis = currMillis;

    //update whether each LED should be blinking based on signal timestamps
    allSignalGood = true; //this is for comm pin, if any signal lost
      //comm pin will flash
      //^reset comm and recheck all lasers for signal
    for (int i = 1; i < numLasers + 1; i++) {
      //blink laser LED if no signal for >3 seconds
      if (millis() - lastSignal[i] > commLoss) {
        pinBlinking[i + 4] = true; //set individual laser to blink as well
        allSignalGood = false; //we need to have comm blink now
      }
    }

    //If signal missing, blink comm LED.
    if (allSignalGood) {
      //digitalWrite(commPin, HIGH);//*******************
      pinBlinking[commPin] = false;
    }
    else {
      pinBlinking[commPin] = true; //set commPin also to blink state
    }
    updateLEDs(); //update all LEDs except for comm
  }

  //Run the blinking method
  errorBlink();
}


//process input from control boxes
void process() {
  if (XBee.available() > 0) { //check if something is available in xbee buffer to read from
    in = XBee.read(); //we assume here that over sufficient time you will read all lasers statistically

    //First check that the received byte is any of the expected valid values, otherwise exit and retry
    if (in != 10 && in != 11 && in != 20 && in != 21 && in != 30 && in != 31 && in != 40 && in != 41 && in != 91 && in != 92 && in != 93 && in != 94) {
      return;
    }

    //check for error codes, which should be set to 90+laserNumber
    if (in > 90) {
      laserError(in); //sets pinBlinking array element state for correspond laser in error
      return;
    }

    //tens digit = laser number
    //ones digit = state of the corresponding laser (1 for on, 0 for off)
    int laser = in / 10;
    int state = in - (laser * 10);
    //this is useful for when checking if comm should blink in main loop
    //every 2s if no signal then blink comm for any lost signal
    if (laser <= numLasers) { //only update the laserState if it's from a valid laser (i.e. if numLasers=1, don't accept signal from Laser 2)
      laserState[laser] = state; //set to 1 or 0 from controller output
      //update timestamp and state
      lastSignal[laser] = millis(); //set to current timestamp
      //set blinking to false because received signal
      pinBlinking[laser + 4] = false;
    }
  }
}

//When error code received, the corresponding laser light blinks,
// but comm light does not blink.
void laserError(int errorCode) {
  Serial.print("ERROR! CHECK MONITOR BOX #");
  Serial.println(errorCode - 90);
  pinBlinking[errorCode - 90 + 4] = true;
  //assuming controller sent correct error code, this sets correct laser LED into error state
}

//all LEDs that should be blinking will be blinking together at 1Hz
void errorBlink() {

  //compare current timestamp with last time everything blinked
  currMillis = millis();
  if (long(currMillis - lastBlink) >= 500 || long(currMillis - lastBlink) < 0) { //added long() around both OR expressions so they can return a negative value
    //check every pin in pinBlinking. If true, switch state.
    for (int i = 0; i <= commPin; i++) {
      if (pinBlinking[i]) {
        digitalWrite(i, blinkState);
      }
    }

    //update timestamp
    lastBlink = millis();
    //switch the blinkState for next LED state change
    blinkState = !blinkState;
  }
}

//For each laser LED with an active comm link (i.e. is not set for blinking), update its on/off state based on laserState[]
void updateLEDs() {
  for (int i = 1; i <= numLasers; i++) {
    if (!pinBlinking[i + 4])
      //update the on/off state of LED only if there is no commError for that pin
      digitalWrite(i + 4, laserState[i]); //this will also write LOW to any unused laser LEDs since array was initialied with all 0's
  }
  //Only update the comm pin to stay on (i.e. not blinking) if all signals from active lasers are okay
  if (allSignalGood){
    digitalWrite(commPin, HIGH);
  }
}
