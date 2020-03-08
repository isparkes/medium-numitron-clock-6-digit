#ifndef outputmanager_h
#define outputmanager_h

#include "Arduino.h"
#include "DisplayDefs.h"
#include "SPIFFS.h"
#include "LEDManager.h"

extern byte numberArray[];
extern byte displayType[];
extern boolean led1State;
extern boolean led2State;
extern boolean ledLState;
extern boolean ledRState;
extern boolean blankTubes;

// ************************ Display management ************************
// Layout
//   Segment a                          --a-- 
//   Segment b                         |     |
//   Segment c                         f     b
//   Segment d                         |     |
//   Segment e                          --g--  
//   Segment f                         |     |
//   Segment g                         e     c
//   Segment indicator                 |     |
//                                      --d--  

// ************************ Display management ************************
#define SEG_A   B00000001
#define SEG_B   B00000010
#define SEG_C   B00000100
#define SEG_D   B00001000
#define SEG_E   B00010000
#define SEG_F   B00100000
#define SEG_G   B01000000
#define SEG_IND B10000000

#define DIGIT_0 SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F
#define DIGIT_1 SEG_B | SEG_C
#define DIGIT_2 SEG_A | SEG_B | SEG_D | SEG_D | SEG_E | SEG_G
#define DIGIT_3 SEG_A | SEG_B | SEG_C | SEG_D | SEG_G
#define DIGIT_4 SEG_B | SEG_C | SEG_F | SEG_G
#define DIGIT_5 SEG_A | SEG_C | SEG_D | SEG_F | SEG_G
#define DIGIT_6 SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_7 SEG_A | SEG_B | SEG_C
#define DIGIT_8 SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define DIGIT_9 SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G
#define DIGIT_MINUS SEG_G
#define DIGIT_BLANK 0

#define LETTER_S SEG_A | SEG_C | SEG_D | SEG_F | SEG_G
#define LETTER_P SEG_A | SEG_B | SEG_E | SEG_F | SEG_G
#define LETTER_I SEG_B | SEG_C
#define LETTER_F SEG_A | SEG_E | SEG_F | SEG_G
#define LETTER_n SEG_C | SEG_E | SEG_G
#define LETTER_E SEG_A | SEG_D | SEG_E | SEG_F | SEG_G
#define LETTER_t SEG_D | SEG_E | SEG_F | SEG_G
#define LETTER_C SEG_A | SEG_D | SEG_E | SEG_F
#define LETTER_O SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F
#define LETTER__ SEG_D
#define LETTER_r SEG_E | SEG_G
#define LETTER_A SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G
#define LETTER_U SEG_B | SEG_C | SEG_D | SEG_E | SEG_F
#define LETTER_L SEG_D | SEG_E | SEG_F
#define LETTER_b SEG_C | SEG_D | SEG_E | SEG_F | SEG_G
#define LETTER_d SEG_B | SEG_C | SEG_D | SEG_E | SEG_G

// ************************ Display management ************************

// Shift register
#define LATCHPin    14    // D5
#define CLOCKPin    13    // D7
#define DATAPin     12    // D6
#define BLANKPin    16    // D0

// ************************ Display management ************************

class OutputManager
{
  public:
    void setUp();
    void outputDisplay();
    void outputDisplayDiags();
    void setLDRValue(unsigned int newBrightness);
    int getTubeLag(int currentLDR);
    
  private:
    boolean _blinkState;
    spiffs_config_t *cc = &current_config;
    byte _preheatCounter = 0;
    int _ldrValue = 0;
    int _lastLDRValue = 0;
    
    byte digitData[16] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, LETTER_A, LETTER_b, LETTER_C, LETTER_d, LETTER_E, LETTER_F};
    
    void shiftDigitsOut(byte *displayDigits);
    void digitsOut(unsigned long highBits, unsigned long lowBits);
    void shiftOut24(uint8_t _dataPin1,
                uint8_t _clockPin,
                uint8_t _bitOrder,
                uint32_t _val1);
    void blankDigits(byte *digits, byte *prevNums, bool *smoothRun);
    void smoothDigits(byte *digits, byte *prevNums, bool *smoothRun);
    void setBlankingPin();
};


// ----------------- Exported Variables ------------------

static OutputManager outputManager;

#endif
