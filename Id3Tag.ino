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

// this utility function reads id3v1 and id3v2 tags, if any are present, from
// mp3 audio files. if no tags are found, just use the title of the file. :-|

void get_title_from_id3tag () {
  unsigned char id3[3];       // pointer to the first 3 characters to read in

  // visit http://www.id3.org/id3v2.3.0 to learn all(!) about the id3v2 spec.
  // move the file pointer to the beginning, and read the first 3 characters.

  sd_file.seek(0);
  sd_file.read(id3, 3);
  
  // if these first 3 characters are 'ID3', then we have an id3v2 tag. if so,
  // a 'TIT2' (for ver2.3) or 'TT2' (for ver2.2) frame holds the song title.

  if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
    unsigned char pb[4];       // pointer to the last 4 characters we read in
    unsigned char c;           // the next 1 character in the file to be read
    
    // our first task is to find the length of the (whole) id3v2 tag. knowing
    // this means that we can look for 'TIT2' or 'TT2' frames only within the
    // tag's length, rather than the entire file (which can take a while).

    // skip 3 bytes (that we don't use), then read in the last 4 bytes of the
    // header, which contain the tag's length.

    sd_file.read(pb, 3);
    sd_file.read(pb, 4);
    
    // to combine these 4 bytes together into the single value, we first have
    // to shift each one over to get it into its correct 'digits' position. a
    // quirk of the spec is that bit 7 (the msb) of each byte is set to 0.
    
    unsigned long v2l = ((unsigned long) pb[0] << (7 * 3)) +
                        ((unsigned long) pb[1] << (7 * 2)) +
                        ((unsigned long) pb[2] << (7 * 1)) + pb[3];
                        
    // we just moved the file pointer 10 bytes into the file, so we reset it.
    
    sd_file.seek(0);

    while (1) {
      // read in bytes of the file, one by one, so we can check for the tags.
      
      sd_file.read(&c, 1);

      // keep shifting over previously-read bytes as we read in each new one.
      // that way we keep testing if we've found a 'TIT2' or 'TT2' frame yet.
      
      pb[0] = pb[1];
      pb[1] = pb[2];
      pb[2] = pb[3];
      pb[3] = c;

      if (pb[0] == 'T' && pb[1] == 'I' && pb[2] == 'T' && pb[3] == '2') {
        // found an id3v2.3 frame! the title's length is in the next 4 bytes.
        
        sd_file.read(pb, 4);

        // only the last of these bytes is likely needed, as it can represent
        // titles up to 255 characters. but to combine these 4 bytes together
        // into the single value, we first have to shift each one over to get
        // it into its correct 'digits' position. 

        unsigned long tl = ((unsigned long) pb[0] << (8 * 3)) +
                           ((unsigned long) pb[1] << (8 * 2)) +
                           ((unsigned long) pb[2] << (8 * 1)) + pb[3];
        tl--;
        
        // skip 2 bytes (we don't use), then read in 1 byte of text encoding. 

        sd_file.read(pb, 2);
        sd_file.read(&c, 1);
        
        // if c=1, the title is in unicode, which uses 2 bytes per character.
        // skip the next 2 bytes (the byte order mark) and decrement tl by 2.
        
        if (c) {
          sd_file.read(pb, 2);
          tl -= 2;
        }
        // remember that titles are limited to only max_title_len bytes long.
        
        if (tl > max_title_len) tl = max_title_len;
        
        // read in tl bytes of the title itself. add an 'end-of-string' byte.

        sd_file.read(title, tl);
        title[tl] = '\0';
        break;
      }
      else
      if (pb[1] == 'T' && pb[2] == 'T' && pb[3] == '2') {
        // found an id3v2.2 frame! the title's length is in the next 3 bytes,
        // but we read in 4 then ignore the last, which is the text encoding.
        
        sd_file.read(pb, 4);
        
        // shift each byte over to get it into its correct 'digits' position. 
        
        unsigned long tl = ((unsigned long) pb[0] << (8 * 2)) +
                           ((unsigned long) pb[1] << (8 * 1)) + pb[2];
        tl--;
        
        // remember that titles are limited to only max_title_len bytes long.

        if (tl > max_title_len) tl = max_title_len;

        // there's no text encoding, so read in tl bytes of the title itself.
        
        sd_file.read(title, tl);
        title[tl] = '\0';
        break;
      }
      else
      if (sd_file.position() == v2l) {
        // we reached the end of the id3v2 tag. use the file name as a title.

        strncpy(title, fn, max_name_len);
        break;
      }
    }
  }
  else {
    // the file doesn't have an id3v2 tag so search for an id3v1 tag instead.
    // an id3v1 tag begins with the 3 characters 'TAG'. if these are present,
    // then they are located exactly 128 bits from the end of the file.
    
    sd_file.seek(sd_file.size() - 128);
    sd_file.read(id3, 3);
    
    if (id3[0] == 'T' && id3[1] == 'A' && id3[2] == 'G') {
      // found it! now read in the full title, which is always 30 bytes long.
      
      sd_file.read(title, 30);
      
      // strip spaces and non-printable characters from the end of the title.
      // you may have to expand this range to incorporate unicode characters.
      
      for (char i = 30 - 1; i >= 0; i--) {
        if (title[i] <= ' ' || title[i] > 126) {
          title[i] = '\0';
        }
        else {
          break;
        }
      }
    }
    else {
      // we reached the end of the id3v1 tag. use the file name as a title.
      
      strncpy(title, fn, max_name_len);
    }
  }
}
