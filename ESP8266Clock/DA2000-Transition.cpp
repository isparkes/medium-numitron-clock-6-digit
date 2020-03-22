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
  digit_buffer_t * dbs = OutputManager::Instance().getDigitBuffer();
  value_buffer_t * vbs = OutputManager::Instance().getValueBuffer();
  if (now < _end) {
    int msCount = now - _started;
    // Wipe In blanking
    if (msCount < _effectInDuration) {
      _digit = msCount * (DIGIT_COUNT + 1) / _effectInDuration;
      if (_digit > 0)
        vbs->valueDisplayType[_digit-1] = BLANKED;      
    }
    // Wipe In date values
    else if (msCount < _effectInDuration * 2) {
      _digit = (msCount - _effectInDuration) * DIGIT_COUNT / _effectInDuration;
      dbs->numberArray[_digit] = _alternateDisplay[_digit];
      vbs->valueDisplayType[_digit] = NORMAL;
    }
    // Hold date display
    else if (msCount < _effectInDuration * 2 + _holdDuration) {
      loadAlternateValues();
    }
    // Wipe Out blanking
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration) {
      _digit = (msCount - _holdDuration - _effectInDuration * 2) * DIGIT_COUNT / _effectOutDuration;
      vbs->valueDisplayType[_digit] = BLANKED;    
    }
    // Wipe Out to time values
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration * 2) {
      _digit = (msCount - _holdDuration - _effectInDuration * 2 - _effectOutDuration) * DIGIT_COUNT / _effectOutDuration;
      dbs->numberArray[_digit] = _regularDisplay[_digit];
      if (!blankLeading || _digit != 0 || dbs->numberArray[_digit] != 0)
        vbs->valueDisplayType[_digit] = NORMAL;
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
  value_buffer_t * vbs = OutputManager::Instance().getValueBuffer();
  if (now < _end) {
    int msCount = now - _started;
    // Bang In blanking
    if (msCount < _effectInDuration) {
      memset(vbs->valueDisplayType, BLANKED, DIGIT_COUNT);
    }
    // Bang In date values
    else if (msCount < _effectInDuration * 2) {
      loadAlternateValues();
      memset(vbs->valueDisplayType, NORMAL, DIGIT_COUNT);
    }
    // Hold date display
    else if (msCount < _effectInDuration * 2 + _holdDuration) {
      loadAlternateValues();
    }
    // Bang Out blanking
    else if (msCount < _effectInDuration * 2 + _holdDuration + _effectOutDuration) {
      memset(vbs->valueDisplayType, BLANKED, DIGIT_COUNT);
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
  digit_buffer_t * dbs = OutputManager::Instance().getDigitBuffer();
  memcpy(_regularDisplay, dbs->numberArray, sizeof(_regularDisplay));
}

void Transition::setAlternateValues() {
  digit_buffer_t * dbs = OutputManager::Instance().getDigitBuffer();
  memcpy(_alternateDisplay, dbs->numberArray, sizeof(_alternateDisplay));
}

void Transition::loadRegularValues() {
  digit_buffer_t * dbs = OutputManager::Instance().getDigitBuffer();
  memcpy(dbs->numberArray, _regularDisplay, sizeof(_regularDisplay));
}

void Transition::loadAlternateValues() {
  digit_buffer_t * dbs = OutputManager::Instance().getDigitBuffer();
  memcpy(dbs->numberArray, _alternateDisplay, sizeof(_alternateDisplay));
}

void Transition::saveCurrentDisplayType() {
  value_buffer_t * vbs = OutputManager::Instance().getValueBuffer();
  memcpy(_savedDisplayType, vbs->valueDisplayType, sizeof(_savedDisplayType));
}

void Transition::restoreCurrentDisplayType() {
  value_buffer_t * vbs = OutputManager::Instance().getValueBuffer();
  memcpy(vbs->valueDisplayType, _savedDisplayType, sizeof(_savedDisplayType));
}

unsigned long Transition::getEnd() {
  return _started + _effectInDuration * 2 + _holdDuration + _effectOutDuration * 2;
}
