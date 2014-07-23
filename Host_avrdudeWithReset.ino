/*
This sketch sets us a DEVICE device to demonstrate 
a serial link between RFDuino modules

This test behaves as a serial pass thru between two RFduinos.
If you disconnect DTR from RESET and connect it to GPIO6, it
will reset a target Arduino UNO for over-air programming

This code uses buffering Serial and Radio packets
Also uses using timout to finding end of serial data

Made by Joel Murphy, Summer 2014
Free to use and share. This code presented as-is. No promises!
*/

#include <RFduinoGZLL.h>

device_t role = HOST;  // This is the HOST code!

const int numBuffers = 10;              // buffer depth
char serialBuffer[numBuffers] [32];  	// buffers to hold serial data
int bufferLevel = 0;                 	// counts which buffer array we are using
int serialIndex[numBuffers];         	// Buffer position counter
int numPackets = 0;                  	// number of packets to send/receive on radio
int serialBuffCounter = 0;
unsigned long serialTimer;              // used to time end of serial message

char radioBuffer[300];		        // buffer to hold radio data
int radioIndex = 0;                  	// used in sendToHost to protect len value
int packetCount = 0;                    // used to keep track of packets in received radio message
int packetsReceived = 0;                // used to count incoming packets

boolean serialToSend = false;           // set when serial data is ready to go to radio
boolean radioToSend = false;            // set when radio data is ready to go to radio
boolean serialTiming = false;           // used to time end of serial message

int resetPin = 6;              // using GPIO6 to feel the DTR go low 
int resetPinValue;             // value of GPIO6 goes here
int lastResetPinValue;         // used to watch the rising/falling edge of GPIO6
char resetMessage[1];          // message sent to DEVICE telling it to set or clear it's own GPIO6
boolean toggleReset = false;   // reset flag


void setup(){
  
  RFduinoGZLL.begin(role);     // start the GZLL stack
  Serial.begin(115200);        // start the serial port
  
  serialIndex[0] = 1;          // save buffer[0][0] to hold number of packets!
  for(int i=0; i<numBuffers; i++){
    serialIndex[i] = 0;        // initialize indexes to 0
  }
  
  pinMode(resetPin,INPUT);     // DTR routed to GPIO6  
  lastResetPinValue = digitalRead(resetPin);  // initialize the lastResetPinValue
  
}



void loop(){
  
  resetPinValue = digitalRead(resetPin);    // read the state of DTR
  if(resetPinValue != lastResetPinValue){   // if it's changed
    if(resetPinValue == LOW){    
      resetMessage[0] = '$';                // '$' indicates falling edge
    }else{
      resetMessage[0] = '#';                // '#' indicates rising edge
    }
    lastResetPinValue = resetPinValue;      // keep track of resetPin state
    toggleReset = true;                     // set toggleReset flag
  }

  if(serialTiming){                      // if the serial port is active
    if(millis() - serialTimer > 2){      // if the time is up
      if(serialIndex[bufferLevel] == 0){bufferLevel--;}  // don't send more buffers than we have!
      serialBuffer[0][0] = bufferLevel +1;  // drop the number of packets into [0][0] position
      serialBuffCounter = 0;          	    // keep track of how many buffers we send
      serialTiming = false;	            // clear serialTiming flag
      serialToSend = true;                  // set serialToSend flag
    }
  }
  
  if(radioToSend){                   // when data comes in on the radio
    for(int i=0; i<radioIndex; i++){
      Serial.write(radioBuffer[i]);  // send it out the serial port
    }
    radioIndex = false;              // reset radioIndex counter
    radioToSend = false;             // reset radioToSend flag
  }
  

if (Serial.available()){      
    while(Serial.available() > 0){            // when the serial port is active
      serialBuffer[bufferLevel][serialIndex[bufferLevel]] = Serial.read();    
      serialIndex[bufferLevel]++;             // count up the buffer size
      if(serialIndex[bufferLevel] == 32);     // when the buffer is full,
        bufferLevel++;			      // next buffer please
      }  // if we just got the last byte, and advanced the bufferLevel, the serialTimeout will catch it
    serialTiming = true;                      // set serialTiming flag    
    serialTimer = millis();                   // start the time-out clock
  }
  
}// end of loop


void RFduinoGZLL_onReceive(device_t device, int rssi, char *data, int len){
  
  if(toggleReset){            // if the DTR pin changes
    RFduinoGZLL.sendToDevice(device, resetMessage, 1);  // send message to DEVICE
    toggleReset = false;      // clear toggleReset flag
  }
  
  if(serialToSend){             // when there is serial data ready to send on radio
    RFduinoGZLL.sendToDevice(device,serialBuffer[serialBuffCounter], serialIndex[serialBuffCounter]); // send it!
    serialBuffCounter++;	            // get ready for next buffered packet
    if(serialBuffCounter == bufferLevel +1){// when we send all the packets
      serialToSend = false; 		    // put down bufferToSend flag
      bufferLevel = 0;			    // initialize bufferLevel
      serialIndex[0] = 1;		    // leave room for packet count
      for(int i=1; i<numBuffers; i++){	
        serialIndex[i] = 0;		    // initialize serialIndecies
      }
    }
  }
  
  if(len > 0){                  // when there is data on the radio
    int startIndex = 0;	        // get ready to read this packet   
    if(packetCount == 0){	// if this is a fresh transaction  
      packetCount = data[0];	// get the number of packets to expect in message
      startIndex = 1;		// skip the first byte when retrieving radio data
    }		
    for(int i = startIndex; i < len; i++){
      radioBuffer[radioIndex] = data[i];    // read in the data packet
      radioIndex++;                         // increment radioBuffer index counter
    }
    packetsReceived++;                          // count the packets that we get
    if(packetsReceived == packetCount){		// when we get all the packets
      packetsReceived = 0;                      // reset packetsReceived flag
      packetCount = 0;                          // reset packetCount
      radioToSend = true;			// set radioToSend flag
    }
  }

}// end of onReceive
