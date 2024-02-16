#include <SPI.h>
#include <MIDIUSB.h> //MIDIUSB library by Gary Grewal
#include "pitchToFrequency.h"
#include <stdlib.h>
#include <time.h>
#include "midi_autonomous_performance_v3.h"

#define AUTO_PIN 19

//Definitions for note off/on management:



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200); //baud rate used by examples in USBMIDI library
  SPI.begin();
  pinMode(12, INPUT_PULLUP);
  pinMode(FAULT_PIN, INPUT_PULLUP);
  pinMode(BUZZ_PIN, OUTPUT);       
                                                                                                              
}

void loop() {
  // put your main code here, to run repeatedly:
  //midiEventPacket_t rx = MidiUSB.read();
  
  //Note cur_note = {-1, 0, 0};
  
  Note cur_note = read_midi();
  
  update_note_timers(note_is_off_arr, cur_note);
  check_note_timers(note_is_off_arr);

  while(digitalRead(12) == LOW){
    int song_length = 32;
    int time_sig = 4;
    int energy_level = 3;
    Note* song = new Note[song_length];

    song = autonomous_seq_generation(song, energy_level, song_length, time_sig); //song_length must be multiple of time_sig*4!

    for(int i = 0; i<song_length; i++){
      Serial.print("Generated Note (index, duration, velocity): ");
      Serial.print(song[i].note_index);
      Serial.print(", ");
      Serial.print(song[i].duration);
      Serial.print(", ");
      Serial.println(song[i].velocity);

      //Serial.println(i);
    }

    Serial.println();
    Serial.println("Song is now performed:");

    perform_song(song, song_length, 60);
    
    delete[] song;
    delay(2000);
  }

  //delay(1000);    
}
