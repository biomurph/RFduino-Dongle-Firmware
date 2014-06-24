/*
This sketch sets up a gazelle DEVICE 

This test of programming arduino over air

the avrdude protocol from the target arduino side uses
0x14 as start of transmission char
0x10 as end of transmission char

avrdude protocol from the host pc side uses
various command chars for start of transmission
0x20 as end of transmission char

without identified command, this sketch looks for the 0x20 on the serial
then sends a <= 32 byte packet over air

program pages begin with 'd' command from pc
two bytes following 'd' indicate size of page 
upload xfer = 133 bytes long ('d' + 2 byte page length + 'F' + 128bytes + 0x20) 4 packets + 5 bytes

verification of upload begins with 't' command from pc
two bytes following 't' indicate size of page 
verify xfer = 130 bytes long (0x14 + 128bytes + 0x10) 4 packets + 2 bytes

!! DISCONNECT THE RESET PIN FROM THE HOST RFDUINO MODULE OR ELSE YOU WILL PROGRAM THE HOST !!

press reset on arduino UNO before compiling and time release with start of upload.

This code presented as-is with no promise of usabliltiy


*/

#include <RFduinoGZLL.h>

device_t role = DEVICE0;      // This is the DEVICE code

char bleDataPacket [133];     // holds incomming radio data to send via serial, bigger than needed
int blePacketLength = 0;      // counter for bleDataPacket array
// use 2D buffer to manage 32 byte package size
const int numBuffers = 5;            // buffer depth
char serialBuffer[numBuffers] [32];  // buffers to hold serial data
int bufferLevel = 0;                 // counts which buffer array we are using
int bufferIndex[numBuffers];         // Buffer position counter
int pageLength = 0;                  // size of current upload/verify page length
int packetLength;                    // used in sendToHost to protect len value

unsigned long lastPoll;           // used to time poll sent to host

boolean verifying = false;        // flag gets set when 't' found in first radio byte
boolean uploading = false;        // flag gets set when 'd' found in first radio byte
boolean bleToSend = false;        // set when radio data is ready to go to serial
boolean serialToSend = false;     // set when serial data is ready to go to serial
boolean bufferToSend = false;
boolean needPageLength = false;   // used to find pageLength during upload/verify


void setup(){
  
  RFduinoGZLL.begin(role);  // start the GZLL stack
  Serial.begin(115200);     // start the serial port

  for(int i=0; i<numBuffers; i++){
    bufferIndex[i] = 0;         // initialize all indexes to 0
  }
  
  lastPoll = millis();    // set time to perfom next poll 



}



void loop(){
  
  if(bleToSend){          // when we get a full packet from radio, 
    for (int i=0; i < blePacketLength; i++){
      Serial.write(bleDataPacket[i]);  // send out on Serial.write
    }
    bleToSend = false;    // reset bleToSend flag
    blePacketLength = 0;  // reset blePacketLength counter
    lastPoll = millis();  // start lastPoll timer to avoid radio crash
    RFduinoGZLL.sendToHost(NULL,0);  // send a request to HOST if they have more to send (uploading)
  }

    
  if (millis() - lastPoll > 20){      // maybe adjust the timing with a variable?
    if(!Serial.available() || !uploading){  // don't do this if the serial is active or uploading
      RFduinoGZLL.sendToHost(NULL,0); // ping the host if they want to send a packet
    }
    lastPoll = millis();              // keep track of the time!
  } 

}// end of loop


void serialEvent(){
    char c = Serial.read();

    if(!verifying){               // when not verifying, expect to get short serial messages
      serialBuffer[0][bufferIndex[0]] = c;    // store the incoming serial in first array
      bufferIndex[0]++;           // keep track of the serialPacketLength
      if(c == 0x10){               // when we hit the 0x10 the message is done
        packetLength = bufferIndex[0];        // protect the len parameter (?)
        RFduinoGZLL.sendToHost(serialBuffer[0],packetLength);
        bufferIndex[0] = 0;       // reset the buffer index for next time
      }      
    }

    if(verifying){                  // when verifying, we need to buffer the data
      serialBuffer[bufferLevel][bufferIndex[bufferLevel]] = c;    // store the new byte in Buffer array
      
      bufferIndex[bufferLevel]++;              // count up the buffer size
      if(bufferIndex[bufferLevel] == 32){bufferLevel++;}  // when the buffer is full, next buffer please
      if((bufferLevel*32 + bufferIndex[bufferLevel]) == pageLength){  // when the array[s] fill to pageLength      
        verifying = false;          // reset uploading flag
        pageLength = 0;             // reset pageLength
        bufferLevel = 0;            // reset buffer depth
        bufferToSend = true;        // time to send Buffer to the radio!
      }
    }
  }


void RFduinoGZLL_onReceive(device_t device, int rssi, char *data, int len)
{
  
  if(bufferToSend){
    RFduinoGZLL.sendToHost(serialBuffer[bufferLevel], bufferIndex[bufferLevel]);
    bufferLevel++;
    if(bufferLevel > 4){
      bufferToSend = false; 
      bufferLevel = 0;
      for(int i=0; i<numBuffers; i++){
        bufferIndex[i] = 0;
      }
    }
  }
  
  if(len > 0){  // if there is data in the Host message
    if(blePacketLength == 0){testFirstByte(data[0]);}  // test first byte for command chars of interest

    if(!uploading){               // expect small packets when not uploading
      for(int i=0; i<len; i++){
        bleDataPacket[i] = data[i];    // buffer radio data
        if(i == 2 && needPageLength){  // this is where we may get verify page size
          pageLength = data[i] + 2;    // keep track of length of data payload size for verifying
          needPageLength = false;      // reset needPageLength flag 'cause we did that
        } 
      }
      blePacketLength = len;        
      bleToSend = true;
    }

    if(uploading){                  // upload page standard 128 bytes + 5
      for(int i=0; i<len; i++){
        bleDataPacket[blePacketLength] = data[i]; // load the incoming program page into buffer
        if(blePacketLength == 2 && needPageLength){ // this is where we get program page length
          pageLength = data[i] + 5;  // take note of the page length
          needPageLength = false;    // reset needPageLength flag 'cause we did that
        }
        blePacketLength++;    // cound the number of bytes in the program page
      }             // pass the page upload directly to serial when it's ready
      if(blePacketLength == pageLength){
        uploading = false;
        bleToSend = true;
      }
    }
  }
  lastPoll = millis();  // whenever we get an ACK, reset the lastPoll time 
}
  
  
  
  
void testFirstByte(char z){
  switch(z){
    case 'd':           
      uploading = true;       // we're uploading!
      needPageLength = true;  // look for page size in radio data
      break;
    case 't':
      verifying = true;       // we're verifying!
      needPageLength = true;  // look for page size in radio data
      bufferLevel = 0;        // initialize Buffer array
      for(int i=0; i<numBuffers; i++){
        bufferIndex[i] = 0;   // initialize all indexes to 0
      }
      break;
    default:
      break;
  }
}

  

