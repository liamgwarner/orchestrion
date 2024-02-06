/*
 * MIDI Control Code for Orchestrion Project
 * Author(s): Liam Warner
 * Last Modified: 01/05/2024
 * 
 * Adapted from MIDIUSB_buzzer.ino by Paulo Costa
 */ 

#include <SPI.h>
#include <MIDIUSB.h>
#include "pitchToFrequency.h"
#include <stdlib.h>
#include <time.h>
#include "midi_autonomous_performance.h"

#define BUZZ_PIN 5 //pin for testing with speaker

// SETUP AND MAIN LOOP

Note read_midi(){
  midiEventPacket_t rx = MidiUSB.read();
  
  Note cur_note = {-1, 0, 0};

  switch (rx.header) {
    case 0:
      break; //No pending events
    case 0x9:
      cur_note = noteOn(
        rx.byte1 & 0xF,  //channel
        rx.byte2,        //pitch
        rx.byte3         //velocity
      );
      break;  
    case 0x8:
      noteOff(
        rx.byte1 & 0xF,  //channel
        rx.byte2,        //pitch
        rx.byte3         //velocity
      );
      break;
    case 0xB:
      controlChange(
        rx.byte1 & 0xF,  //channel
        rx.byte2,        //control
        rx.byte3         //value
      );
      break;
    default:
      Serial.print("Unhandled MIDI message: ");
      Serial.print(rx.header, HEX);
      Serial.print("-");
      Serial.print(rx.byte1, HEX);
      Serial.print("-");
      Serial.print(rx.byte2, HEX);
      Serial.print("-");
      Serial.println(rx.byte3, HEX);
  }
  return cur_note;
}

void setup() {
  Serial.begin(115200);
  pinMode(12, INPUT);
  pinMode(5, OUTPUT);
}

void loop() {
  
  //get midi notes 1 and/or 2 
  Note note_1 = read_midi();
  Note note_2 = read_midi();

  //if 2 notes read at the same time:
  if(note_1.note_index >= 0 && note_2.note_index >= 0){
    if(note_1.velocity - note_2.velocity >=0){
      delay(get_solenoid_on_delay(note_2));
      send_SPI_message_off(note_2);
      
      delay(get_solenoid_on_delay(note_1) - get_solenoid_on_delay(note_2)); //delay for remainder of time
      send_SPI_message_off(note_1); 
    }else{
      delay(get_solenoid_on_delay(note_1));
      send_SPI_message_off(note_1); 
      
      delay(get_solenoid_on_delay(note_2) - get_solenoid_on_delay(note_1)); //delay for remainder of time
      send_SPI_message_off(note_2); 
  
  //else if 1 note read:
  }else if(note_1.note_index >= 0){
    delay(get_solenoid_on_delay(note_1));
    send_SPI_message_off(note_1);
  }

  //Serial.print("Note 1 index: ");
  //Serial.println(note_1.note_index);

  //Serial.print("Note 2 index: ");
  //Serial.println(note_2.note_index); 
  
  while(digitalRead(12) == HIGH){
    Note* song = autonomous_seq_generation(0, 32, 4);
    perform_song(song, 32, 90);
  }

}