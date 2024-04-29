#include <SPI.h>
#include <MIDIUSB.h> //MIDIUSB library by Gary Grewal
#include "pitchToFrequency.h"
#include <stdlib.h>
#include <time.h>
#include "midi_autonomous_performance_v4.h"
#include <Wire.h>
#include <Adafruit_ADS7830.h>

#define AUTO_PIN 5 //pin used as switch for autonomous mode
#define SENSOR_PIN 6 //pin used as switch for sensor mode

void setup() {
  Wire.begin(); //I2C interface intialization
  Serial.begin(115200); //baud rate used by examples in USBMIDI library, matching here for safety
  SPI.begin(); //SPI interface intialization

  pinMode(AUTO_PIN, INPUT);
  pinMode(FAULT_PIN, INPUT_PULLUP); //is driven LOW when fault is detected
  pinMode(BUZZ_PIN, OUTPUT);       
  pinMode(CS_PIN0, OUTPUT);
  pinMode(CS_PIN1, OUTPUT);
  pinMode(CS_PIN2, OUTPUT);
  pinMode(CS_PIN3, OUTPUT);
  pinMode(6, OUTPUT); //output enable

  digitalWrite(6, HIGH);
  digitalWrite(CS_PIN0, HIGH);
  digitalWrite(CS_PIN1, HIGH);
  digitalWrite(CS_PIN2, HIGH);
  digitalWrite(CS_PIN3, HIGH);         

  //make sure ad7830 is set up
  if (!ad7830.begin()) {
    Serial.println("Failed to initialize ADS7830!");
    while (1);
  }                                                                                 
}

void loop() {
  
  //read midi input if any, note_index of returned note is -1 if no note received
  Note cur_note = read_midi();
  
  //FOR CHECKOFF: print stuff if note input is valid
  if(cur_note.note_index >= 0){
    //print note from read_midi()
    Serial.print("Note returned to MAIN (idx, dur, vel): ");
    Serial.print(cur_note.note_index);
    Serial.print(", ");
    Serial.print(cur_note.duration);
    Serial.print(", ");
    Serial.println(cur_note.velocity);

    
    //print inactive note array to be passed into update/check note timer fns
    Serial.print("Inactive note array: {");
    for(int i=0; i<8; i++){
      Serial.print(note_inactive_arr[i]);
      Serial.print(", ");
    }
    Serial.println("}");
    
  }

  //cur_note printed earlier
  update_note_timers(note_inactive_arr, cur_note);
  check_note_timers(note_inactive_arr, cur_note);
  
  while(digitalRead(AUTO_PIN) == LOW){ // low for autonomous mode
    int song_length = 48;
    int time_sig = 3;
    int energy_level = 2; 
    int bpm = 120;
    Note* song = new Note[song_length];

    Prandom R; //generate seed for random numbers
    song = autonomous_seq_generation(song, energy_level, song_length, time_sig, R, bpm); //song_length must be multiple of (time_sig*4)!

    for(int i = 0; i<song_length; i++){
      Serial.print("Generated Note (index, duration, velocity): ");
      Serial.print(song[i].note_index);
      Serial.print(", ");
      Serial.print(song[i].duration);
      Serial.print(", ");
      Serial.println(song[i].velocity);
    }
    
    delete[] song;
    delay(2000);
  }

  while(digitalRead(SENSOR_PIN == LOW)){
    

  }
}
