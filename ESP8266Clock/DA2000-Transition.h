#ifndef Transition_h
#define Transition_h

#include "Arduino.h"
#include "DisplayDefs.h"
#include "ClockDefs.h"

extern byte numberArray[];
extern byte displayType[];

class Transition
{
  public:
    Transition(int, int, int, int);
    void start(unsigned long);
    boolean isMessageOnDisplay(unsigned long);
    void updateRegularDisplaySeconds(int);    
    void setRegularValues();
    void setAlternateValues();
    void saveCurrentDisplayType();
    boolean runEffect(unsigned long, boolean blankLeading);
    
  private:
    int _effectInDuration;
    int _effectOutDuration;
    int _holdDuration;
    int _selectedEffect;
    int _digit;
    unsigned long _started;
    unsigned long _end;
    byte _regularDisplay[DIGIT_COUNT] = {0, 0, 0, 0, 0, 0};
    byte _alternateDisplay[DIGIT_COUNT] = {0, 0, 0, 0, 0, 0};
    byte _savedDisplayType[DIGIT_COUNT] = {NORMAL, NORMAL, NORMAL, NORMAL, NORMAL, NORMAL};
    
    boolean wipeInWipeOut(unsigned long, boolean blankLeading);
    boolean bangInBangOut(unsigned long);
    void loadRegularValues();
    void loadAlternateValues();
    void restoreCurrentDisplayType();
    unsigned long getEnd();
};

#endif
