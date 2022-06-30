#include "Encoder.h"
#include "SevSeg.h"

#define PIN_TX 0
#define PIN_RX 1

#define PIN_ENCODER_A 2
#define PIN_ENCODER_B 3

#define PIN_ENCBUTTON A2
#define PIN_REDBUTTON A3

#define BUTTON_PRESSED LOW
#define BUTTON_RELEASED HIGH
#define BUTTON_JUST_PRESSED FALLING
#define BUTTON_JUST_RELEASED RISING

#define PIN_BUZZER 9

#define PIN_NEOSEG 10

#define PIN_REL_HEAT_TOP A0
#define PIN_REL_HEAT_BOT A1
#define PIN_REL_FAN 5
#define PIN_REL_LIGHT 7


#define TIME(t) (t)
#define CLOCK_MULT 10
#define FREQ(f) (f)

#define DISPLAY_WELCOME_MESSAGE "HELO"
#define MAX_DISP_NUM 9999

#define RED_BUTTON_HOLD_TIME TIME(3000)

#define ENCODER_DEBOUNCE_DELAY_MS TIME(10)    // the debounce time; increase if the output flickers
#define ENCODER_ROTATE_MULT 4    // the debounce time; increase if the output flickers
unsigned long lastDebounceRotaryTime = TIME(0);  // the last time the output pin was toggled
unsigned long lastDebounceTime = TIME(0);  // the last time the output pin was toggled
unsigned long buttonStartPress = 0;
int lastButtonState = HIGH;

unsigned long redbuttonStartPress = 0;


Encoder myEnc(PIN_ENCODER_A, PIN_ENCODER_B);

enum mode {
  BOOTUP,
  TIMER,
  RANDOM,
  TONE,
  METRONOME,
  COUNTER,
  TEST,
  UNKNOWN
};

enum state {
  READY,
  SET_MODE,
};

enum inputState {
  IDLE,
  TAP_RELEASE,
  HOLD,
  HOLD_RELEASE,
  ROTATE_UP,
  ROTATE_DOWN,
  HOLD_ROTATE_UP,
  HOLD_ROTATE_DOWN,
};

inputState currentInputState = IDLE;

int modeSelector = COUNTER;
#define TOTAL_MODES 12
mode currentMode = BOOTUP;
state currentState = SET_MODE;

/** MODE STATE VARIABLES **/
// TODO: Move these into class VARIABLES

// TEST
int counter = 0;
int selector = 0;

// RANDOM
bool randomizingState = false;

// TIMER
bool timerActive = false;
unsigned long timerDuration = 120;
unsigned long timerDurationms = TIME(timerDuration * 1000);
unsigned long timerStartTime = 0;

// TONE
bool playTone = false;
unsigned int toneFreq = 440;

// METRONOME
#define METRO_TONE_LENGTH TIME(10)
unsigned int metroBpm = 60;
unsigned int metroBpms = TIME(60000 / metroBpm);
unsigned int metroMeterMs = metroBpms * 4;
unsigned int metroOffset = 0;
bool metroActive = false;


#include "SevSeg.h"
SevSeg sevseg; //Instantiate a seven segment controller object


uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

void setup() {
  bool updateWithDelays = false; // Default 'false' is Recommended
  bool leadingZeros = false; // Use 'true' if you'd like to keep the leading zeros
  bool disableDecPoint = false; // Use 'true' if your decimal point doesn't exist or isn't connected


  pinMode(PIN_REDBUTTON, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_NEOSEG, OUTPUT);
  pinMode(PIN_REL_HEAT_TOP, OUTPUT);
  pinMode(PIN_REL_HEAT_BOT, OUTPUT); 
  pinMode(PIN_REL_FAN, OUTPUT);
  pinMode(PIN_REL_LIGHT, OUTPUT);
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCBUTTON, INPUT);

  tone(PIN_BUZZER, FREQ(880), TIME(20));

  sevseg.begin(4, PIN_NEOSEG, updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(5);
  sevseg.blank();  
  sevseg.setBkgColor(0);
  sevseg.setBkgColor(Color(0,0,0));
  sevseg.setColor(Color(255,28,0));

}

// TODO - maybe ints can just be passed as enum to changeMode()?
void changeModeByIndex(mode newMode) {
  switch (newMode)
  {
  case TIMER:
    changeMode(TIMER);
    break;

  case RANDOM:
    changeMode(RANDOM);
    randomizingState = true;
    break;

  case METRONOME:
    changeMode(METRONOME);
    metroActive = false;
    break;

  case COUNTER:
    changeMode(COUNTER);
    break;

  case TONE:
    changeMode(TONE);
    playTone = false;
    break;

  case TEST:
    changeMode(TEST);
    break;

  default:
    changeMode(UNKNOWN);
    break;
  }
}

// TODO - combine with changeModeByIndex()
void changeMode(mode newMode) {
  currentMode = newMode;
  tone(PIN_BUZZER, FREQ(220), TIME(10));
  currentState = READY;
  sevseg.setColor(Color(16,255,0));
  sevseg.blank();
}

void exitMode() {
  currentMode = BOOTUP;
  tone(PIN_BUZZER, FREQ(220), TIME(10));
  currentState = SET_MODE;
  sevseg.setColor(Color(255,28,0));
  sevseg.blank();
}


  // BOOTUP,
  // TIMER,
  // RANDOM,
  // TONE,
  // METRONOME,
  // COUNTER,
  // TEST,
  // UNKNOWN
void indicateMode(mode selectedMode) {

  switch (selectedMode)
  {
    case TIMER:
      sevseg.setChars("TIM");
      break;
    case RANDOM:
      sevseg.setChars("RAND");
      break;
    case TONE:
      sevseg.setChars("TONE");
      break;
    case METRONOME:
      sevseg.setChars("METR");
    case TEST:
      sevseg.setChars("TEST");
      break;
    
  default:
    sevseg.setNumber(selectedMode);
    break;
  }

}

void loop() {
  // Check cancel button first
  if(currentState == READY) {
    if(digitalRead(PIN_REDBUTTON) == HIGH) {
      buttonStartPress = 0;
    } else if(buttonStartPress == 0) {
      buttonStartPress = millis();
    }

    if(buttonStartPress > 0 && millis() - buttonStartPress > RED_BUTTON_HOLD_TIME) {
      exitMode();
    }
  }

  inputState lastInputState = currentInputState;
  currentInputState = IDLE;
  int rotateDirection = 0;

  // UI stuff
  long encoderPos = myEnc.read();

  if (abs(encoderPos) >= ENCODER_ROTATE_MULT) {
    //if((unsigned long)(millis() - lastDebounceRotaryTime) >= ENCODER_DEBOUNCE_DELAY_MS) {
      //lastDebounceRotaryTime = millis();
      if(encoderPos < 0) {
        rotateDirection = 1;
      } else {
        rotateDirection = -1;
      }
    //}
    if(encoderPos < 0) {
      myEnc.write(encoderPos + ENCODER_ROTATE_MULT);
    } else {
      myEnc.write(encoderPos - ENCODER_ROTATE_MULT);
    }
  }

  int buttonChange = 0;
  int buttonState = digitalRead(PIN_ENCBUTTON);
  if(buttonState != lastButtonState) {
    lastButtonState = buttonState;
    if(buttonState == BUTTON_PRESSED) {
      buttonStartPress = millis();
      buttonChange = BUTTON_JUST_PRESSED;
    } else if (buttonState == BUTTON_RELEASED) {
      buttonChange = BUTTON_JUST_RELEASED;
    }
  }

  /********** DETERMINE INPUT STATE *****************/
  if(buttonState == BUTTON_RELEASED && rotateDirection == 1) {
    currentInputState = ROTATE_UP;
  }

  if(buttonState == BUTTON_RELEASED && rotateDirection == -1) {
    currentInputState = ROTATE_DOWN;
  }

  // Check for press+turn
  if(currentInputState == IDLE && buttonState == BUTTON_PRESSED) {
    if(rotateDirection == 1) {
      currentInputState = HOLD_ROTATE_UP;
    } else if(rotateDirection == -1) {
      currentInputState = HOLD_ROTATE_DOWN;
    } else if(lastInputState == HOLD || lastInputState == HOLD_ROTATE_UP || lastInputState == HOLD_ROTATE_DOWN) {
      currentInputState = HOLD;
    }
  }

  if(currentInputState == IDLE && buttonChange == BUTTON_JUST_RELEASED) {
    currentInputState = TAP_RELEASE;
  }

  /************* DONE WITH INPUT **************/



  /************** HANDLE MODES/STATES ****************/

  if(currentState == SET_MODE) {
    if(currentInputState == ROTATE_UP) {
      modeSelector ++;
      if(modeSelector >= 12) modeSelector = 0;
      indicateMode(modeSelector);
    } else if(currentInputState == ROTATE_DOWN) {
      modeSelector --;
      if(modeSelector < 0) modeSelector = 11;
      indicateMode(modeSelector);
    } else if(currentInputState == TAP_RELEASE) {
      // Selection finished
      changeModeByIndex(modeSelector);
    }
  } else if(currentState == READY) {

    if(currentMode == BOOTUP) {
      /**** BOOTUP ***/

      sevseg.setChars(DISPLAY_WELCOME_MESSAGE);
      if(millis() > TIME(2000)) {
        changeModeByIndex(modeSelector);
      }
    } else if(currentMode == RANDOM) {
      if(currentInputState == TAP_RELEASE) {
        randomizingState = !randomizingState;
      }
      if(randomizingState) {
        sevseg.setNumber(random(1000,MAX_DISP_NUM));
      }


    } else if(currentMode == TEST) {
      /**** TEST COUNTER ***/

      if(currentInputState == ROTATE_UP) {
        selector++;
        if(selector > 3) selector = 3;
      } else if(currentInputState == ROTATE_DOWN) {
        selector--;
        if(selector < 0) selector = 0;
      }

      int changePin;
      switch (selector)
      {
      case 0:
        sevseg.setChars("o   ");
        changePin = PIN_REL_HEAT_TOP;
        break;
      case 1:
        sevseg.setChars(" o  ");
        changePin = PIN_REL_HEAT_BOT;
        break;
      case 2:
        sevseg.setChars("  o ");
        changePin = PIN_REL_FAN;
        break;
      case 3:
        sevseg.setChars("   o");
        changePin = PIN_REL_LIGHT;
        break;
      }

      if(currentInputState == TAP_RELEASE) {
        sevseg.blank();
        sevseg.refreshDisplay();
        delay(100);
        digitalWrite(changePin, !digitalRead(changePin));
      }


    } else if(currentMode == COUNTER) {
      /**** COUNTER (WIP) ***/

      if(currentInputState == TAP_RELEASE) {
        counter++;
      }
      sevseg.setNumber(counter);


    } else if(currentMode == TONE) {
      /**** TONE ***/

      if(currentInputState == ROTATE_UP) {
        toneFreq++;
        if(toneFreq > MAX_DISP_NUM) toneFreq = MAX_DISP_NUM;
        if(playTone) {
          tone(PIN_BUZZER, toneFreq * CLOCK_MULT);
        }
      } else if(currentInputState == ROTATE_DOWN) {
        toneFreq--;
        if(toneFreq < 1) toneFreq = 1;
        if(playTone) {
          tone(PIN_BUZZER, toneFreq * CLOCK_MULT);
        }
      } else if(currentInputState == TAP_RELEASE) {
        playTone = !playTone;
        if(playTone) {
          tone(PIN_BUZZER, toneFreq * CLOCK_MULT);
        } else {
          noTone(PIN_BUZZER);
        }
      }

      sevseg.setNumber(toneFreq);
      if(playTone) {
        #ifdef SLEEP_AFTER_MS
        // Keep from sleeping while timer is going
        // @TODO add timer feature for watchdog/wake up when timer done
        lastActive = millis();
        #endif
        sevseg.setPeriod(3);
      } else {
        sevseg.setPeriod(3, false);
      }

    } else if(currentMode == METRONOME) {
      /**** METRONOME ***/

      if(currentInputState == ROTATE_UP) {
        metroBpm++;
        if(metroBpm > MAX_DISP_NUM) metroBpm = MAX_DISP_NUM;
        metroBpms = TIME(60000/metroBpm);
        metroMeterMs = metroBpms * 4;
        if(metroActive) {
          metroOffset = millis()- ((millis() - metroOffset) % metroMeterMs);
        }
      } else if(currentInputState == ROTATE_DOWN) {
        metroBpm--;
        if(metroBpm < 1) metroBpm = 1;
        metroBpms = TIME(60000/metroBpm);
        metroMeterMs = metroBpms * 4;
        if(metroActive) {
          metroOffset = millis()- ((millis() - metroOffset) % metroMeterMs);
        }
      } else if(currentInputState == TAP_RELEASE) {
        metroActive = !metroActive;
        if(metroActive) {
          metroOffset = millis();
        } else {
          sevseg.setPeriod(0, false);
          sevseg.setPeriod(1, false);
          sevseg.setPeriod(2, false);
          sevseg.setPeriod(3, false);
        }
      }

      sevseg.setNumber(metroBpm);

      if(metroActive) {

        #ifdef SLEEP_AFTER_MS
        // Keep from sleeping while timer is going
        // @TODO add timer feature for watchdog/wake up when timer done
        lastActive = millis();
        #endif
        unsigned int barPos = (millis() - metroOffset) % metroMeterMs;
        unsigned int beatPos = barPos % metroBpms;
        unsigned int beatNum = floor(barPos / metroBpms);

        if(beatPos < METRO_TONE_LENGTH) {
          if(beatNum == 0) {
            tone(PIN_BUZZER, FREQ(880), METRO_TONE_LENGTH);
          } else {
            tone(PIN_BUZZER, FREQ(440), METRO_TONE_LENGTH);
          }
        }

        if(beatPos < metroBpms / 2) {
          sevseg.setPeriod(beatNum, true);
        }
      }


    } else if(currentMode == TIMER) {
      /**** TIMER ***/

      if(currentInputState == ROTATE_UP) {
        if(!timerActive && timerDuration < MAX_DISP_NUM) {
          timerDuration++;
        }
      } else if(currentInputState == ROTATE_DOWN) {
        if(!timerActive && timerDuration > 1) {
          timerDuration--;
        }
      } else if(currentInputState == TAP_RELEASE) {
        timerActive = !timerActive;
        if(timerActive) {
          tone(PIN_BUZZER, FREQ(440), 10);
          timerStartTime = millis();
          timerDurationms = TIME(timerDuration * 1000);
        }
      }
      if(timerActive) {
        if(((unsigned long)(millis() - timerStartTime)) >= timerDurationms) {
          timerActive = false;
          tone(PIN_BUZZER, FREQ(2000), TIME(2000));
        } else {

          #ifdef SLEEP_AFTER_MS
          // Keep from sleeping while timer is going
          // @TODO add timer feature for watchdog/wake up when timer done
          lastActive = millis();
          #endif

          int timeLeft = ceil((timerDurationms - (unsigned long)(millis() - timerStartTime)) * CLOCK_MULT / 1000.0);
          sevseg.setNumber(timeLeft);
        }
      } else {
        sevseg.setNumber(timerDuration);
      }


    } else if(currentMode == UNKNOWN) {
      /**** UNKNOWN ***/

      sevseg.setChars("----");
    }
  }

  sevseg.refreshDisplay(); // Must run repeatedly
}
