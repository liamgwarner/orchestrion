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
   
  //  //FOR CHECKOFF: print stuff if note input is valid
  //  if(cur_note.note_index >= 0){
  //    //print note from read_midi()
  //    Serial.print("Note returned to MAIN (idx, dur, vel): ");
  //    Serial.print(cur_note.note_index);
  //    Serial.print(", ");
  //    Serial.print(cur_note.duration);
  //    Serial.print(", ");
  //    Serial.println(cur_note.velocity);
  //
  //    
  //    //print inactive note array to be passed into update/check note timer fns
  //    Serial.print("Inactive note array: {");
  //    for(int i=0; i<8; i++){
  //      Serial.print(note_inactive_arr[i]);
  //      Serial.print(", ");
  //    }
  //    Serial.println("}");
  //    
  //  }
  
    //cur_note printed earlier
    update_note_timers(note_inactive_arr, cur_note);
    check_note_timers(note_inactive_arr, cur_note);

    //DO IF AUTONOMOUS MODE:
  }else if(digitalRead(AUTO_PIN) == LOW && !(fault_detected)){ // low for autonomous mode
    Prandom R;
    int bpm = 90;
    /*
    int song_length = 48;
    int time_sig = 3;
    int energy_level = 2; 
    int bpm = 120;
    Note* song = new Note[song_length];

     //generate seed for random numbers
    song = autonomous_seq_generation(song, energy_level, song_length, time_sig, R, bpm); //song_length must be multiple of (time_sig*4)!
    
    delete[] song;
    delay(2000);
    */

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

  //otherwise do nothing
}



//void loop() {
  //Prandom R;
  //play_licks(1, 4, 4, R, 90);
  //delay(1000);
  //read_sensor_vals();
  //Serial.print(myRTC.getHour(h12Flag, pmFlag));

  /*	
    // Start with the year
	Serial.print("2");
	if (century) {			// Won't need this for 89 years.
		Serial.print("1");
	} else {
		Serial.print("0");
	}
  
	Serial.print(myRTC.getYear(), DEC);
	Serial.print(' ');
	
	// then the month
	Serial.print(myRTC.getMonth(century), DEC);
	Serial.print(" ");
  
	// then the date
	Serial.print(myRTC.getDate(), DEC);
	Serial.print(" ");
  
	// and the day of the week
	Serial.print(myRTC.getDoW(), DEC);
	Serial.print(" ");
  
  Serial.print(myRTC.getHour(h12Flag, pmFlag), DEC);
	Serial.print(" ");
	Serial.print(myRTC.getMinute(), DEC);
	Serial.print(" ");
	Serial.print(myRTC.getSecond(), DEC);
  delay(1000);
  */
//}

