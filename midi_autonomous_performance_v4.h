/* Filename: midi_autonomous_performance.h
 * Author: Liam Warner
 * Purpose: defines all functions and variables for live MIDI and autonomous performance
 *          for the orchestrion project in ECE44x Oregon State University
 */

#define BUZZ_PIN 18 //pin for testing with speaker
#define FAULT_PIN 9
#define CS_PIN0 10 //pins for Chip Select on TPICs
#define CS_PIN1 11
#define CS_PIN2 12
#define CS_PIN3 13
#define NOTE_ON 1
#define NOTE_OFF 0
#define SOLENOID_ON_TIME 60

#define AUTO_PIN 15 //pin used as switch for autonomous mode
#define SENSOR_PIN 14 //pin used as switch for sensor mode aka A0
#define OUTPUT_EN 6

#include "Prandom.h" //Prandom library by Rob Tillaart
#include <iostream>
#include <vector>
#include <cmath>
#include <SPI.h>
#include <Adafruit_ADS7830.h>
#include <DS3231.h>
#include <algorithm>


//1 MHz SPI clock, shifts in data MSB first, data mode is 0
//see https://en.wikipedia.org/wiki/Serial_Peripheral_Interface for more detail
SPISettings spi_settings = {100000, MSBFIRST, SPI_MODE0};

Adafruit_ADS7830 ad7830;

DS3231 myRTC;
bool century = false;
bool h12Flag;
bool pmFlag;
byte alarmDay, alarmHour, alarmMinute, alarmSecond, alarmBits;
bool alarmDy, alarmH12Flag, alarmPmFlag;

static Prandom R;

struct Note {
  int note_index;
  float duration; //rounded to nearest 0.5 (eighth note)
  int velocity; //1, 2, 3
  int probability; //0 to 100, IRRELEVANT except for licks
};

static int this_note_time = millis(); //DEBUG PURPOSES           
static int past_time = millis();

const int lick_mode_sensor_threshold = 80;
static int lick_mode_inactivity_timer = millis();
static bool tried_to_grab_attention = 0;

// Note index corresponds to available_notes array, 1 for available/inactive, 0 for unavailable/active
static int note_inactive_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};

// Note timers keeps track of how long each note is on, turns off if over certain threshold
static int note_timers[8] = {0};

static int sensor_note_timers[8] = {0};

static int sensor_note_wait_timers[8] = {10000};

// Corresponding index updated with velocity level (1, 2, 3) if note is turned on
// Happens at the same time the note_inactive_arr array is updated
static int active_note_vel_arr[8] = {0}; 

// Available_notes defines integer associated available notes for song generation, and associated octave
// Integer conversion here: 60=C4, 61=C#4/Db4, 62=D, 63=D#/Eb, 64=E, 65=F, 66=F#/Gb, 67=G, 68=G#/Ab, 69=A, 70=A#/Bb, 71=B
//const int available_notes[8] = {60, 62, 63, 67, 69, 72, 74, 75};
const int available_notes[8] = {60, 69, 67, 75, 63, 74, 62, 72};

// Probability array for determining the starting note in a phrase
const float start_note_prob_array[8] = {0.35, 0.05, 0.05, 0.1, 0.05, 0.30, 0.05, 0.05};

// Probability matrices for determining the next note in a phrase
// Each row is a "state" (current note is something, ex: row 0 for cur_note = C4)
// Each column in a given row is the probability that the next note is that note
// These notes respectively correspond to the notes in the available_notes array
/*const float next_note_prob_matrix_2[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05}};
*/

//fixed to reflect note offsets shown in available notes array lines 50-51
const float next_note_prob_matrix_2[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 }};


/*
std::vector<std::vector<float>> next_note_prob_matrix_2 = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                                           {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                                           {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                                           {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05},
                                                           {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                                           {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                                           {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                                           {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 }};
*/

const float next_note_prob_matrix_1[8][8] = {{0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00}};

const float next_note_prob_matrix_3[8][8] = {{0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20}};

static int next_note_selection_array[8] = {0, 0, 0, 0, 0, 0, 0, 0};

//stores sensor values for associated notes in avaiable_notes array
//also associated with ADC channels 0-7 respectively
static uint8_t sensor_values[8] = {0};

// past sensor values are updated every 100ms to get rate of change estimate for sensor values
// which is effectively how fast someone is moving their hand
static uint8_t past_sensor_values[8] = {0};

static int sensor_rate_of_change[8] = {0}; // see


/***********************************************************
 * Function: Example Function Header
 * Description:
 * 
 ***********************************************************/

/***************************
 * SPI FUNCTIONS START HERE
 **************************/

/******************************************
 * Function: checkFault()
 * Description: This function checks if the
 * fault pin is low, returns true if so.
 *****************************************/
bool checkFault(){
  if(digitalRead(FAULT_PIN) == LOW){
    return 1;
  }else{
    return 0;
  }
}

/***********************************************************
 * Function: get_cs_pin()
 * Description: Returns the CS_PIN constant
 * given the note_index.
 ***********************************************************/
int get_cs_pin(int note_index){
  if(note_index <= 1){
    return CS_PIN0;
  }else if(note_index <= 3){
    return CS_PIN1;
  }else if(note_index <= 5){
    return CS_PIN2;
  }else if(note_index <= 7){
    return CS_PIN3;
  }else{ //just in case
    return CS_PIN0;
  }
}

/***********************************************************
 * Function: byte get_other_bits(byte, Note, bool)
 * Description: Returns an 8-bit SPI message with the other
 * bits of the other note in its TPIC pairing (ex: C4 paired with
 * D4) if the other note is active, meaning its velocity is
 * set in the active_note_vel_arr array.
 ***********************************************************/
byte get_other_bits(byte message, Note cur_note, bool first_note_in_pair){
  int cur_index = 0;
  if(first_note_in_pair == 1){ // if first note was already set 
  
    cur_index = cur_note.note_index + 1; //check if second note is currently active
    if(active_note_vel_arr[cur_index] == 1){
      message = message | 0b00010000;
    }else if (active_note_vel_arr[cur_index] == 2){
      message = message | 0b00101000;
    }else if (active_note_vel_arr[cur_index] == 3){
      message = message | 0b00111000;
    }

  }else{ // if second note was set already
  
    cur_index = cur_note.note_index - 1; //check if second note is currently active
    if(active_note_vel_arr[cur_index] == 1){
      message = message | 0b00000010;
    }else if (active_note_vel_arr[cur_index] == 2){
      message = message | 0b00000101;
    }else if (active_note_vel_arr[cur_index] == 3){
      message = message | 0b00000111;
    }
  }
  return message;
}


/***********************************************************
 * Function: get_SPI_message(Note cur_note)
 * Description: Returns an SPI message of a given note,
 * determined by its velocity and note_index. Uses the
 * get_other_bits_function to account for other active notes.
 ***********************************************************/
byte get_SPI_message(Note cur_note){
  byte message = 0b00000000; //initialize message to all zeros
  if(cur_note.note_index % 2 == 0){ //if first of pair of notes (ex: C4 in C4/D4 pair)
    if(cur_note.velocity == 1){ //sets LSBs to send message
      message = 0b00000010;
    }else if(cur_note.velocity == 2){
      message = 0b00000101;
    }else if(cur_note.velocity == 3){
      message = 0b00000111;
    }

    message = get_other_bits(message, cur_note, 1);

  }else{
    if(cur_note.velocity == 1){ //if second of pair of notes (ex: D4 in C4/D4 pair)
      message = 0b00010000; //sets MSBs to send message
    }else if(cur_note.velocity == 2){
      message = 0b00101000;
    }else if(cur_note.velocity == 3){
      message = 0b00111000;
    }
    
    message = get_other_bits(message, cur_note, 0);
  
  }
  return message;
}


//LEGACY, MAY GET RID OF THIS
int get_solenoid_on_delay(int velocity){
  if(velocity == 1){
    return (SOLENOID_ON_TIME + 30); //modify if 1-solenoid needs more on time
  }else if(velocity == 2){
    return SOLENOID_ON_TIME;
  }else{ //velocity is max
    return SOLENOID_ON_TIME;
  }
}

/***********************************************************
 * Function: int get_note_duration_delay(Note cur_note, int bpm)
 * Description: Returns a note duration delay based on the
 * bpm and cur_note's duration.
 ***********************************************************/
int get_note_duration_delay(Note cur_note, int bpm){
  return (1000 / (4.0 * bpm / cur_note.duration / 60));
}

/***********************************************************
 * Function: void send_SPI_message_on(Note cur_note)
 * Description: This sends an SPI message for the cur_note
 * using the appropriate CS_PIN and SPI pins. Uses the
 * get_cs_pin and get_SPI_message functions.
 ***********************************************************/
void send_SPI_message_on(Note cur_note){
  int cs_pin = get_cs_pin(cur_note.note_index);
  byte spi_received = 0;
  byte spi_message = get_SPI_message(cur_note);

  SPI.beginTransaction(spi_settings);
  digitalWrite(cs_pin, LOW);
  
  spi_received = SPI.transfer(spi_message);
  
  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();
  
  Serial.print("SPI ON Message: ");
  Serial.println(spi_message, BIN);
  Serial.print("Chip Select: ");
  Serial.println(cs_pin);

}

/***********************************************************
 * Function: void send_SPI_message_off(Note cur_note)
 * Description: This turns a particular note OFF, depending
 * on its note_index. Accounts for other active notes with
 * with the get_other_bits function (doesn't turn them off
 * prematurely).
 ***********************************************************/
void send_SPI_message_off(Note cur_note){
  //turn solenoid(s) off regardless
  int cs_pin = get_cs_pin(cur_note.note_index);
  byte message = 0b00000000;
  byte spi_received = 0;

  if(cur_note.note_index % 2 == 0){
    message = get_other_bits(message, cur_note, 1);
  }else{
    message = get_other_bits(message, cur_note, 0);
  }

  SPI.beginTransaction(spi_settings);
  digitalWrite(cs_pin, LOW);
  
  spi_received = SPI.transfer(message);

  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();

  Serial.print("SPI OFF Message: ");
  Serial.println(message, BIN);
  Serial.print("Chip Select: ");
  Serial.println(cs_pin);
}

/***********************************************************
 * Function: update_note_timers(int note_inactive_arr[], Note cur_note)
 * Description: Updates the note_timers array for notes that
 * are "off" (not currently being actuated).
 ***********************************************************/
void update_note_timers(int note_inactive_arr[], Note cur_note){
  for(int i=0; i<8; i++){
    // Only updates timers for notes that are off
    // Notes that are on retain timer value from when they were turned on
    if(note_inactive_arr[i]){ 
      note_timers[i] = millis();
      /*
      Serial.print("Note timer ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(note_timers[i]);
      */
    }else{
      //FOR DEBUG!
      //Serial.print("Note is still on: ");
      //Serial.println(i);
      
    }
  }
}



/***********************************************************
 * Function: void check_note_timers(int note_inactive_arr[])
 * Description: Checks the note timers array for any notes
 * that have been on longer than SOLENOID_ON_TIME. If so,
 * it turns that note off and updates the appropriate arrays.
 ***********************************************************/
void check_note_timers(int note_inactive_arr[], Note cur_note){
  for(int i=0; i<8; i++){
    if(millis() - note_timers[i] >= get_solenoid_on_delay(active_note_vel_arr[i])){ 
      Serial.print("Time exceeded solenoid on time: ");
      Serial.print(millis() - note_timers[i]);
      Serial.println(" ms.");

      Note cur_note = {i, 0, 3};
      Serial.println("Note is now off!");
      note_inactive_arr[i] = 1; //note is now off again
      active_note_vel_arr[i] = 0; //reset corresponding velocity to zero now that note is off
      send_SPI_message_off(cur_note); //now actually turn it off
    }
  }
}



/***************************
 * MIDI FUNCTIONS START HERE
 **************************/

/***********************************************************
 * Function: const char* pitch_name(byte pitch)
 * Description: Gets the name of a pitch depending on its
 * MIDI pitch value (0-127).
 ***********************************************************/
const char* pitch_name(byte pitch) {
  static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return names[pitch % 12];
}

/***********************************************************
 * Function: const char* pitch_name(byte pitch)
 * Description: Returns the octave of a MIDI pitch.
 ***********************************************************/
int pitch_octave(byte pitch) {
  return (pitch / 12) - 1; //may need to adjust +/-1 for different hardware
}

/***********************************************************
 * Function: int velocity_level(byte velocity)
 * Description: Returns the velocity level 1, 2, 3 depending
 * on MIDI velocity (0-127)
 ***********************************************************/
int velocity_level(byte velocity) {
  if (1 <= velocity && velocity <= 40) 
    return 1;
  else if (40 < velocity && velocity <= 80)
    return 2;
  else if (80 < velocity)
    return 3;
  else //velocity is zero or invalid
    return 0; //JUST in case
}

/***********************************************************
 * Function: int is_valid_note(byte pitch)
 * Description: checks if note is valid (in available_notes),
 * returns note_index if so, if not valid returns -1 which is
 * an indicator to NOT send an SPI message.
 ***********************************************************/
int is_valid_note(byte pitch){
  for(int i=0; i < sizeof(available_notes) / sizeof(available_notes[0]); i++){
    if(pitch == available_notes[i]){ //sometimes needs plus/minus twelve for octave adjustment
      return i;
    }
  }
  return -1;
}

/***********************************************************
 * Function: Note noteOn(byte channel, byte pitch, byte velocity)
 * Description: A general function to turn "on" a note from
 * incoming MIDI data parameters (channel, pitch, velocity).
 * First, it gets the velocity and note_index of the note to
 * turn on (duration doesn't matter for live MIDI input).
 * Then, it checks if the note is valid (note_index>=0) AND
 * if the note is off (not being actuated currently), only then
 * sends the SPI message and updates the appropriate arrays.
 ***********************************************************/
Note noteOn(byte channel, byte pitch, byte velocity){
  Note cur_note;
  cur_note.velocity = velocity_level(velocity);
  cur_note.note_index = is_valid_note(pitch);

  if(cur_note.note_index >= 0 && note_inactive_arr[cur_note.note_index] && cur_note.velocity != 0){
    Serial.println("Note was turned on!");
    Serial.println("Time since last note on (ms): ");
    Serial.println(millis() - this_note_time);
    this_note_time = millis();
    note_inactive_arr[cur_note.note_index] = 0; //whatever note was played is now on
    active_note_vel_arr[cur_note.note_index] = cur_note.velocity; // record its velocity while it's on
    send_SPI_message_on(cur_note);
    if(checkFault()){
      Serial.println("FAULT!");
    }
  }

  return cur_note;
}

//LEGACY, may get rid of this. check_note_timers handles turning notes off
void noteOff(byte channel, byte pitch, byte velocity) {
  Note cur_note;
  cur_note.note_index = is_valid_note(pitch);
  note_inactive_arr[cur_note.note_index] = 1; //now the note is off again
  active_note_vel_arr[cur_note.note_index] = 0; //record no velocity now that it's off
  send_SPI_message_off(cur_note);
}

/***********************************************************
 * Function: Note read_midi()
 * Description: Called from main. Reads any incoming MIDI
 * message and if it is a noteOn event (case 0x9), then
 * calls noteOn function. Intializes cur_note with note_index
 * of -1 to ensure that any valid note indices are only from
 * actual notes.
 ***********************************************************/
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
  }
  return cur_note;
}

/*********************************
 * SENSOR FUNCTIONS START HERE
 *********************************/
void read_sensor_vals(){
  for(int i=0; i<8; i++){
     sensor_values[i] = ad7830.readADCsingle(i); //reading channel i
     
     Serial.print("Sensor value CH");
     Serial.print(i);
     Serial.print(": ");
     Serial.println(sensor_values[i]);
    
  }

  uint8_t sampling_period = 100; // how long between sensor value samples (ms)
  
  //static uint8_t sensor_rate_of_change[8] = {0}; // updates every 


  //get past sensor values for rate of change estimate
  if(past_time < millis() - sampling_period){
    
    past_time = millis();
    Serial.println();
    Serial.print("Change in sensor vals: ");
  
    for(int i=0; i<8; i++){
      sensor_rate_of_change[i] = sensor_values[i] - past_sensor_values[i]; //update rate of change
      past_sensor_values[i] = sensor_values[i]; // now update past with current
      
      Serial.print(sensor_rate_of_change[i]);
      Serial.print(" ");
    }
    Serial.println();
    
    // after rate of change is updated, now update the past sensor values
    //std::copy(sensor_values, sensor_values+8, past_sensor_values); //copies past sensor values to 
  }
}

void check_sensors(){
  int threshold = 40;
  Note cur_note = {-1, 0, 1};
  for(int i=0; i<8; i++){
    if(sensor_values[i] >= threshold){
      Note cur_note = {i, 0, 2};
      if((millis() - sensor_note_timers[i] >= sensor_note_wait_timers[i]) && note_inactive_arr[i]){
        note_inactive_arr[cur_note.note_index] = 0;
        active_note_vel_arr[cur_note.note_index] = cur_note.velocity;
        send_SPI_message_on(cur_note);
        
        Serial.print("SENSOR: note on at: ");
        Serial.print(millis());
        Serial.println(" ms.");
      }
      
      Serial.print("Sensor note timer ");
      Serial.print(i);
      Serial.print(" :");
      Serial.print(sensor_note_timers[i]);
      Serial.println(" ms.");
    }else if(sensor_values[i] < threshold){

    }
  }

}

void update_sensor_note_timers(int note_inactive_arr[]){
  for(int i=0; i<8; i++){
    // Only updates timers for notes that are off
    // Notes that are on retain timer value from when they were turned on
    if(note_inactive_arr[i]){ 
      note_timers[i] = millis();

      /*
      Serial.print("Sensor note wait timer ");
      Serial.print(i);
      Serial.print(" :");
      Serial.println(sensor_note_wait_timers[i]);
      */
    }else{
      //FOR DEBUG!
      //Serial.print("Note is still on: ");
      //Serial.println(i);
      sensor_note_timers[i] = millis();

       /*
      Serial.print("Sensor note wait timer ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(sensor_note_wait_timers[i]);
      */
    }

    //sensor_note_wait_timers[i] = abs((int)(4*(250 - sensor_values[i])));
    sensor_note_wait_timers[i] = (int)(34081.0 / sensor_values[i]) - 90;
  }
}


void check_sensor_note_timers(int note_inactive_arr[]){
  for(int i=0; i<8; i++){
    if(millis() - note_timers[i] >= get_solenoid_on_delay(active_note_vel_arr[i])){ 
      Serial.print("Time exceeded solenoid on time: ");
      Serial.print(millis() - note_timers[i]);
      Serial.println(" ms.");

      Note cur_note = {i, 0, 3};
      Serial.println("Note is now off!");
      note_inactive_arr[i] = 1; //note is now off again
      active_note_vel_arr[i] = 0; //reset corresponding velocity to zero now that note is off
      send_SPI_message_off(cur_note); //now actually turn it off
      sensor_note_timers[i] = millis(); //reset the sensor note timer
    }
  }
}



void modify_prob_matrix(int energy_level){
  if(energy_level <= 1){
    
  }else if(energy_level == 2){

  }else if(energy_level >= 3){

  }
}


/*********************************
 * AUTONOMOUS FUNCTIONS START HERE
 *********************************/

int getStartNoteIndex(Prandom R){
  float randomProb = (R.uniform(0.0, 100.0) / 100.0);
  float cumulativeProb = 0.0;
    for (int i = 0; i < 8; i++) {
        cumulativeProb += start_note_prob_array[i];
        if (randomProb <= cumulativeProb + 1e-5) {
            return i;
        }
    }
    return -1;
}

int getNextNoteIndex(int currentRow, int energy_level, Prandom R) {
    // Ensure randomization based on current time

    float randomProb = (R.uniform(0.0, 100.0) / 100.0);
    //Serial.println(randomProb);

    // Accumulate probabilities and find the index
    float cumulativeProb = 0.0;
    for (int i = 0; i < 8; i++) {
        
        if(energy_level <=1){
          cumulativeProb += next_note_prob_matrix_2[currentRow][i];
        }else if(energy_level == 2){
          cumulativeProb += next_note_prob_matrix_2[currentRow][i];
        }else if(energy_level >= 3){
          cumulativeProb += next_note_prob_matrix_2[currentRow][i];
        }
        
        if (randomProb <= cumulativeProb + 1e-5) {
          return i;
        }
    }
    // This should not happen, but return -1 if it does
    return -1;
}


int check_note_leap(Note* song, int time_sig, int i, int j, Prandom R){

  if (j < 1 || j >= (4 * time_sig)) {
      return j; // Check for boundary condition to prevent out-of-bounds access
  }

  if(abs(song[(i*time_sig*4)+j].note_index - song[(i*time_sig*4)+j - 1].note_index) >= 4){
    //do cool resolution
    while(song[(i * time_sig * 4) + j].note_index != 0 && j<(4*time_sig)){
      j++;
      song[(i*time_sig*4)+j].duration = round(R.uniform(0.5, 2.5));
      song[(i*time_sig*4)+j].note_index = song[(i*time_sig*4)+j-1].note_index - 1; 
      song[(i*time_sig*4)+j].velocity = round(R.uniform(0.5, 3.5));
    }
  }

  return j;
}

Note* autonomous_seq_generation(Note* song, int energy_level, int song_length, int time_sig, Prandom R, int bpm){
  int rand_note_index = 0;
  int cur_note_on_time = millis();
  int cur_note_off_time = 999999999;
  int note_still_on = 0;
  bool repeat_note = 0;
  int rep_note_on_time = 999999999;
  int rep_note_off_time = 999999999;
  int sensor_delay = 0;

  Serial.println(song_length / (time_sig*4));

  for (int i=0; i<(song_length / (time_sig*4)); i++){
    
    Serial.println("New Phrase");
    song[i*4*time_sig].note_index = getStartNoteIndex(R); //input starting note
    song[i*4*time_sig].duration = 4;
    //song[i*4*time_sig].velocity = round(R.uniform(0.5, 3.5));
    song[i*4*time_sig].velocity = 2;

    Serial.println(song[i*4*time_sig].note_index);

    for (int j=1; j<(4*time_sig); j++){
      
      //read sensor data here if sensor mode on
      read_sensor_vals();
      //modify prob matrices with value
      //modify_prob_matrix(energy_level);


      rand_note_index = getNextNoteIndex(song[(i*time_sig*4)+j-1].note_index, energy_level, R);
      
      if(rand_note_index == -1){
        Serial.println("UNEXPECTED NOTE!"); //do nothing if this happens, or THROW ERROR
        break;
      }
      
      song[(i*time_sig*4)+j].note_index = rand_note_index;
      song[(i*time_sig*4)+j].duration = round(R.uniform(0.5, 2.5));
      //song[(i*time_sig*4)+j].velocity = round(R.uniform(0.5, 3.5));
      song[(i*time_sig*4)+j].velocity = 2;
  
      j = check_note_leap(song, time_sig, i, j, R);

      //NOW PLAY NOTE that was just generated!

      //FOR TESTING WITH SOLENOIDS (COMMENT THE OTHER OUT)
      send_SPI_message_on(song[(i*time_sig*4)+j]); //send SPI message for note on
      cur_note_on_time = millis(); //time (ms) when the note was turned on
      note_still_on = 1;

      Serial.print("Auto note on at (ms): ");
      Serial.println(cur_note_on_time);

      //tone(BUZZ_PIN, pitchFrequency[available_notes[song[i].note_index]]); //for speaker testing
      //while loop continues to update note timer until FULL note duration is complete
      while(millis() - cur_note_on_time < 1000 / (4.0 * bpm / song[(i*time_sig*4)+j].duration / 60)){

        //read_sensor_vals();
        /*
        if(sensor_values[song[(i*time_sig*4)+j].note_index] > 254 && (millis() - cur_note_off_time) > sensor_delay){
          repeat_note = 1;
          sensor_delay = 400 - sensor_values[song[(i*time_sig*4)+j].note_index];
        }else{
          repeat_note = 0;
          sensor_delay = 0;
        }
        */
        
        //for initial note
        if(millis() - cur_note_on_time >= get_solenoid_on_delay(song[(i*time_sig*4)+j].velocity) && note_still_on && !repeat_note){
          send_SPI_message_off(song[(i*time_sig*4)+j]);
          cur_note_off_time = millis();
          Serial.print("Auto note off at (ms): ");
          Serial.println(cur_note_off_time);
          Serial.println();
          note_still_on = 0;
        }
        //for repeated notes
        /*
        }else if(millis() - rep_note_on_time >= get_solenoid_on_delay(song[(i*time_sig*4)+j].velocity) && note_still_on && repeat_note){
          send_SPI_message_off(song[(i*time_sig*4)+j]);
          Serial.print("Repeated note off at (ms): ");
          Serial.println(millis());
          Serial.println();
          note_still_on = 0;
          rep_note_off_time = millis();
        }
        */

        //turn on repeated note
        /*
        if(note_still_on == 0 && repeat_note){ //actuate note a second time???
          delay(sensor_delay);
          send_SPI_message_on(song[(i*time_sig*4)+j]); //send SPI message for note on
          cur_note_on_time = millis(); //time (ms) when the note was turned on
          note_still_on = 1; //turn note_still_on back on
          Serial.print("Repeated note on at (ms): ");
          Serial.println(cur_note_on_time);
        }
        */

      }

    };

    if(rand_note_index == -1)
      break;

  };

  //play ending note

  if(song[song_length-1].note_index != 0){
    Note final_note = {0, 4, 2};
    send_SPI_message_on(final_note); //send SPI message
    delay(SOLENOID_ON_TIME); //wait
    send_SPI_message_off(final_note); //turn solenoids off
    Serial.println();

    //tone(BUZZ_PIN, pitchFrequency[available_notes[final_note.note_index]]);
    
    delay(1000 / (4.0 * bpm / final_note.duration / 60));
    //noTone(BUZZ_PIN);
  }

  return song;
}

//struct Note {
//  int note_index;
// float duration; 
//  int velocity; //1, 2, 3
//};

/* BANK OF LICKS PRELIMINARY CODE
 * NEEDS TESTING
*/

#define MAX_NOTES 16

// Define the Lick struct with a statically sized array of Notes
/*
struct Lick {
    int time_sig_num;     // time signature numerator
    int time_sig_denom;   // time signature denominator
    int length;           // length of lick in measures
    int energy_level;
    int num_notes;        // Number of Notes in this lick
    struct Note data[MAX_NOTES];  // Predefined array of notes (up to MAX_NOTES)
};
*/

struct Lick {
    int time_sig_num;     // time signature numerator
    int time_sig_denom;   // time signature denominator
    int length;           // length of lick in measures
    int energy_level;
    int num_notes;        // Number of Notes in this lick
    //struct Note data[MAX_NOTES];  // Predefined array of notes (up to MAX_NOTES)
    std::vector<Note> data;
};

const int BoL_len = 19;

// Integer conversion here: 60=C4, 61=C#4/Db4, 62=D, 63=D#/Eb, 64=E, 65=F, 66=F#/Gb, 67=G, 68=G#/Ab, 69=A, 70=A#/Bb, 71=B
//const int available_notes[8] = {60, 69, 67, 75, 63, 74, 62, 72};
// C4, A4, G4, Eb5, Eb4, D5, 

// Below is a descrambler for the note indicies
// Input: index in order of pitch 0 is lowest note on drum, 7 is highest
// Output: index that correctly maps to hardware and SPI functions
int get_unscrambled_idx(int idx){
  std::vector<int> mapping = {0, 6, 4, 2, 1, 7, 5, 3};
  return mapping[idx];
}

// Static bank of licks with predefined notes and variable note count
// NOTE: bank of licks is programmed with unscrambled indicies using
struct Lick Bank_of_licks[BoL_len] = {
    // 4/4 
    {4, 4, 1, 1, 5, {{0, 4, 2, 100}, {1, 2, 2, 100}, {2, 2, 2, 100}, {3, 4, 2, 100}, {4, 4, 2, 100}}},
    {4, 4, 1, 1, 3, {{0, 2, 3, 100}, {1, 2, 3, 100}, {2, 12, 2, 100}}},
    {4, 4, 1, 1, 4, {{5, 4, 1, 100}, {2, 4, 1, 100}, {5, 2, 1, 100}, {3, 6, 1, 100}}},    
    {4, 4, 1, 1, 4, {{6, 4, 2, 100}, {1, 4, 2, 100}, {2, 4, 2, 100}, {3, 4, 2, 100}}},
    {4, 4, 1, 1, 4, {{5, 4, 2, 100}, {2, 4, 2, 100}, {2, 4, 2, 100}, {3, 4, 2, 100}}},
    {4, 4, 2, 1, 5, {{3, 4, 2, 100}, {1, 4, 2, 100}, {2, 4, 2, 100}, {3, 4, 2, 100}, {4, 4, 2, 100}}},
  
    {4, 4, 2, 2, 12, {{5, 4, 2, 100}, {3, 2, 2, 100}, {5, 2, 2, 100}, {4, 4, 2, 100}, {3, 2, 2, 100}, {2, 2, 2, 100}, {1, 2, 2, 100}, {2, 2, 2, 100}, {0, 4, 2, 100}, {1, 2, 2, 100}, {6, 2, 2, 100}, {5, 4, 2, 100}}},
    {4, 4, 1, 2, 6, {{2, 4, 2, 100}, {3, 3, 2, 100}, {5, 3, 2, 100}, {4, 1, 2, 100}, {1, 1, 2, 100}, {0, 4, 2, 100}}}, 
    {4, 4, 1, 2, 8, {{5, 2, 2, 100}, {4, 2, 2, 100}, {3, 2, 2, 100}, {2, 1, 2, 100}, {1, 1, 2, 100}, {0, 2, 2, 100}, {5, 2, 2, 100}, {3, 4, 2, 100}}},

    {4, 4, 1, 3, 13, {{0, 1, 2, 100}, {1, 1, 2, 100}, {2, 2, 2, 100}, {3, 1, 2, 100}, {4, 1, 2, 100}, {5, 1, 2, 100}, {6, 1, 2, 100}, {7, 2, 2, 100}, {2, 1, 2, 100}, {0, 1, 2, 100}, {1, 2, 2, 100}, {5, 1, 2, 100}, {7, 1, 2, 100}}},
    {4, 4, 2, 3, 12, {{0, 2, 0, 100}, {0, 2, 2, 100}, {2, 2, 2, 100}, {3, 2, 2, 100}, {4, 2, 2, 100}, {3, 2, 2, 100}, {4, 2, 2, 100}, {2, 2, 2, 100}, {3, 2, 2, 100}, {2, 2, 2, 100}, {0, 2, 2, 100}, {0, 2, 10, 100}}},
    {4, 4, 1, 3, 10, {{0, 4, 0, 100}, {0, 1.333, 2, 100}, {3, 1.333, 2, 100}, {5, 1.333, 2, 100}, {0, 1.333, 2, 100}, {3, 1.333, 2, 100}, {5, 1.333, 2, 100}, {0, 1.333, 2, 100}, {3, 1.333, 2, 100}, {5, 1.333, 2, 100}, {0, 1.333, 2, 100}, {3, 1.333, 2,}, {5, 1.333, 2, 100}}},

    // 6/8
    {6, 8, 1, 1, 6, {{0, 6, 2, 100}, {1, 6, 2, 100}, {2, 6, 2, 100}, {3, 6, 2, 100}, {4, 6, 2, 100}, {5, 6, 2, 100}}},
    {6, 8, 2, 1, 8, {{0, 6, 2, 100}, {1, 6, 2, 100}, {2, 6, 2, 100}, {3, 6, 2, 100}, {4, 6, 2, 100}, {5, 6, 2, 100}, {6, 6, 2, 100}, {7, 6, 2, 100}}},

    // 5/4
    {5, 4, 1, 2, 5, {{0, 5, 2, 100}, {1, 5, 2, 100}, {2, 5, 2, 100}, {3, 5, 2, 100}, {4, 5, 2, 100}}},
    {5, 4, 3, 2, 6, {{0, 4, 2, 100}, {1, 4, 2, 100}, {2, 4, 2, 100}, {3, 4, 2, 100}, {4, 4, 2, 100}, {5, 4, 2, 100}}},

    // 3/4
    {3, 4, 1, 3, 6, {{0, 3, 2, 100}, {1, 3, 2, 100}, {2, 3, 2, 100}, {3, 3, 2, 100}, {4, 3, 2, 100}, {5, 3, 2, 100}}},
    {7, 4, 2, 3, 8, {{0, 7, 2, 100}, {1, 7, 2, 100}, {2, 7, 2, 100}, {3, 7, 2, 100}, {4, 7, 2, 100}, {5, 7, 2, 100}}},

    // Special energy level 4 lick for inactivity:
    {4, 4, 1, 4, 8, {{0, 0.25, 2, 100}, {1, 0.25, 2, 100}, {2, 0.25, 2, 100}, {3, 0.25, 2, 100}, {4, 0.25, 2, 100}, {5, 0.25, 2, 100}, {6, 0.25, 2, 100}, {7, 0.25, 2, 100}}}
};

// TO DO: Incorporate logic for each lick to add/subtract additional notes or put it back to its default state
// Function to add a note to the lick, determining parameters
void add_note_to_lick(Lick& lick, int position) {

  if (position >= 0 && position <= lick.data.size()) {
    // Determine new note based on current note
    Note new_note;
    
    // gets half the duration 
    new_note.duration = lick.data[position].duration / 2;

    // same velocity
    new_note.velocity = lick.data[position].velocity;

    // gets probability of 100 FOR NOW
    new_note.probability = 100;
    
    // now determine its note value
    int cur_index = lick.data[position].note_index;
    int max_interval = 1; // set max distance between grace notes

    if(cur_index == 0){
      new_note.note_index = static_cast<int>round(R.uniform(cur_index - 0.5, cur_index + max_interval + 0.499));
    }else if (cur_index == 7){
      new_note.note_index = static_cast<int>round(R.uniform(cur_index - max_interval - 0.5, cur_index + 0.499));
    }else{
      new_note.note_index = static_cast<int>round(R.uniform(cur_index - max_interval - 0.5, cur_index + max_interval + 0.499));
    }

    bool after;
    after = static_cast<int>round(R.uniform(0, 1));
    
    // want to insert after if position is zero and NOT when its the last note, randomly chosen otherwise
    if((after || position == 0) && position != lick.data.size() - 1){
      lick.data[position].duration = lick.data[position].duration / 2;
      lick.data.insert(lick.data.begin() + position + 1, new_note);
    // otherwise before is fine
    }else{ 
      lick.data[position - 1].duration = lick.data[position - 1].duration / 2; // always want to adjust the preceding note's duration
      lick.data.insert(lick.data.begin() + position, new_note);
    }
    
    lick.num_notes = lick.data.size();
  }
}
    // Update num_notes to reflect the new count





// Function to print the details of each lick
void print_lick(struct Lick* lick) {
    printf("Time Signature: %d/%d\n", lick->time_sig_num, lick->time_sig_denom);
    printf("Lick Length: %d measure(s)\n", lick->length);
    printf("Energy Level: %d\n", lick->energy_level);
    printf("Number of Notes: %d\n", lick->num_notes);
    printf("Notes:\n");

    for (int i = 0; i < lick->num_notes; i++) {
        printf("  Note %d: Index = %d, Duration = %.1f, Velocity = %d\n",
               i + 1, lick->data[i].note_index,
               lick->data[i].duration,
               lick->data[i].velocity);
    }
}

// Function to filter and return all licks with energy_level == 1
void filter_licks_by_energy_level(struct Lick* bank, int size, int target_energy_level) {
    printf("\nLicks with energy level %d:\n", target_energy_level);
    
    for (int i = 0; i < size; i++) {
        if (bank[i].energy_level == target_energy_level) {
            print_lick(&bank[i]);  // Print the matching lick
        }
    }
}


struct Lick** pick_licks_by_criteria(struct Lick* bank, int size, int target_energy_level, int target_time_sig_num, int target_time_sig_denom, int* result_count) {
    // Dynamically allocate memory for an array of Lick pointers (a list)
    struct Lick** result = (struct Lick**)malloc(size * sizeof(struct Lick*)); // Max possible matches
    *result_count = 0;  // Initialize result count

    // Iterate over the bank of licks to find matching licks
    for (int i = 0; i < size; i++) {
        if (bank[i].energy_level == target_energy_level &&
            bank[i].time_sig_num == target_time_sig_num &&
            bank[i].time_sig_denom == target_time_sig_denom) {
            
            // Store the pointer to the matching lick in the result array
            result[*result_count] = &bank[i];
            (*result_count)++;  // Increment the result count
        }
    }

    // If no matches, return NULL
    if (*result_count == 0) {
        free(result);  // Free allocated memory if no matches
        return NULL;
    }

    return result;  // Return the list of matching licks
}


int update_bpm(int bpm){
  //get hour from RTC and convert to int
  int hour = static_cast<int>(myRTC.getHour(h12Flag, pmFlag)); //0, 0 for 24 hour mode
  //Serial.print(hour);

  if(hour < 10){
    bpm = static_cast<int>(0.8*bpm);
  }else if(hour < 14){
    bpm = static_cast<int>(1.1*bpm); //change nothing
  }else if(hour < 22){
    bpm = static_cast<int>(1.2*bpm);
  }
  //Serial.println(bpm);
  return bpm;
}

int get_next_note_idx_from_sensors(){
  int num_selected_notes = 0;

  for(int i=0; i<8; i++){
    if(sensor_values[i] > lick_mode_sensor_threshold){
      next_note_selection_array[i] = 1; //now this note is available to be played;
      num_selected_notes++;
    }else{
      next_note_selection_array[i] = 0; //need to set back to zero if sensor value is below threshold
    }
    Serial.println(next_note_selection_array[i]);
  }

  int temp = static_cast<int>round(R.uniform(0.5, num_selected_notes+0.499));
  int count = 0;
  Serial.print("temp value for sensor note selection: ");
  Serial.println(temp);
  for(int i=0; i<8; i++){
    if(next_note_selection_array[i]){ // this shouldn't be met if no notes are selected
      count++; // increment count if note is selected 
      if(temp == count){
        return i;
      }
    }
  }

  return -1; // return -1 if no note is seleted
}

int get_velocity_from_sensors(int note_idx){
  int val = sensor_values[note_idx];
  if(val > lick_mode_sensor_threshold){
    if(val < 140){
      return 1;
    }else if(val < 200){
      return 2;
    }else if(val <= 255){
      return 3;   
    }
  }
  return 2;
}

// Time-based responses for drum
int get_lick_wait_period(int bpm, int time_sig_num, int time_sig_denom){
  int hour = static_cast<int>(myRTC.getHour(h12Flag, pmFlag));
  int num_measures = 0;
  Serial.print("Hour: ");
  Serial.println(hour);
  if(hour < 10){
    num_measures = 16;
  }else if(hour < 14){
    num_measures = 8; //change nothing
  }else if(hour < 18){
    num_measures = 4;
  }else{
    num_measures = 16;
  }

  int max_sensor_val = 0;
  for(int i=0; i<8; i++){
    // want to read max sensor_val and have it be 
    if(sensor_values[i] > max_sensor_val && sensor_values[i] > lick_mode_sensor_threshold){
      max_sensor_val = sensor_values[i];
    }
  }
  float sensor_mult = (255 - max_sensor_val) / 255.0; // want this to reduce 
  num_measures = static_cast<int>(sensor_mult * num_measures);

  int num_sixteenths = num_measures * (time_sig_num*(4.0/time_sig_denom)*4);
  if(num_sixteenths == 0){
    return 0;
  }

  //final 4 is for converting time sig into sixteenth note units
  Serial.print("Number of measures to wait: ");
  Serial.println(num_measures);
  double denom = (4.0 * bpm / num_sixteenths / 60);
  if(denom == 0){
    return 0;
  }
  Serial.print("Denominator (DEBUG): ");
  Serial.println(denom);
  int wait_period = static_cast<int>(1000 / denom);
  return wait_period;
}

int update_energy_level() {
  int hour = static_cast<int>(myRTC.getHour(h12Flag, pmFlag));
  Serial.println(hour);
  if(hour < 10){
    return 1;
  }else if(hour < 14){
    return 2;
  }else if(hour < 18){
    return 3;
  }else{
    return 1;
  }
}

int check_sensor_inactivity(int energy_level) {
  
  for(int i=0; i<8; i++){
    if(sensor_values[i] > lick_mode_sensor_threshold){
      lick_mode_inactivity_timer = millis();
      tried_to_grab_attention = 0;
      return update_energy_level();
    }
  }
  
  int inactivity_wait_time = 5000;

  if(tried_to_grab_attention){
    return update_energy_level();
  //tried_to_grab_attention
  }else if(lick_mode_inactivity_timer < millis() - inactivity_wait_time && !tried_to_grab_attention){
    tried_to_grab_attention = 1;
    return 4; // return special energy level
  }else{
    return update_energy_level();
  }

}



/***********************************************************
 * Function: Note* play_licks()
 * Description: Called from main. Needs external inputs 
 ***********************************************************/
void play_licks(int energy_level, int time_sig_num, int time_sig_denom, Prandom R, int bpm){
  int rand_note_index = 0;
  int cur_note_on_time = millis();
  int cur_note_off_time = 2147483647; //max value on int
  bool next_note_ready = 1;
  struct Lick cur_lick;

  int orig_bpm = bpm;
  bool play_another_lick = 1;
  int result_count = 0;
  int lick_wait_period = 0;
  int previous_millis = 0;
  
    
  // play until some condition is met, TBD?
  while (play_another_lick){
    if(digitalRead(AUTO_PIN) != LOW){
      break;
    }
    
    read_sensor_vals(); 

    bpm = update_bpm(orig_bpm);
    Serial.print("BPM: ");
    Serial.println(bpm);

    // these use clock's hour value to update their values accordingly
    lick_wait_period = get_lick_wait_period(bpm, time_sig_num, time_sig_denom);
    energy_level = update_energy_level();
    //energy_level = check_sensor_inactivity(energy_level);

    Serial.print("Lick wait period: ");
    Serial.println(lick_wait_period);
    Serial.print("Energy level: ");
    Serial.println(energy_level);

    Serial.println("New Lick");
    result_count = 0;

    // Pick all licks with passed energy level
    struct Lick** matching_licks = pick_licks_by_criteria(Bank_of_licks, BoL_len, energy_level, time_sig_num, time_sig_denom, &result_count);

    if (matching_licks != NULL) {
        printf("Found %d matching licks:\n", result_count);
        // Print the matching licks
        for (int i = 0; i < result_count; i++) {
            print_lick(matching_licks[i]);
            // WANT TO CHOOSE LICK SOMEHOW
            int rnd_lick_idx = static_cast<int>(round(R.uniform(-0.5, result_count-0.5)));
            cur_lick = *matching_licks[rnd_lick_idx];
        }
        free(matching_licks);  // Free memory after use
    } else {
        printf("No matching licks found with the given criteria.\n");
        printf("Please add more licks to the bank of licks.\n");
    }

    if(static_cast<int>(round(R.uniform(0, 1)))){
      // randomly select 
      add_note_to_lick(cur_lick, static_cast<int>(round(R.uniform(-0.5, cur_lick.num_notes + 0.499))));
    }

    int j = 0;
    Note cur_note;
    
    if(millis() - previous_millis >= lick_wait_period){
      
      // PLAYING LICK

      //play the lick, iterating through the notes
      while(j < cur_lick.num_notes){

        read_sensor_vals(); 

        //only get note when ready, prevents overriding index with other values
        if(next_note_ready){

          cur_note = cur_lick.data[j];
          cur_note.note_index = get_unscrambled_idx(cur_note.note_index); //update with unscrambled value

          // some chance to use markov matrices to determine the note based on previous (increase variety)
          if(j > 0 && static_cast<int>round(R.uniform(0, 0.7))){
            cur_note.note_index = getNextNoteIndex(cur_lick.data[j-1].note_index, 2, R);
          }

          // SENSORS ACTIVE
          // only update if someone is next to the drum and is close enough
          // fn returns -1 if no notes are selected
          int selected_note_idx = get_next_note_idx_from_sensors();
          if(selected_note_idx >= 0){
            cur_note.note_index = selected_note_idx;
          }

          cur_note.velocity = get_velocity_from_sensors(cur_note.note_index);
        
        }

        update_note_timers(note_inactive_arr, cur_note);

        if(note_inactive_arr[cur_note.note_index] && next_note_ready){
          send_SPI_message_on(cur_note); //send SPI message for note on
          note_inactive_arr[cur_note.note_index] = 0;
          cur_note_on_time = millis();
          next_note_ready = 0;

          Serial.print("Lick note on at (ms): ");
          Serial.println(cur_note_on_time);
        }

        if ((!note_inactive_arr[cur_note.note_index]) && (millis() - cur_note_on_time >= get_solenoid_on_delay(cur_note.velocity)) && !next_note_ready) {
          send_SPI_message_off(cur_note);  // Send SPI message to turn off the note
          note_inactive_arr[cur_note.note_index] = 1;                 // Mark the note as no longer active
          Serial.print("Auto note off at (ms): ");
          Serial.println(millis());
          Serial.println();
        }

        if(millis() - cur_note_on_time >= 1000 / (4.0 * bpm / cur_note.duration / 60)){
          next_note_ready = 1;
          j++;
        }
          
      };

      Serial.print("Lick Finished!!\n");
      previous_millis = millis(); //update previous_millis now that lick is finished
    }

    if(rand_note_index == -1)
      break;

  };
}
