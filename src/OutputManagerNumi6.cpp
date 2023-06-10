#include "OutputManagerNumi6.h"
#include "TimeLib.h"
#include "ClockUtils.h"

// ************************************************************
// Instance value
// ************************************************************
OutputManager* OutputManager::pInstance;

// ************************************************************
// Singleton accessor
// ************************************************************
OutputManager& OutputManager::Instance(){
  return *OutputManager::pInstance;
}

// ************************************************************
// Singleton creator
// ************************************************************
void OutputManager::CreateInstance(){
  if (OutputManager::pInstance == nullptr) {
    OutputManager::pInstance = new OutputManager();
  }
}

// ************************************************************
// Set up the manager
// ************************************************************
void OutputManager::setUp() {
  pinMode(LATCHPin, OUTPUT);
  pinMode(CLOCKPin, OUTPUT);
  pinMode(DATAPin, OUTPUT);
  pinMode(BLANKPin, OUTPUT);  

  analogWriteRange(1023);
  analogWrite(BLANKPin, 0);
}

// ************************************************************
// Set up the manager
// ************************************************************
void OutputManager::setConfigObject(spiffs_config_t* ccPtr) {
  cc = ccPtr;
}

// ************************************************************
// Do a single complete display
// ************************************************************
void OutputManager::outputDisplay() {
  byte digits[DIGIT_COUNT];
  static byte prevNumberArray[DIGIT_COUNT];
  static bool smoothRunning[DIGIT_COUNT];

  // Deal with blink, calculate if we are on or off
  _blinkCounter--;
  if (_blinkCounter <= 0) {
    _blinkState = !_blinkState;
    _blinkCounter = _blinkState ? BLINK_COUNT_ON : BLINK_COUNT_OFF;
  }

  // Decode numbers to 7-segment digits
  for (int i = 0; i < DIGIT_COUNT; i++) digits[i] = digitData[_digit_buffer.numberArray[i]];

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
  if (cc->separatorDimFactor == SEP_DIM) {
    _separatorDim++;
    if (_separatorDim & 0x01) {
      // Add in the orange separator "colon" LEDs
      displayDigits[2] |= SEG_IND * led1State;
      displayDigits[1] |= SEG_IND * led2State;
      displayDigits[3] |= SEG_IND * led2State;
      displayDigits[4] |= SEG_IND * led1State;
      
      // Add in the red indicator LEDs
      displayDigits[0] |= SEG_IND * ledLState;
      displayDigits[5] |= SEG_IND * ledRState;
    } else {
      if (cc->doNotDimIndLEDs) {
        // Add in the red indicator LEDs back in - don't dim
        displayDigits[0] |= SEG_IND * ledLState;
        displayDigits[3] |= SEG_IND * ledRState;
      }
    }
  } else {
    // Add in the orange separator "colon" LEDs
    displayDigits[2] |= SEG_IND * led1State;
    displayDigits[1] |= SEG_IND * led2State;
    displayDigits[3] |= SEG_IND * led2State;
    displayDigits[4] |= SEG_IND * led1State;
    
    // Add in the red indicator LEDs
    displayDigits[0] |= SEG_IND * ledLState;
    displayDigits[5] |= SEG_IND * ledRState;
  }

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
  
  digitsOut(out1, out2);
}

// ************************************************************
// Set the 48 output bits on the shift registers without trying
// to interpret them as numbers. This is used when displaying
// non-numerical values.
// ************************************************************
void OutputManager::outputDisplayDiags() {
  // Mix in the digits
  unsigned long out1 = _digit_buffer.numberArray[5] | (_digit_buffer.numberArray[4] << 8) | (_digit_buffer.numberArray[3] << 16);
  unsigned long out2 = _digit_buffer.numberArray[2] | (_digit_buffer.numberArray[1] << 8) | (_digit_buffer.numberArray[0] << 16);
  digitsOut(out1, out2);
}

// ************************************************************
// Send the 48 output bits to the TLC5916 latches for display
// ************************************************************
void OutputManager::digitsOut(unsigned long highBits, unsigned long lowBits) {
  digitalWrite(LATCHPin, LOW);
  shiftOut24(DATAPin, CLOCKPin, lowBits);
  shiftOut24(DATAPin, CLOCKPin, highBits);
  digitalWrite(LATCHPin, HIGH);
}

// ************************************************************
// Perform the parallel shift out to the registers
// ************************************************************
void OutputManager::shiftOut24(uint8_t _dataPin1,
                uint8_t _clockPin,
                uint32_t _val1) {
  uint8_t i;

  for (i = 0; i < 24; i++) {
    digitalWrite(_dataPin1, !!(_val1 & (1 << (23 - i))));
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
    if (_digit_buffer.displayType[i] == BLANKED || (_digit_buffer.displayType[i] == BLINK && !_blinkState)) {
      digits[i] = DIGIT_BLANK;
      smoothRun[i] = false;
      prevNums[i] = _digit_buffer.numberArray[i];
      digitWasBlanked[i] = true;
    }
    else if (digitWasBlanked[i]) {                  // Inhibits smoothing when digit goes
      prevNums[i] = _digit_buffer.numberArray[i];   // from BLANKED to NORMAL because that
      digitWasBlanked[i] = false;                   // would not look good.
    }
  }
}

// ************************************************************
// Smoothing of changing digits
// ************************************************************
void OutputManager::smoothDigits(byte *digits, byte *prevNums, bool *smoothRun) {
  static unsigned long smoothStartMillis[DIGIT_COUNT];
  ledManager.setSyncColourTime(false);
  for (int i = 0; i < DIGIT_COUNT; i++) {
    if (_digit_buffer.numberArray[i] != prevNums[i]) {
      digits[i] |= digitData[prevNums[i]];
      if (!smoothRun[i]) {
        smoothRun[i] = true;
        smoothStartMillis[i] = millis();
      }
      else {
        if (millis() - smoothStartMillis[i] >= _tubeLag) {
          prevNums[i] = _digit_buffer.numberArray[i];
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
void OutputManager::setLDRValue(int newBrightness) {
  if (_ldrValue != (1023 - newBrightness)) {
    _ldrValue = (1023 - newBrightness);
    _tubeLag = (.000655 * pow(newBrightness, 2) + -.01159 * newBrightness + 148) / (6 - cc->preheatStrength);
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

//**********************************************************************************
//**********************************************************************************
//*                             Utility functions                                  *
//**********************************************************************************
//**********************************************************************************

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void OutputManager::loadNumberArrayTime() {
  _digit_buffer.numberArray[5] = second() % 10;
  _digit_buffer.numberArray[4] = second() / 10;
  _digit_buffer.numberArray[3] = minute() % 10;
  _digit_buffer.numberArray[2] = minute() / 10;
  if (cc->hourMode) {
    _digit_buffer.numberArray[1] = hourFormat12() % 10;
    _digit_buffer.numberArray[0] = hourFormat12() / 10;
  } else {
    _digit_buffer.numberArray[1] = hour() % 10;
    _digit_buffer.numberArray[0] = hour() / 10;
  }
}

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void OutputManager::loadNumberArraySameValue(byte val) {
  _digit_buffer.numberArray[5] = val;
  _digit_buffer.numberArray[4] = val;
  _digit_buffer.numberArray[3] = val;
  _digit_buffer.numberArray[2] = val;
  _digit_buffer.numberArray[1] = val;
  _digit_buffer.numberArray[0] = val;
}

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void OutputManager::loadNumberArrayPOSTMessage(byte val) {
  switch (val) {
    case DIAGS_START:
      {
        _digit_buffer.numberArray[0] = LETTER_S;
        _digit_buffer.numberArray[1] = LETTER_t;
        _digit_buffer.numberArray[2] = LETTER_A;
        _digit_buffer.numberArray[3] = LETTER_r;
        _digit_buffer.numberArray[4] = LETTER_t;
        _digit_buffer.numberArray[5] = LETTER__;
        break;
      }
    case DIAGS_SPIFFS:
      {
        _digit_buffer.numberArray[0] = LETTER_S;
        _digit_buffer.numberArray[1] = LETTER_P;
        _digit_buffer.numberArray[2] = LETTER_I;
        _digit_buffer.numberArray[3] = LETTER_F;
        _digit_buffer.numberArray[4] = LETTER_F;
        _digit_buffer.numberArray[5] = LETTER_S;
        break;
      }
    case DIAGS_WIFI:
      {
        _digit_buffer.numberArray[0] = LETTER_n;
        _digit_buffer.numberArray[1] = LETTER_E;
        _digit_buffer.numberArray[2] = LETTER_t;
        _digit_buffer.numberArray[3] = LETTER__;
        _digit_buffer.numberArray[4] = LETTER_U;
        _digit_buffer.numberArray[5] = LETTER_P;
        break;
      }
    case DIAGS_NTP:
      {
        _digit_buffer.numberArray[0] = LETTER_n;
        _digit_buffer.numberArray[1] = LETTER_t;
        _digit_buffer.numberArray[2] = LETTER_P;
        _digit_buffer.numberArray[3] = LETTER__;
        _digit_buffer.numberArray[4] = LETTER_U;
        _digit_buffer.numberArray[5] = LETTER_P;
        break;
      }
    case DIAGS_RTC:
      {
        _digit_buffer.numberArray[0] = LETTER_r;
        _digit_buffer.numberArray[1] = LETTER_t;
        _digit_buffer.numberArray[2] = LETTER_C;
        _digit_buffer.numberArray[3] = LETTER__;
        _digit_buffer.numberArray[4] = LETTER_U;
        _digit_buffer.numberArray[5] = LETTER_P;
        break;
      }
    case DIAGS_DEBUG:
      {
        _digit_buffer.numberArray[0] = LETTER_A;
        _digit_buffer.numberArray[1] = LETTER_L;
        _digit_buffer.numberArray[2] = LETTER_L;
        _digit_buffer.numberArray[3] = LETTER__;
        _digit_buffer.numberArray[4] = LETTER_O;
        _digit_buffer.numberArray[5] = LETTER_n;
        break;
      }
    case AP_MODE:
      {
        _digit_buffer.numberArray[0] = LETTER_A;
        _digit_buffer.numberArray[1] = LETTER_P;
        _digit_buffer.numberArray[2] = LETTER__;
        _digit_buffer.numberArray[3] = LETTER__;
        _digit_buffer.numberArray[4] = LETTER_O;
        _digit_buffer.numberArray[5] = LETTER_n;
        break;
      }
  }
}

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void OutputManager::loadNumberArrayDate() {
  switch (cc->dateFormat) {
    case DATE_FORMAT_YYMMDD:
      _digit_buffer.numberArray[5] = day() % 10;
      _digit_buffer.numberArray[4] = day() / 10;
      _digit_buffer.numberArray[3] = month() % 10;
      _digit_buffer.numberArray[2] = month() / 10;
      _digit_buffer.numberArray[1] = (year() - 2000) % 10;
      _digit_buffer.numberArray[0] = (year() - 2000) / 10;
      break;
    case DATE_FORMAT_MMDDYY:
      _digit_buffer.numberArray[5] = (year() - 2000) % 10;
      _digit_buffer.numberArray[4] = (year() - 2000) / 10;
      _digit_buffer.numberArray[3] = day() % 10;
      _digit_buffer.numberArray[2] = day() / 10;
      _digit_buffer.numberArray[1] = month() % 10;
      _digit_buffer.numberArray[0] = month() / 10;
      break;
    case DATE_FORMAT_DDMMYY:
      _digit_buffer.numberArray[5] = (year() - 2000) % 10;
      _digit_buffer.numberArray[4] = (year() - 2000) / 10;
      _digit_buffer.numberArray[3] = month() % 10;
      _digit_buffer.numberArray[2] = month() / 10;
      _digit_buffer.numberArray[1] = day() % 10;
      _digit_buffer.numberArray[0] = day() / 10;
      break;
  }
}

// ************************************************************
// Break the LDR reading into displayable digits
// ************************************************************
void OutputManager::loadNumberArrayLDR() {
  _digit_buffer.numberArray[5] = 0;
  _digit_buffer.numberArray[4] = 0;

  int ldrValueDisp = 1023 - _ldrValue;

  _digit_buffer.numberArray[3] = (ldrValueDisp / 1) % 10;
  _digit_buffer.numberArray[2] = (ldrValueDisp / 10) % 10;
  _digit_buffer.numberArray[1] = (ldrValueDisp / 100) % 10;
  _digit_buffer.numberArray[0] = (ldrValueDisp / 1000) % 10;
}

// ************************************************************
// Test digits
// ************************************************************
void OutputManager::loadNumberArrayTestDigits() {
  _digit_buffer.numberArray[5] =  second() % 10;
  _digit_buffer.numberArray[4] = (second() + 1) % 10;
  _digit_buffer.numberArray[3] = (second() + 2) % 10;
  _digit_buffer.numberArray[2] = (second() + 3) % 10;
  _digit_buffer.numberArray[1] = (second() + 4) % 10;
  _digit_buffer.numberArray[0] = (second() + 5) % 10;
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void OutputManager::loadNumberArrayConfInt(int confValue, int confNum) {
  _digit_buffer.numberArray[5] = (confNum) % 10;
  _digit_buffer.numberArray[4] = (confNum / 10) % 10;
  _digit_buffer.numberArray[3] = (confValue / 1) % 10;
  _digit_buffer.numberArray[2] = (confValue / 10) % 10;
  _digit_buffer.numberArray[1] = (confValue / 100) % 10;
  _digit_buffer.numberArray[0] = (confValue / 1000) % 10;
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void OutputManager::loadNumberArrayConfIntWide(int confValue) {
  _digit_buffer.numberArray[5] = (confValue / 1) % 10;
  _digit_buffer.numberArray[4] = (confValue / 10) % 10;
  _digit_buffer.numberArray[3] = (confValue / 100) % 10;
  _digit_buffer.numberArray[2] = (confValue / 1000) % 10;
  _digit_buffer.numberArray[1] = (confValue / 10000) % 10;
  _digit_buffer.numberArray[0] = (confValue / 100000) % 10;
}

// ************************************************************
// Show a boolean configuration value
// ************************************************************
void OutputManager::loadNumberArrayConfBool(boolean confValue, int confNum) {
  int boolInt;
  if (confValue) {
    boolInt = 1;
  } else {
    boolInt = 0;
  }
  _digit_buffer.numberArray[5] = (confNum) % 10;
  _digit_buffer.numberArray[4] = (confNum / 10) % 10;
  _digit_buffer.numberArray[3] = boolInt;
  _digit_buffer.numberArray[2] = 0;
  _digit_buffer.numberArray[1] = 0;
  _digit_buffer.numberArray[0] = 0;
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void OutputManager::loadNumberArrayIP(byte byte1, byte byte2) {
  _digit_buffer.numberArray[5] = (byte2) % 10;
  _digit_buffer.numberArray[4] = (byte2 / 10) % 10;
  _digit_buffer.numberArray[3] = (byte2 / 100) % 10;
  _digit_buffer.numberArray[2] = (byte1) % 10;
  _digit_buffer.numberArray[1] = (byte1 / 10) % 10;
  _digit_buffer.numberArray[0] = (byte1 / 100) % 10;
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void OutputManager::loadNumberArrayESPID(String ID) {
  byte byteArray[6];
  hexCharacterStringToBytes(byteArray, ID.c_str());
  _digit_buffer.numberArray[5] = byteArray[5];
  _digit_buffer.numberArray[4] = byteArray[4];
  _digit_buffer.numberArray[3] = byteArray[3];
  _digit_buffer.numberArray[2] = byteArray[2];
  _digit_buffer.numberArray[1] = byteArray[1];
  _digit_buffer.numberArray[0] = byteArray[0];
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void OutputManager::loadNumberArrayValueToShow() {
  _digit_buffer.numberArray[5] = (_value_buffer.valueToShow / 1) % 10;
  _digit_buffer.numberArray[4] = (_value_buffer.valueToShow / 10) % 10;
  _digit_buffer.numberArray[3] = (_value_buffer.valueToShow / 100) % 10;
  _digit_buffer.numberArray[2] = (_value_buffer.valueToShow / 1000) % 10;
  _digit_buffer.numberArray[1] = (_value_buffer.valueToShow / 10000) % 10;
  _digit_buffer.numberArray[0] = (_value_buffer.valueToShow / 100000) % 10;
}

// ************************************************************
// highlight years taking into account the date format
// ************************************************************
void OutputManager::highlightYearsDateFormat() {
  switch (cc->dateFormat) {
    case DATE_FORMAT_YYMMDD:
      highlight0and1();
      break;
    case DATE_FORMAT_MMDDYY:
      highlight4and5();
      break;
    case DATE_FORMAT_DDMMYY:
      highlight4and5();
      break;
  }
}

// ************************************************************
// highlight years taking into account the date format
// ************************************************************
void OutputManager::highlightMonthsDateFormat() {
  switch (cc->dateFormat) {
    case DATE_FORMAT_YYMMDD:
      highlight2and3();
      break;
    case DATE_FORMAT_MMDDYY:
      highlight0and1();
      break;
    case DATE_FORMAT_DDMMYY:
      highlight2and3();
      break;
  }
}

// ************************************************************
// highlight days taking into account the date format
// ************************************************************
void OutputManager::highlightDaysDateFormat() {
  switch (cc->dateFormat) {
    case DATE_FORMAT_YYMMDD:
      highlight4and5();
      break;
    case DATE_FORMAT_MMDDYY:
      highlight2and3();
      break;
    case DATE_FORMAT_DDMMYY:
      highlight0and1();
      break;
  }
}

// ************************************************************
// Display preset, highlight digits 0 and 1
// ************************************************************
void OutputManager::highlight0and1() {
  _digit_buffer.displayType[0] = BLINK;
  _digit_buffer.displayType[1] = BLINK;
  _digit_buffer.displayType[2] = NORMAL;
  _digit_buffer.displayType[3] = NORMAL;
  _digit_buffer.displayType[4] = NORMAL;
  _digit_buffer.displayType[5] = NORMAL;
}

// ************************************************************
// Display preset, highlight digits 2 and 3
// ************************************************************
void OutputManager::highlight2and3() {
  _digit_buffer.displayType[0] = NORMAL;
  _digit_buffer.displayType[1] = NORMAL;
  _digit_buffer.displayType[2] = BLINK;
  _digit_buffer.displayType[3] = BLINK;
  _digit_buffer.displayType[4] = NORMAL;
  _digit_buffer.displayType[5] = NORMAL;
}

// ************************************************************
// Display preset, highlight digits 4 and 5
// ************************************************************
void OutputManager::highlight4and5() {
  _digit_buffer.displayType[0] = NORMAL;
  _digit_buffer.displayType[1] = NORMAL;
  _digit_buffer.displayType[2] = NORMAL;
  _digit_buffer.displayType[3] = NORMAL;
  _digit_buffer.displayType[4] = BLINK;
  _digit_buffer.displayType[5] = BLINK;
}

// ************************************************************
// Display preset
// ************************************************************
void OutputManager::allNormal(bool leadingBlank) {

  if (leadingBlank)
    applyBlanking();
  else 
    _digit_buffer.displayType[0] = NORMAL;

  _digit_buffer.displayType[1] = NORMAL;
  _digit_buffer.displayType[2] = NORMAL;
  _digit_buffer.displayType[3] = NORMAL;
  _digit_buffer.displayType[4] = NORMAL;
  _digit_buffer.displayType[5] = NORMAL;
}

// ************************************************************
// Display preset
// ************************************************************
void OutputManager::displayConfig() {
  _digit_buffer.displayType[0] = NORMAL;
  _digit_buffer.displayType[1] = NORMAL;
  _digit_buffer.displayType[2] = NORMAL;
  _digit_buffer.displayType[3] = NORMAL;
  _digit_buffer.displayType[4] = BLINK;
  _digit_buffer.displayType[5] = BLINK;
}

// ************************************************************
// Display preset
// ************************************************************
void OutputManager::allBlanked() {
  _digit_buffer.displayType[0] = BLANKED;
  _digit_buffer.displayType[1] = BLANKED;
  _digit_buffer.displayType[2] = BLANKED;
  _digit_buffer.displayType[3] = BLANKED;
  _digit_buffer.displayType[4] = BLANKED;
  _digit_buffer.displayType[5] = BLANKED;
}

// ************************************************************
// Display preset
// ************************************************************
void OutputManager::loadDisplaySetValueType() {
  _digit_buffer.displayType[5] = _value_buffer.valueDisplayType[5];
  _digit_buffer.displayType[4] = _value_buffer.valueDisplayType[4];
  _digit_buffer.displayType[3] = _value_buffer.valueDisplayType[3];
  _digit_buffer.displayType[2] = _value_buffer.valueDisplayType[2];
  _digit_buffer.displayType[1] = _value_buffer.valueDisplayType[1];
  _digit_buffer.displayType[0] = _value_buffer.valueDisplayType[0];
}

// ************************************************************
// Apply leading zero blanking
// ************************************************************
void OutputManager::applyBlanking() {

  // We only want to blank the hours tens digit
  if (cc->blankLeading && _digit_buffer.numberArray[0] == 0) {
    _digit_buffer.displayType[0] = BLANKED;
  }
  else {
    _digit_buffer.displayType[0] = NORMAL;
  }
}

// ************************************************************
// Set the time we are going to do the value display for
// ************************************************************
void OutputManager::setValueDisplayTime(int timeToDisplay) {
  _value_buffer.valueDisplayTime = timeToDisplay;
}

// ************************************************************
// Set the format to use for the value display
// ************************************************************
void OutputManager::setValueFormat(int newValueFormat) {
  for (int idx = 0 ; idx < DIGIT_COUNT ; idx++ ) {
    byte digitFormat = newValueFormat % 10;
    if (digitFormat > FORMAT_MAX) digitFormat = NORMAL;
    _value_buffer.valueDisplayType[DIGIT_COUNT - idx - 1] = digitFormat;
    newValueFormat = newValueFormat / 10;
  }
}

// ************************************************************
// Get the time we are going to do the value display for
// ************************************************************
int OutputManager::getValueDisplayTime() {
  return _value_buffer.valueDisplayTime;
}

// ************************************************************
// Decrement the time we are going to do the value display for
// ************************************************************
void OutputManager::decValueDisplayTime() {
  _value_buffer.valueDisplayTime--;
}

// ************************************************************
// Set the value to display
// ************************************************************
void OutputManager::setValueToShow(int newValue) {
  int maskVal = 10^DIGIT_COUNT;
  _value_buffer.valueToShow = newValue % maskVal;
}

// ************************************************************
// Get the digit at index idx
// ************************************************************
byte OutputManager::getNumberArrayIndexedValue(byte idx) {
  return _digit_buffer.numberArray[idx];
}

// ************************************************************
// Set the digit at index idx
// ************************************************************
void OutputManager::setNumberArrayIndexedValue(byte idx, byte value) {
  _digit_buffer.numberArray[idx] = value;
}

// ************************************************************
// Get the display type at index idx
// ************************************************************
byte OutputManager::getDisplayTypeIndexedValue(byte idx) {
  return _digit_buffer.displayType[idx];
}

// ************************************************************
// Set the display type at index idx
// ************************************************************
void OutputManager::setDisplayTypeIndexedValue(byte idx, byte value) {
  _digit_buffer.displayType[idx] = value;
}
