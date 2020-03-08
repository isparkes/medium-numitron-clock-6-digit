#include "OutputManagerNumi6.h"

// ************************************************************
// Set up the manager
// ************************************************************
void OutputManager::setUp() {
  pinMode(LATCHPin, OUTPUT);
  pinMode(CLOCKPin, OUTPUT);
  pinMode(DATAPin, OUTPUT);
  pinMode(BLANKPin, OUTPUT);  

  analogWrite(BLANKPin, 0);  
}

// ************************************************************
// Do a single complete display
// ************************************************************
void OutputManager::outputDisplay() {
  byte digits[6];
  static byte prevNumberArray[6];
  static bool smoothRunning[6];

  // Deal with blink, calculate if we are on or off
  static uint16_t blinkCounter = BLINK_COUNT_OFF;
  blinkCounter--;
  if (blinkCounter <= 0) {
    _blinkState = !_blinkState;
    blinkCounter = _blinkState ? BLINK_COUNT_ON : BLINK_COUNT_OFF;
  }

  // Decode numbers to 7-segment digits
  for (int i = 0; i <= 5; i++) digits[i] = digitData[numberArray[i]];

  blankDigits(digits, prevNumberArray, smoothRunning);
  smoothDigits(digits, prevNumberArray, smoothRunning);

  shiftDigitsOut(digits);
  setBlankingPin();
}

// ************************************************************
// Set the 48 output bits with the given value for the current
// display. "Current" display is what we are fading from, or
// the stable display when we are not cross-fading.
// ************************************************************
void OutputManager::shiftDigitsOut(byte *displayDigits) {
  static unsigned long lastOut1 = 0;
  static unsigned long lastOut2 = 0;

  // Add in the orange separator "colon" LEDs
  displayDigits[2] |= SEG_IND * led1State;
  displayDigits[4] |= SEG_IND * led1State;
  displayDigits[1] |= SEG_IND * led2State;
  displayDigits[3] |= SEG_IND * led2State;

  // Add in the red indicator LEDs
  displayDigits[0] |= SEG_IND * ledLState;
  displayDigits[5] |= SEG_IND * ledRState;

  // Mix in the digits
  unsigned long out1 = displayDigits[5] | (displayDigits[4] << 8) | (displayDigits[3] << 16);
  unsigned long out2 = displayDigits[2] | (displayDigits[1] << 8) | (displayDigits[0] << 16);
  

  // Digit pre-heat
  if (cc->preheatStrength < PREHEAT_OFF) {
    _preheatCounter++;
    if (_preheatCounter > cc->preheatStrength) {
      _preheatCounter = 0;
      out1 |= 0x7f7f7f;
      out2 |= 0x7f7f7f;
    }
  }
  
  // If nothing has changed don't bother updating tubes - it would be redundant
  if (out1 != lastOut1 || out2 != lastOut2) {
    lastOut1 = out1;
    lastOut2 = out2;
    digitsOut(out1, out2);
  }
}

// ************************************************************
// Set the 48 output bits on the shift registers without trying
// to interpret them as numbers. This is used when displaying
// non-numerical values.
// ************************************************************
void OutputManager::outputDisplayDiags() {
  // Mix in the digits
  unsigned long out1 = numberArray[5] | (numberArray[4] << 8) | (numberArray[3] << 16);
  unsigned long out2 = numberArray[2] | (numberArray[1] << 8) | (numberArray[0] << 16);
  digitsOut(out1, out2);
}

// ************************************************************
// Send the 48 output bits to the TLC5916 latches for display
// ************************************************************
void OutputManager::digitsOut(unsigned long highBits, unsigned long lowBits) {
  digitalWrite(LATCHPin, LOW);
  shiftOut24(DATAPin, CLOCKPin, MSBFIRST, lowBits);
  shiftOut24(DATAPin, CLOCKPin, MSBFIRST, highBits);
  digitalWrite(LATCHPin, HIGH);
}

// ************************************************************
// Perform the parallel shift out to the registers
// ************************************************************
void OutputManager::shiftOut24(uint8_t _dataPin1,
                uint8_t _clockPin,
                uint8_t _bitOrder,
                uint32_t _val1) {
  uint8_t i;

  for (i = 0; i < 24; i++) {
    if (_bitOrder == LSBFIRST) {
      digitalWrite(_dataPin1, !!(_val1 & (1 << i)));
    }
    else {
      digitalWrite(_dataPin1, !!(_val1 & (1 << (23 - i))));
    }
    digitalWrite(_clockPin, HIGH);
    digitalWrite(_clockPin, LOW);
  }
}
  
// ************************************************************
// Blanking and Blinking of individual digits
// ************************************************************
void OutputManager::blankDigits(byte *digits, byte *prevNums, bool *smoothRun) {
  static bool digitWasBlanked[6];
  for (int i = 0; i <= 5; i++) {
    if (displayType[i] == BLANKED || (displayType[i] == BLINK && !_blinkState)) {
      digits[i] = DIGIT_BLANK;
      smoothRun[i] = false;
      prevNums[i] = numberArray[i];
      digitWasBlanked[i] = true;
    }
    else if (digitWasBlanked[i]) {      // Inhibits smoothing when digit goes
      prevNums[i] = numberArray[i];     // from BLANKED to NORMAL because that
      digitWasBlanked[i] = false;       // would not look good.
    }
  }
}

// ************************************************************
// Smoothing of changing digits
// ************************************************************
void OutputManager::smoothDigits(byte *digits, byte *prevNums, bool *smoothRun) {
  static unsigned long smoothStartMillis[6];
  ledManager.setSyncColourTime(false);
  for (int i = 0; i <= 5; i++) {
    if (numberArray[i] != prevNums[i]) {
      digits[i] |= digitData[prevNums[i]];
      if (!smoothRun[i]) {
        smoothRun[i] = true;
        smoothStartMillis[i] = millis();
      }
      else {
        if (millis() - smoothStartMillis[i] >= _tubeLag) {
          prevNums[i] = numberArray[i];
          smoothRun[i] = false;
        }
      }
      ledManager.setSyncColourTime(true);
    }
  }
}

// ************************************************************
// set the PWM Brighness
// ************************************************************
void OutputManager::setLDRValue(unsigned int newBrightness) {
  if (_ldrValue != newBrightness) {
    _ldrValue = newBrightness;
    _tubeLag = (.000655 * pow(newBrightness, 2) + -.01159 * newBrightness + 148) / (6 - current_config.preheatStrength);
  }
}

// ************************************************************
// Set the blanking pin to show the display. If we have gone
// into display blanking mode, slowly fade out the display to
// completely off.
// ************************************************************
void OutputManager::setBlankingPin() {
  if (!blankTubes) {
    _lastLDRValue = _ldrValue;
  } else {
    // Fade out slowly
    if (_lastLDRValue < 1023) {
      _lastLDRValue++;
    }
  }

  analogWrite(BLANKPin, _lastLDRValue);
}
