#ifndef ledmanager_h
#define ledmanager_h

#include "Arduino.h"
#include "ClockDefs.h"
#include "SPIFFS.h"
#include "DisplayDefs.h"

#include <NeoPixelBus.h>        // https://github.com/Makuna/NeoPixelBus (Makuna 2.3.4)

// -------------------------------------------------------------------------------
#define BACKLIGHT_MIN                   0
#define BACKLIGHT_FIXED                 0   // Just define a colour and stick to it
#define BACKLIGHT_PULSE                 1   // pulse the defined colour
#define BACKLIGHT_CYCLE                 2   // cycle through random colours
#define BACKLIGHT_FIXED_DIM             3   // A defined colour, but dims with bulb dimming
#define BACKLIGHT_PULSE_DIM             4   // pulse the defined colour, dims with bulb dimming
#define BACKLIGHT_CYCLE_DIM             5   // cycle through random colours, dims with bulb dimming
#define BACKLIGHT_COLOUR_TIME           6   // use "ColourTime" - different colours for each digit value
#define BACKLIGHT_COLOUR_TIME_DIM       7   // use "ColourTime" - dims with bulb dimming
#define BACKLIGHT_MAX                   7
#define BACKLIGHT_DEFAULT               4

// -------------------------------------------------------------------------------
#define CYCLE_SPEED_MIN                 4
#define CYCLE_SPEED_MAX                 64
#define CYCLE_SPEED_DEFAULT             10

#define BACKLIGHT                       false
#define UNDERLIGHT                      true

#define NO_DIM                          false
#define DIMMED                          true

#define NO_PULSE                        false
#define PULSED                          true

extern byte numberArray[];

class LEDManager
{
  public:
    void setUp();

    // reset internal values based on slow-moving values (e.g. backlight dim factor)
    void recalculateVariables();

    // recalculate internal values based on the LDR reading
    void setLDRValue(int ldrValue);

    // recalculate internal values based on the pulsing factor
    void setPulseValue(int secsDelta);

    // Change LED blanking status
    void setBlanked(boolean blanked);

    void setSyncColourTime(boolean value);
    void setDiagnosticLED(byte stepNumber, byte state);

    // This processes the values and outputs the buffer
    void processLedStatus();

  private:
    float _backlightDim = 1.0;
    float _underlightDim = 1.0;
    float _ldrValue = 0.0;
    float _ldrDimFactor = 1.0;
    float _pwmFactor = 1.0;
    boolean _blanked = false;
    byte _ledMode = BACKLIGHT_DEFAULT;
    byte _cycleCount = 0;
    byte _cycleSpeed = CYCLE_SPEED_DEFAULT;
    boolean _syncColourTime = false;

    // Strategy 3
    int changeSteps = 0;
    byte currentColour = 0;
    
    int colors[3];
    byte cycleCount = 0;
    byte ledR[DIGIT_COUNT];
    byte ledG[DIGIT_COUNT];
    byte ledB[DIGIT_COUNT];

    void setBacklightLEDs(byte red, byte green, byte blue);
    void setUnderlightLEDs(byte red, byte green, byte blue);
    void outputLEDBuffer();
    byte getLEDAdjusted(byte rawValue, boolean dimmed, boolean usePulse, boolean backOrUnder);
    void cycleColours3(int colors[3]);
};

// ************************************************************
// LED brightness correction: The perceived brightness is not linear
// ************************************************************
const byte dim_curve[] = {
  0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
  3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
  4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
  6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
  8,   8,   9,   9,   9,   9,   9,   9,   10,  10,  10,  10,  10,  11,  11,  11,
  11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
  15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
  20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
  27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
  36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
  48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
  63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
  83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99,  101, 103, 105, 107, 109,
  110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
  146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
  193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255,
};

const byte rgb_backlight_curve[] = {0, 16, 32, 48, 64, 80, 99, 112, 128, 144, 160, 176, 192, 208, 224, 240, 255};

// Used to define "colourTime" colours
//                             0    1    2    3    4    5    6    7    8    9
const byte colourTimeR[] = { 255, 255, 204,  51,   0,   0,   0, 153, 204, 255};
const byte colourTimeG[] = {   0, 153, 192, 192, 255, 192, 102,   0,   0,   0};
const byte colourTimeB[] = {   0,   0,   0,   0,  51, 192, 255, 255, 255, 153};

// These are the "pure" HSV values turned to RGB
// Number                        0    1    2    3    4    5    6    7    8    9
// Degrees                       0   36   72  108  144  180  216  252  288  324
//const byte colourTimeR[] = { 255, 255, 204,  51,   0,   0,   0,  51, 204, 255};
//const byte colourTimeG[] = {   0, 153, 255, 255, 255, 255, 102,   0,   0,   0};
//const byte colourTimeB[] = {   0,   0,   0,   0, 102, 255, 255, 255, 255, 153};
// With these values 2/3 and 6/7 are not easily distinguishable
// 2 is too green, 6 is too blue


// ----------------- Exported Variables ------------------

static LEDManager ledManager;

#endif
