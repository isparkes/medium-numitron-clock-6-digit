/**
 * A class that displays a message by wiping it into and out of the display
 *
 * Thanks Judge & Ty!
 */

#include "Arduino.h"
#include "DA2000-Transition.h"

Transition::Transition(int effectInDuration, int effectOutDuration, int holdDuration, int selectedEffect) {
  _effectInDuration = effectInDuration;
  _effectOutDuration = effectOutDuration;
  _holdDuration = holdDuration;
  _selectedEffect = selectedEffect;
  _started = 0;
  _end = 0;
}

void Transition::start(unsigned long now) {
  if (_end < now) {
    _started = now;
    _end = getEnd();
    saveCurrentDisplayType(); 
  }
  // else we are already running!
}

boolean Transition::runEffect(unsigned long now, boolean blankLeading) {
  switch (_selectedEffect) {
    case SLOTS_MODE_WIPE_WIPE:
      return wipeInWipeOut(now, blankLeading);
      break;
    case SLOTS_MODE_BANG_BANG:
      return bangInBangOut(now);
      break;
    default:
      return false;
      break;
  }
}

boolean Transition::isMessageOnDisplay(unsigned long now)
{
  return (now < _end);
}

// we need to get the seconds updated, otherwise we show the old
// time at the end of the stunt
void Transition::updateRegularDisplaySeconds(int seconds) {
  _regularDisplay[5] = seconds % 10;
  _regularDisplay[4] = seconds / 10;
}

boolean Transition::wipeInWipeOut(unsigned long now, boolean blankLeading)
{
  if (now < _end) {
    int msCount = now - _started;
    // Wipe In blanking
    if (msCount < _effectInDuration) {
      _digit = msCount * (DIGIT_COUNT + 1) / _effectInDuration;
      if (_digit > 0)
        displayType[_digit-1] = BLANKED;      
    }
    // Wipe In date values
    else if (msCount < _effectInDuration * 2) {
      _digit = (msCount - _effectInDuration) * DIGIT_COUNT / _effectInDuration;
      numberArray[_digit] = _alternateDisplay[_digit];
      displayType[_digit] = NORMAL;
    }
    // Hold date display
    else if (msCount < _effectInDuration * 2 + _holdDuration) {
      loadAlternateValues();
    }
    // Wipe Out blanking
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration) {
      _digit = (msCount - _holdDuration - _effectInDuration * 2) * DIGIT_COUNT / _effectOutDuration;
      displayType[_digit] = BLANKED;    
    }
    // Wipe Out to time values
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration * 2) {
      _digit = (msCount - _holdDuration - _effectInDuration * 2 - _effectOutDuration) * DIGIT_COUNT / _effectOutDuration;
      numberArray[_digit] = _regularDisplay[_digit];
      if (!blankLeading || _digit != 0 || numberArray[_digit] != 0)
        displayType[_digit] = NORMAL;
    }
    // We now return you to your regularly scheduled program
    else {
      loadRegularValues();
      restoreCurrentDisplayType();
      _end = 0;
   }
    return true;  // we are still running
  }
  return false;   // We aren't running
}

boolean Transition::bangInBangOut(unsigned long now)
{
  if (now < _end) {
    int msCount = now - _started;
    // Bang In blanking
    if (msCount < _effectInDuration) {
      memset(displayType, BLANKED, DIGIT_COUNT);
    }
    // Bang In date values
    else if (msCount < _effectInDuration * 2) {
      loadAlternateValues();
      memset(displayType, NORMAL, DIGIT_COUNT);
    }
    // Hold date display
    else if (msCount < _effectInDuration * 2 + _holdDuration) {
      loadAlternateValues();
    }
    // Bang Out blanking
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration) {
      memset(displayType, BLANKED, DIGIT_COUNT);
    }
    // Bang Out to time values
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration * 2) {
      loadRegularValues();
      restoreCurrentDisplayType();
    }
    // We now return you to your regularly scheduled program
    else {
      loadRegularValues();
      restoreCurrentDisplayType();
      _end = 0;
   }
    return true;  // we are still running
  }
  return false;   // We aren't running
}

void Transition::setRegularValues() {
  memcpy(_regularDisplay, numberArray, sizeof(_regularDisplay));
}

void Transition::setAlternateValues() {
  memcpy(_alternateDisplay, numberArray, sizeof(_alternateDisplay));
}

void Transition::loadRegularValues() {
  memcpy(numberArray, _regularDisplay, sizeof(_regularDisplay));
}

void Transition::loadAlternateValues() {
  memcpy(numberArray, _alternateDisplay, sizeof(_alternateDisplay));
}

void Transition::saveCurrentDisplayType() {
  memcpy(_savedDisplayType, displayType, sizeof(_savedDisplayType));
}

void Transition::restoreCurrentDisplayType() {
  memcpy(displayType, _savedDisplayType, sizeof(_savedDisplayType));
}

unsigned long Transition::getEnd() {
  return _started + _effectInDuration * 2 + _holdDuration + _effectOutDuration * 2;
}
