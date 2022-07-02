#include "Encoder.h"
#include "SevSeg.h"
#include <SPI.h>
#include "Adafruit_MAX31855.h"

#define THERM_TOP_SPI_CS A4
#define THERM_BOT_SPI_CS A5

// initialize the Thermocouple
Adafruit_MAX31855 thermTop(THERM_TOP_SPI_CS);
Adafruit_MAX31855 thermBot(THERM_BOT_SPI_CS);

#define PIN_RESET 12

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

#define DISPLAY_WELCOME_MESSAGE "HELO"
#define MAX_DISP_NUM 9999

#define CANCEL_HOLD_TIME 3000

#define ENCODER_DEBOUNCE_DELAY_MS 10    // the debounce time; increase if the output flickers
#define ENCODER_ROTATE_MULT 4    // the debounce time; increase if the output flickers
unsigned long lastDebounceRotaryTime = 0;  // the last time the output pin was toggled
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long buttonStartPress = 0;
int lastButtonState = HIGH;

unsigned long redbuttonStartPress = 0;
int redlastButtonState = HIGH;

// Temp stuff
float destTempTop = 0;
float destTempBot = 0;
float currentTempTop = 0;
float currentTempBot = 0;


Encoder myEnc(PIN_ENCODER_A, PIN_ENCODER_B);

enum mode {
  NONE,
  TIMER,
  RANDOM,
  TONE,
  COUNTER,
  TEST,
  TEMP,
  UNKNOWN
};

enum programState {
  CHOOSE_MODE,
  MODE_ACTIVE,
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
  HOLDING_CANCEL,
};

inputState currentInputState = IDLE;

int modeSelector = COUNTER;
#define TOTAL_MODES 12
mode currentMode = NONE;
programState currentState = MODE_ACTIVE; // This is so we start on NONE (bootup)

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
unsigned long timerDurationms = timerDuration * 1000;
unsigned long timerStartTime = 0;

// TONE
bool playTone = false;
unsigned int toneFreq = 440;

#include "SevSeg.h"
SevSeg sevseg; //Instantiate a seven segment controller object


uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)r << 16) | ((uint32_t)g <<  8) | b;
}

void setup() {
  digitalWrite(PIN_RESET, HIGH);
  pinMode(PIN_RESET, OUTPUT);
  pinMode(PIN_REDBUTTON, INPUT_PULLUP);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_NEOSEG, OUTPUT);
  pinMode(PIN_REL_HEAT_TOP, OUTPUT);
  pinMode(PIN_REL_HEAT_BOT, OUTPUT); 
  pinMode(PIN_REL_FAN, OUTPUT);
  pinMode(PIN_REL_LIGHT, OUTPUT);
  pinMode(PIN_ENCODER_A, INPUT_PULLUP);
  pinMode(PIN_ENCODER_B, INPUT_PULLUP);
  pinMode(PIN_ENCBUTTON, INPUT);

  sevseg.begin(4, PIN_NEOSEG, false, false, true);
  sevseg.setBrightness(4);
  sevseg.blank();  
  sevseg.setBkgColor(Color(0,0,0));
  sevseg.setColor(Color(255,28,0));

  if (!thermTop.begin() || !thermBot.begin()) {
    sevseg.setColor(Color(255,0,0));
    sevseg.setChars("Err ");
    sevseg.refreshDisplay();
    while (1) {
      delay(10);
    }
  }
}

void changeMode(mode newMode) {
  switch (newMode)
  {
    case RANDOM:
      randomizingState = true;
      break;
    case TONE:
      playTone = false;
      break;
    case TEMP:
      selector = 0;
      break;
  }

  currentMode = newMode;
  tone(PIN_BUZZER, 220, 10);
  currentState = MODE_ACTIVE;
  sevseg.setColor(Color(16,255,0));
  sevseg.blank();
}

void exitMode() {
  // Turn everything off
  digitalWrite(PIN_REL_HEAT_BOT, LOW);
  digitalWrite(PIN_REL_HEAT_TOP, LOW);
  digitalWrite(PIN_REL_LIGHT, LOW);
  digitalWrite(PIN_REL_FAN, LOW);
  currentMode = NONE;
  tone(PIN_BUZZER, 220, 10);
  currentState = CHOOSE_MODE;
  sevseg.setColor(Color(255,28,0));
  sevseg.blank();
}

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
    case TEST:
      sevseg.setChars("TEST");
      break;
    case TEMP:
      sevseg.setChars("TEMP");
      break;
    
  default:
    sevseg.setNumber(selectedMode);
    break;
  }

}

void loop() {
  // Temp reading stuff
  currentTempTop = thermTop.readCelsius();
  currentTempBot = thermBot.readCelsius();

  // UI stuff
  inputState lastInputState = currentInputState;
  currentInputState = IDLE;
  int rotateDirection = 0;
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

  int redbuttonChange = 0;
  int redbuttonState = digitalRead(PIN_REDBUTTON);
  if(redbuttonState != redlastButtonState) {
    redlastButtonState = redbuttonState;
    if(redbuttonState == BUTTON_PRESSED) {
      redbuttonStartPress = millis();
      redbuttonChange = BUTTON_JUST_PRESSED;
    } else if (redbuttonState == BUTTON_RELEASED) {
      redbuttonChange = BUTTON_JUST_RELEASED;
    }
  }

  /********** DETERMINE INPUT STATE *****************/
  if(buttonState == BUTTON_RELEASED) {
    if(rotateDirection == 1) {
      currentInputState = ROTATE_UP;
    }
    if(rotateDirection == -1) {
      currentInputState = ROTATE_DOWN;
    }
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

  if(redbuttonState == BUTTON_PRESSED) {
    currentInputState = HOLDING_CANCEL;
  }

  /************* DONE WITH INPUT **************/



  /************** HANDLE MODES/STATES ****************/

  if(currentState == CHOOSE_MODE) {
    if(currentInputState == ROTATE_UP) {
      modeSelector ++;
      if(modeSelector >= 12) modeSelector = 0;
      indicateMode(static_cast<mode>(modeSelector));
    } else if(currentInputState == ROTATE_DOWN) {
      modeSelector --;
      if(modeSelector < 0) modeSelector = 11;
      indicateMode(static_cast<mode>(modeSelector));
    } else if(currentInputState == TAP_RELEASE) {
      // Selection finished
      changeMode(static_cast<mode>(modeSelector));
    }
  } else if(currentState == MODE_ACTIVE) {

    if(currentMode == NONE) {
      /**** NONE ***/

      sevseg.setChars(DISPLAY_WELCOME_MESSAGE);
      if(millis() > 2000) {
        exitMode();
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
        sevseg.setColor(Color(255,0,0));
        break;
      case 1:
        sevseg.setChars(" o  ");
        changePin = PIN_REL_HEAT_BOT;
        sevseg.setColor(Color(255,0,0));
        break;
      case 2:
        sevseg.setChars("  o ");
        changePin = PIN_REL_LIGHT;
        sevseg.setColor(Color(128,128,0));
        break;
      case 3:
        sevseg.setChars("   o");
        changePin = PIN_REL_FAN;
        sevseg.setColor(Color(128,128,0));
        break;
      }

      if(currentInputState == TAP_RELEASE) {
        digitalWrite(changePin, !digitalRead(changePin));
      }


    } else if(currentMode == TEMP) {
      /**** SHOW TEMPs ***/

      if(currentInputState == ROTATE_UP) {
        selector++;
        if(selector > 1) selector = 1;
      } else if(currentInputState == ROTATE_DOWN) {
        selector--;
        if(selector < 0) selector = 0;
      }

      if(selector == 0) {
        // TOP
        sevseg.setColor(Color(0,0,128));
        sevseg.setNumber(round(currentTempTop));
      } else {
        // BOTTOM
        sevseg.setColor(Color(0,64,128));
        sevseg.setNumber(round(currentTempBot));
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
          tone(PIN_BUZZER, toneFreq);
        }
      } else if(currentInputState == ROTATE_DOWN) {
        toneFreq--;
        if(toneFreq < 1) toneFreq = 1;
        if(playTone) {
          tone(PIN_BUZZER, toneFreq);
        }
      } else if(currentInputState == TAP_RELEASE) {
        playTone = !playTone;
        if(playTone) {
          tone(PIN_BUZZER, toneFreq);
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
          tone(PIN_BUZZER, 440, 10);
          timerStartTime = millis();
          timerDurationms = timerDuration * 1000;
        }
      }
      if(timerActive) {
        if(((unsigned long)(millis() - timerStartTime)) >= timerDurationms) {
          timerActive = false;
          tone(PIN_BUZZER, 2000, 2000);
        } else {

          #ifdef SLEEP_AFTER_MS
          // Keep from sleeping while timer is going
          // @TODO add timer feature for watchdog/wake up when timer done
          lastActive = millis();
          #endif

          int timeLeft = ceil((timerDurationms - (unsigned long)(millis() - timerStartTime)));
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

  // This is last priority
  // This allows for the above mode actions to carry out
  // But we can still override the display here 
  // @TODO - maybe wire up reset pin and trigger that instead
  if(currentInputState == HOLDING_CANCEL) {
    sevseg.setChars("---");
    if(millis() - redbuttonStartPress > CANCEL_HOLD_TIME) {
      exitMode();
      tone(PIN_BUZZER, 800, 1000);
      delay(500);
      digitalWrite(PIN_RESET, LOW);
    }
  }

  sevseg.refreshDisplay(); // Must run repeatedly
}
