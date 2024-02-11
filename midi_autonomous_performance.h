/* Filename: midi_autonomous_performance.h
 * Author: Liam Warner
 * Purpose: defines all functions and variables for live MIDI and autonomous performance
 *          for the orchestrion project in ECE44x Oregon State University
 */

#define BUZZ_PIN 5 //pin for testing with speaker
#define CS_PIN0 10 //pins for Chip Select on TPICs
#define CS_PIN1 11
#define CS_PIN2 12
#define CS_PIN3 13
#define NOTE_ON 1
#define NOTE_OFF 0

#include "Prandom.h"
#include <vector>
#include <cmath>
#include <SPI.h>

//1 MHz SPI clock, shifts in data MSB first, data mode is 0
//see https://en.wikipedia.org/wiki/Serial_Peripheral_Interface for more detail
SPISettings spi_settings = {1000000, MSBFIRST, SPI_MODE0};

static Prandom R;

struct Note {
  int note_index;
  float duration; //rounded to nearest 0.5 (eight note)
  int velocity; //1, 2, 3
};

// available_notes defines integer associated available notes for song generation, and associated octave
// integer conversion here: 60=C4, 61=C#4/Db4, 62=D, 63=D#/Eb, 64=E, 65=F, 66=F#/Gb, 67=G, 68=G#/Ab, 69=A, 70=A#/Bb, 71=B
const int available_notes[8] = {60, 62, 63, 67, 69, 72, 74, 75};

const float start_note_prob_array[8] = {0.35, 0.05, 0.05, 0.1, 0.05, 0.30, 0.05, 0.05};

const float next_note_prob_matrix_1[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05}};

const float next_note_prob_matrix_2[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05}};

const float next_note_prob_matrix_3[8][8] = {{0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.2,  0.05, 0.05, 0.05, 0.05, 0.05},
                                             {0.2,  0.2,  0.2,  0.15, 0.05, 0.1,  0.05, 0.05},
                                             {0.35, 0.2,  0.05, 0.05, 0.05, 0.2,  0.05, 0.05},
                                             {0.05, 0.2,  0.05, 0.35, 0.05, 0.05, 0.2,  0.2 },
                                             {0.1,  0.1,  0.1,  0.2,  0.05, 0.15, 0.1,  0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.05, 0.2 },
                                             {0.05, 0.05, 0.05, 0.2,  0.05, 0.35, 0.2,  0.05}};

/***************************
 * SPI FUNCTIONS START HERE
 **************************/

//determines cs pin based on which note is being played
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
  }else{
    if(cur_note.velocity == 1){ //if second of pair of notes (ex: D4 in C4/D4 pair)
      message = 0b00010000; //sets MSBs to send message
    }else if(cur_note.velocity == 2){
      message = 0b00101000;
    }else if(cur_note.velocity == 3){
      message = 0b00111000;
    }
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
  
  Serial.print("SPI Message: ");
  Serial.println(spi_message, BIN);
  Serial.print("Chip Select: ");
  Serial.println(cs_pin);

}

void send_SPI_message_off(Note cur_note){
  //turn solenoid(s) off regardless
  int cs_pin = get_cs_pin(cur_note.note_index);
  byte spi_off = 0b00000000;
  byte spi_received = 0;

  SPI.beginTransaction(spi_settings);
  digitalWrite(cs_pin, LOW);
  
  spi_received = SPI.transfer(spi_off);

  digitalWrite(cs_pin, HIGH);
  SPI.endTransaction();
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
int is_valid_note(byte pitch){
  for(int i=0; i < sizeof(available_notes) / sizeof(available_notes[0]); i++){
    if(pitch - 12 == available_notes[i]){ //minus twelve for octave adjustment
      return i;
    }
  }
  return 0;
}

Note noteOn(byte channel, byte pitch, byte velocity) {
  Note cur_note;
  cur_note.velocity = velocity_level(velocity);
  cur_note.note_index = is_valid_note(pitch);
  send_SPI_message_on(cur_note);
  /* SERIAL PRINTS FOR DEBUG
  Serial.print("Note On: ");
  Serial.print(pitch);
  Serial.print(pitch_name(pitch));
  Serial.print(pitch_octave(pitch));
  Serial.print(", channel=");
  Serial.print(channel);
  Serial.print(", num_solenoids=");
  Serial.println(num_solenoids);
  */
  return cur_note;
}
 
void noteOff(byte channel, byte pitch, byte velocity) {
  Note cur_note;
  cur_note.note_index = is_valid_note(pitch);
  send_SPI_message_off(cur_note);
  
  /* SERIAL PRINTS FOR DEBUG
  Serial.print("Note Off: ");
  Serial.print(pitch_name(pitch));
  Serial.print(pitch_octave(pitch));
  Serial.print(", channel=");
  Serial.print(channel);
  Serial.print(", velocity=");
  Serial.println(velocity);
  */
}

void controlChange(byte channel, byte control, byte value) {
  //do control change stuff in here
  
  /* SERIAL PRINTS FOR DEBUG
  Serial.print("Control change: control=");
  Serial.print(control);
  Serial.print(", value=");
  Serial.print(value);
  Serial.print(", channel=");
  Serial.println(channel);
  */
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

int getNextNoteIndex(int currentRow) {
    // Ensure randomization based on current time

    float randomProb = (R.uniform(0.0, 100.0) / 100.0);
    Serial.println(randomProb);

    // Accumulate probabilities and find the index
    float cumulativeProb = 0.0;
    for (int i = 0; i < 8; i++) {
        cumulativeProb += next_note_prob_matrix_1[currentRow][i];
        if (randomProb <= cumulativeProb + 1e-5) {
            return i;
        }
    }
    // This should not happen, but return -1 if it does
    return -1;
}

int check_note_leap(Note* song, int cur_index){
  int j = cur_index;

  if(abs(song[j].note_index - song[j - 1].note_index) >= 4){
    //do cool resolution
    while(!(song[j].note_index == 0)){
      j++;
      song[j].duration = round(R.uniform(1.5, 4.5));
      song[j].note_index = song[j-1].note_index - 1; 
    }
  }

  return j;
}

Note* autonomous_seq_generation(int energy_level, int song_length, int time_sig){
  static Note* song = new Note[song_length];
  int rand_note_index = 0;
  song[0].note_index = 0; //input starting note
  song[0].duration = round(R.uniform(1.5, 4.5));
  Serial.println(song[0].duration);

  for (int i=0; i<song_length; i++){

    for (int j=1; j<(4*time_sig); j++){
      rand_note_index = getNextNoteIndex(song[(i*time_sig*4)+j-1].note_index);
      if(rand_note_index == -1){
        while(1); //do nothing if this happens, or THROW ERROR
      }
      song[i+j].note_index = rand_note_index;
      song[i+j].duration = round(R.uniform(1.5, 4.5));
      Serial.println(song[i+j].note_index);
      j = check_note_leap(song, j);
      //Serial.print(song[i]);
    }
    Serial.println("New Phrase");
    song[i*4*time_sig].note_index = getStartNoteIndex();

  };
  return song;
}

void perform_song(Note* song, int song_length, int bpm){
  for (int i=0; i<song_length; i++){
    //Serial.print("reached song thing");
    tone(BUZZ_PIN, pitchFrequency[available_notes[song[i].note_index]]);
    delay(1000 / (4.0 * bpm / song[i].duration / 60));
    noTone(BUZZ_PIN);
  };
}
