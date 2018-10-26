#include <PS2Keyboard.h>
/**
   The Only true code for tesla coil interrupter
*/


#include <SPI.h>
#include <SdFat.h>
#include <Pulser.h>

#define DBG(x) //Serial.println(x)
#define DBGP(x) //Serial.print(x)

#define PLAYLIST_LENGTH 7


#define MAX_DUTY 0.05 //from 0 to 1.0
#define MAX_FREQUENCY 1000 //in Hertz
#define MAX_MIDI_SIZE 300 //in Bytes (divide by 3 to get number of notes)

#define MIN_KEYBOARD_NOTE_OFFSET -1//means it can be set MIN_KEYBOARD_NOTE_OFFSET*12 notes (octaves) under default notes
#define MAX_KEYBOARD_NOTE_OFFSET 4//means it can be set MAX_KEYBOARD_NOTE_OFFSET*12 notes (octaves) over default notes

#define DEBOUNCE_TIME 50 //in millis  after how many millis it will be possible to switch to the next state
#define LONGPRESS_TIME 800 //in millis, to chance to special state to switch songs

//#define INTERRUPTER_PIN 2 //needs to be changed in library: Pulser.h
#define SD_PIN 4 //CS pin of SD
#define KEYBOARD_DATA_PIN 5 //data pin of PS2Keyboard (analog pin)
#define DUTY_PIN 5 //input of potentiometer
#define FREQ_PIN 4//input of potentiometer

#define BUTTON_PIN 6 //input of mode switching button
#define LED_KNOB_PIN 9 //led indicating whether knob mode is active
#define LED_KEYBOARD_PIN 8 //led indicating whether keyboard mode is active
#define LED_SD_PIN 7 //led indicating whether sd mode is active

#define STATE_SD 0
#define STATE_KNOB 1
#define STATE_KEYBOARD 2
#define STATE_TRICK 3

#define MUSIC_NULL "null"

const PROGMEM uint16_t frequencyList[] = {//from http://subsynth.sourceforge.net/midinote2freq.html
  8,
  9,
  9,
  10,
  10,
  11,
  12,
  12,
  13,
  14,
  15,
  15,

  16,
  17,
  18,
  19,
  21,
  22,
  23,
  25,
  26,
  28,
  29,
  31,

  33,
  35,
  37,
  39,
  41,
  44,
  46,
  49,
  52,
  55,
  58,
  62,

  65,
  69,
  73,
  78,
  82,
  87,
  92,
  98,
  104,
  110,
  117,
  123,

  131,
  139,
  147,
  156,
  165,
  175,
  185,
  196,
  208,
  220,
  233,
  247,

  262,
  277,
  294,
  311,
  330,
  349,
  370,
  392,
  415,
  440,
  466,
  494,

  523,
  554,
  587,
  622,
  659,
  698,
  740,
  784,
  831,
  880,
  932,
  988,

  0 //silence index:84
};
File myFile;
Pulser pulser;
SdFat sd;
PS2Keyboard keyboard;

String musics[PLAYLIST_LENGTH];
String music;
byte musicIndex = -1;

int transpose;
double speedi;
uint16_t maxFrequency;
double maxDuty;
uint16_t frequency;
double duty;

bool sdWasRead;


uint8_t state;
//bool settingsOverride = false; //used in sd mode: "knobs affect the music"=false or "change settings according to command.txt"=true
//bool knobDisable = false;
//
//bool oneNoteMode = false; //gets set from command.txt , if true it disables normal behavior of device, it will play only one note specified by freq: and duty: commands and  nothhing else-> knobs and button wont do anything

bool playing;//midi variable, if song is played from sd


void setup() {
  transpose = 0;
  speedi = 1;
  for (int i = 0 ; i < PLAYLIST_LENGTH; i++) {
    musics[i] = MUSIC_NULL;
  }
  state = 100;//some non existing value

  pinMode(LED_KNOB_PIN, OUTPUT);
  pinMode(LED_KEYBOARD_PIN, OUTPUT);
  pinMode(LED_SD_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT);
  pinMode(2, OUTPUT);

  changeState(STATE_KNOB);


  maxDuty = MAX_DUTY;
  maxFrequency = MAX_FREQUENCY;
  duty = maxDuty;


  sdWasRead = sdInit(); //tried to init sd

  musicIndex = 0;
  music = musics[musicIndex];
  /* if (oneNoteMode) {
     pulser.pulse(maxFrequency, maxDuty);
     while (true) {}
    }*/
  //keyboar dintt
  delay(200);
  keyboard.begin(KEYBOARD_DATA_PIN);
  delay(500);
 //logMe("Keyboard initialized");

}

void loop() {
  modeSwitchTick();
  if (!playing && state == STATE_SD)
    playMIDI();
  if (state == STATE_SD)
    midiTick();
  //if (!settingsOverride)//that means the settings of duty and freq is ignored
 //   knobTick();
  if (!playing && state == STATE_TRICK)
    playMIDI();
  if (state == STATE_KEYBOARD || state == STATE_TRICK)
    keyboardTick();
}
//======================================MODE SWITCHER===========================================
bool buttonPressed;
long refreshTime;
bool longMode;
void modeSwitchTick() {
  if (digitalRead(BUTTON_PIN) != 0) {
    if (!buttonPressed) {
      buttonPressed = true;
      refreshTime = millis() + DEBOUNCE_TIME;
    }
  } else {
    if (buttonPressed) {
      buttonPressed = false;
      long m = millis();
      if (m > refreshTime) {
        bool longPress = m > refreshTime + LONGPRESS_TIME;
        switch (state) {
          case STATE_KNOB:
            changeState(STATE_KEYBOARD);
            break;
          case STATE_KEYBOARD:
            changeState(STATE_SD);
            break;
          case STATE_SD:
            if (longMode) {
              if (longPress) {
                longMode = false;
                blinkLongMode(false);
                changeState(STATE_TRICK);
              } else longModeSwitch(STATE_SD);
            }
            else if (longPress) {
              longMode = true;
              blinkLongMode(true);
              printBinaryDigits(musicIndex);
            } else
              changeState(STATE_TRICK);
            break;
          case STATE_TRICK:
            if (longMode) {
              if (longPress) {
                longMode = false;
                blinkLongMode(false);
                changeState(STATE_KNOB);
              } else longModeSwitch(STATE_TRICK);
            }
            else if (longPress) {
              longMode = true;
              blinkLongMode(true);
              printBinaryDigits(musicIndex);
            } else
              changeState(STATE_KNOB);
            break;
        }
      }
    }
  }
}
void longModeSwitch(uint8_t state) {
  switch (state) {//deInit of old state
    case STATE_SD:
    case STATE_TRICK:
      pulser.noPulse();
      musicIndex++;
      musicIndex %= PLAYLIST_LENGTH;
      String m = musics[musicIndex];
      if (m.equals(MUSIC_NULL)) {
        musicIndex = 0;
        m = musics[musicIndex];
      }
      music = m;
      //stopMIDI();
      if (playMIDI()) {
        printBinaryDigits(musicIndex);
      } else {
        if (state == STATE_SD)
          blinkInvalid(false, false, true);
        else blinkInvalid(true, true, true);
      }
      break;
  }
}
void printBinaryDigits(uint8_t digit) {
  uint8_t d = digit + 1;
  digitalWrite(LED_KNOB_PIN, (d & 1) != 0);
  digitalWrite(LED_KEYBOARD_PIN, ((d >> 1) & 1) != 0);
  digitalWrite(LED_SD_PIN, ((d >> 2) & 1) != 0);
}
void changeState(uint8_t newState) {
  if (newState == state)
    return;

  digitalWrite(LED_KNOB_PIN, 0);
  digitalWrite(LED_KEYBOARD_PIN, 0);
  digitalWrite(LED_SD_PIN, 0);

  switch (state) {//deInit of old state
    case STATE_KNOB:
      break;
    case STATE_TRICK:
      break;
    case STATE_KEYBOARD:
      stopMIDI();
      break;
    case STATE_SD:
      stopMIDI();
      break;
  }
  pulser.noPulse();
  state = newState;

  switch (state) {//init of new state
    case STATE_KNOB:
      digitalWrite(LED_KNOB_PIN, 1);
      pulser.pulse(frequency, duty);
      break;
    case STATE_KEYBOARD:
      digitalWrite(LED_KEYBOARD_PIN, 1);
      keyboard.begin(KEYBOARD_DATA_PIN);
      break;
    case STATE_TRICK:
      keyboard.begin(KEYBOARD_DATA_PIN);
      if (!sdWasRead) {//todo should blink until sd is valid
        blinkInvalid(true, true, true);
      }
      digitalWrite(LED_KEYBOARD_PIN, 1);
      digitalWrite(LED_KNOB_PIN, 1);
      digitalWrite(LED_SD_PIN, 1);
      break;
    case STATE_SD:
      //settingsOverride = false;
      if (!sdWasRead) {//todo should blink until sd is valid
        blinkInvalid(false, false, true);
      }
      digitalWrite(LED_SD_PIN, 1);
      break;
  }
}
void blinkInvalid(bool keyBoardPin, bool knobPin, bool sdPin) {
  blinkPrep();
  for (int i = 0; i < 7; i++) {//blink to signalize invalid sd setup
    digitalWrite(LED_KEYBOARD_PIN, keyBoardPin);
    digitalWrite(LED_KNOB_PIN, knobPin);
    digitalWrite(LED_SD_PIN, sdPin);
    delay(100);
    digitalWrite(LED_KEYBOARD_PIN, 0);
    digitalWrite(LED_KNOB_PIN, 0);
    digitalWrite(LED_SD_PIN, 0);
    delay(100);
  }
  blinkDone();
}
void blinkLongMode(bool offToOn) {
  blinkPrep();
  if (offToOn) {
    digitalWrite(LED_KEYBOARD_PIN, 0);
    digitalWrite(LED_KNOB_PIN, 0);
    digitalWrite(LED_SD_PIN, 0);
    delay(150);
    digitalWrite(LED_KNOB_PIN, 1);
    delay(150);
    digitalWrite(LED_KEYBOARD_PIN, 1);
    delay(150);
    digitalWrite(LED_SD_PIN, 1);
    delay(350);
  } else {
    digitalWrite(LED_KEYBOARD_PIN, 1);
    digitalWrite(LED_KNOB_PIN, 1);
    digitalWrite(LED_SD_PIN, 1);
    delay(150);
    digitalWrite(LED_SD_PIN, 0);
    delay(150);
    digitalWrite(LED_KEYBOARD_PIN, 0);
    delay(150);
    digitalWrite(LED_KNOB_PIN, 0);
    delay(350);
  }
  blinkDone();

}
bool keyboardOn;
bool sdOn;
bool knobOn;
void blinkPrep() {
  keyboardOn = digitalRead(LED_KEYBOARD_PIN);
  sdOn = digitalRead(LED_SD_PIN);
  knobOn = digitalRead(LED_KNOB_PIN);
}
void blinkDone() {
  digitalWrite(LED_KEYBOARD_PIN, keyboardOn);
  digitalWrite(LED_KNOB_PIN, knobOn);
  digitalWrite(LED_SD_PIN, sdOn);
}

//======================================KEYBOARD================================================
byte noteOffsett;
bool released;
byte pressed;

void keyboardTick() {
  if (keyboard.available()) {
    byte b = keyboard.read();
    if (b == PS2_RELEASE) {
      released = true;
    }
    else if (released) {
      released = false;
      if (pressed == b) { //stop from currently held key
        pressed = 0;
        pulser.noPulse();
        DBG("No");
      }

    }
    else if (b == pressed) {} //we dont care about holding button forever
    else {
      if (state == STATE_TRICK) {
        int freq = nextNote();
        while (freq == 0) {
          freq = nextNote();
          if (freq == -1)
            break;
        }
        if (freq != -1) {
          pulser.pulse(freq, duty);
        }
      }
      else if (b == PS2_Z || b == PS2_X) {
        if (b == PS2_Z)
          noteOffsett--;
        else
          noteOffsett++;
        if (noteOffsett < MIN_KEYBOARD_NOTE_OFFSET){
          noteOffsett = MIN_KEYBOARD_NOTE_OFFSET;
          //blinkLongMode(true);
        }
        if (noteOffsett > MAX_KEYBOARD_NOTE_OFFSET){
          noteOffsett = MAX_KEYBOARD_NOTE_OFFSET;
          //blinkLongMode(false);
        }
      }
      else {

        byte possibleNote = getNoteFromChar(b);
        if (possibleNote != 0) {
          possibleNote += noteOffsett * 12;
          if (possibleNote > 83)
            possibleNote = 83;
          if (possibleNote < 0)
            possibleNote = 0;

          uint16_t freq = pgm_read_word_near(frequencyList + possibleNote);
          if (freq != 0) {
            pulser.pulse(freq, duty);
            DBGP(freq);
            DBG(" Toone");
          }
        }
      }
      pressed = b;
    }
  }
}
byte getNoteFromChar(byte b) {
  switch (b) {
    case PS2_A: return 24;
    case PS2_W: return 25;
    case PS2_S: return 26;
    case PS2_E: return 27;
    case PS2_D: return 28;
    case PS2_F: return 29;
    case PS2_T: return 30;
    case PS2_G: return 31;
    case PS2_Y: return 32;
    case PS2_H: return 33;
    case PS2_U: return 34;
    case PS2_J: return 35;
    case PS2_K: return 36;
    case PS2_O: return 37;
    case PS2_L: return 38;
    case PS2_P: return 39;
    case PS2_SEMICOLON: return 40;
    case PS2_APOSTROPHE: return 41;
  }
  return 0;
}
//============================KNOB=============================================================
#define KNOB_AVERAGE_READINGS 20
/*uint8_t knobIndex;
uint16_t freqSum;
double dutSum;*/
void knobTick() {
  
 /* if (knobDisable)
    return;
  uint16_t freq = map(analogRead(FREQ_PIN), 0, 1023, 2, maxFrequency);
  double dut = map(analogRead(DUTY_PIN), 0, 1023, 0, maxDuty * 100);
  freqSum += freq;
  dutSum += dut;
  knobIndex++;
  if (knobIndex == KNOB_AVERAGE_READINGS) {
    knobIndex = 0;
    freq = freqSum / KNOB_AVERAGE_READINGS;
    dut = dutSum / KNOB_AVERAGE_READINGS;
    freqSum = 0;
    dutSum = 0;
    if (frequency != freq || dut != duty * 100) {
      dut /= 100.0;
      frequency = freq;
      duty = dut;
      if (state == STATE_KNOB) {
        pulser.pulse(frequency, duty);
      }
    }
  }*/
}


//======================SD_INIT===============================================================
bool sdInit() {
  if (!sd.begin(SD_PIN, SPI_HALF_SPEED))
    return false;

  myFile = sd.open("command.txt", FILE_READ);
  if (myFile) {
    char line[30];
    int n;
    while ((n = myFile.fgets(line, sizeof(line))) > 0)
      command(String(line));
    myFile.close();
  } else return false;

  DBG("SD initaliazed!");
  return true;
}
void logMe(String s) {
  if (!sdWasRead)
    return;
  if (!myFile.open("log.txt", O_RDWR | O_CREAT | O_AT_END)) {
    sd.errorHalt("opening log.txt for write failed");
  }
  // if the file opened okay, write to it:
  myFile.println(s);

  // close the file:
  myFile.close();
}
void command(String s) {
  if (s.startsWith("freq:")) {
    maxFrequency = s.substring(5).toInt();
    DBGP("Frequency from file:");
    DBG(maxFrequency);
  }
  if (s.startsWith("transpose:")) {
    transpose = s.substring(10).toInt();
    DBGP("transpose from file:");
    DBG(transpose);
  }
  if (s.startsWith("speed:")) {
    speedi = (double)(s.substring(6).toFloat() / 100.0);
    DBGP("speed from file:");
    DBG(speedi);
  }
  else if (s.startsWith("override")) {
   // settingsOverride = true;
    DBG("override mode active");
  }
  else if (s.startsWith("knob_disable")) {
    //knobDisable = true;
    DBG("knob disabled");
  }
  else if (s.startsWith("one_note_mode")) {
    //oneNoteMode = true;
    DBG("one note mode active");
  }
  else if (s.startsWith("duty:")) {
    maxDuty = (double)s.substring(5).toFloat() / 100.0;
    if (maxDuty > MAX_DUTY)
      maxDuty = MAX_DUTY;
    if (duty > maxDuty)
      duty = maxDuty;
    DBGP("Duty from file:");
    DBG(maxDuty);
  }
  else if (s.startsWith("musics:")) {
    musicIndex = 0;
  }
  else if (musicIndex >= 0) {
    musics[musicIndex] = removeEnter(s);
    musicIndex++;
    musicIndex %= PLAYLIST_LENGTH;
  }
}
String removeEnter(String s) {
  if (s.endsWith("\n")) {
    return s.substring(0, s.length() - 1);
  }
  return s;
}

//=======================START=====MIDI========================================================
uint8_t midiBuffer[MAX_MIDI_SIZE];
uint16_t currentB;
uint8_t musicPiece = 0;
long taskTime = 0;
//needs to be called every loop cycle
void midiTick() {
  if (!playing)
    return;
  if (taskTime > millis())
    return;
  if (currentB < MAX_MIDI_SIZE) {
    uint16_t duration = (midiBuffer[currentB + 0] << 4) & 3840; //and B111100000000
    duration |= midiBuffer[currentB + 1];
    duration *= speedi;

    uint16_t note = (midiBuffer[currentB + 0] << 8) & 3840;
    note |= midiBuffer[currentB + 2];
    if (note != 84) {
      note += transpose;
      if (note < 0)
        note = 0;
      if (note > 83)
        note = 83;
    }

    uint16_t freq = pgm_read_word_near(frequencyList + note);

    if (freq == 0 || duty == 0)
      pulser.noPulse();
    else {
      if (freq > maxFrequency) //assure that freq will never be higher than maxFreq
        freq = maxFrequency;
      pulser.pulse(freq, duty);
    }
    taskTime = millis() + duration;
    currentB += 3;
    if (currentB >= MAX_MIDI_SIZE) { //time to load new data
      if (!fillBuffer(music, musicPiece)) {
        playing = false;
      } else {
        musicPiece++;
        currentB = 0;
      }
    }
  }
}
/**
   returns next note=freq in midi
   can output 0 that means pause
   can output -1 that means no More Notes
*/
int nextNote() {
  if (!playing)
    return -1;
  if (currentB < MAX_MIDI_SIZE) {
    uint16_t note = (midiBuffer[currentB + 0] << 8) & 3840;
    note |= midiBuffer[currentB + 2];
    if (note != 84) {
      note += transpose;
      if (note < 0)
        note = 0;
      if (note > 83)
        note = 83;
    }
    uint16_t freq = pgm_read_word_near(frequencyList + note);
    if (freq > maxFrequency) //assure that freq will never be higher than maxFreq
      freq = maxFrequency;

    currentB += 3;
    if (currentB >= MAX_MIDI_SIZE) { //time to load new data
      if (!fillBuffer(music, musicPiece)) {
        playing = false;
      } else {
        musicPiece++;
        currentB = 0;
      }
    }
    return freq;
  }
  return -1;
}
//starts playing song from sd specified by string music
//return true if music was successfully loaded
bool playMIDI() {
  if (fillBuffer(music, 0)) {
    currentB = 0;
    musicPiece = 1;
    playing = true;
    taskTime = millis() + 1;
    return true;
  }
  return false;
}
//stops playing song from sd
void stopMIDI() {
  playing = false;
  pulser.noPulse();
}
/**
   fills ram buffer with data from sd
   returns false if musicIndex or Name does not exist!
*/
bool fillBuffer(String musicName, uint8_t musicIndex) {
  myFile = sd.open(musicName + musicIndex + ".txt");
  if (myFile) {
    if (myFile.available()) {
      myFile.read(midiBuffer, MAX_MIDI_SIZE);
    }
    myFile.close();
    return true;
  }
  DBGP("cannot load: ");
  DBG(musicName + musicIndex + ".txt");
  return false;
}
//===========================END===MIDI========================================================





