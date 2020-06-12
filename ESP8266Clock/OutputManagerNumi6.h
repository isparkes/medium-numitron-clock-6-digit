#ifndef outputmanager_h
#define outputmanager_h

#include "Arduino.h"
#include "DisplayDefs.h"
#include "SPIFFS.h"
#include "LEDManager.h"

#define DIGIT_COUNT 6

extern boolean led1RState;
extern boolean led1LState;
extern boolean led2RState;
extern boolean led2LState;
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

#define PREHEAT_MIN           1
#define PREHEAT_V_STRONG      1
#define PREHEAT_STRONG        2
#define PREHEAT_WEAK          3
#define PREHEAT_V_WEAK        4
#define PREHEAT_OFF           5
#define PREHEAT_DEFAULT       PREHEAT_WEAK
#define PREHEAT_MAX           5

// Display mode, set per digit
#define BLANKED               0
#define NORMAL                1
#define BLINK                 2
#define FORMAT_MAX            BLINK
#define BLINK_COUNT_ON        70                      // The number of impressions blinking digits are ON
#define BLINK_COUNT_OFF       55                      // The number of impressions blinking digits are OFF
                                                      // Blink is asymetrical because Numitrons light up slowly
#define SEP_DIM_MIN           1
#define SEP_BRIGHT            1
#define SEP_DIM               2
#define SEP_DIM_MAX           2
#define SEP_DIM_DEFAULT       SEP_DIM

// ************************** Pin Allocations *************************

// Shift register
#define LATCHPin               14    // D5
#define CLOCKPin               13    // D7
#define DATAPin                12    // D6
#define BLANKPin               16    // D0

// ************************* Shared Structures ************************

typedef struct {
    byte numberArray[DIGIT_COUNT];
    byte displayType[DIGIT_COUNT];  
} digit_buffer_t;

typedef struct {
  int valueToShow;
  byte valueDisplayTime;
  byte valueDisplayType[DIGIT_COUNT];
} value_buffer_t;

// ************************ Display management ************************
class OutputManager {
  public:
    static void CreateInstance();
    static OutputManager& Instance();

    void setUp();
    void setConfigObject(spiffs_config_t* ccPtr);
    void outputDisplay();
    void outputDisplayDiags();
    void setLDRValue(unsigned int newBrightness);

    void loadNumberArrayTime();
    void loadNumberArrayDate();
    void loadNumberArraySameValue(byte val);
    void loadNumberArrayPOSTMessage(byte val);
    void loadNumberArrayTestDigits();
    void loadNumberArrayConfInt(int confValue, int confNum);
    void loadNumberArrayConfIntWide(int confValue);
    void loadNumberArrayConfBool(boolean confValue, int confNum);
    void loadNumberArrayIP(byte byte1, byte byte2);
    void loadNumberArrayLDR();
    void loadNumberArrayESPID(String ID);
    void loadNumberArrayValueToShow();
    void loadDisplaySetValueType();

    void allNormal(bool leadingBlank);
    void highlight0and1();
    void highlight2and3();
    void highlight4and5();
    void highlightDaysDateFormat();
    void highlightMonthsDateFormat();
    void highlightYearsDateFormat();
    void displayConfig();
    void allBlanked();

    // Arbitrary value
    void setValueDisplayTime(int timeToDisplay);
    int  getValueDisplayTime();
    void decValueDisplayTime();
    void setValueToShow(int newValue);
    void setValueFormat(int newValueFormat);

    // used for transition stunts
    byte getNumberArrayIndexedValue(byte idx);
    void setNumberArrayIndexedValue(byte idx, byte value);
    byte getDisplayTypeIndexedValue(byte idx);
    void setDisplayTypeIndexedValue(byte idx, byte value);
  private:
    static OutputManager* pInstance;

    int   _blinkCounter;
    boolean _blinkState;
    byte _preheatCounter = 0;
    int _ldrValue = 0;
    int _lastLDRValue = 0;
    int _tubeLag = 0;
    byte _separatorDim = 0;

    spiffs_config_t *cc;

    byte digitData[16] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9, LETTER_A, LETTER_b, LETTER_C, LETTER_d, LETTER_E, LETTER_F};
    
    digit_buffer_t _digit_buffer = {{0,0,0,0,0,0}, {0,0,0,0,0,0}};
    value_buffer_t _value_buffer = {0,10,{NORMAL,NORMAL,NORMAL,NORMAL,NORMAL,NORMAL}};

    void shiftDigitsOut(byte *displayDigits);
    void digitsOut(unsigned long highBits, unsigned long lowBits);
    void shiftOut24(uint8_t _dataPin1,
                uint8_t _clockPin,
                uint32_t _val1);
    void blankDigits(byte *digits, byte *prevNums, bool *smoothRun);
    void smoothDigits(byte *digits, byte *prevNums, bool *smoothRun);
    void setBlankingPin();
    void applyBlanking();
};

#endif
