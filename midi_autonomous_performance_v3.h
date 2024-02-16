/* Filename: midi_autonomous_performance.h
 * Author: Liam Warner
 * Purpose: defines all functions and variables for live MIDI and autonomous performance
 *          for the orchestrion project in ECE44x Oregon State University
 */

#define BUZZ_PIN 5 //pin for testing with speaker
#define FAULT_PIN 9
#define CS_PIN0 10 //pins for Chip Select on TPICs
#define CS_PIN1 11
#define CS_PIN2 12
#define CS_PIN3 13
#define NOTE_ON 1
#define NOTE_OFF 0
#define SOLENOID_ON_TIME 100

#include "Prandom.h" //Prandom library by Rob Tillaart
#include <vector>
#include <cmath>
#include <SPI.h>

//1 MHz SPI clock, shifts in data MSB first, data mode is 0
//see https://en.wikipedia.org/wiki/Serial_Peripheral_Interface for more detail
SPISettings spi_settings = {100000, MSBFIRST, SPI_MODE0};

static Prandom R;

struct Note {
  int note_index;
  float duration; //rounded to nearest 0.5 (eight note)
  int velocity; //1, 2, 3
};

static int this_note_time = millis(); //DEBUG PURPOSES           

// Note index corresponds to available_notes array, 1 for available, 0 for off
static int note_is_off_arr[8] = {1, 1, 1, 1, 1, 1, 1, 1};

// Note timers keeps track of how long each note is on, turns off if over certain threshold
static int note_timers[8] = {0};

// Corresponding index updated with velocity level (1, 2, 3) if note is turned on
// Happens at the same time the note_is_off_arr array is updated
static int note_is_off_velocity[8] = {0}; 

// Available_notes defines integer associated available notes for song generation, and associated octave
// Integer conversion here: 60=C4, 61=C#4/Db4, 62=D, 63=D#/Eb, 64=E, 65=F, 66=F#/Gb, 67=G, 68=G#/Ab, 69=A, 70=A#/Bb, 71=B
const int available_notes[8] = {60, 62, 63, 67, 69, 72, 74, 75};

const float start_note_prob_array[8] = {0.35, 0.05, 0.05, 0.1, 0.05, 0.30, 0.05, 0.05};

const float next_note_prob_matrix_2[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05}};

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

/***************************
 * SPI FUNCTIONS START HERE
 **************************/

bool checkFault(){
  if(digitalRead(FAULT_PIN) == LOW){
    return 1;
  }else{
    return 0;
  }
}

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

byte get_other_bits(byte message, Note cur_note, bool first_note_in_pair){
  int cur_index = 0;
  if(first_note_in_pair == 1){ // if first note was already set 
  
    cur_index = cur_note.note_index + 1; //check if second note is currently active
    if(note_is_off_velocity[cur_index] == 1){
      message = message | 0b00010000;
    }else if (note_is_off_velocity[cur_index] == 2){
      message = message | 0b00101000;
    }else if (note_is_off_velocity[cur_index] == 3){
      message = message | 0b00111000;
    }

  }else{ // if second note was set already
  
    cur_index = cur_note.note_index - 1; //check if second note is currently active
    if(note_is_off_velocity[cur_index] == 1){
      message = message | 0b00000010;
    }else if (note_is_off_velocity[cur_index] == 2){
      message = message | 0b00000101;
    }else if (note_is_off_velocity[cur_index] == 3){
      message = message | 0b00000111;
    }
  }
  return message;
}


//function to get the SPI_message to send, determines which bits to set based on note
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


//NEEDS UPDATING WITH CURRENT VALUES!!!!!!
int get_solenoid_on_delay(Note cur_note){
  if(cur_note.velocity == 1){
    return 100; 
  }else if(cur_note.velocity == 2){
    return 100;
  }else{ //velocity is max
    return 100;
  }
}

int get_note_duration_delay(Note cur_note, int bpm){
  return (1000 / (4.0 * bpm / cur_note.duration / 60));
}

void send_SPI_message_on(Note cur_note){
  int cs_pin = get_cs_pin(cur_note.note_index);
  byte spi_off = 0b00000000;
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


void update_note_timers(int note_is_off_arr[], Note cur_note){
  for(int i=0; i<8; i++){
    // Only updates timers for notes that are off
    // Notes that are on retain timer value from when they were turned on
    if(note_is_off_arr[i]){ 
      note_timers[i] = millis();
      /*
      Serial.print("Note timer ");
      Serial.print(i);
      Serial.print(": ");
      Serial.println(note_timers[i]);
      */
    }else{
      Serial.print("Note is still on: ");
      Serial.println(i);
    }
  }
}

void check_note_timers(int note_is_off_arr[]){
  for(int i=0; i<8; i++){
    if(millis() - note_timers[i] >= SOLENOID_ON_TIME){ 
      Serial.print("Time exceeded solenoid on time: ");
      Serial.print(millis() - note_timers[i]);
      Serial.println(" ms.");

      Note cur_note = {i, 0, 3};
      Serial.println("Note is now off!");
      note_is_off_arr[i] = 1; //note is now off again
      note_is_off_velocity[i] = 0; //reset corresponding velocity to zero now that note is off
      send_SPI_message_off(cur_note); //now actually turn it off
    }
  }
}

/***************************
 * MIDI FUNCTIONS START HERE
 **************************/

const char* pitch_name(byte pitch) {
  static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  return names[pitch % 12];
}

int pitch_octave(byte pitch) {
  return (pitch / 12) - 1;
}

int velocity_level(byte velocity) {
  if (velocity <= 40)
    return 1;
  else if (40 < velocity && velocity <= 80)
    return 2;
  else if (80 < velocity)
    return 3;
  else
    return 2;
}
 
//checks if the note is a valid note, and if so returns the note index in available_notes
//otherwise returns -1
int is_valid_note(byte pitch){
  for(int i=0; i < sizeof(available_notes) / sizeof(available_notes[0]); i++){
    if(pitch == available_notes[i]){ //minus twelve for octave adjustment
      return i;
    }
  }
  return -1;
}

Note noteOn(byte channel, byte pitch, byte velocity) {
  Note cur_note;
  cur_note.velocity = velocity_level(velocity);
  cur_note.note_index = is_valid_note(pitch);

  if(cur_note.note_index >= 0){
    Serial.println("Note was turned on!");
    Serial.println("Time since last note on (ms): ");
    Serial.println(millis() - this_note_time);
    this_note_time = millis();
    note_is_off_arr[cur_note.note_index] = 0; //whatever note was played is now on
    note_is_off_velocity[cur_note.note_index] = cur_note.velocity; // record its velocity while it's on
    send_SPI_message_on(cur_note);
    if(checkFault()){
      Serial.println("FAULT!");
    }
  }

  return cur_note;
}
 
void noteOff(byte channel, byte pitch, byte velocity) {
  Note cur_note;
  cur_note.note_index = is_valid_note(pitch);
  note_is_off_arr[cur_note.note_index] = 1; //now the note is off again
  note_is_off_velocity[cur_note.note_index] = 0; //record no velocity now that it's off
  send_SPI_message_off(cur_note);
}

void controlChange(byte channel, byte control, byte value) {
  //do control change stuff in here

}

//read midi function, does one note at a time
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
 * AUTONOMOUS FUNCTIONS START HERE
 *********************************/

int getStartNoteIndex(){
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

int getNextNoteIndex(int currentRow, int energy_level) {
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


int check_note_leap(Note* song, int time_sig, int i, int j){

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

      /*
      Serial.print("Generated Note (index, duration, velocity): ");
      Serial.print(song[(i*time_sig*4)+j].note_index);
      Serial.print(", ");
      Serial.print(song[(i*time_sig*4)+j].duration);
      Serial.print(", ");
      Serial.println(song[(i*time_sig*4)+j].velocity);
      */

    }
  }

  return j;
}

Note* autonomous_seq_generation(Note* song, int energy_level, int song_length, int time_sig){
  int rand_note_index = 0;

  Serial.println(song_length / (time_sig*4));

  for (int i=0; i<(song_length / (time_sig*4)); i++){
    
    Serial.println("New Phrase");
    song[i*4*time_sig].note_index = getStartNoteIndex(); //input starting note
    song[i*4*time_sig].duration = 4;
    song[i*4*time_sig].velocity = round(R.uniform(0.5, 3.5));

    Serial.println(song[i*4*time_sig].note_index);

    for (int j=1; j<(4*time_sig); j++){
      rand_note_index = getNextNoteIndex(song[(i*time_sig*4)+j-1].note_index, energy_level);
      
      if(rand_note_index == -1){
        Serial.println("UNEXPECTED NOTE!"); //do nothing if this happens, or THROW ERROR
        break;
      }
      
      song[(i*time_sig*4)+j].note_index = rand_note_index;
      song[(i*time_sig*4)+j].duration = round(R.uniform(0.5, 2.5));
      song[(i*time_sig*4)+j].velocity = round(R.uniform(0.5, 3.5));
      
      /*
      Serial.print("Generated Note (index, duration, velocity): ");
      Serial.print(song[(i*time_sig*4)+j].note_index);
      Serial.print(", ");
      Serial.print(song[(i*time_sig*4)+j].duration);
      Serial.print(", ");
      Serial.println(song[(i*time_sig*4)+j].velocity);
      */
      
      j = check_note_leap(song, time_sig, i, j);

    };

    if(rand_note_index == -1)
      break;

  };
  return song;
}

void perform_song(Note* song, int song_length, int bpm){
  for (int i=0; i<song_length; i++){
    Serial.print("Note played (index, duration, velocity): ");
    Serial.print(song[i].note_index);
    Serial.print(", ");
    Serial.print(song[i].duration);
    Serial.print(", ");
    Serial.println(song[i].velocity);
    
    send_SPI_message_on(song[i]); //send SPI message
    Serial.println();

    tone(BUZZ_PIN, pitchFrequency[available_notes[song[i].note_index]]);
    
    delay(1000 / (4.0 * bpm / song[i].duration / 60));
    noTone(BUZZ_PIN);
  };
}
