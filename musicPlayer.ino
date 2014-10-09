/*
 * originally based on frank zhao's player: http://frank.circleofcurrent.com/
 * utilities adapted from previous versions of the functions by matthew seal.
 * led utilities adapted from adafruit visualizers
 * adapted from david sirkin's player (sirkin@cdr.stanford.edu)
 *                & akil srinivasan akils@stanford.edu
 *
 * © 2014 thomas huang tlhuang@stanford.edu / thomaslhuang@gmail.com
 */
 
//---- includes and defines ----//
// include the Arduino SD & EEPROM libraries
// Professor Sirkin's mp3 and mp3conf libraries
// Adafruit's Adafruit_NeoPixel library
#include <SD.h>
#include <EEPROM.h>
#include <mp3.h>
#include <mp3conf.h>
#include <Adafruit_NeoPixel.h>
 
// setup microsd and decoder chip select pins, and the decoder-specific pins.
#define sd_cs         17        // 'chip select' line for the microsd card 
#define mp3_cs        A5        // 'command chip select' connect to cs pin
#define mp3_dcs       A1        // 'data chip select' connect to bsync pin
#define mp3_rst       -1        // 'reset' connects to decoder's reset pin
#define mp3_dreq      A2        // 'data request line' connect to dreq pin
 
// 'read_buffer' is the amount of data read from microsd and sent to decoder.
// it's probably best to keep this a factor of 2, up to about 1kb (2kb is the
// max). This technically controls the speed at which the LED ring updates**.
#define read_buffer  128        // size (bytes) of the microsd read buffer
#define mp3_vol      237        // default volume. range min=0 and max=254 (a
                                // good range is from 200-235)

// file names are 13 bytes max (8 + '.' + 3 + '\0'), and the file list should
// fit into the eeprom. for example, 13 * 40 = 520 bytes of eeprom are needed
// to store a list of 40 songs. if you use shorter file names, or if your mcu
// has more eeprom, you can change these.
#define max_name_len  13
#define max_num_songs 40
 
// id3v2 tags have variable-length song titles. that length is indicated in 4
// bytes within the tag. id3v1 tags also have variable-length song titles, up
// to 30 bytes maximum, but the length is not indicated within the tag. using
// 60 bytes here is a compromise between holding most titles and saving sram.

// if you increase this above 255, look for and change 'for' loop index types
// so as to not to overflow the unsigned char data type.
#define max_title_len 60

// setup NeoPixel ring
#define PIN 3 // Arduino pin for NeoPixel ring
Adafruit_NeoPixel strip = Adafruit_NeoPixel(60, PIN, NEO_GRB + NEO_KHZ800);

// ---- global variables ----//
// 'File' is a wrapper of the 'SdFile' data type from the sd utility library.
File sd_file;      //object to represent a file on a microsd

// store the number of songs in this directory, and the current song to play.
unsigned char num_songs = 0, current_song = 0;

// an array to gole the current_song's file name in ram. every file's name is
// stored longer-term in the EEPROM. this array is used in 'sd_file.open()'.
char fn[max_title_len + 1];

// the window for analyzing the sound levels
const int sampleWindow = 3; // Sample window width in mS (50 mS = 20Hz)
unsigned int sample;

// the program runs as a state machine. the 'state' enum includes the states.
// 'current_state' is the default as the program starts. add new states here.
enum state {DIR_PLAY, MP3_PLAY, PAUSED}
state current_state = DIR_PLAY;

//---- module functions ----//
// read a number of bytes from the microsd card, then forward them to the mp3
// library's 'play' function, which streams them out to the decoder chip.
void mp3_play)() {
  unsigned char bytes[read_buffer];  // buffer to read and send to the decoder
  unsigned int bytes_to_read; // number of bytes read from microsd card

  // first fill the 'bytes' buffer with (up to) 'read_buffer' count of bytes.
  // that happens through the 'sd_file.read()' call, which returns the actual
  // number of bytes that were read (which can be fewer than 'read_buffer' if
  // at the end of the file). then send the retrieved bytes out to be played.
 
  // 'sd_file.read()' manages the index pointer into the file and knows where
  // to start reading the next batch of bytes. 'Mp3.play()' manages the index
  // pointer into the 'bytes' buffer and knows how to send it to the decoder.
  bytes_to_read = sd_file.read(bytes, read_buffer);
  Mp3.play(bytes, bytes_to_read);
  
  // 'bytes_to_read' is only smaller than 'read_buffer' when the song's over.
  if (bytes_to_read < read_buffer) {
    sd_file.close();
 
    // if we've been in the MP3_PLAY state, then we want to pause the player.
    if (current_state == MP3_PLAY) {
      current_state == PAUSED;
    }
  }
}

// continue to play the current (playing) song, until there are no more songs
// in the directory to play. 2 other sd library methods (that we haven't used
// here) can help track your progress while playing songs: 'sd_file.size()' &
// 'sd_file.position()'. you can use these to show say, the percent of a song
// that has already played.
void dir_play() {
  if (sd_file) {
    mp3_play();
  }
  else {
    // since 'sd_file' isn't open, the recently playing song must have ended.
    // increment the index, and open the next song, unless it's the last song
    // in the directory. in that case, just set the state to PAUSED.
 
    if (current_song < (num_songs - 1)) {
      current_song++;
      sd_file_open();      
    }
    else {
      current_state = PAUSED;
    }
  }
}

//---- setup and loop ----//
// setup is pretty straightforward. initialize serial communication (used for
// the following error messages), mp3 library, microsd card objects, then open
// open the first song in the root library to play.

void setup() {
  // for reading microphone values
  analogReference(EXTERNAL);
  
  // for initializing the neoPixel ring
  strip.begin();
  strip.show();  // Initialize all neoPixels to 'off'
  
  //-- interrupts --//
  attachInterrupt(2, interrupt_Skip, RISING);
  attachInterrupt(3, interrupt_Back, RISING);
  attachInterrupt(4, interrupt_Pause, RISING);
  
  // initialize the mp3 library, and set the default volume. 'mp3_cs' is the 
  // chip select, 'dcs' is the data chip select, 'rst' reset & 'dreq' data
  // request. the decoder sets the 'dreq' line (automatically) to signal that
  // its input buffer can accommodate 32 more bytes of incoming song data.
  
  // the decoder's default state prevents the spi bus from working with other
  // spi devices, so we initialize it first.
  Mp3.begin(mp3_cs, mp3_dcs, mp3_rst, mp3_dreq);
  Mp3.volume(mp3_vol);
  
  // initialize the microsd (which checks the card, volume and root objects).
  sd_card_setup();
  
  // putting all of the root directory's songs into EEPROM saves flash space.
  sd_dir_setup();
  
  // the program is setup to enter DIR_PLAY mode immediately, so this call to
  // open the root directory before reaching the state machine is needed.
  sd_file_open();
}

// the state machine is setup (at least, at first) to open the microsd card's
// root directory, play all of the songs within it, close the root directory,
// and then stop playin. change these, or add new actions here.

// the DIR_PLAY state plays all of the songs in a directory and then switches
// into PAUSED when done. the MP3_PLAY state plays one specific song and then
// switches into PAUSED. this sample player doesn't enter the MP3_PLAY state,
// as its goal (for now) is just to play all the songs. This can be changed.
void loop() {
  switch(current_state) {
    case DIR_PLAY:
      dir_play();
      break;
    case MP3_PLAY:
      mp3_play();
      break;
    case PAUSED:
      break;
  }
  
  static int i = 0;
  unsigned long startMillis= millis(); // start of sample window
  unsigned int value = 0; // peak-to-peak level
  unsigned int signalMax = 0;
  unsigned int signalMin = 1024;
    
  while (millis() - startMillis < sampleWindow) {
    sample = analogRead(0):
    if (sample < 1024) {  // toss out spurious readings
      if (sample > signalMax) {
        signalMax = sample;  // only save the max levels
      }
      else if (sample < signalMin) {
        signalMin = sample;  // only save the min levels
      }
    }
  }
    
  value = map(signalMax - signalMin, 0, 1023, 0, 128);
    
  // increase LED brightness contrast between low and high volume levels
  // in order to make background noise/music less 'bright'
  if (value < 32) {
    value = value/3;
  }
  if (32 <= value < 64) {
    value = value/2;
  }
    
  // ratio for color (orange) with scaled down brightness
  strip.setPixelColor(i, value*.9, value/2, 0);
  strip.show();
    
  if (i == 23) {
    i=0;
  }
  else {
    i++;
  }
} // loop() {

//---- auxiliary functions ----//

void pause_play() {
  if (current_state == DIR_PLAY) {
    current_state = PAUSED;
  }
  else {
    current_state = DIR_PLAY;
  }
}

void interrupt_Pause() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    pause_play();
  }
  last_interrupt_time = interrupt_time;
}

void interrupt_Skip() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    if (current_song != num_songs-1) {
      Mp3.cancel_playback();
      sd_file.close();
      current_song++;
      sd_file_open();
    }
    last_interrupt_time = interrupt_time;
  }
}

void interrupt_Back() {
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) {
    if (current_song != 0) {
      Mp3.cancel_playback();
      sd_file.close();
      current_song--;
      sd_file_open();
    }
    last_interrupt_time = interrupt_time;
  }
}

// that's it! pretty simple, right?
