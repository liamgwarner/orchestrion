#include <SPI.h>
#include <MIDIUSB.h> //MIDIUSB library by Gary Grewal
#include "pitchToFrequency.h"
#include <stdlib.h>
#include <time.h>
#include "midi_autonomous_performance_v4.h"
#include <Wire.h>
#include <Adafruit_ADS7830.h>
#include <DS3231.h>

// Pin # Definitions are in header file

bool fault_detected;

void fault_interrupt(){
 //turn off all notes
    for(int i=0; i<8; i++){
      Note temp_note = {i, 0, 3};
      send_SPI_message_off(temp_note);
    }
    digitalWrite(OUTPUT_EN, LOW); //turn off output enable pin
    fault_detected = 1; //fault has been detected, don't do midi/auto/sensor/this (stop everything)
    Serial.println("ERROR!!! FAULT DETECTED!!! ERROR!!!"); //print message to terminal
    while(1); // STOP EVERYTHING
}

void setup() {
  Wire.begin(); //I2C interface intialization
  Serial.begin(115200); //baud rate used by examples in USBMIDI library, matching here for safety
  SPI.begin(); //SPI interface intialization

  pinMode(AUTO_PIN, INPUT_PULLUP);
  pinMode(SENSOR_PIN, INPUT_PULLUP);
  pinMode(FAULT_PIN, INPUT_PULLUP); //is driven LOW when fault is detected
  pinMode(BUZZ_PIN, OUTPUT);       
  pinMode(CS_PIN0, OUTPUT);
  pinMode(CS_PIN1, OUTPUT);
  pinMode(CS_PIN2, OUTPUT);
  pinMode(CS_PIN3, OUTPUT);
  pinMode(OUTPUT_EN, OUTPUT); //output enable

  digitalWrite(OUTPUT_EN, HIGH); //enabled at setup
  digitalWrite(CS_PIN0, HIGH);
  digitalWrite(CS_PIN1, HIGH);
  digitalWrite(CS_PIN2, HIGH);
  digitalWrite(CS_PIN3, HIGH);         

  //attachInterrupt(digitalPinToInterrupt(FAULT_PIN), fault_interrupt, FALLING);

  //do adc setup
  ad7830.begin();         

  fault_detected = 0;                                                                     
}


void loop() {
  
  //DO IF MIDI MODE:
  if(digitalRead(SENSOR_PIN) == HIGH && digitalRead(AUTO_PIN) == HIGH && !(fault_detected)){
    
    //read midi input if any, note_index of returned note is -1 if no note received
    Note cur_note = read_midi();
  
    //cur_note printed earlier
    update_note_timers(note_inactive_arr, cur_note);
    check_note_timers(note_inactive_arr, cur_note);

  //DO IF AUTONOMOUS MODE:
  }else if(digitalRead(AUTO_PIN) == LOW && !(fault_detected)){ // low for autonomous mode
    Prandom R;
    int bpm = 90;

    play_licks(2, 4, 4, R, bpm);
  
  //DO IF SENSOR MODE
  }else if(digitalRead(SENSOR_PIN) == LOW && !(fault_detected)){
      read_sensor_vals();
      check_sensors(); 
      update_sensor_note_timers(note_inactive_arr);
      check_sensor_note_timers(note_inactive_arr);
  }

//if fault pin is driven low disable TPIC output
//  if(digitalRead(FAULT_PIN) == LOW && !(fault_detected)){
//    //turn off all notes
//    for(int i=0; i<8; i++){
//      Note temp_note = {i, 0, 3};
//      send_SPI_message_off(temp_note);
//    }
//    digitalWrite(OUTPUT_EN, LOW); //turn off output enable pin
//    fault_detected = 1; //fault has been detected, don't do midi/auto/sensor/this (stop everything)
//    Serial.println("ERROR!!! FAULT DETECTED!!! ERROR!!!"); //print message to terminal
//  }

}

