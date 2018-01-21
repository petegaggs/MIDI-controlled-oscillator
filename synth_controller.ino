/*
 * synth controller, by Peter Gaggs
 * 16 bit timer is used to generate a square wave at the required frequency
 * this is used to reset the integrator (ramp generator)
 * the drive for the integrator is provided by a 12bit SPI DAC
 * runs on arduino nano or similar 328p
 * note we use the timer directly, hence only use on 328p or 168p
 * needs arduino MIDI library
 */
#include <MIDI.h>
#include <midi_Defs.h>
#include <midi_Message.h>
#include <midi_Namespace.h>
#include <midi_Settings.h> 
MIDI_CREATE_DEFAULT_INSTANCE();
#include <avr/io.h> 
#include <SPI.h>
#define SLAVE_SELECT_PIN 10 //spi chip select
#define TIMER_PIN 9 //OC1A output
#define GATE_PIN 8 //gate control
#define MIDI_BASE_NOTE 21 //midi tables start at this note

//MIDI variables
int currentMidiNote; //the note currently being played
int keysPressedArray[128] = {0}; //to keep track of which keys are pressed

// tables
// OCR1A timer values, starting at midi note 21, ending at 108 See spreadsheet
uint16_t timerTable[] = {
  36363,34322,32395,30577,28861,27241,25712,24269,22907,21621,20407,19262,18181,17160,16197,15288,
  14430,13620,12855,12134,11453,10810,10203,9630,9090,8580,8098,7644,7214,6809,6427,6066,5726,
  5404,5101,4815,4544,4289,4049,3821,3607,3404,3213,3033,2862,2702,2550,2407,2272,2144,2024,1910,
  1803,1702,1606,1516,1431,1350,1275,1203,1135,1072,1011,955,901,850,803,757,715,675,637,601,567,
  535,505,477,450,425,401,378,357,337,318,300,283,267,252,238
};

// DAC value for integrator drive. For 12 bit DAC
uint16_t dacTable[] = {
  27,28,30,32,34,36,38,40,43,45,48,51,54,57,60,64,68,72,76,81,85,90,96,102,108,114,121,128,136,144,
  152,161,171,181,192,203,215,228,241,256,271,287,304,322,341,362,383,406,430,456,483,512,542,574,
  608,645,683,724,767,812,860,912,966,1023,1084,1149,1217,1289,1366,1447,1533,1624,1721,1823,1932,
  2047,2168,2297,2434,2579,2732,2894,3066,3249,3442,3647,3863,4093
};

void setTimer1(uint16_t val) {
  OCR1AH = val >> 8;
  OCR1AL = val;
}

void setup() {
  //MIDI stuff
  // Initiate MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);      
  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(HandleNoteOn);  // Put only the name of the function
  // Do the same for NoteOffs
  MIDI.setHandleNoteOff(handleNoteOff);
  //SPI stuff
  pinMode (SLAVE_SELECT_PIN, OUTPUT);
  digitalWrite(SLAVE_SELECT_PIN,HIGH);
  SPI.begin();
  pinMode(TIMER_PIN,OUTPUT);// OC1A output
  pinMode(GATE_PIN,OUTPUT);
  digitalWrite(GATE_PIN,LOW);
  // fast PWM mode 15, x8 prescaling
  TCCR1A = _BV(COM1A0) | _BV(WGM11) | _BV(WGM10);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS11);
  //setTimer1(0x8E0B); //lowest note 27.5Hz
  setTimer1(0xEE); //highest note 4186Hz
  //dacWrite(27); //lowest note
  //dacWrite(4095); //highest note
  setNotePitch(60); //middle C for test
}

void loop() {
  MIDI.read();
}

void dacWrite(int value) {
  //write a 12 bit number to the MCP4921 DAC
  // take the SS pin low to select the chip:
  digitalWrite(SLAVE_SELECT_PIN,LOW);
  //send a value to the DAC
  SPI.transfer(0x30 | ((value >> 8) & 0x0F)); //bits 0..3 are bits 8..11 of 12 bit value, bits 4..7 are control data 
  SPI.transfer(value & 0xFF); //bits 0..7 of 12 bit value
  // take the SS pin high to de-select the chip:
  digitalWrite(SLAVE_SELECT_PIN,HIGH); 
}

void HandleNoteOn(byte channel, byte pitch, byte velocity) { 
  // this function is called automatically when a note on message is received 
  keysPressedArray[pitch] = 1;
  synthNoteOn(pitch);
}

void handleNoteOff(byte channel, byte pitch, byte velocity)
{
  keysPressedArray[pitch] = 0; //update the array holding the keys pressed 
  if (pitch == currentMidiNote) {
    //only act if the note released is the one currently playing, otherwise ignore it
    int highestKeyPressed = findHighestKeyPressed(); //search the array to find the highest key pressed, will return -1 if no keys pressed
    if (highestKeyPressed != -1) { 
      //there is another key pressed somewhere, so the note off becomes a note on for the highest note pressed
      synthNoteOn(highestKeyPressed);
    }    
    else  {
      //there are no other keys pressed so proper note off
      synthNoteOff();
    }
  }  
}


void setNotePitch(int note) {
  //receive a midi note number and set both integrator drive and timer accordingly
  int tableVal = note - MIDI_BASE_NOTE;
  dacWrite(dacTable[tableVal]); // set the dac voltage
  setTimer1(timerTable[tableVal]); // set the timer
}

int findHighestKeyPressed(void) {
  //search the array to find the highest key pressed. Return -1 if no keys are pressed
  int highestKeyPressed = -1; 
  for (int count = 0; count < 127; count++) {
    //go through the array holding the keys pressed to find which is the highest (highest note has priority), and to find out if no keys are pressed
    if (keysPressedArray[count] == 1) {
      highestKeyPressed = count; //find the highest one
    }
  }
  return(highestKeyPressed);
}

void synthNoteOn(int note) {
  //starts playback of a note
  setNotePitch(note); //set the oscillator pitch
  //digitalWrite(GATE_PIN,LOW); //turn gate off momentarily to retrigger LFO
  //delayMicroseconds(GATE_RETRIGGER_DELAY_US); //should not do delays here really but get away with this which seems to be the minimum a montotron needs (may be different for other synths)
  digitalWrite(GATE_PIN,HIGH); //turn gate on
  currentMidiNote = note; //store the current note
}

void synthNoteOff(void) {
  digitalWrite(GATE_PIN,LOW); //turn gate off
}


