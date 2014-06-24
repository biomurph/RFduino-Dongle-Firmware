/*

Host Arduino Upload withReset

This sketch sets up a gazelle HOST 

the avrdude protocol from the target arduino side uses
0x14 as start of transmission char
0x10 as end of transmission char

avrdude protocol from the host pc side uses
various command chars for start of transmission
0x20 as end of transmission char

this sketch mostly looks for the 0x20 in the serial string before sending packet over air
if the upload/verify command is encountered, the data is further buffered before sending

all setup commands use above scheme.

program pages begin with 'd' command from pc
two bytes following 'd' indicate size of page 
upload xfer = 133 bytes long ('d' + 2 byte page length + 'F' + 128bytes + 0x20) 4 packets + 5 bytes

verification of upload begins with 't' command from pc
two bytes following 't' indicate size of page 
verify xfer = 130 bytes long (0x14 + 128bytes + 0x10) 4 packets + 2 bytes

RESET control routed to GPIO6 of RFduino Host.
Host sends '$' when RESET goes from HIGH to LOW
Host sends '#' when RESET goes from LOW to HIGH

DEVICE GPIO6 will mirror behavior of HOST GPIO6

This code presented as-is with no promise of usabliltiy
    Use it or Loose it

*/

#include <RFduinoGZLL.h>

device_t role = HOST;  // This is the HOST code!

const int numBuffers = 5;               // buffer depth
char serialBuffer[numBuffers] [32];     // buffers to hold serial data
int bufferLevel = 0;                    // counts which buffer array we are using
int bufferIndex[numBuffers];            // Buffer position counter
int pageLength = 0;                     // size of current upload/verify page length
int packetLength;                       // used in sendToDevice to protect len value

char bleDataPacket [130];   // holds incomming radio data
int blePacketLength = 0;    // counter for bleDataPacket array

int reset = 6;                    // reset pin routed to GPIO6
int resetValue;                   // holds GPIO6 value
int lastResetValue;               // keep track of last GPIO6 value
char resetMessage[] = {'$'};      // used to tell DEVICE to put reset LOW
char completeMessage[] = {'#'};   // used to tell DEVICE to put reset HIGH

boolean uploading = false;      // set when 'd' is found in serial
boolean verifying = false;      // set whe 't' is found in serial
boolean bleToSend = false;      // set when radio data is ready to go to serial
boolean serialToSend = false;   // set when serial data is ready to go to radio
boolean bufferToSend = false;   // set when serialBuffer is filled to pageLength
boolean needPageLength = false; // used to find page size during upload/verify
boolean resetTarget = false;    // set on rising edge of GPIO6
boolean uploadComplete = false; // set on falling edge of GPIO6


void setup(){
  
  RFduinoGZLL.begin(role); // start the GZLL stack
  Serial.begin(115200);  // start the serial port
  
  for(int i=0; i<numBuffers; i++){
    bufferIndex[i] = 0;         // initialize all indexes to 0
  }
  
  pinMode(reset,INPUT);     // setup re-route of reset pin to GPIO6
  lastResetValue = digitalRead(reset);  // pre-load lastResetValue
}



void loop(){
  resetValue = digitalRead(reset);      // poll reset pin
  if (resetValue != lastResetValue){
    if(resetValue == LOW){              // if reset goes from HIGH to LOW
      resetTarget = true;               // avrdude is going to upload new firmware!
    }else{                              // if reset goes from LOW to HIGH
      uploadComplete = true;            // we're done uploading, regarless of success!
    }
    lastResetValue = resetValue;        // keep track of reset pin!
  }
  
  
  if (bleToSend){         // when radio data is ready to go serial
    for (int i=0; i<blePacketLength; i++){
      Serial.write(bleDataPacket[i]);   // send whatever is in the bleDataPacket
    }
    bleToSend = false;    // put down the bleToSend flag
    blePacketLength = 0;  // reset blePacketLength for next time
  }
  
} // end of loop


void serialEvent(){
    char c = Serial.read();   // read the seiral port
    
    if(bufferIndex[0] == 0){testFirstSerialByte(c);}  // test for command byte of interest

    if(!uploading){                 // if this is a normally short avrdude message
      serialBuffer[0][bufferIndex[0]] = c;   // store the incoming serial in array
      if(bufferIndex[0] == 2 && needPageLength){  // this is where we get verify page size
        pageLength = c + 2;    // keep track of length of data payload size for verifying
        needPageLength = false;   // reset needPageLength flag 'cause we did that
      }
      bufferIndex[0]++;       // keep track of the serialPacketLength
      if(c == 0x20){serialToSend = true;}  // when the 0x20 hits, send to radio
    }

    if(uploading){                  // uploading requires a buffer between serial and radio
      serialBuffer[bufferLevel][bufferIndex[bufferLevel]] = c;    // store the upload page in Buffer array
      if(bufferIndex[0] == 2 && needPageLength){  // timely check for upload page length
        pageLength = c + 5;      // keep track of length of data payload size for uploading
        needPageLength = false;     // reset needPageLength flag 'cause we did that
      }
      bufferIndex[bufferLevel]++;              // count up the buffer size
      if(bufferIndex[bufferLevel] == 32){bufferLevel++;}  // when the buffer is full, next buffer please
      if((bufferLevel*32 + bufferIndex[bufferLevel]) == pageLength){  // when the array is full to pageLength      
        uploading = false;          // reset uploading flag
        pageLength = 0;             // reset pageLength
        bufferLevel = 0;            // reset buffer depth
        bufferToSend = true;        // time to send Buffer to the radio!
      }
    }
    
}


void RFduinoGZLL_onReceive(device_t device, int rssi, char *data, int len){
  
  if(resetTarget){          // when avrdude want's to upload firmware
    resetTarget = false;    // send signal to DEVICE 
    RFduinoGZLL.sendToDevice(device, resetMessage, 1);
    return;                 // get outa here
  }

  if(uploadComplete){       // when avrdude is done with uploading
    uploadComplete = false; // send signal to DEVICE
    RFduinoGZLL.sendToDevice(device, completeMessage, 1);
    return;                 // get outa here
  }  

  if(serialToSend){   // when data packet is ready
    packetLength = bufferIndex[0];
    RFduinoGZLL.sendToDevice(device, serialBuffer[0], packetLength);
    serialToSend = false;     // reset serialToSend flag
    bufferIndex[0] = 0;   // reset serialPacketLength counter
  }

  if(bufferToSend){
    RFduinoGZLL.sendToDevice(device, serialBuffer[bufferLevel], bufferIndex[bufferLevel]);
    bufferLevel++;
    if(bufferLevel > 4){
      bufferToSend = false; 
      bufferLevel = 0;
      for(int i=0; i<numBuffers; i++){
        bufferIndex[i] = 0;
      }
    }
  }
  
  if(len > 0){                // when we are recieving radio data
    if(!verifying){           // if we're not verifying, treat this like standard short message
      for (int i=0; i<len; i++){
        bleDataPacket[i] = data[i]; // load the data packet
      } 
      blePacketLength = len;  // take note of the packet length
      bleToSend = true;       // set flag to send radio data over serial
    }

    if(verifying){            // if we are verifying
      for(int i=0; i<len; i++) {
        bleDataPacket[blePacketLength] = data[i];   // add ble byte to buffer
        blePacketLength++; // keep count of the incoming bytes
      }
      if(blePacketLength == pageLength){
        bleToSend = true;
        verifying = false;
      }
    }
  }

}// end of onReceive



void testFirstSerialByte(char z){
      switch (z){
        case 'd':
          uploading = true; // 
          needPageLength = true;
          bufferLevel = 0;         // initialize data buffer
          for(int i=0; i<numBuffers; i++){
            bufferIndex[i] = 0;    // initialize all indexes to 0
          }
          break;
        case 't':
          verifying = true; // know that we expect 130 bytes before 0x10
          needPageLength = true;  // anticipate final page byte length 
          blePacketLength = 0;
          break;
        default:
          break;
      }    
}
    
    
    
   

