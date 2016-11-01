/*
  ----------------------------------------------------------------------------

  This file is part of the Sanworks ChoiceWheel repository
  Copyright (C) 2016 Sanworks LLC, Sound Beach, New York, USA

  ----------------------------------------------------------------------------

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, version 3.

  This program is distributed  WITHOUT ANY WARRANTY and without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/
#include "ArCOM.h"
ArCOM USB(SerialUSB); // USB is an ArCOM object. ArCOM wraps Arduino's SerialUSB interface, to
// simplify moving data types between Arduino and MATLAB/GNU Octave.

// Hardware setup
byte EncoderPinA = A0;
byte EncoderPinB = A1;
byte PreTrialPin = 8;
byte TrialStartPin = 9;
byte LeftChoiceTTLPin = 10;
byte RightChoiceTTLPin = 11;
byte TimeoutTTLPin = 12;

// Parameters
unsigned short leftThreshold = 412; // in range 0-1024, corresponding to 0-360 degrees
unsigned short rightThreshold = 612; // in range 0-1024, corresponding to 0-360 degrees
unsigned long timeout = 5000; // in ms
unsigned long preTrialIdleTime = 1000; // Time the ball must be idle to start a trial in ms
unsigned long preTrialDuration = 0; // Actual time animal spent moving ball before a trial could start
unsigned short idleTimeMotionGrace = 10; // During pre-trial idle, moving this distance in either direction resets the idle timer
unsigned short TTLDuration = 1; // in ms

// State variables
boolean isStreaming = 0; // If currently streaming position and time data
boolean isLogging = 0; // If currently logging position and time to RAM memory
boolean inPreTrial = 0; // If currently waiting for ball to idle for idleTime2Start ms before trial start
boolean inTrial = 0; // If currently in a behavior trial
boolean playingTTL = 0; // If a trial-end TTL is being played
int EncoderPos = 0; // Current position of the rotary encoder

// Program variables
byte opCode = 0;
byte param = 0;
boolean EncoderPinAValue = 0;
boolean EncoderPinALastValue = 0;
boolean EncoderPinBValue = 0;
boolean posChange = 0;
word EncoderPos16Bit =  0;
unsigned long timeLog[10000] = {0};
unsigned short posLog[10000] = {0};
unsigned long choiceTime = 0;
unsigned int dataPos = 0;
word dataMax = 10000;
unsigned long startTime = 0;
unsigned long currentTime = 0;
unsigned long timeFromStart = 0;
byte terminatingEvent = 0;
unsigned short idleTimeMotionGraceLow = 512-idleTimeMotionGrace;
unsigned short idleTimeMotionGraceHigh = 512+idleTimeMotionGrace;

void setup() {
  // put your setup code here, to run once:
  SerialUSB.begin(115200);
  pinMode(EncoderPinA, INPUT);
  pinMode (EncoderPinB, INPUT);
  pinMode(PreTrialPin, OUTPUT);
  pinMode(TrialStartPin, OUTPUT);
  pinMode(LeftChoiceTTLPin, OUTPUT);
  pinMode (RightChoiceTTLPin, OUTPUT);
  pinMode (TimeoutTTLPin, OUTPUT);
}

void loop() {
  currentTime = millis();
  if (SerialUSB.available() > 0) {
    opCode = USB.readByte();
    switch(opCode) {
      case 'C': // Handshake
        USB.writeByte(217);
      break;
      case 'S': // Start streaming
        EncoderPos = 512; // Set to mid-range
        isStreaming = true;
        startTime = currentTime;
      break;
      case 'T': // Start trial
        inPreTrial = true;
        EncoderPos = 512;
        dataPos = 0;
        preTrialDuration = 0;
        digitalWriteDirect(PreTrialPin, HIGH);
        startTime = currentTime;
      break;
      case 'P': // Program parameters
        param = USB.readByte();
        switch(param) {
          case 'L':
            leftThreshold = USB.readUint16();
          break;
          case 'R':  
            rightThreshold = USB.readUint16();
          break;
          case 'I':
            preTrialIdleTime = USB.readUint32();
          break;
          case 'G':
            idleTimeMotionGrace = USB.readUint16();
          break;
          case 'T':
            timeout = USB.readUint32();
          break;
          case 'E':
            PreTrialPin = USB.readByte();
            TrialStartPin = USB.readByte();
            LeftChoiceTTLPin = USB.readByte();
            RightChoiceTTLPin = USB.readByte();
            TimeoutTTLPin = USB.readByte();
            pinMode(PreTrialPin, OUTPUT); digitalWriteDirect(PreTrialPin, LOW);
            pinMode(TrialStartPin, OUTPUT); digitalWriteDirect(TrialStartPin, LOW);
            pinMode(LeftChoiceTTLPin, OUTPUT); digitalWriteDirect(LeftChoiceTTLPin, LOW);
            pinMode (RightChoiceTTLPin, OUTPUT); digitalWriteDirect(RightChoiceTTLPin, LOW);
            pinMode (TimeoutTTLPin, OUTPUT); digitalWriteDirect(TimeoutTTLPin, LOW);
            
          break;
        }
      break;
      case 'R': // Return data
        isLogging = false;
        USB.writeUint16(dataPos);
        USB.writeUint16Array(posLog,dataPos); 
        USB.writeUint32Array(timeLog,dataPos); 
        dataPos = 0;
      break;
      case 'Q': // Return current encoder position
        USB.writeUint16(EncoderPos);
      break;
      case 'X': // Exit
        isStreaming = false;
        isLogging = false;
        inTrial = false;
        inPreTrial = false;
        dataPos = 0;
        EncoderPos = 512;
      break;
    } // End switch(opCode)
  } // End if (SerialUSB.available())
  timeFromStart = currentTime - startTime;
  EncoderPinAValue = digitalReadDirect(EncoderPinA);
  if (EncoderPinAValue == HIGH && EncoderPinALastValue == LOW) {
    EncoderPinBValue = digitalReadDirect(EncoderPinB);
    if (EncoderPinBValue == HIGH) {
      EncoderPos--; posChange = true;
    } else {
      EncoderPos++; posChange = true;
    }
    if (EncoderPos == 1024) {
      EncoderPos = 0;
    } else if (EncoderPos == -1) {
      EncoderPos = 1023;
    }
    if (isStreaming) {
      EncoderPos16Bit = (word)EncoderPos;
      USB.writeUint16(EncoderPos16Bit);
      USB.writeUint32(timeFromStart);
    }
  }
  if (inTrial) {
    if (posChange) { // If the position changed since previous loop
      if (EncoderPos <= leftThreshold) {
        digitalWriteDirect(LeftChoiceTTLPin, HIGH);
        inTrial = false;
        terminatingEvent = 1;
      }
      if (EncoderPos >= rightThreshold) {
        digitalWriteDirect(RightChoiceTTLPin, HIGH);
        inTrial = false;
        terminatingEvent = 2;
      }
    }
    if (timeFromStart >= timeout) {
      digitalWriteDirect(TimeoutTTLPin, HIGH);
      inTrial = false;
      terminatingEvent = 3;
    }
    if (!inTrial) {
      playingTTL = true; startTime = currentTime;
      if (dataPos<dataMax) { // Add final data point
        posLog[dataPos] = EncoderPos;
        timeLog[dataPos] = timeFromStart;
        dataPos++;
      }
      isLogging = false; // Stop logging
    }
  } else if (playingTTL) {
    if (timeFromStart >= TTLDuration) { // If trial outcome TTL should finish
      digitalWriteDirect(LeftChoiceTTLPin, LOW);
      digitalWriteDirect(RightChoiceTTLPin, LOW);
      digitalWriteDirect(TimeoutTTLPin, LOW);
      digitalWriteDirect(TrialStartPin, LOW);
      playingTTL = false;
      USB.writeUint32(preTrialDuration); // Return data
      USB.writeUint16(dataPos); 
      USB.writeUint16Array(posLog,dataPos); 
      USB.writeUint32Array(timeLog,dataPos);
      USB.writeByte(terminatingEvent); 
      dataPos = 0;
    }
  } else if (inPreTrial) {
    if (EncoderPos <= idleTimeMotionGraceLow) {
      preTrialDuration += timeFromStart;
      startTime = currentTime;
      EncoderPos = 512;
    }
    if (EncoderPos >= idleTimeMotionGraceHigh) {
      preTrialDuration += timeFromStart;
      startTime = currentTime;
      EncoderPos = 512;
    }
    if (timeFromStart >= preTrialIdleTime) {
      digitalWriteDirect(PreTrialPin, LOW);
      digitalWriteDirect(TrialStartPin, HIGH);
      inTrial = true;
      isLogging = true;
      inPreTrial = false;
      startTime = currentTime;
      preTrialDuration += timeFromStart;
      timeFromStart = 0;
    }
  }
  if (isLogging) {
    if (posChange) { // If the position changed since previous loop
      posChange = false;
      if (dataPos<dataMax) {
        posLog[dataPos] = EncoderPos;
        timeLog[dataPos] = timeFromStart;
        dataPos++;
      }
    }
  }
  EncoderPinALastValue = EncoderPinAValue;
}

byte digitalReadDirect(int pin){ // ARM code allows faster digital reads than Arduino's digitalRead
  return !!(g_APinDescription[pin].pPort -> PIO_PDSR & g_APinDescription[pin].ulPin);
}
void digitalWriteDirect(int pin, boolean val){ // ARM code allows faster digital writes than Arduino's digitalWrite
  if(val) g_APinDescription[pin].pPort -> PIO_SODR = g_APinDescription[pin].ulPin;
  else    g_APinDescription[pin].pPort -> PIO_CODR = g_APinDescription[pin].ulPin;
}
