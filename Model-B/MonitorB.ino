//This v5 version is a copy of v4 with improvements:
//  1. random delay moved to inside if statement to ensure no repeated misses
//  2. random delay increased to 100 ms instead of 20 ms

/*
   The signal sent to the indicator light is a boolean value
   indicating whether the laser is on, instead of randem readings

   Overview of operation: each control box broadcasts a two digit integer to any listening
   indicator boxes with a matched configured channel/ID. e.g. "11" int is broadcast, the first
   1 represents laser control box 1 is referred to and the second 1 says its indicator
   light should be on (binary 1). Max number of control boxes is 4 due to limit of
   4 lights on indicator box. 4 simulatenous have been tested, in principle 5 would also
   work. The signal is broadcast is every 50ms + a random extra number of ms from 0 to 20
   ms to avoid collisions in network. Random collisions may still occur, but odds of
   multiple or repeating collisions drops rapidly with time. Each indicator box will
   still read every control box but ignore if channel id does not match. The decision
   of which state to broadcast every 50+R is determined by current mode vs light sensor mode
   and subsequent reading on sensor pin or ttl pin. Each controller only emits 1
   ID and multiple doors/indicators can read from. Similarly, if 4 lasers, each
   controller has its own unique ID to broadcast. TTL mode reads 5V differential
   on pin and gnd on arduino (see assembly instructions). This code should boot up every time its powered
   and there are no known scenarios that will stop the code from executing but it
   does have some error checks on the controller side so that if it sees an error
   state it will broadcast this state for this laser to all indicators, and the
   indicator LED strip for that laser will begin flashing repeatedly until the
   error is resolved (whether fixes itself or user intervention). Each xbee has
   in the controller transmits with a sender ID (part of xbee settings different
   than laser ID) and you can program the xbees on indicators to only listen to
   that one. This is redundant info with laser ID within one room, but would be
   important to make unique per room where there could be overlap of laser IDs.
   Each xbee also has a channel you can set, so for collisions or adjacent you
   would need same channel number and same sender ID (neither of these are laser ID). Idea
   to have open source code on github with a page where user/builders claim unused
   channels and sender IDs. We will make sure all on our end are built unique. Operates
   at 2.4 GHz nominally but varies from that a bit depending on channel.

   xbee network config: https://learn.sparkfun.com/tutorials/exploring-xbees-and-xctu/configuring-networks
*/


#include <SoftwareSerial.h>
SoftwareSerial XBee(2, 3); // RX set to pin 2 of arduino, TX set to pin 3 of arduino
//those are serial pins on arduino

//------------------------------------
//Specify the ID of the control box
const int laserLabel = 1;
//-------------------------------------

const int LIGHT_SENSOR_PIN = 5; //this is the analog pin for the photocell in Model B
const int LED_PIN = 13; //output pin for the LED on the light sensor
const int ANALOG_THRESHOLD = 100; //threshold for light sensor intensity to be high/low
int analogValue; //variable to store the value read from analog read on sensor

int modeState = 0; //variable to store the mode state after reading ttl vs current (high, low)
int powerToggle = 8; //power toggle pin will be pin D8. state of switch: ttl vs sensor
int ttlRead = 10; //ttl read pin will be pin D10

int out; //laserLabel*10+state which is broadcast to indicators
int state; //state of laser which is incorporated in "out" broadcast
int errorCode = 90;  //setup() generates unique errorCode for this laser (errorCode+laserLabel)

unsigned long prevMillis = 0; //timestamps used

const long interval = 50;  //broadcast interval in ms
int nextInterval = 50 + random(100);


void setup() {
  // put your setup code here, to run once:
  XBee.begin(9600); //begin serial comm
  Serial.begin(9600); //begin serial comm

  pinMode(LED_PIN, OUTPUT); //set LED pin for light sensor to output mode
  pinMode(powerToggle, INPUT_PULLUP); //set to input with built-in pullup resistor
    //powerToggle == LOW is CURRENT SENSING mode
    //powerToggle == HIGH is TTL SENSING mode
  pinMode(ttlRead, INPUT_PULLUP); //set to input with built-in pullup resistor

  //"out" is set to errorCode and broadcast if occurs
  //errorCode = 90+laserLabel. Eg. for laser 1, 91 is its unique error
  //code for the indicators to recognize
  errorCode = errorCode +laserLabel;

}


void loop() {
  modeState = digitalRead(powerToggle);
  //Check for which mode is in use
  if (modeState == LOW){
    out = lightSensor();
    //Serial.println("current");
  } else if (modeState == HIGH){
    out = ttlSensor();
    //Serial.println("TTL");
  } else {
    //The toggle might be stuck in the middle, where both read pins are LOW
    out = error();
  }
 
  //send signal every 50+random(20) ms. The random number is for avoiding collisions
  unsigned long currMillis = millis(); //provides current timestamp
    //returns number of ms since the arduino began running current program
    //number overflows, go back to zero, after approximately 50 days
  if (long(currMillis - prevMillis) > nextInterval || long(currMillis - prevMillis) < 0) { //added 4/11 so <0 possible with signed long
    //output every 50+R or as soon as we roll over millis() after 50 days on time
    prevMillis = currMillis;
    XBee.write(out); //broadcast to indicator
    nextInterval = interval + random(100);
//    Serial.println(out); //printing for debugging
  }
}

int lightSensor(){
  analogValue = analogRead(LIGHT_SENSOR_PIN);
  if (analogValue > ANALOG_THRESHOLD){
    state = HIGH;
    digitalWrite(LED_PIN, HIGH);
  }
  else {
    state = LOW;
    digitalWrite(LED_PIN, LOW);    
  }
  return (laserLabel * 10 + state);
}


int ttlSensor(){
  //Serial.println("TTL");
  state = digitalRead(ttlRead);
  Serial.print("ttl reading: ");
  Serial.println(state);
  return (laserLabel * 10 + state);
}


int error(){
  return (errorCode);
}

