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

#include "Prandom.h" //Prandom library by Rob Tillaart
#include <iostream>
#include <vector>
#include <cmath>
#include <SPI.h>
#include <Adafruit_ADS7830.h>

//1 MHz SPI clock, shifts in data MSB first, data mode is 0
//see https://en.wikipedia.org/wiki/Serial_Peripheral_Interface for more detail
SPISettings spi_settings = {100000, MSBFIRST, SPI_MODE0};

Adafruit_ADS7830 ad7830;

static Prandom R;

struct Note {
  int note_index;
  float duration; //rounded to nearest 0.5 (eight note)
  int velocity; //1, 2, 3
};

static int this_note_time = millis(); //DEBUG PURPOSES           

// Note index corresponds to available_notes array, 1 for available/inactive, 0 for unavailable/active
static int note_inactive_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};

// Note timers keeps track of how long each note is on, turns off if over certain threshold
static int note_timers[8] = {0};

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
/*const float next_note_prob_matrix_2[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 }};
*/

std::vector<std::vector<float>> next_note_prob_matrix_2 = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                                           {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                                           {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                                           {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05},
                                                           {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                                           {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                                           {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                                           {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 }};

std::vector<std::vector<float>> next_note_prob_matrix_1 = {{0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00},
                                                           {0.25, 0.25, 0.25, 0.25, 0.00, 0.00, 0.00, 0.00}};

std::vector<std::vector<float>> next_note_prob_matrix_3 = {{0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20},
                                                           {0.05, 0.05, 0.05, 0.05, 0.20, 0.20, 0.20, 0.20}};                                

//stores sensor values for associated notes in avaiable_notes array
//also associated with ADC channels 0-7 respectively
uint8_t sensor_values[8] = {0};


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
 * bits of the other note in its TPIC pairing (C4 paired with
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
    return (SOLENOID_ON_TIME + 5); //modify if 1-solenoid needs more on time
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
}

void modify_prob_matrices(int energy_level){
  
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
        
        if(energy_level <= 1){
          cumulativeProb += next_note_prob_matrix_1[currentRow][i];
        }else if(energy_level == 2){
          cumulativeProb += next_note_prob_matrix_2[currentRow][i];
        }else if(energy_level >= 3){
          cumulativeProb += next_note_prob_matrix_3[currentRow][i];
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
  int cur_note_timer = millis();
  int cur_note_on_time = millis();
  int note_still_on = 0;

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
      modify_prob_matrices(energy_level);
      
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

        //check for 
        if(millis() - cur_note_on_time >= get_solenoid_on_delay(song[(i*time_sig*4)+j].velocity) && note_still_on){
          send_SPI_message_off(song[(i*time_sig*4)+j]);
          Serial.print("Auto note off at (ms): ");
          Serial.println(millis());
          Serial.println();
          note_still_on = 0;
          
        }
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

      /*
      if(note_still_on == 0 && repeat_note){ //actuate note a second time???
        send_SPI_message_on(song[i]); //send SPI message for note on
        cur_note_on_time = millis(); //time (ms) when the note was turned on
        note_still_on = 1; //turn note_still_on back on
        delay(127-value); //delay by 127-value -> if closer, repeat rapidly
      }
      */


/*
void perform_song(Note* song, int song_length, int bpm){
  
  int cur_note_timer = millis();
  int cur_note_on_time = millis();
  int note_still_on = 0;

  for (int i=0; i<song_length; i++){
    Serial.print("Note played (index, duration, velocity): ");
    Serial.print(song[i].note_index);
    Serial.print(", ");
    Serial.print(song[i].duration);
    Serial.print(", ");
    Serial.println(song[i].velocity);

    //read sensor data here
    read_sensor_vals();
    if(i != 0){
      get_next_note(song, i);
    }

    //do something with value

    //FOR TESTING WITH SOLENOIDS (COMMENT THE OTHER OUT)
    send_SPI_message_on(song[i]); //send SPI message for note on
    cur_note_on_time = millis(); //time (ms) when the note was turned on
    note_still_on = 1;

    Serial.print("Auto note on at (ms): ");
    Serial.println(cur_note_on_time);

    //tone(BUZZ_PIN, pitchFrequency[available_notes[song[i].note_index]]); //for speaker testing
    //while loop continues to update note timer until FULL note duration is complete
    while(millis() - cur_note_on_time < 1000 / (4.0 * bpm / song[i].duration / 60)){

      //check for 
      if(millis() - cur_note_on_time >= get_solenoid_on_delay(song[i].velocity) && note_still_on){
        send_SPI_message_off(song[i]);
        Serial.print("Auto note off at (ms): ");
        Serial.println(millis());
        Serial.println();
        note_still_on = 0;
        
      }
    }

    //noTone(BUZZ_PIN);
    
    //delay(1000 / (4.0 * bpm / song[i].duration / 60)); //time in ms for note duration

  };

  //cute final note "resolution" (just plays c4)
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

}
*/
