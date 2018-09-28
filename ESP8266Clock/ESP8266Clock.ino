//**********************************************************************************
//* Main code for an ESP8266 based Nixie/VFD clock. Features:                      *
//*  - WiFi Clock interface for the WiFiModule  OR                                 *
//*  - Real Time Clock interface for DS3231                                        *
//*  - Digit fading with configurable fade length                                  *
//*  - Digit scrollback with configurable scroll speed                             *
//*  - Configuration stored in Flash (JSON)                                        *
//*  - Low hardware component count (as much as possible done in code)             *
//*  - Single button operation with software debounce                              *
//*  - Automatic dimming, using a Light Dependent Resistor                         *
//*  - RGB back light management using individually addressable WS2812B            *
//*  - PIR sensor to turn off display when no one is around                        *
//*                                                                                *
//*  nixie@protonmail.ch                                                           *
//*                                                                                *
//**********************************************************************************
//**********************************************************************************
// Standard Libraries
#include <Wire.h>

// Clock specific libraries, install these with "Sketch -> Include Library -> Add .ZIP library
// using the ZIP files in the "libraries" directory
#include <DS3231.h>             // Wickert V1.0.2
#include <TimeLib.h>            // Margolis V1.5.0
#include <NeoPixelBus.h>        // Makuna V2.3.4
#include <WiFiManager.h>        // tzapu V0.14.0
#include <ArduinoJson.h>        // Blanchon V5.13.6 NOT V6.x.x!

// Other parts of the code, broken out for clarity
#include "ClockButton.h"
#include "Transition.h"
#include "DisplayDefs.h"

// Software version shown in config menu
#define SOFTWARE_VERSION      1

// how often we make reference to the external time provider
#define READ_TIME_PROVIDER_MILLIS 60000 // Update the internal time provider from the external source once every minute

#define DEBUG_OFF     // DEBUG / DEBUG_OFF

// Display handling
#define DIGIT_DISPLAY_COUNT   3000 // The number of times to traverse inner fade loop per digit
#define DIGIT_DISPLAY_ON      100  // Switch on the digit after the "front porch" by default
#define DIGIT_DISPLAY_OFF     2000 // Switch off the digit at the end by default
#define DIGIT_DISPLAY_NEVER   -1   // When we don't want to switch on or off (i.e. blanking)
#define DISPLAY_COUNT_MAX     2999 // Maximum value we can set to
#define DISPLAY_COUNT_MIN     101  // Minimum value we can set to

#define MIN_DIM_DEFAULT       100  // The default minimum dim count
#define MIN_DIM_MIN           100  // The minimum dim count
#define MIN_DIM_MAX           500  // The maximum dim count

#define SENSOR_LOW_MIN        0
#define SENSOR_LOW_MAX        900
#define SENSOR_LOW_DEFAULT    100  // Dark
#define SENSOR_HIGH_MIN       0
#define SENSOR_HIGH_MAX       900
#define SENSOR_HIGH_DEFAULT   700 // Bright

#define SENSOR_SMOOTH_READINGS_MIN     1
#define SENSOR_SMOOTH_READINGS_MAX     255
#define SENSOR_SMOOTH_READINGS_DEFAULT 100  // Speed at which the brighness adapts to changes

#define BLINK_COUNT_MAX                25   // The number of impressions between blink state toggle

// How quickly the scroll works
#define SCROLL_STEPS_DEFAULT 4
#define SCROLL_STEPS_MIN     1
#define SCROLL_STEPS_MAX     40

// The number of dispay impessions we need to fade by default
// 100 is about 1 second
#define FADE_STEPS_DEFAULT 50
#define FADE_STEPS_MAX     200
#define FADE_STEPS_MIN     20

#define SECS_MAX  60
#define MINS_MAX  60
#define HOURS_MAX 24

#define COLOUR_CNL_MAX                  15
#define COLOUR_RED_CNL_DEFAULT          15
#define COLOUR_GRN_CNL_DEFAULT          0
#define COLOUR_BLU_CNL_DEFAULT          0
#define COLOUR_CNL_MIN                  0

// Clock modes - normal running is MODE_TIME, other modes accessed by a middle length ( 1S < press < 2S ) button press
#define MODE_MIN                        0
#define MODE_TIME                       0

// Time setting, need all six digits, so no flashing mode indicator
#define MODE_HOURS_SET                  1
#define MODE_MINS_SET                   2
#define MODE_SECS_SET                   3
#define MODE_DAYS_SET                   4
#define MODE_MONTHS_SET                 5
#define MODE_YEARS_SET                  6

// Basic settings
#define MODE_12_24                      7  // Mode "00" 0 = 24, 1 = 12
#define HOUR_MODE_DEFAULT               false
#define MODE_LEAD_BLANK                 8  // Mode "01" 1 = blanked
#define LEAD_BLANK_DEFAULT              false
#define MODE_SCROLLBACK                 9  // Mode "02" 1 = use scrollback
#define SCROLLBACK_DEFAULT              false
#define MODE_FADE                       10 // Mode "03" 1 = use fade
#define FADE_DEFAULT                    false
#define MODE_DATE_FORMAT                11 // Mode "04"
#define MODE_DAY_BLANKING               12 // Mode "05"
#define MODE_HR_BLNK_START              13 // Mode "06" - skipped if not using hour blanking
#define MODE_HR_BLNK_END                14 // Mode "07" - skipped if not using hour blanking

#define MODE_USE_LDR                    16 // Mode "09" 1 = use LDR, 0 = don't (and have 100% brightness)
#define MODE_USE_LDR_DEFAULT            true

#define MODE_BLANK_MODE                 17 // Mode "10" 
#define MODE_BLANK_MODE_DEFAULT         BLANK_MODE_BOTH

// Display tricks
#define MODE_FADE_STEPS_UP              18 // Mode "11"
#define MODE_FADE_STEPS_DOWN            19 // Mode "12"
#define MODE_DISPLAY_SCROLL_STEPS_UP    20 // Mode "13"
#define MODE_DISPLAY_SCROLL_STEPS_DOWN  21 // Mode "14"

#define MODE_SLOTS_MODE                 22 // Mode "15"

// PIR
#define MODE_PIR_TIMEOUT_UP             23 // Mode "16"
#define MODE_PIR_TIMEOUT_DOWN           24 // Mode "17"

// Back light
#define MODE_BACKLIGHT_MODE             25 // Mode "18"
#define MODE_RED_CNL                    26 // Mode "19"
#define MODE_GRN_CNL                    27 // Mode "20"
#define MODE_BLU_CNL                    28 // Mode "21"
#define MODE_CYCLE_SPEED                29 // Mode "22"

// Minimum dimming value for old tubes
#define MODE_MIN_DIM_UP                 34 // Mode "27"
#define MODE_MIN_DIM_DOWN               35 // Mode "28"

// Anti-ghosting for old tubes - Not Available over I2C
#define MODE_ANTI_GHOST_UP              36 // Mode "29"
#define MODE_ANTI_GHOST_DOWN            37 // Mode "30"

// Use the PIR pin pullup resistor - Not Available over I2C
#define MODE_USE_PIR_PULLUP             38 // Mode "31" 1 = use Pullup
#define USE_PIR_PULLUP_DEFAULT          true

// Temperature
#define MODE_TEMP                       39 // Mode "32"

// Software Version
#define MODE_VERSION                    40 // Mode "33"

// Tube test - all six digits, so no flashing mode indicator
#define MODE_TUBE_TEST                  41

#define MODE_MAX                        41

// Temporary display modes - accessed by a short press ( < 1S ) on the button when in MODE_TIME
#define TEMP_MODE_MIN                   0
#define TEMP_MODE_DATE                  0 // Display the date for 5 S
#define TEMP_MODE_TEMP                  1 // Display the temperature for 5 S
#define TEMP_MODE_LDR                   2 // Display the normalised LDR reading for 5S, returns a value from 100 (dark) to 999 (bright)
#define TEMP_MODE_VERSION               3 // Display the version for 5S
#define TEMP_IP_ADDR12                  4 // IP xxx.yyy.zzz.aaa: xxx.yyy
#define TEMP_IP_ADDR34                  5 // IP xxx.yyy.zzz.aaa: zzz.aaa
#define TEMP_IMPR                       6 // number of impressions per second
#define TEMP_MODE_MAX                   6

#define DATE_FORMAT_MIN                 0
#define DATE_FORMAT_YYMMDD              0
#define DATE_FORMAT_MMDDYY              1
#define DATE_FORMAT_DDMMYY              2
#define DATE_FORMAT_MAX                 2
#define DATE_FORMAT_DEFAULT             2

#define DAY_BLANKING_MIN                0
#define DAY_BLANKING_NEVER              0  // Don't blank ever (default)
#define DAY_BLANKING_WEEKEND            1  // Blank during the weekend
#define DAY_BLANKING_WEEKDAY            2  // Blank during weekdays
#define DAY_BLANKING_ALWAYS             3  // Always blank
#define DAY_BLANKING_HOURS              4  // Blank between start and end hour every day
#define DAY_BLANKING_WEEKEND_OR_HOURS   5  // Blank between start and end hour during the week AND all day on the weekend
#define DAY_BLANKING_WEEKDAY_OR_HOURS   6  // Blank between start and end hour during the weekends AND all day on week days
#define DAY_BLANKING_WEEKEND_AND_HOURS  7  // Blank between start and end hour during the weekend
#define DAY_BLANKING_WEEKDAY_AND_HOURS  8  // Blank between start and end hour during week days
#define DAY_BLANKING_MAX                8
#define DAY_BLANKING_DEFAULT            0

#define BLANK_MODE_MIN                  0
#define BLANK_MODE_TUBES                0  // Use blanking for tubes only 
#define BLANK_MODE_LEDS                 1  // Use blanking for LEDs only
#define BLANK_MODE_BOTH                 2  // Use blanking for tubes and LEDs
#define BLANK_MODE_MAX                  2
#define BLANK_MODE_DEFAULT              2

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

#define CYCLE_SPEED_MIN                 4
#define CYCLE_SPEED_MAX                 64
#define CYCLE_SPEED_DEFAULT             10

#define ANTI_GHOST_MIN                  0
#define ANTI_GHOST_MAX                  50
#define ANTI_GHOST_DEFAULT              0

#define PIR_TIMEOUT_MIN                 60    // 1 minute in seconds
#define PIR_TIMEOUT_MAX                 3600  // 1 hour in seconds
#define PIR_TIMEOUT_DEFAULT             300   // 5 minutes in seconds

#define TEMP_DISPLAY_MODE_DUR_MS        5000

#define USE_LDR_DEFAULT                 true

// Limit on the length of time we stay in test mode
#define TEST_MODE_MAX_MS                60000

#define SLOTS_MODE_MIN                  0
#define SLOTS_MODE_NONE                 0   // Don't use slots effect
#define SLOTS_MODE_1M_SCR_SCR           1   // use slots effect every minute, scroll in, scramble out
#define SLOTS_MODE_MAX                  1
#define SLOTS_MODE_DEFAULT              1

// RTC address
#define RTC_I2C_ADDRESS                 0x68

#define MAX_WIFI_TIME                  5

#define DO_NOT_APPLY_LEAD_0_BLANK     false
#define APPLY_LEAD_0_BLANK            true

#define PixelCount  6

#define colorSaturation 16

//**********************************************************************************
//**********************************************************************************
//*                               Variables                                        *
//**********************************************************************************
//**********************************************************************************

// ***** Pin Defintions ****** Pin Defintions ****** Pin Defintions ******

// output gpio run by 2 74HC595, driven by DATA1 and DATA2
// Layout
// DATA 1:
//   Segment a
//   Segment b
//   Segment c
//   Segment d
//   Segment e
//   Segment f
//   Segment g
//   Segment dp

// DATA 2:
//   grid Hx10
//   grid Hx1
//   grid Mx10
//   grid Mx1
//   grid Sx10
//   grid Sx1
//   LED 1 (between H and M)
//   LED 2 (between M and S)


// button input
#define inputPin1   5     // TX needs to move to 5 (D1) was 1

// PIR input
#define pirPin      16    // D0 

// PWM capable output for backlight - Neo Pixel chain
#define LED_DOUT    2     // D4

#define LDRPin      A0    // ADC input

// Shift register
#define LATCHPin    14    // D5
#define CLOCKPin    12    // D6
#define DATA1Pin    15    // D8 - Segments
#define DATA2Pin    13    // D7 - Grids

//**********************************************************************************
Transition transition(500, 1000, 3000);

// ************************ Display management ************************
#define SEG_A  B00000001
#define SEG_B  B00000010
#define SEG_C  B00000100
#define SEG_D  B00001000
#define SEG_E  B00010000
#define SEG_F  B00100000
#define SEG_G  B01000000
#define SEG_DP B10000000

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


byte digitData[10] = {DIGIT_0, DIGIT_1, DIGIT_2, DIGIT_3, DIGIT_4, DIGIT_5, DIGIT_6, DIGIT_7, DIGIT_8, DIGIT_9};
byte data1[6] = {1, 2, 4, 8, 16, 32};

#define MSK_A  B01111111
#define MSK_B  B10111111
#define MSK_C  B11011111
#define MSK_D  B11101111
#define MSK_E  B11110111
#define MSK_F  B11111011
#define MSK_G  B11111101
#define MSK_DP B11111110

#define GRD_1  B10000000
#define GRD_2  B01000000
#define GRD_3  B00100000
#define GRD_4  B00010000
#define GRD_5  B00001000
#define GRD_6  B00000100
#define TLED1  B00000010
#define TLED2  B00000001

#define MSK_1   B01111111
#define MSK_2   B10111111
#define MSK_3   B11011111
#define MSK_4   B11101111
#define MSK_5   B11110111
#define MSK_6   B11111011
#define MSK_TL1 B11111101
#define MSK_TL2 B11111110

#define MSK_ALL B00000011

byte register1 = 0;
byte register2 = 0;

byte NumberArray[6]    = {0, 0, 0, 0, 0, 0};
byte currNumberArray[6] = {0, 0, 0, 0, 0, 0};
byte displayType[6]    = {FADE, FADE, FADE, FADE, FADE, FADE};
byte fadeState[6]      = {0, 0, 0, 0, 0, 0};
byte ourIP[4]          = {0, 0, 0, 0}; // set by the WiFi module
byte valueDisplayTime  = 0;
byte valueToShow[3]    = {0, 0, 0};
byte valueDisplayType[3] = {0x33, 0x33, 0x33}; // All normal by default

// Decimal point indicators
boolean dpArray[6]         = {false, false, false, false, false, false};

// how many fade steps to increment (out of DIGIT_DISPLAY_COUNT) each impression
// 100 is about 1 second
int fadeSteps = FADE_STEPS_DEFAULT;
int digitOffCount = DIGIT_DISPLAY_OFF;
int scrollSteps = SCROLL_STEPS_DEFAULT;
boolean scrollback = true;
boolean fade = true;
byte antiGhost = ANTI_GHOST_DEFAULT;
int dispCount = DIGIT_DISPLAY_COUNT + antiGhost;
float fadeStep = DIGIT_DISPLAY_COUNT / fadeSteps;

// For software blinking
int blinkCounter = 0;
boolean blinkState = true;

// leading digit blanking
boolean blankLeading = false;

// Dimming value
const int DIM_VALUE = DIGIT_DISPLAY_COUNT / 5;
int minDim = MIN_DIM_DEFAULT;

unsigned int tempDisplayModeDuration;      // time for the end of the temporary display
int  tempDisplayMode;

byte slotsMode = SLOTS_MODE_DEFAULT;

byte currentMode = MODE_TIME;   // Initial cold start mode
byte nextMode = currentMode;
byte blankHourStart = 0;
byte blankHourEnd = 0;
byte blankMode = 0;
boolean blankTubes = false;
boolean blankLEDs = false;

int tlPWM = 0;

// ************************ Ambient light dimming ************************
int dimDark = SENSOR_LOW_DEFAULT;
int dimBright = SENSOR_HIGH_DEFAULT;
double sensorLDRSmoothed = 0;
double sensorFactor = (double)(DIGIT_DISPLAY_OFF) / (double)(dimBright - dimDark);
int sensorSmoothCountLDR = SENSOR_SMOOTH_READINGS_DEFAULT;
int sensorSmoothCountHV = SENSOR_SMOOTH_READINGS_DEFAULT / 8;
boolean useLDR = true;
boolean usePIRPullup = true;

// ************************ Clock variables ************************
// RTC, uses Analogue pins A4 (SDA) and A5 (SCL)
DS3231 Clock;

// State variables for detecting changes
byte lastSec;
unsigned long nowMillis = 0;
unsigned long lastCheckMillis = 0;

byte dateFormat = DATE_FORMAT_DEFAULT;
byte dayBlanking = DAY_BLANKING_DEFAULT;
boolean blanked = false;
byte blankSuppressStep = 0;    // The press we are on: 1 press = suppress for 1 min, 2 press = 1 hour, 3 = 1 day
unsigned long blankSuppressedMillis = 0;   // The end time of the blanking, 0 if we are not suppressed
unsigned long blankSuppressedSelectionTimoutMillis = 0;   // Used for determining the end of the blanking period selection timeout
boolean hourMode = false;

// PIR
unsigned long pirTimeout = PIR_TIMEOUT_DEFAULT;
unsigned long pirLastSeen = 0;

boolean triggeredThisSec = false;

byte useRTC = false;  // true if we detect an RTC
byte useWiFi = 0; // the number of minutes ago we recevied information from the WiFi module, 0 = don't use WiFi

// **************************** LED management ***************************
boolean upOrDown;

// Blinking colons led in settings modes
int ledBlinkCtr = 0;
int ledBlinkNumber = 0;

byte backlightMode = BACKLIGHT_DEFAULT;

// Back light intensities
byte redCnl = COLOUR_RED_CNL_DEFAULT;
byte grnCnl = COLOUR_GRN_CNL_DEFAULT;
byte bluCnl = COLOUR_BLU_CNL_DEFAULT;
byte cycleCount = 0;
byte cycleSpeed = CYCLE_SPEED_DEFAULT;

int colors[3];

// individual channel colours for the LEDs
byte ledR[6];
byte ledG[6];
byte ledB[6];

// set up the NeoPixel library
NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(PixelCount, LED_DOUT);

// Strategy 3
int changeSteps = 0;
byte currentColour = 0;

int impressionsPerSec = 0;
int lastImpressionsPerSec = 0;

// ********************** Input switch management **********************
ClockButton button1(inputPin1, true);

WiFiManager wifiManager;

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
const byte colourTimeR[] = { 255, 255,   0,   0,   0, 255, 255,   0,  15, 127};
const byte colourTimeG[] = {   0, 255, 255, 255,   0,   0,  64, 255,  31, 127};
const byte colourTimeB[] = {   0,   0,   0, 255, 255, 255,   0,  31, 255, 127};

//**********************************************************************************
//**********************************************************************************
//*                                    Setup                                       *
//**********************************************************************************
//**********************************************************************************
void setup()
{
  setupDebug();

  pinMode(LED_DOUT, OUTPUT);

  pinMode(pirPin, INPUT_PULLUP);
  digitalWrite(pirPin, HIGH);

  pinMode(LATCHPin, OUTPUT);
  pinMode(CLOCKPin, OUTPUT);
  pinMode(DATA1Pin, OUTPUT);
  pinMode(DATA2Pin, OUTPUT);

  debugMsg("Starting");

  // NOTE:
  // Grounding the input pin causes it to actuate
  // pinMode(inputPin1, INPUT_PULLUP);
  pinMode(inputPin1, INPUT);

  // The LEDS sometimes glow at startup, it annoys me, so turn them completely off
  //tlPWM = 0;
  setAllLEDs(0,0,0);

  // **********************************************************************

  debugMsg("Setting up LEDs");
  
  // Set up the LED output
  leds.Begin();

  // Set up the PRNG with something so that it looks random
  randomSeed(analogRead(LDRPin));

  // Test if the button is pressed for factory reset
  for (int i = 0 ; i < 20 ; i++ ) {
    button1.checkButton(nowMillis);
  }

  // User requested factory reset
  if (button1.isButtonPressedNow()) {
    // Flash some ramdom c0lours to signal that we have accepted the factory reset
    for (int i = 0 ; i < 60 ; i++ ) {
      randomRGBFlash(20);
    }

    // mark that we have done the EEPROM setup
  //  EEPROM.write(EE_NEED_SETUP, true);
  }

  // If the button is held down while we are flashing, then do the test pattern
  boolean doTestPattern = false;

  // Detect factory reset: button pressed on start or uninitialised EEPROM
//  if (EEPROM.read(EE_NEED_SETUP)) {
    doTestPattern = true;
//  }

  // Clear down any spurious button action
  button1.reset();

  // Test if the button is pressed for factory reset
  for (int i = 0 ; i < 20 ; i++ ) {
    button1.checkButton(nowMillis);
  }

  if (button1.isButtonPressedNow()) {
    doTestPattern = true;
  }

  // Read EEPROM values
//  readEEPROMValues();

  // Not setting the pullup resistor allows us to use 3V3 devices (e.g. RCWL-0516)
  digitalWrite(pirPin, usePIRPullup); // set pull up resistor - if no PIR connected, default to "triggered"

  if (doTestPattern) {
    debugMsg("Doing test sequence");
    boolean oldUseLDR = useLDR;
    byte oldBacklightMode = backlightMode;

    // reset the EEPROM values
    factoryReset();

    // turn off LDR
    useLDR = false;

    // turn off Scrollback
    scrollback = false;

    // set backlights to change with the displayed digits
    backlightMode = BACKLIGHT_COLOUR_TIME;

    // All the digits on full
    allBright();

    int secCount = 0;
    int lastSecCount = 0;
    lastCheckMillis = millis();

    boolean inLoop = true;

    // We don't want to stay in test mode forever
    long startTestMode = lastCheckMillis;


    while (inLoop) {
      // Otherwise we get SOFT WDT
      yield();

      nowMillis = millis();
      if (nowMillis - lastCheckMillis > 1000) {
        lastCheckMillis = nowMillis;
        secCount++;
        secCount = secCount % 10;
      }

      // turn off test mode
      blankTubes = (nowMillis - startTestMode > TEST_MODE_MAX_MS);

      loadNumberArraySameValue(secCount);

      // debugMsg("Setting display: " + String(secCount));
      outputDisplay();

      // debugMsg("Setting LEDs");
      if (lastSecCount != secCount) {
        setLeds();
        lastSecCount = secCount;
      }

      // debugMsg("Checing test mode exit condition");
      button1.checkButton(nowMillis);
      if (button1.isButtonPressedNow() && (secCount == 8)) {
        inLoop = false;
      }
    }

    useLDR = oldUseLDR;
    backlightMode = oldBacklightMode;
  }

  // reset the LEDs
  setLeds();

  // Clear down any spurious button action
  button1.reset();

  // initialise the internal time (in case we don't find the time provider)
  nowMillis = millis();
  setTime(19, 23, 00, 28, 9, 2018);
  getRTCTime();

  // Show the version for 1 s
  tempDisplayMode = TEMP_MODE_VERSION;
  tempDisplayModeDuration = TEMP_DISPLAY_MODE_DUR_MS;

  // don't blank anything right now
  blanked = false;
  setTubesAndLEDSBlankMode();

  // mark that we have done the EEPROM setup
  // EEPROM.write(EE_NEED_SETUP, false);

  debugMsg("Exit startup");
}

//**********************************************************************************
//**********************************************************************************
//*                              Main loop                                         *
//**********************************************************************************
//**********************************************************************************
boolean millisNotSet = true;
void loop()
{  
  nowMillis = millis();

  // shows us how fast the inner loop is running
  impressionsPerSec++;

  // We don't want to get the time from the external time provider always,
  // just enough to keep the internal time provider correct
  // This keeps the outer loop fast and responsive
  // We do this once per minute
  if (abs(nowMillis - lastCheckMillis) >= 1000) {
    performOncePerSecondProcessing();

    // we only want to once per minute processing to be called once each minute 
    if ((second() == 0) && (!triggeredThisSec)) {
      performOncePerMinuteProcessing();
      triggeredThisSec = true;
    }

    if ((second() == 1) && triggeredThisSec) {
      triggeredThisSec = false;
    }

    lastCheckMillis = nowMillis;
  }

  // Check button, we evaluate below
  button1.checkButton(nowMillis);

  // ******* Preview the next display mode *******
  // What is previewed here will get actioned when
  // the button is released
  if (button1.isButtonPressed2S()) {
    // Just jump back to the start
    nextMode = MODE_MIN;
  } else if (button1.isButtonPressed1S()) {
    nextMode = currentMode + 1;

    if (nextMode > MODE_MAX) {
      nextMode = MODE_MIN;
    }
  }

  // ******* Set the display mode *******
  if (button1.isButtonPressedReleased2S()) {
    currentMode = MODE_MIN;

    // Store the EEPROM if we exit the config mode
//    saveEEPROMValues();

    // Preset the display
    allFadeOrNormal(DO_NOT_APPLY_LEAD_0_BLANK);

    nextMode = currentMode;
  } else if (button1.isButtonPressedReleased1S()) {
    currentMode++;

    if (currentMode > MODE_MAX) {
      currentMode = MODE_MIN;

      // Store the EEPROM if we exit the config mode
//      saveEEPROMValues();

      // Preset the display
      allFadeOrNormal(DO_NOT_APPLY_LEAD_0_BLANK);
    }

    nextMode = currentMode;
  }

  // ************* Process the modes *************
  if (nextMode != currentMode) {
    setNextMode();
  } else {
    processCurrentMode();
  }

  // get the LDR ambient light reading
  digitOffCount = getDimmingFromLDR();
  fadeStep = digitOffCount / fadeSteps;

  // Prepare the tick and backlight LEDs
  setLeds();

  outputDisplay();
}

// ************************************************************
// Called once per second
// ************************************************************
void performOncePerSecondProcessing() {

  debugMsg("Time: " + String(hour()) + ":" + String(minute()) + ":" + String(second()));

  // Store the current value and reset
  lastImpressionsPerSec = impressionsPerSec;
  impressionsPerSec = 0;

  // Change the direction of the pulse
  upOrDown = !upOrDown;

  // If we are in temp display mode, decrement the count
  if (tempDisplayModeDuration > 0) {
    if (tempDisplayModeDuration > 1000) {
      tempDisplayModeDuration -= 1000;
    } else {
      tempDisplayModeDuration = 0;
    }
  }

  // decrement the blanking supression counter
  if (blankSuppressedMillis > 0) {
    if (blankSuppressedMillis > 1000) {
      blankSuppressedMillis -= 1000;
    } else {
      blankSuppressedMillis = 0;
    }
  }

  // Get the blanking status, this may be overridden by blanking suppression
  // Only blank if we are in TIME mode
  if (currentMode == MODE_TIME) {
    boolean pirBlanked = checkPIR(nowMillis);
    boolean nativeBlanked = checkBlanking();

    // Check if we are in blanking suppression mode
    blanked = (nativeBlanked || pirBlanked) && (blankSuppressedMillis == 0);

    if (blanked) {
      debugMsg("blanked");
      if (pirBlanked)
        debugMsg(" -> PIR");
      if (nativeBlanked)
        debugMsg(" -> Native");
    }

    // reset the blanking period selection timer
    if (nowMillis > blankSuppressedSelectionTimoutMillis) {
      blankSuppressedSelectionTimoutMillis = 0;
      blankSuppressStep = 0;
    }
  } else {
    blanked = false;
  }

  setTubesAndLEDSBlankMode();

  // Decrement the value display counter
  // 0xff means "forever", so don't decrement it
  if ((valueDisplayTime > 0) && (valueDisplayTime < 0xff)) {
    valueDisplayTime--;
  }
}

// ************************************************************
// Called once per minute
// ************************************************************
void performOncePerMinuteProcessing() {
  if (useWiFi > 0) {
    if (useWiFi == MAX_WIFI_TIME) {
      // We recently got an update, send to the RTC (if installed)
      setRTC();      
    }
    useWiFi--;
  } else {
    // get the time from the external RTC provider - (if installed)
    getRTCTime();
  }

  // AM/PM indicator
  if (hourMode) {
    dpArray[1] = (hour() > 12);
  }

  for (int idx = 0 ; idx < 6; idx++) {
    debugMsg("  Idx: " + String(idx));
    debugMsg("   NA: " + String(NumberArray[idx]));
    debugMsg("  CNA: " + String(currNumberArray[idx]));
    debugMsg("   DT: " + String(displayType[idx]));
  }
}

// ************************************************************
// Set the seconds tick led(s) and the back lights
// ************************************************************
void setLeds()
{
  int secsDelta;
  if (upOrDown) {
    secsDelta = (nowMillis - lastCheckMillis);
  } else {
    secsDelta = 1000 - (nowMillis - lastCheckMillis);
  }

  // calculate the PWM factor, goes between minDim% and 100%
  float dimFactor = (float) digitOffCount / (float) DIGIT_DISPLAY_OFF;
  float pwmFactor = (float) secsDelta / (float) 1000.0;

  // Tick led output
  tlPWM = getLEDAdjusted(255, pwmFactor, dimFactor);

  if (blankLEDs) {
          setAllLEDs( getLEDAdjusted(0, 1, 1), 
                      getLEDAdjusted(0, 1, 1), 
                      getLEDAdjusted(0, 1, 1));
  } else {
    // RGB Backlight PWM led output
    if (currentMode == MODE_TIME) {
      switch (backlightMode) {
        case BACKLIGHT_FIXED:
          setAllLEDs( getLEDAdjusted(rgb_backlight_curve[redCnl], 1, 1), 
                      getLEDAdjusted(rgb_backlight_curve[grnCnl], 1, 1), 
                      getLEDAdjusted(rgb_backlight_curve[bluCnl], 1, 1));
          break;
        case BACKLIGHT_PULSE:
          setAllLEDs( getLEDAdjusted(rgb_backlight_curve[redCnl], pwmFactor, 1),
                      getLEDAdjusted(rgb_backlight_curve[grnCnl], pwmFactor, 1),
                      getLEDAdjusted(rgb_backlight_curve[bluCnl], pwmFactor, 1));
          break;
        case BACKLIGHT_CYCLE:
          cycleColours3(colors);
          setAllLEDs( getLEDAdjusted(colors[0], 1, 1),
                      getLEDAdjusted(colors[1], 1, 1),
                      getLEDAdjusted(colors[2], 1, 1));
          break;
        case BACKLIGHT_FIXED_DIM:
          setAllLEDs( getLEDAdjusted(rgb_backlight_curve[redCnl], 1, dimFactor), 
                      getLEDAdjusted(rgb_backlight_curve[grnCnl], 1, dimFactor), 
                      getLEDAdjusted(rgb_backlight_curve[bluCnl], 1, dimFactor));
          break;
        case BACKLIGHT_PULSE_DIM:
          setAllLEDs( getLEDAdjusted(rgb_backlight_curve[redCnl], pwmFactor, dimFactor),
                      getLEDAdjusted(rgb_backlight_curve[grnCnl], pwmFactor, dimFactor),
                      getLEDAdjusted(rgb_backlight_curve[bluCnl], pwmFactor, dimFactor));
          break;
        case BACKLIGHT_CYCLE_DIM:
          cycleColours3(colors);
          setAllLEDs( getLEDAdjusted(colors[0], 1, dimFactor),
                      getLEDAdjusted(colors[1], 1, dimFactor),
                      getLEDAdjusted(colors[2], 1, dimFactor));
          break;
        case BACKLIGHT_COLOUR_TIME:
            for (int i = 0 ; i < 6 ; i++) {
              ledR[5-i] = getLEDAdjusted(colourTimeR[NumberArray[i]],1,1);
              ledG[5-i] = getLEDAdjusted(colourTimeG[NumberArray[i]],1,1);
              ledB[5-i] = getLEDAdjusted(colourTimeB[NumberArray[i]],1,1);
            }
            outputLEDBuffer();
          break;
        case BACKLIGHT_COLOUR_TIME_DIM:
            for (int i = 0 ; i < 6 ; i++) {
              ledR[5-i] = getLEDAdjusted(colourTimeR[NumberArray[i]],1,dimFactor);
              ledG[5-i] = getLEDAdjusted(colourTimeG[NumberArray[i]],1,dimFactor);
              ledB[5-i] = getLEDAdjusted(colourTimeB[NumberArray[i]],1,dimFactor);
            }
            outputLEDBuffer();
          break;
      }
    } else {
      // Settings modes
      ledBlinkCtr++;
      if (ledBlinkCtr > 40) {
        ledBlinkCtr = 0;

        ledBlinkNumber++;
        if (ledBlinkNumber > nextMode) {
          // Make a pause
          ledBlinkNumber = -2;
        }
      }

      if ((ledBlinkNumber <= nextMode) && (ledBlinkNumber > 0)) {
        if (ledBlinkCtr < 3) {
          setAllLEDs(255,255,255);
        } else {
          setAllLEDs(0,0,0);
        }
      }
    }
  }
}

// ************************************************************
// Check the PIR status. If we don't have a PIR installed, we
// don't want to respect the pin value, because it would defeat
// normal day blanking. The first time the PIR takes the pin low
// we mark that we have a PIR and we should start to respect
// the sensor.
// Returns true if PIR sensor is installed and we are blanked
// ************************************************************
boolean checkPIR(unsigned long nowMillis) {
  boolean pirvalue = (digitalRead(pirPin) == HIGH);

  // testing
  pirvalue = true;

  dpArray[5] = pirvalue;
  
  if (pirvalue) {
    pirLastSeen = nowMillis;
    return false;
  } else {
    if (nowMillis > (pirLastSeen + (pirTimeout * 1000))) {
      dpArray[4] = false;
      return true;
    } else {
      dpArray[4] = true;
      return false;
    }
  }
}

// ************************************************************
// Set back light LEDs to the same colour
// ************************************************************
void setAllLEDs(byte red, byte green, byte blue) {
  for (int i = 0 ; i < 6 ; i++) {
    ledR[i] = red;
    ledG[i] = green;
    ledB[i] = blue;
  }
  outputLEDBuffer();
}

// ************************************************************
// Put the led buffers out
// ************************************************************
void outputLEDBuffer() {
  for (int i = 0 ; i < 6 ; i++) {
    RgbColor color(ledR[i], ledG[i], ledB[i]);
    leds.SetPixelColor(i, color);
  }
  leds.Show();
}

// ************************************************************
// Show the preview of the next mode - we stay in this mode until the 
// button is released
// ************************************************************
void setNextMode() {
  // turn off blanking
  blanked = false;
  setTubesAndLEDSBlankMode();

  switch (nextMode) {
    case MODE_TIME: {
        loadNumberArrayTime();
        allFadeOrNormal(APPLY_LEAD_0_BLANK);
        break;
      }
    case MODE_HOURS_SET: {
        if (useWiFi > 0) {
          // skip past the time settings
          nextMode++;
          currentMode++;
          nextMode++;
          currentMode++;
          nextMode++;
          currentMode++;
        }
        loadNumberArrayTime();
        highlight0and1();
        break;
      }
    case MODE_MINS_SET: {
        loadNumberArrayTime();
        highlight2and3();
        break;
      }
    case MODE_SECS_SET: {
        loadNumberArrayTime();
        highlight4and5();
        break;
      }
    case MODE_DAYS_SET: {
        if (useWiFi > 0) {
          // skip past the time settings
          nextMode++;
          currentMode++;
          nextMode++;
          currentMode++;
          nextMode++;
          currentMode++;
        }
        loadNumberArrayDate();
        highlightDaysDateFormat();
        break;
      }
    case MODE_MONTHS_SET: {
        loadNumberArrayDate();
        highlightMonthsDateFormat();
        break;
      }
    case MODE_YEARS_SET: {
        loadNumberArrayDate();
        highlightYearsDateFormat();
        break;
      }
    case MODE_12_24: {
        loadNumberArrayConfBool(hourMode, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_LEAD_BLANK: {
        loadNumberArrayConfBool(blankLeading, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_SCROLLBACK: {
        loadNumberArrayConfBool(scrollback, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_FADE: {
        loadNumberArrayConfBool(fade, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_DATE_FORMAT: {
        loadNumberArrayConfInt(dateFormat, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_DAY_BLANKING: {
        loadNumberArrayConfInt(dayBlanking, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_HR_BLNK_START: {
        if (dayBlanking < DAY_BLANKING_HOURS) {
          // Skip past the start and end hour if the blanking mode says it is not relevant
          nextMode++;
          currentMode++;
          nextMode++;
          currentMode++;
        }

        loadNumberArrayConfInt(blankHourStart, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_HR_BLNK_END: {
        loadNumberArrayConfInt(blankHourEnd, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_BLANK_MODE: {
        loadNumberArrayConfInt(blankMode, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_USE_LDR: {
        loadNumberArrayConfBool(useLDR, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_FADE_STEPS_UP:
    case MODE_FADE_STEPS_DOWN: {
        loadNumberArrayConfInt(fadeSteps, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_DISPLAY_SCROLL_STEPS_UP:
    case MODE_DISPLAY_SCROLL_STEPS_DOWN: {
        loadNumberArrayConfInt(scrollSteps, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_PIR_TIMEOUT_UP:
    case MODE_PIR_TIMEOUT_DOWN: {
        loadNumberArrayConfInt(pirTimeout, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_SLOTS_MODE: {
        loadNumberArrayConfInt(slotsMode, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_BACKLIGHT_MODE: {
        loadNumberArrayConfInt(backlightMode, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_RED_CNL: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }
        loadNumberArrayConfInt(redCnl, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_GRN_CNL: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }
        loadNumberArrayConfInt(grnCnl, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_BLU_CNL: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }
        loadNumberArrayConfInt(bluCnl, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_CYCLE_SPEED: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }
        loadNumberArrayConfInt(cycleSpeed, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_MIN_DIM_UP:
    case MODE_MIN_DIM_DOWN: {
        loadNumberArrayConfInt(minDim, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_ANTI_GHOST_UP:
    case MODE_ANTI_GHOST_DOWN: {
        loadNumberArrayConfInt(antiGhost, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_USE_PIR_PULLUP: {
        loadNumberArrayConfBool(usePIRPullup, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_TEMP: {
        loadNumberArrayTemp(nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_VERSION: {
        loadNumberArrayConfInt(SOFTWARE_VERSION, nextMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_TUBE_TEST: {
        loadNumberArrayTestDigits();
        allNormal();
        break;
      }
  }
}

// ************************************************************
// Show the next mode - once the button is released
// ************************************************************
void processCurrentMode() {
  switch (currentMode) {
    case MODE_TIME: {
        if (button1.isButtonPressedAndReleased()) {
          // Deal with blanking first
          if ((nowMillis < blankSuppressedSelectionTimoutMillis) || blanked) {
            if (blankSuppressedSelectionTimoutMillis == 0) {
              // Apply 5 sec tineout for setting the suppression time
              blankSuppressedSelectionTimoutMillis = nowMillis + TEMP_DISPLAY_MODE_DUR_MS;
            }

            blankSuppressStep++;
            if (blankSuppressStep > 3) {
              blankSuppressStep = 3;
            }

            if (blankSuppressStep == 1) {
              blankSuppressedMillis = 10000;
            } else if (blankSuppressStep == 2) {
              blankSuppressedMillis = 3600000;
            } else if (blankSuppressStep == 3) {
              blankSuppressedMillis = 3600000 * 4;
            }
            blanked = false;
            setTubesAndLEDSBlankMode();
          } else {
            // Always start from the first mode, or increment the temp mode if we are already in a display
            if (tempDisplayModeDuration > 0) {
              tempDisplayModeDuration = TEMP_DISPLAY_MODE_DUR_MS;
              tempDisplayMode++;
            } else {
              tempDisplayMode = TEMP_MODE_MIN;
            }

            if (tempDisplayMode > TEMP_MODE_MAX) {
              tempDisplayMode = TEMP_MODE_MIN;
            }

            tempDisplayModeDuration = TEMP_DISPLAY_MODE_DUR_MS;
          }
        }

        if (tempDisplayModeDuration > 0) {
          blanked = false;
          setTubesAndLEDSBlankMode();
          if (tempDisplayMode == TEMP_MODE_DATE) {
            loadNumberArrayDate();
          }

          if (tempDisplayMode == TEMP_MODE_TEMP) {
            byte timeProv = 10;
            if (useRTC) timeProv += 1;
            if (useWiFi > 0) timeProv += 2;
            loadNumberArrayTemp(timeProv);
          }

          if (tempDisplayMode == TEMP_MODE_LDR) {
            loadNumberArrayLDR();
          }

          if (tempDisplayMode == TEMP_MODE_VERSION) {
            loadNumberArrayConfInt(SOFTWARE_VERSION, 0);
          }

          if (tempDisplayMode == TEMP_IP_ADDR12) {
            if (useWiFi > 0) {
              loadNumberArrayIP(ourIP[0], ourIP[1]);
            } else {
              // we can't show the IP address if we have the RTC, just skip
              tempDisplayMode++;
            }
          }

          if (tempDisplayMode == TEMP_IP_ADDR34) {
            if (useWiFi > 0) {
              loadNumberArrayIP(ourIP[2], ourIP[3]);
            } else {
              // we can't show the IP address if we have the RTC, just skip
              tempDisplayMode++;
            }
          }

          if (tempDisplayMode == TEMP_IMPR) {
            loadNumberArrayConfInt(lastImpressionsPerSec, 0);
          }

          allFadeOrNormal(DO_NOT_APPLY_LEAD_0_BLANK);

        } else {
          if (slotsMode > SLOTS_MODE_MIN) {
            if (second() == 50) {

              // initialise the slots values
              loadNumberArrayDate();
              transition.setAlternateValues();
              loadNumberArrayTime();
              transition.setRegularValues();
              allFadeOrNormal(DO_NOT_APPLY_LEAD_0_BLANK);

              transition.start(nowMillis);
            }

            // initialise the slots mode
            boolean msgDisplaying;
            switch (slotsMode) {
              case SLOTS_MODE_1M_SCR_SCR:
              {
                msgDisplaying = transition.scrollInScrambleOut(nowMillis);
                break;
              }
            }
              
            // Continue slots
            if (msgDisplaying) {
              transition.updateRegularDisplaySeconds(second());
            } else {
              // no slots mode, check if we are in valueDisplayMode
              if (valueDisplayTime > 0) {
                loadNumberArrayValue();
                loadDisplayConfigValue();
              } else {
                // normal time
                loadNumberArrayTime();
                allFadeOrNormal(APPLY_LEAD_0_BLANK);
              }
            }
          } else {
            // no slots mode, check if we are in valueDisplayMode
            if (valueDisplayTime > 0) {
              loadNumberArrayValue();
              loadDisplayConfigValue();
            } else {
              // normal time
              loadNumberArrayTime();
              allFadeOrNormal(APPLY_LEAD_0_BLANK);
            }
          }
        }
        break;
      }
    case MODE_MINS_SET: {
        if (button1.isButtonPressedAndReleased()) {
          incMins();
        }
        loadNumberArrayTime();
        highlight2and3();
        break;
      }
    case MODE_HOURS_SET: {
        if (button1.isButtonPressedAndReleased()) {
          incHours();
        }
        loadNumberArrayTime();
        highlight0and1();
        break;
      }
    case MODE_SECS_SET: {
        if (button1.isButtonPressedAndReleased()) {
          resetSecond();
        }
        loadNumberArrayTime();
        highlight4and5();
        break;
      }
    case MODE_DAYS_SET: {
        if (button1.isButtonPressedAndReleased()) {
          incDays();
        }
        loadNumberArrayDate();
        highlightDaysDateFormat();
        break;
      }
    case MODE_MONTHS_SET: {
        if (button1.isButtonPressedAndReleased()) {
          incMonths();
        }
        loadNumberArrayDate();
        highlightMonthsDateFormat();
        break;
      }
    case MODE_YEARS_SET: {
        if (button1.isButtonPressedAndReleased()) {
          incYears();
        }
        loadNumberArrayDate();
        highlightYearsDateFormat();
        break;
      }
    case MODE_12_24: {
        if (button1.isButtonPressedAndReleased()) {
          hourMode = ! hourMode;
        }
        loadNumberArrayConfBool(hourMode, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_LEAD_BLANK: {
        if (button1.isButtonPressedAndReleased()) {
          blankLeading = !blankLeading;
        }
        loadNumberArrayConfBool(blankLeading, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_SCROLLBACK: {
        if (button1.isButtonPressedAndReleased()) {
          scrollback = !scrollback;
        }
        loadNumberArrayConfBool(scrollback, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_FADE: {
        if (button1.isButtonPressedAndReleased()) {
          fade = !fade;
        }
        loadNumberArrayConfBool(fade, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_DATE_FORMAT: {
        if (button1.isButtonPressedAndReleased()) {
          dateFormat++;
          if (dateFormat > DATE_FORMAT_MAX) {
            dateFormat = DATE_FORMAT_MIN;
          }
        }
        loadNumberArrayConfInt(dateFormat, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_DAY_BLANKING: {
        if (button1.isButtonPressedAndReleased()) {
          dayBlanking++;
          if (dayBlanking > DAY_BLANKING_MAX) {
            dayBlanking = DAY_BLANKING_MIN;
          }
        }
        loadNumberArrayConfInt(dayBlanking, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_HR_BLNK_START: {
        if (button1.isButtonPressedAndReleased()) {
          blankHourStart++;
          if (blankHourStart > HOURS_MAX) {
            blankHourStart = 0;
          }
        }
        loadNumberArrayConfInt(blankHourStart, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_HR_BLNK_END: {
        if (button1.isButtonPressedAndReleased()) {
          blankHourEnd++;
          if (blankHourEnd > HOURS_MAX) {
            blankHourEnd = 0;
          }
        }
        loadNumberArrayConfInt(blankHourEnd, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_BLANK_MODE: {
        if (button1.isButtonPressedAndReleased()) {
          blankMode++;
          if (blankMode > BLANK_MODE_MAX) {
            blankMode = BLANK_MODE_MIN;
          }
        }
        loadNumberArrayConfInt(blankMode, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_USE_LDR: {
        if (button1.isButtonPressedAndReleased()) {
          useLDR = !useLDR;
        }
        loadNumberArrayConfBool(useLDR, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_FADE_STEPS_UP: {
        if (button1.isButtonPressedAndReleased()) {
          fadeSteps++;
          if (fadeSteps > FADE_STEPS_MAX) {
            fadeSteps = FADE_STEPS_MIN;
          }
        }
        loadNumberArrayConfInt(fadeSteps, currentMode - MODE_12_24);
        displayConfig();
        fadeStep = DIGIT_DISPLAY_COUNT / fadeSteps;
        break;
      }
    case MODE_FADE_STEPS_DOWN: {
        if (button1.isButtonPressedAndReleased()) {
          fadeSteps--;
          if (fadeSteps < FADE_STEPS_MIN) {
            fadeSteps = FADE_STEPS_MAX;
          }
        }
        loadNumberArrayConfInt(fadeSteps, currentMode - MODE_12_24);
        displayConfig();
        fadeStep = DIGIT_DISPLAY_COUNT / fadeSteps;
        break;
      }
    case MODE_DISPLAY_SCROLL_STEPS_DOWN: {
        if (button1.isButtonPressedAndReleased()) {
          scrollSteps--;
          if (scrollSteps < SCROLL_STEPS_MIN) {
            scrollSteps = SCROLL_STEPS_MAX;
          }
        }
        loadNumberArrayConfInt(scrollSteps, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_DISPLAY_SCROLL_STEPS_UP: {
        if (button1.isButtonPressedAndReleased()) {
          scrollSteps++;
          if (scrollSteps > SCROLL_STEPS_MAX) {
            scrollSteps = SCROLL_STEPS_MIN;
          }
        }
        loadNumberArrayConfInt(scrollSteps, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_PIR_TIMEOUT_DOWN: {
        if (button1.isButtonPressedAndReleased()) {
          pirTimeout-=10;
          if (pirTimeout < PIR_TIMEOUT_MIN) {
            pirTimeout = PIR_TIMEOUT_MAX;
          }
        }
        loadNumberArrayConfInt(pirTimeout, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_PIR_TIMEOUT_UP: {
        if (button1.isButtonPressedAndReleased()) {
          pirTimeout+=10;
          if (pirTimeout > PIR_TIMEOUT_MAX) {
            pirTimeout = PIR_TIMEOUT_MIN;
          }
        }
        loadNumberArrayConfInt(pirTimeout, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_SLOTS_MODE: {
        if (button1.isButtonPressedAndReleased()) {
          slotsMode++;
          if (slotsMode > SLOTS_MODE_MAX) {
            slotsMode = SLOTS_MODE_MIN;
          }
        }
        loadNumberArrayConfInt(slotsMode, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_BACKLIGHT_MODE: {
        if (button1.isButtonPressedAndReleased()) {
          backlightMode++;
          if (backlightMode > BACKLIGHT_MAX) {
            backlightMode = BACKLIGHT_MIN;
          }
        }
        loadNumberArrayConfInt(backlightMode, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_RED_CNL: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }

        if (button1.isButtonPressedAndReleased()) {
          redCnl++;
          if (redCnl > COLOUR_CNL_MAX) {
            redCnl = COLOUR_CNL_MIN;
          }
        }
        loadNumberArrayConfInt(redCnl, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_GRN_CNL: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }

        if (button1.isButtonPressedAndReleased()) {
          grnCnl++;
          if (grnCnl > COLOUR_CNL_MAX) {
            grnCnl = COLOUR_CNL_MIN;
          }
        }
        loadNumberArrayConfInt(grnCnl, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_BLU_CNL: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }

        if (button1.isButtonPressedAndReleased()) {
          bluCnl++;
          if (bluCnl > COLOUR_CNL_MAX) {
            bluCnl = COLOUR_CNL_MIN;
          }
        }
        loadNumberArrayConfInt(bluCnl, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_CYCLE_SPEED: {
        if ((backlightMode == BACKLIGHT_CYCLE) || (backlightMode == BACKLIGHT_CYCLE_DIM) || (backlightMode == BACKLIGHT_COLOUR_TIME) || (backlightMode == BACKLIGHT_COLOUR_TIME_DIM))  {
          // Skip if we are in cycle mode
          nextMode++;
          currentMode++;
        }

        if (button1.isButtonPressedAndReleased()) {
          cycleSpeed = cycleSpeed + 2;
          if (cycleSpeed > CYCLE_SPEED_MAX) {
            cycleSpeed = CYCLE_SPEED_MIN;
          }
        }
        loadNumberArrayConfInt(cycleSpeed, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_MIN_DIM_UP: {
        if (button1.isButtonPressedAndReleased()) {
          minDim += 10;
          if (minDim > MIN_DIM_MAX) {
            minDim = MIN_DIM_MAX;
          }
        }
        loadNumberArrayConfInt(minDim, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_MIN_DIM_DOWN: {
        if (button1.isButtonPressedAndReleased()) {
          minDim -= 10;
          if (minDim < MIN_DIM_MIN) {
            minDim = MIN_DIM_MIN;
          }
        }
        loadNumberArrayConfInt(minDim, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_ANTI_GHOST_UP: {
        if (button1.isButtonPressedAndReleased()) {
          antiGhost += 1;
          if (antiGhost > ANTI_GHOST_MAX) {
            antiGhost = ANTI_GHOST_MAX;
          }
          dispCount = DIGIT_DISPLAY_COUNT + antiGhost;
        }
        loadNumberArrayConfInt(antiGhost, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_ANTI_GHOST_DOWN: {
        if (button1.isButtonPressedAndReleased()) {
          antiGhost -= 1;
          if (antiGhost < ANTI_GHOST_MIN) {
            antiGhost = ANTI_GHOST_MIN;
          }
          dispCount = DIGIT_DISPLAY_COUNT + antiGhost;
        }
        loadNumberArrayConfInt(antiGhost, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_USE_PIR_PULLUP: {
        if (button1.isButtonPressedAndReleased()) {
          usePIRPullup = !usePIRPullup;
        }
        loadNumberArrayConfBool(usePIRPullup, currentMode - MODE_12_24);
        digitalWrite(pirPin, usePIRPullup);
        displayConfig();
        break;
      }
    case MODE_TEMP: {
        loadNumberArrayTemp(currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_VERSION: {
        loadNumberArrayConfInt(SOFTWARE_VERSION, currentMode - MODE_12_24);
        displayConfig();
        break;
      }
    case MODE_TUBE_TEST: {
        allNormal();
        loadNumberArrayTestDigits();
        break;
      }
  }
}

// ************************************************************
// output a PWM LED channel, adjusting for dimming and PWM
// brightness:
// rawValue: The raw brightness value between 0 - 255
// ledPWMVal: The pwm factor between 0 - 1
// dimFactor: The dimming value between 0 - 1
// ************************************************************
byte getLEDAdjusted(float rawValue, float ledPWMVal, float dimFactor) {
  byte dimmedPWMVal = (byte)(rawValue * ledPWMVal * dimFactor);
  return dim_curve[dimmedPWMVal];
}

// ************************************************************
// Colour cycling 3: one colour dominates
// ************************************************************
void cycleColours3(int colors[3]) {
  cycleCount++;
  if (cycleCount > cycleSpeed) {
    cycleCount = 0;

    if (changeSteps == 0) {
      changeSteps = random(256);
      currentColour = random(3);
    }

    changeSteps--;

    switch (currentColour) {
      case 0:
        if (colors[0] < 255) {
          colors[0]++;
          if (colors[1] > 0) {
            colors[1]--;
          }
          if (colors[2] > 0) {
            colors[2]--;
          }
        } else {
          changeSteps = 0;
        }
        break;
      case 1:
        if (colors[1] < 255) {
          colors[1]++;
          if (colors[0] > 0) {
            colors[0]--;
          }
          if (colors[2] > 0) {
            colors[2]--;
          }
        } else {
          changeSteps = 0;
        }
        break;
      case 2:
        if (colors[2] < 255) {
          colors[2]++;
          if (colors[0] > 0) {
            colors[0]--;
          }
          if (colors[1] > 0) {
            colors[1]--;
          }
        } else {
          changeSteps = 0;
        }
        break;
    }
  }
}

//**********************************************************************************
//**********************************************************************************
//*                             Utility functions                                  *
//**********************************************************************************
//**********************************************************************************

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void loadNumberArrayTime() {
  NumberArray[5] = second() % 10;
  NumberArray[4] = second() / 10;
  NumberArray[3] = minute() % 10;
  NumberArray[2] = minute() / 10;
  if (hourMode) {
    NumberArray[1] = hourFormat12() % 10;
    NumberArray[0] = hourFormat12() / 10;
  } else {
    NumberArray[1] = hour() % 10;
    NumberArray[0] = hour() / 10;
  }
}

// ************************************************************
// Show the value we got over I2C
// ************************************************************
void loadNumberArrayValue() {
  NumberArray[5] = valueToShow[2] & 0xf;
  NumberArray[4] = valueToShow[2] >> 4;
  NumberArray[3] = valueToShow[1] & 0xf;
  NumberArray[2] = valueToShow[1] >> 4;
  NumberArray[1] = valueToShow[0] & 0xf;
  NumberArray[0] = valueToShow[0] >> 4;
}

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void loadNumberArraySameValue(byte val) {
  NumberArray[5] = val;
  NumberArray[4] = val;
  NumberArray[3] = val;
  NumberArray[2] = val;
  NumberArray[1] = val;
  NumberArray[0] = val;
}

// ************************************************************
// Break the time into displayable digits
// ************************************************************
void loadNumberArrayDate() {
  switch (dateFormat) {
    case DATE_FORMAT_YYMMDD:
      NumberArray[5] = day() % 10;
      NumberArray[4] = day() / 10;
      NumberArray[3] = month() % 10;
      NumberArray[2] = month() / 10;
      NumberArray[1] = (year() - 2000) % 10;
      NumberArray[0] = (year() - 2000) / 10;
      break;
    case DATE_FORMAT_MMDDYY:
      NumberArray[5] = (year() - 2000) % 10;
      NumberArray[4] = (year() - 2000) / 10;
      NumberArray[3] = day() % 10;
      NumberArray[2] = day() / 10;
      NumberArray[1] = month() % 10;
      NumberArray[0] = month() / 10;
      break;
    case DATE_FORMAT_DDMMYY:
      NumberArray[5] = (year() - 2000) % 10;
      NumberArray[4] = (year() - 2000) / 10;
      NumberArray[3] = month() % 10;
      NumberArray[2] = month() / 10;
      NumberArray[1] = day() % 10;
      NumberArray[0] = day() / 10;
      break;
  }
}

// ************************************************************
// Break the temperature into displayable digits
// ************************************************************
void loadNumberArrayTemp(int confNum) {
  NumberArray[5] = (confNum) % 10;
  NumberArray[4] = (confNum / 10) % 10;
  float temp = getRTCTemp();
  int wholeDegrees = int(temp);
  temp = (temp - float(wholeDegrees)) * 100.0;
  int fractDegrees = int(temp);

  NumberArray[3] = fractDegrees % 10;
  NumberArray[2] =  fractDegrees / 10;
  NumberArray[1] =  wholeDegrees % 10;
  NumberArray[0] = wholeDegrees / 10;
}

// ************************************************************
// Break the LDR reading into displayable digits
// ************************************************************
void loadNumberArrayLDR() {
  NumberArray[5] = 0;
  NumberArray[4] = 0;

  NumberArray[3] = (digitOffCount / 1) % 10;
  NumberArray[2] = (digitOffCount / 10) % 10;
  NumberArray[1] = (digitOffCount / 100) % 10;
  NumberArray[0] = (digitOffCount / 1000) % 10;
}

// ************************************************************
// Test digits
// ************************************************************
void loadNumberArrayTestDigits() {
  NumberArray[5] =  second() % 10;
  NumberArray[4] = (second() + 1) % 10;
  NumberArray[3] = (second() + 2) % 10;
  NumberArray[2] = (second() + 3) % 10;
  NumberArray[1] = (second() + 4) % 10;
  NumberArray[0] = (second() + 5) % 10;
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void loadNumberArrayConfInt(int confValue, int confNum) {
  NumberArray[5] = (confNum) % 10;
  NumberArray[4] = (confNum / 10) % 10;
  NumberArray[3] = (confValue / 1) % 10;
  NumberArray[2] = (confValue / 10) % 10;
  NumberArray[1] = (confValue / 100) % 10;
  NumberArray[0] = (confValue / 1000) % 10;
}

// ************************************************************
// Show a boolean configuration value
// ************************************************************
void loadNumberArrayConfBool(boolean confValue, int confNum) {
  int boolInt;
  if (confValue) {
    boolInt = 1;
  } else {
    boolInt = 0;
  }
  NumberArray[5] = (confNum) % 10;
  NumberArray[4] = (confNum / 10) % 10;
  NumberArray[3] = boolInt;
  NumberArray[2] = 0;
  NumberArray[1] = 0;
  NumberArray[0] = 0;
}

// ************************************************************
// Show an integer configuration value
// ************************************************************
void loadNumberArrayIP(byte byte1, byte byte2) {
  NumberArray[5] = (byte2) % 10;
  NumberArray[4] = (byte2 / 10) % 10;
  NumberArray[3] = (byte2 / 100) % 10;
  NumberArray[2] = (byte1) % 10;
  NumberArray[1] = (byte1 / 10) % 10;
  NumberArray[0] = (byte1 / 100) % 10;
}

// ************************************************************
// Do a single complete display, including any fading and
// dimming requested. Performs the display loop
// DIGIT_DISPLAY_COUNT times for each digit, with no delays.
// This is the heart of the display processing!
// ************************************************************
void outputDisplay()
{
  int digitOnTime;
  int digitOffTime;
  int digitSwitchTime;
  float digitSwitchTimeFloat;
  int tmpDispType;

  // used to blank all leading digits if 0
  boolean leadingZeros = true;

  for ( int i = 0 ; i < 6 ; i ++ )
  {
    if (blankTubes) {
      tmpDispType = BLANKED;
    } else {
      tmpDispType = displayType[i];
    }

    switch (tmpDispType) {
      case BLANKED:
        {
          digitOnTime = DIGIT_DISPLAY_NEVER;
          digitOffTime = DIGIT_DISPLAY_ON;
          break;
        }
      case DIMMED:
        {
          digitOnTime = DIGIT_DISPLAY_ON;
          digitOffTime = DIM_VALUE;
          break;
        }
      case BRIGHT:
        {
          digitOnTime = DIGIT_DISPLAY_ON;
          digitOffTime = DIGIT_DISPLAY_OFF;
          break;
        }
      case FADE:
      case NORMAL:
        {
          digitOnTime = DIGIT_DISPLAY_ON;
          digitOffTime = digitOffCount;
          break;
        }
      case BLINK:
        {
          if (blinkState) {
            digitOnTime = DIGIT_DISPLAY_ON;
            digitOffTime = digitOffCount;
          } else {
            digitOnTime = DIGIT_DISPLAY_NEVER;
            digitOffTime = DIGIT_DISPLAY_ON;
          }
          break;
        }
      case SCROLL:
        {
          digitOnTime = DIGIT_DISPLAY_ON;
          digitOffTime = digitOffCount;
          break;
        }
    }

    // Do scrollback when we are going to 0
    if ((NumberArray[i] != currNumberArray[i]) &&
        (NumberArray[i] == 0) &&
        scrollback) {
      tmpDispType = SCROLL;
    }

    // manage fading, each impression we show 1 fade step less of the old
    // digit and 1 fade step more of the new
    // manage fading, each impression we show 1 fade step less of the old
    // digit and 1 fade step more of the new
    if (tmpDispType == SCROLL) {
      digitSwitchTime = DIGIT_DISPLAY_OFF;
      if (NumberArray[i] != currNumberArray[i]) {
        if (fadeState[i] == 0) {
          // Start the fade
          fadeState[i] = scrollSteps;
        }

        if (fadeState[i] == 1) {
          // finish the fade
          fadeState[i] = 0;
          currNumberArray[i] = currNumberArray[i] - 1;
        } else if (fadeState[i] > 1) {
          // Continue the scroll countdown
          fadeState[i] = fadeState[i] - 1;
        }
      }
    } else if (tmpDispType == FADE) {
      if (NumberArray[i] != currNumberArray[i]) {
        if (fadeState[i] == 0) {
          // Start the fade
          fadeState[i] = fadeSteps;
          digitSwitchTime = (int) fadeState[i] * fadeStep;
        }
      }

      if (fadeState[i] == 1) {
        // finish the fade
        fadeState[i] = 0;
        currNumberArray[i] = NumberArray[i];
        digitSwitchTime = DIGIT_DISPLAY_COUNT;
      } else if (fadeState[i] > 1) {
        // Continue the fade
        fadeState[i] = fadeState[i] - 1;
        digitSwitchTime = (int) fadeState[i] * fadeStep;
      }
    } else {
      digitSwitchTime = DIGIT_DISPLAY_COUNT;
      currNumberArray[i] = NumberArray[i];
    }

    for (int timer = 0 ; timer < DIGIT_DISPLAY_COUNT ; timer++) {
      if (timer == DIGIT_DISPLAY_ON) {
        digitOn(i, currNumberArray[i]);
      }

      if  (timer == digitSwitchTime) {
        digitOn(i, NumberArray[i]);
        if (dpArray[i]) {
          // setDP();
        }
      }

      if (timer == digitOffTime) {
        digitOff();
      }
    }
  }

  // Deal with blink, calculate if we are on or off
  blinkCounter++;
  if (blinkCounter == BLINK_COUNT_MAX) {
    blinkCounter = 0;
    blinkState = !blinkState;
  }
}

// ************************************************************
// Set a digit with the given value and turn the HVGen on
// Assumes that all digits have previously been turned off
// by a call to "digitOff"
// ************************************************************
void digitOn(byte digit, byte value) {
  shiftOutDual(LATCHPin, DATA1Pin, DATA2Pin, CLOCKPin, MSBFIRST, data1[digit],digitData[value]);
}

// ************************************************************
// Finish displaying a digit and turn the HVGen off
// ************************************************************
void digitOff() {
  shiftOutDual(LATCHPin, DATA1Pin, DATA2Pin, CLOCKPin, MSBFIRST, 0, 0);
}

// ************************************************************
// Display preset - apply leading zero blanking
// ************************************************************
void applyBlanking() {
  // If we are not blanking, just get out
  if (blankLeading == false) {
    return;
  }

  // We only want to blank the hours tens digit
  if (NumberArray[0] == 0) {
    if (displayType[0] != BLANKED) {
      displayType[0] = BLANKED;
    }
  }
}

// ************************************************************
// Display preset
// ************************************************************
void allFadeOrNormal(boolean blanking) {
  if (fade) {
    allFade();
  } else {
    allNormal();
  }

  if (blanking) {
    applyBlanking();
  }
}

// ************************************************************
// Display preset
// ************************************************************
void allFade() {
  if (displayType[0] != FADE) displayType[0] = FADE;
  if (displayType[1] != FADE) displayType[1] = FADE;
  if (displayType[2] != FADE) displayType[2] = FADE;
  if (displayType[3] != FADE) displayType[3] = FADE;
  if (displayType[4] != FADE) displayType[4] = FADE;
  if (displayType[5] != FADE) displayType[5] = FADE;
}

// ************************************************************
// Display preset
// ************************************************************
void allBright() {
  if (displayType[0] != BRIGHT) displayType[0] = BRIGHT;
  if (displayType[1] != BRIGHT) displayType[1] = BRIGHT;
  if (displayType[2] != BRIGHT) displayType[2] = BRIGHT;
  if (displayType[3] != BRIGHT) displayType[3] = BRIGHT;
  if (displayType[4] != BRIGHT) displayType[4] = BRIGHT;
  if (displayType[5] != BRIGHT) displayType[5] = BRIGHT;
}

// ************************************************************
// highlight years taking into account the date format
// ************************************************************
void highlightYearsDateFormat() {
  switch (dateFormat) {
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
void highlightMonthsDateFormat() {
  switch (dateFormat) {
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
void highlightDaysDateFormat() {
  switch (dateFormat) {
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
void highlight0and1() {
  if (displayType[0] != BRIGHT) displayType[0] = BRIGHT;
  if (displayType[1] != BRIGHT) displayType[1] = BRIGHT;
  if (displayType[2] != DIMMED) displayType[2] = DIMMED;
  if (displayType[3] != DIMMED) displayType[3] = DIMMED;
  if (displayType[4] != DIMMED) displayType[4] = DIMMED;
  if (displayType[5] != DIMMED) displayType[5] = DIMMED;
}

// ************************************************************
// Display preset, highlight digits 2 and 3
// ************************************************************
void highlight2and3() {
  if (displayType[0] != DIMMED) displayType[0] = DIMMED;
  if (displayType[1] != DIMMED) displayType[1] = DIMMED;
  if (displayType[2] != BRIGHT) displayType[2] = BRIGHT;
  if (displayType[3] != BRIGHT) displayType[3] = BRIGHT;
  if (displayType[4] != DIMMED) displayType[4] = DIMMED;
  if (displayType[5] != DIMMED) displayType[5] = DIMMED;
}

// ************************************************************
// Display preset, highlight digits 4 and 5
// ************************************************************
void highlight4and5() {
  if (displayType[0] != DIMMED) displayType[0] = DIMMED;
  if (displayType[1] != DIMMED) displayType[1] = DIMMED;
  if (displayType[2] != DIMMED) displayType[2] = DIMMED;
  if (displayType[3] != DIMMED) displayType[3] = DIMMED;
  if (displayType[4] != BRIGHT) displayType[4] = BRIGHT;
  if (displayType[5] != BRIGHT) displayType[5] = BRIGHT;
}

// ************************************************************
// Display preset
// ************************************************************
void allNormal() {
  if (displayType[0] != NORMAL) displayType[0] = NORMAL;
  if (displayType[1] != NORMAL) displayType[1] = NORMAL;
  if (displayType[2] != NORMAL) displayType[2] = NORMAL;
  if (displayType[3] != NORMAL) displayType[3] = NORMAL;
  if (displayType[4] != NORMAL) displayType[4] = NORMAL;
  if (displayType[5] != NORMAL) displayType[5] = NORMAL;
}

// ************************************************************
// Display preset
// ************************************************************
void displayConfig() {
  if (displayType[0] != BRIGHT) displayType[0] = BRIGHT;
  if (displayType[1] != BRIGHT) displayType[1] = BRIGHT;
  if (displayType[2] != BRIGHT) displayType[2] = BRIGHT;
  if (displayType[3] != BRIGHT) displayType[3] = BRIGHT;
  if (displayType[4] != BLINK)  displayType[4] = BLINK;
  if (displayType[5] != BLINK)  displayType[5] = BLINK;
}

// ************************************************************
// Display preset
// ************************************************************
void displayConfig3() {
  if (displayType[0] != BLANKED) displayType[0] = BLANKED;
  if (displayType[1] != NORMAL) displayType[1] = BRIGHT;
  if (displayType[2] != NORMAL) displayType[2] = BRIGHT;
  if (displayType[3] != NORMAL) displayType[3] = BRIGHT;
  if (displayType[4] != BLINK)  displayType[4] = BLINK;
  if (displayType[5] != BLINK)  displayType[5] = BLINK;
}

// ************************************************************
// Display preset
// ************************************************************
void displayConfig2() {
  if (displayType[0] != BLANKED) displayType[0] = BLANKED;
  if (displayType[1] != BLANKED) displayType[1] = BLANKED;
  if (displayType[2] != NORMAL) displayType[2] = BRIGHT;
  if (displayType[3] != NORMAL) displayType[3] = BRIGHT;
  if (displayType[4] != BLINK)  displayType[4] = BLINK;
  if (displayType[5] != BLINK)  displayType[5] = BLINK;
}

// ************************************************************
// Display preset
// ************************************************************
void allBlanked() {
  if (displayType[0] != BLANKED) displayType[0] = BLANKED;
  if (displayType[1] != BLANKED) displayType[1] = BLANKED;
  if (displayType[2] != BLANKED) displayType[2] = BLANKED;
  if (displayType[3] != BLANKED) displayType[3] = BLANKED;
  if (displayType[4] != BLANKED) displayType[4] = BLANKED;
  if (displayType[5] != BLANKED) displayType[5] = BLANKED;
}

// ************************************************************
// Display preset
// ************************************************************
void loadDisplayConfigValue() {
  displayType[5] = valueDisplayType[2] & 0xf;
  displayType[4] = valueDisplayType[2] >> 4;
  displayType[5] = valueDisplayType[1] & 0xf;
  displayType[2] = valueDisplayType[1] >> 4;
  displayType[1] = valueDisplayType[0] & 0xf;
  displayType[0] = valueDisplayType[0] >> 4;
}

// ************************************************************
// Reset the seconds to 00
// ************************************************************
void resetSecond() {
  byte tmpSecs = 0;
  setTime(hour(), minute(), tmpSecs, day(), month(), year());
  setRTC();
}

// ************************************************************
// increment the time by 1 Sec
// ************************************************************
void incSecond() {
  byte tmpSecs = second();
  tmpSecs++;
  if (tmpSecs >= SECS_MAX) {
    tmpSecs = 0;
  }
  setTime(hour(), minute(), tmpSecs, day(), month(), year());
  setRTC();
}

// ************************************************************
// increment the time by 1 min
// ************************************************************
void incMins() {
  byte tmpMins = minute();
  tmpMins++;
  if (tmpMins >= MINS_MAX) {
    tmpMins = 0;
  }
  setTime(hour(), tmpMins, 0, day(), month(), year());
  setRTC();
}

// ************************************************************
// increment the time by 1 hour
// ************************************************************
void incHours() {
  byte tmpHours = hour();
  tmpHours++;

  if (tmpHours >= HOURS_MAX) {
    tmpHours = 0;
  }
  setTime(tmpHours, minute(), second(), day(), month(), year());
  setRTC();
}

// ************************************************************
// increment the date by 1 day
// ************************************************************
void incDays() {
  byte tmpDays = day();
  tmpDays++;

  int maxDays;
  switch (month())
  {
    case 4:
    case 6:
    case 9:
    case 11:
      {
        maxDays = 31;
        break;
      }
    case 2:
      {
        // we won't worry about leap years!!!
        maxDays = 28;
        break;
      }
    default:
      {
        maxDays = 31;
      }
  }

  if (tmpDays > maxDays) {
    tmpDays = 1;
  }
  setTime(hour(), minute(), second(), tmpDays, month(), year());
  setRTC();
}

// ************************************************************
// increment the month by 1 month
// ************************************************************
void incMonths() {
  byte tmpMonths = month();
  tmpMonths++;

  if (tmpMonths > 12) {
    tmpMonths = 1;
  }
  setTime(hour(), minute(), second(), day(), tmpMonths, year());
  setRTC();
}

// ************************************************************
// increment the year by 1 year
// ************************************************************
void incYears() {
  byte tmpYears = year() % 100;
  tmpYears++;

  if (tmpYears > 50) {
    tmpYears = 15;
  }
  setTime(hour(), minute(), second(), day(), month(), 2000 + tmpYears);
  setRTC();
}

// ************************************************************
// Check the blanking
// ************************************************************
boolean checkBlanking() {
  // Check day blanking, but only when we are in
  // normal time mode
  if (currentMode == MODE_TIME) {
    switch (dayBlanking) {
      case DAY_BLANKING_NEVER:
        return false;
      case DAY_BLANKING_HOURS:
        return getHoursBlanked();
      case DAY_BLANKING_WEEKEND:
        return ((weekday() == 1) || (weekday() == 7));
      case DAY_BLANKING_WEEKEND_OR_HOURS:
        return ((weekday() == 1) || (weekday() == 7)) || getHoursBlanked();
      case DAY_BLANKING_WEEKEND_AND_HOURS:
        return ((weekday() == 1) || (weekday() == 7)) && getHoursBlanked();
      case DAY_BLANKING_WEEKDAY:
        return ((weekday() > 1) && (weekday() < 7));
      case DAY_BLANKING_WEEKDAY_OR_HOURS:
        return ((weekday() > 1) && (weekday() < 7)) || getHoursBlanked();
      case DAY_BLANKING_WEEKDAY_AND_HOURS:
        return ((weekday() > 1) && (weekday() < 7)) && getHoursBlanked();
      case DAY_BLANKING_ALWAYS:
        return true;
    }
  }
}

// ************************************************************
// If we are currently blanked based on hours
// ************************************************************
boolean getHoursBlanked() {
  if (blankHourStart > blankHourEnd) {
    // blanking before midnight
    return ((hour() >= blankHourStart) || (hour() < blankHourEnd));
  } else if (blankHourStart < blankHourEnd) {
    // dim at or after midnight
    return ((hour() >= blankHourStart) && (hour() < blankHourEnd));
  } else {
    // no dimming if Start = End
    return false;
  }
}

// ************************************************************
// Set the tubes and LEDs blanking variables based on blanking mode and 
// blank mode settings
// ************************************************************
void setTubesAndLEDSBlankMode() {
  if (blanked) {
    switch(blankMode) {
      case BLANK_MODE_TUBES:
      {
        blankTubes = true;
        blankLEDs = false;
        break;
      }
      case BLANK_MODE_LEDS:
      {
        blankTubes = false;
        blankLEDs = true;
        break;
      }
      case BLANK_MODE_BOTH:
      {
        blankTubes = true;
        blankLEDs = true;
        break;
      }
    }
  } else {
    blankTubes = false;
    blankLEDs = false;
  }
}

void randomRGBFlash(int delayVal) {
  register2 |= TLED1;
  register2 |= TLED2;
  
  if (random(3) == 0) {
    setAllLEDs(255,0,0);
  }
  if (random(3) == 0) {
    setAllLEDs(0,255,0);
  }
  if (random(3) == 0) {
    setAllLEDs(0,0,255);
  }
  delay(delayVal);
  
  register2 = TLED1;
  register2 |= TLED2;
  
  setAllLEDs(0,0,0);
  delay(delayVal);
}

//**********************************************************************************
//**********************************************************************************
//*                         RTC Module Time Provider                               *
//**********************************************************************************
//**********************************************************************************

// ************************************************************
// Get the time from the RTC
// ************************************************************
void getRTCTime() {
  // Set up the time provider
  // first try to find the RTC, if not available, go into slave mode
  Wire.beginTransmission(RTC_I2C_ADDRESS);
  useRTC = (Wire.endTransmission() == 0);
  if (useRTC) {
    bool PM;
    bool twentyFourHourClock;
    bool century = false;

    byte years = Clock.getYear() + 2000;
    byte months = Clock.getMonth(century);
    byte days = Clock.getDate();
    byte hours = Clock.getHour(twentyFourHourClock, PM);
    byte mins = Clock.getMinute();
    byte secs = Clock.getSecond();
    setTime(hours, mins, secs, days, months, years);

    // Make sure the clock keeps running even on battery
    if (!Clock.oscillatorCheck())
      Clock.enableOscillator(true, true, 0);
  }
}

// ************************************************************
// Set the date/time in the RTC from the internal time
// Always hold the time in 24 format, we convert to 12 in the
// display.
// ************************************************************
void setRTC() {
  if (useRTC) {
    // Set up the time provider
    // first try to find the RTC, if not available, go into slave mode
    Wire.beginTransmission(RTC_I2C_ADDRESS);
    Clock.setClockMode(false); // false = 24h
    Clock.setYear(year() % 100);
    Clock.setMonth(month());
    Clock.setDate(day());
    Clock.setDoW(weekday());
    Clock.setHour(hour());
    Clock.setMinute(minute());
    Clock.setSecond(second());

    Wire.endTransmission();
  }
}

// ************************************************************
// Get the temperature from the RTC
// ************************************************************
float getRTCTemp() {
  if (useRTC) {
    return Clock.getTemperature();
  } else {
    return 0.0;
  }
}

// ************************************************************
// Perform the parallel shift out to the registers
// ************************************************************
void shiftOutDual(uint8_t _loadPin,
                  uint8_t _dataPin1,
                  uint8_t _dataPin2,
                  uint8_t _clockPin,
                  uint8_t _bitOrder,
                  uint8_t _val1,
                  uint8_t _val2) {
  uint8_t i;

  digitalWrite(_loadPin, LOW);
  for (i = 0; i < 8; i++) {
    if (_bitOrder == LSBFIRST) {
      digitalWrite(_dataPin1, !!(_val1 & (1 << i)));
      digitalWrite(_dataPin2, !!(_val2 & (1 << i)));
    }
    else {
      digitalWrite(_dataPin1, !!(_val1 & (1 << (7 - i))));
      digitalWrite(_dataPin2, !!(_val2 & (1 << (7 - i))));
    }
    digitalWrite(_clockPin, HIGH);
    digitalWrite(_clockPin, LOW);
  }
  digitalWrite(_loadPin, HIGH);
}

//**********************************************************************************
//**********************************************************************************
//*                               EEPROM interface                                 *
//**********************************************************************************
//**********************************************************************************

// ************************************************************
// Save current values back to EEPROM
// ************************************************************

// ************************************************************
// read EEPROM values
// ************************************************************

// ************************************************************
// Reset EEPROM values back to what they once were
// ************************************************************
void factoryReset() {
  hourMode = HOUR_MODE_DEFAULT;
  blankLeading = LEAD_BLANK_DEFAULT;
  scrollback = SCROLLBACK_DEFAULT;
  fade = FADE_DEFAULT;
  fadeSteps = FADE_STEPS_DEFAULT;
  dateFormat = DATE_FORMAT_DEFAULT;
  dayBlanking = DAY_BLANKING_DEFAULT;
  dimDark = SENSOR_LOW_DEFAULT;
  scrollSteps = SCROLL_STEPS_DEFAULT;
  dimBright = SENSOR_HIGH_DEFAULT;
  sensorSmoothCountLDR = SENSOR_SMOOTH_READINGS_DEFAULT;
  dateFormat = DATE_FORMAT_DEFAULT;
  dayBlanking = DAY_BLANKING_DEFAULT;
  backlightMode = BACKLIGHT_DEFAULT;
  redCnl = COLOUR_RED_CNL_DEFAULT;
  grnCnl = COLOUR_GRN_CNL_DEFAULT;
  bluCnl = COLOUR_BLU_CNL_DEFAULT;
  blankHourStart = 0;
  blankHourEnd = 7;
  cycleSpeed = CYCLE_SPEED_DEFAULT;
  minDim = MIN_DIM_DEFAULT;
  antiGhost = ANTI_GHOST_DEFAULT;
  pirTimeout = PIR_TIMEOUT_DEFAULT;
  useLDR = USE_LDR_DEFAULT;
  blankMode = BLANK_MODE_DEFAULT;
  slotsMode = SLOTS_MODE_DEFAULT;
  usePIRPullup = USE_PIR_PULLUP_DEFAULT;

  //saveEEPROMValues();
}

//**********************************************************************************
//**********************************************************************************
//*                          Light Dependent Resistor                              *
//**********************************************************************************
//**********************************************************************************

// ******************************************************************
// Check the ambient light through the LDR (Light Dependent Resistor)
// Smooths the reading over several reads.
//
// The LDR in bright light gives reading of around 50, the reading in
// total darkness is around 900.
//
// The return value is the dimming count we are using. 999 is full
// brightness, 100 is very dim.
//
// Because the floating point calculation may return more than the
// maximum value, we have to clamp it as the final step
// ******************************************************************
int getDimmingFromLDR() {
  if (useLDR) {
    int rawLDR = analogRead(LDRPin);
    if (rawLDR < 100) rawLDR = 100;
    if (rawLDR > 1000) rawLDR = 1000;
    int rawSensorVal = 1100 - rawLDR;
    
    double sensorDiff = rawSensorVal - sensorLDRSmoothed;
    sensorLDRSmoothed += (sensorDiff / sensorSmoothCountLDR);

    double sensorSmoothedResult = sensorLDRSmoothed - dimDark;
    if (sensorSmoothedResult < dimDark) sensorSmoothedResult = dimDark;
    if (sensorSmoothedResult > dimBright) sensorSmoothedResult = dimBright;
    sensorSmoothedResult = (sensorSmoothedResult - dimDark) * sensorFactor;

    int returnValue = sensorSmoothedResult * 2;

    if (returnValue < minDim) returnValue = minDim;
    if (returnValue > DIGIT_DISPLAY_OFF) returnValue = DIGIT_DISPLAY_OFF;
    return returnValue;
  } else {
    return DIGIT_DISPLAY_OFF;  
  }
}

void setupDebug() {
  #ifdef DEBUG
  Serial.begin(115200);
  wifiManager.setDebugOutput(true);
  #endif
}

void debugMsg(String message) {
  #ifdef DEBUG
  Serial.println(message);
  #endif
}

void debugMsgCont(String message) {
  #ifdef DEBUG
  Serial.print(message);
  #endif
}
