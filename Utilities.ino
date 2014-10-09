/*
 * example sketch to play audio file(s) in a directory, using the mp3 library
 * for playback and the arduino sd library to read files from a microsd card.
 * pins are setup to work well for teensy 2.0. double-check if using arduino.
 * 
 * originally based on frank zhao's player: http://frank.circleofcurrent.com/
 * utilities adapted from previous versions of the functions by matthew seal.
 *
 * (c) 2011, 2012 david sirkin sirkin@cdr.stanford.edu
 *                akil srinivasan akils@stanford.edu
 */

// first step, declare the variables used later to represent microsd objects.

File sd_root;                   // the sd partition's root ('/') directory

// check that the microsd card is present, can be initialized and has a valid
// root volume. 'sd_root' is a pointer to the card's root volume object.

void sd_card_setup() {
  if (!SD.begin(sd_cs)) {
    //lcd.clearDisplay();
    //lcd.println("sd card failed\nor not present");
    //lcd.display();
    return;
  }
  sd_root = SD.open("/");
  
  if (!sd_root) {
    //lcd.clearDisplay();
    //lcd.println("couldn't mount\nsd root volume");
    //lcd.display();
    return;
  }
}

// for each song file in the current directory, store its file name in eeprom
// for later retrieval. this saves on using program memory for the same task,
// which is helpful as you add more functionality to the program. 

// it also lets users change songs on the microsd card without having to hard
// code file names into the program. ask an instructor if you want to use sub
// directories also.

void sd_dir_setup() {
  num_songs = 0;
  
  sd_root.rewindDirectory();
  
  while (num_songs < max_num_songs) {
    // break out of while loop when we check all files (past the last entry).
    
    File p = sd_root.openNextFile();
    if (!p) break;
    
    // only store current (not deleted) file entries, and ignore the . and ..
    // directory entries. also ignore any sub-directories.
    
    if (p.name()[0] == '~' || p.name()[0] == '.' || p.isDirectory()) {
      continue;
    }
    
    Serial.print(p.name());
    // to find the position of the '.' that precedes the file name extension,
    // we have to search through the file name (stored as an array of chars).
    
    // fat16 prefers 8.3 type file names, and the sd library will shorten any
    // names longer than that. so if we have an mp3 or wav file, the '.' will 
    // appear no later than position 8 ('max_name_len'-5) of the file name.
    
    char i;
    
    for (i = max_name_len - 5; i > 0; i--) {
      if (p.name()[i] == '.') break;
    }
    i++;
    
    // only store mp3 or wav files in eeprom (for now). if you add other file
    // types, you should add their extensions here.

    if ((p.name()[i] == 'M' && p.name()[i+1] == 'P' && p.name()[i+2] == '3') ||
        (p.name()[i] == 'W' && p.name()[i+1] == 'A' && p.name()[i+2] == 'V')) {
          
      // store each character of the file name (including the terminate-array
      // character '\0' at position 12) into a byte in the eeprom.
      
      for (char i = 0; i < max_name_len; i++) {
        EEPROM.write(num_songs * max_name_len + i, p.name()[i]);
      }
      num_songs++;
    }
  }
}

// given the numerical index of a particular song to play, go to its location
// in eeprom, r etrieve its file name and set the global variable 'fn' to it.

void get_current_song_as_fn() {
  for (char i = 0; i < max_name_len; i++) {
    fn[i] = EEPROM.read(current_song * max_name_len + i);
  }
}

// print the title to the lcd. if no title is available, print the file name.

void print_title_to_lcd() {
  get_title_from_id3tag();

  //lcd.clearDisplay();
  //lcd.println(title);
  //lcd.display();
}
