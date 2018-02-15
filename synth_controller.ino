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
#define VOLTAGE_CONTROL_PIN A0
#define MIDI_BASE_NOTE 21 //lowest midi note
#define BASE_NOTE_FREQ 27.5
#define VOLTS_PER_SEMITONE 1.0 / 12.0
#define DAC_MULTIPLIER 0.97783686 // see spreadsheet
#define PITCH_BEND_FACTOR 1.0 / 32768.0; //adjust for desired pitch bend operation
#define ANALOG_CONTROL_SCALING 1.0 / 512.0 //adjust for desired amount of analog control
#define ANALOG_CONTROL // comment this out if analogue control is not required
//MIDI variables
int currentMidiNote; //the note currently being played
int keysPressedArray[128] = {0}; //to keep track of which keys are pressed
float midiControlVoltage; // represents midi note
float analogControlVoltage = 0; // analog control from ADC (optional)
float bendControlVoltage = 0; // represents midi pitch bend

void setTimer1(uint16_t val) {
  OCR1AH = val >> 8;
  OCR1AL = val;
}

void setup() {
  //MIDI stuff
  // Initiate MIDI communications, listen to all channels
  MIDI.begin(MIDI_CHANNEL_OMNI);      
  // Connect the HandleNoteOn function to the library, so it is called upon reception of a NoteOn.
  MIDI.setHandleNoteOn(handleNoteOn);  // Put only the name of the function
  // Do the same for NoteOffs
  MIDI.setHandleNoteOff(handleNoteOff);
  // and also pitchbend, we can do that now we calculate the value on the fly
  MIDI.setHandlePitchBend(handlePitchBend); 
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
  setNotePitch(60); //middle C for test
}

void loop() {
  MIDI.read();
  #ifdef ANALOG_CONTROL
  readAnalogControlVoltage();
  updateNotePitch();
  #endif
}

void readAnalogControlVoltage() {
  analogControlVoltage = (float) (analogRead(VOLTAGE_CONTROL_PIN) - 512) * ANALOG_CONTROL_SCALING;
}

void dacWrite(int value) {
  //write a 12 bit number to the MCP4921 DAC
  // take the SS pin low to select the chip:
  PORTB &= ~4; //faster than digitalWrite
  //send a value to the DAC
  SPI.transfer(0x30 | ((value >> 8) & 0x0F)); //bits 0..3 are bits 8..11 of 12 bit value, bits 4..7 are control data 
  SPI.transfer(value & 0xFF); //bits 0..7 of 12 bit value
  // take the SS pin high to de-select the chip:
  PORTB |= 4; //faster than digitalWrite 
}

void handleNoteOn(byte channel, byte pitch, byte velocity) { 
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

void handlePitchBend (byte channel, int bend) {
  // respond to MIDI pitch bend messages
  bendControlVoltage = (float)bend * PITCH_BEND_FACTOR;
  updateNotePitch();
}

void updateNotePitch() {
  // update note pitch, taking into account midi note, midi pitchbend, analogue control
  float controlVoltage = midiControlVoltage + bendControlVoltage + analogControlVoltage;
  float freqHz = BASE_NOTE_FREQ * pow(2.0, controlVoltage);  
  uint16_t timerSetting = round((1000000.0 / freqHz)-1.0);
  setTimer1(timerSetting);
  dacWrite(round(DAC_MULTIPLIER * freqHz));
}

void setNotePitch(int note) {
  //receive a midi note number and set both integrator drive and timer accordingly
  midiControlVoltage = ((note - MIDI_BASE_NOTE) * VOLTS_PER_SEMITONE);
  updateNotePitch();
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


