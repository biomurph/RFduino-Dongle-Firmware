/*
This sketch sets us a DEVICE device to demonstrate 
a serial link between RFDuino modules

This test behaves as a serial pass thru between two RFduinos,
and will reset a target Arduino UNO with GPIO6 for over-air programming

This code uses buffering Serial and Radio packets
Also uses using timout to finding end of serial data

Made by Joel Murphy, Summer 2014
Free to use and share. This code presented as-is. No promises!

*/

#include <RFduinoGZLL.h>

device_t role = DEVICE0;  // This is the DEVICE code

const int numBuffers = 10;              // buffer depth
char serialBuffer[numBuffers] [32];   // buffers to hold serial data
int bufferLevel = 0;                  // counts which buffer array we are using
int serialIndex[numBuffers];          // Buffer position counter
int numPackets = 0;                   // number of packets to send/receive on radio
int serialBuffCounter = 0;
unsigned long serialTimer;              // used to time end of serial message

char radioBuffer[300];            // buffer to hold radio data
int radioIndex = 0;                   // used in sendToHost to protect len value
int packetCount = 0;                    // used to keep track of packets in received radio message
int packetsReceived = 0;                // used to count incoming packets

boolean serialToSend = false;     // set when serial data is ready to go to radio
boolean radioToSend = false;      // set when radio data is ready to go to serial
boolean serialTiming = false;     // used to time end of serial message

unsigned long lastPoll;         // used to time null message to host

int resetPin = 6;               // GPIO5 is connected to Arduino UNO pin with 1uF cap in series
boolean toggleReset = false;    // reset flat


void setup(){
  RFduinoGZLL.begin(role);  // start the GZLL stack
  Serial.begin(115200,3,2);     // start the serial port, rx = GPIO3, tx = GPIO2
  
  initBuffer();
   
  lastPoll = millis();    // set time to perfom next poll 
  
  pinMode(resetPin,OUTPUT);      // set direction of GPIO6
  digitalWrite(resetPin,HIGH);   // take Arduino out of reset
  
  
}



void loop(){
  
  if(serialTiming){                      // if the serial port is active
    if(millis() - serialTimer > 1){      // if the time is up
      if(serialIndex[bufferLevel] == 0){bufferLevel--;}  // don't send more buffers than we have!
      serialBuffer[0][0] = bufferLevel +1;  // drop the number of packets into zero position
      serialBuffCounter = 0;              // keep track of how many buffers we send
      serialTiming = false;        // clear serialTiming flag
      serialToSend = true;             // set serialToSend flag
      lastPoll = millis();             // put off sending a scheduled poll
      RFduinoGZLL.sendToHost(NULL,0);  // send a poll right now to get the ack back
    }
  }

    
  if (millis() - lastPoll > 50){  // make sure to ping the host if they want to send packet
    if(!serialTiming && !serialToSend && !radioToSend){  // don't poll if we are doing something important!
      RFduinoGZLL.sendToHost(NULL,0);
    }
    lastPoll = millis();          // set timer for next poll time
  }
  

if(radioToSend){                   // when data comes in on the radio
  for(int i=0; i<radioIndex; i++){
    Serial.write(radioBuffer[i]);  // send it out the serial port
  }
  radioIndex = 0;                  // reset radioInex counter
  radioToSend = false;             // reset radioToSend flag
}

if(Serial.available()){
  while(Serial.available() > 0){          // while the serial is active
    serialBuffer[bufferLevel][serialIndex[bufferLevel]] = Serial.read();    
    serialIndex[bufferLevel]++;           // count up the buffer size
    if(serialIndex[bufferLevel] == 32){   // when the buffer is full,
      bufferLevel++;        // next buffer please
    }  // if we just got the last byte, and advanced the bufferLevel, the serialTimeout will catch it
  }
  serialTiming = true;                   // set serialTiming flag
  serialTimer = millis();                // start time-out clock
}

}// end of loop



void RFduinoGZLL_onReceive(device_t DEVICE0, int rssi, char *data, int len)
{
  
  if(serialToSend){ // send buffer to host during onReceive so as not to clog the radio
    RFduinoGZLL.sendToHost(serialBuffer[serialBuffCounter], serialIndex[serialBuffCounter]);
    serialBuffCounter++;        // get ready for next buffered packet
    if(serialBuffCounter == bufferLevel +1){// when we send all the packets
      serialToSend = false;         // put down bufferToSend flag
      bufferLevel = 0;          // initialize bufferLevel
      initBuffer();
    }
  }
  
  if(len == 1){
     if(testFirstByte(data[0])){       // if we get a '$' or '#' 
        return;                         // get outa here!
      }                         // if we didn't get a '$' or '#' the first byte = number of packets 
  }
  
  if(len > 0){
    int startIndex = 0;                 // get ready to read this packet from 0
    if(packetCount == 0){         // if this first packet in transaction 
      packetCount = data[0];  // get the number of packets to expect in message
      startIndex = 1;   // skip the first byte when retrieving radio data
    }   
    for(int i = startIndex; i < len; i++){
      radioBuffer[radioIndex] = data[i];  // read packet into radioBuffer
      radioIndex++;                       // increment the radioBuffer index counter
    }
    packetsReceived++;                    // increment the packet counter
    if(packetsReceived == packetCount){   // when we get all the packets
      packetsReceived = 0;                // reset packets Received for next time
      packetCount = 0;                    // reset packetCount for next time
      radioToSend = true;           // set radioToSend flag
    }else{                                // if we're still expecting packets,
      RFduinoGZLL.sendToHost(NULL,0);     // poll host for next packet
    }
  }
   
    lastPoll = millis();  // whenever we get an ACK, reset the lastPoll time 

}
  
  
  
boolean testFirstByte(char z){  // test the first byte of a new radio packet for reset msg
  boolean r;
  switch(z){
    case '$':                 // HOST sends '$' when its GPIO6 goes LOW
      digitalWrite(resetPin,LOW);  // clear RESET pin
      r = true;  
      break;
    case '#':                 // HOST sends '#' when its GPIO6 goes HIGH
      digitalWrite(resetPin,HIGH); // set RESET pin
      r = true;
      break;   
    default:
      r = false;
      break;
  }
  return r;
}
  

void initBuffer(){
  serialIndex[0] = 1;          // save buffer[0][0] to hold number of packets!
  for(int i=1; i<numBuffers; i++){
    serialIndex[i] = 0;        // initialize indexes to 0
  }
}
