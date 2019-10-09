#ifndef Transition_h
#define Transition_h

#include "Arduino.h"
#include "DisplayDefs.h"

extern byte numberArray[];
extern byte displayType[];
extern boolean blankLeading;

class Transition
{
  public:
    Transition(int, int, int);
    void start(unsigned long);
    boolean isMessageOnDisplay(unsigned long);
    void updateRegularDisplaySeconds(int seconds);
    boolean wipeInWipeOut(unsigned long);
    void setRegularValues();
    void setAlternateValues();
    void loadRegularValues();
    void loadAlternateValues();
    void saveCurrentDisplayType();
    void restoreCurrentDisplayType();
  private:
    int _effectInDuration;
    int _effectOutDuration;
    int _holdDuration;
    int _digit;
    unsigned long _started;
    unsigned long _end;
    byte _regularDisplay[DIGIT_COUNT] = {0, 0, 0, 0, 0, 0};
    byte _alternateDisplay[DIGIT_COUNT] = {0, 0, 0, 0, 0, 0};
    boolean _savedScrollback;
    byte _savedDisplayType[DIGIT_COUNT] = {NORMAL, NORMAL, NORMAL, NORMAL, NORMAL, NORMAL};
  
    unsigned long getEnd();
};

#endif
