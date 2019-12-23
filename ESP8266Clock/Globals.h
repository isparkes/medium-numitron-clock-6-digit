#ifndef __ESP_GLOBALS_H__
#define __ESP_GLOBALS_H__

#include "HtmlServer.h"
#include "DisplayDefs.h"
#include "ClockDefs.h"
#include "DA2000-Transition.h"
#include "ESP_DS1307.h"
#include "ClockButton.h"

// ----------------------- Components ----------------------------

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(DIGIT_COUNT, LED_DOUT);
ClockButton button1(inputPin1, CLOCK_BUTTON_ACTIVE_LO);

Transition transitionWipe(800, 700, 2800, SLOTS_MODE_WIPE_WIPE);  // Wipe In / Wipe Out (DA2000-Transition.h)  
Transition transitionBang(400, 400, 3200, SLOTS_MODE_BANG_BANG);  // Bang In / Bang Out (DA2000-Transition.h)  
Transition transitionDummy(0, 0, 0, SLOTS_MODE_NONE);             // Dummy transition for null pointer prevention
Transition *activeTransition = &transitionDummy;                  // Pointer to selected transition object

// ------------------ Display Variables ----------------

byte numberArray[DIGIT_COUNT]          = {0, 0, 0, 0, 0, 0};
byte displayType[DIGIT_COUNT]          = {NORMAL, NORMAL, NORMAL, NORMAL};
byte valueDisplayTime                  = 0;
int valueToShow                        = 0;
byte valueDisplayType[DIGIT_COUNT / 2] = {NORMAL | NORMAL << 4, NORMAL | NORMAL << 4, NORMAL | NORMAL << 4}; // All normal by default
byte preheatCounter                    = 0;
byte preheatStrength                   = 3;

// ------------- Time management variables -------------

unsigned long nowMillis = 0;
unsigned long lastCheckMillis = 0;
unsigned long lastSecMillis = nowMillis;
unsigned long lastSecond = second();  
boolean secondsChanged = false;

byte currentMode = MODE_TIME;   // Initial cold start mode
byte nextMode = currentMode;
boolean triggeredThisSec = false;

unsigned int tempDisplayModeDuration;      // time for the end of the temporary display
int  tempDisplayMode;

// ---------------- Backlight management ---------------

int colors[3];
byte cycleCount = 0;
byte ledR[DIGIT_COUNT];
byte ledG[DIGIT_COUNT];
byte ledB[DIGIT_COUNT];
boolean syncColourTime;

// ------------------ Usage statistics -----------------

int impressionsPerSec = 0;
int lastImpressionsPerSec = 0;

// ----------------- Real time clock -------------------

byte useRTC = false;  // true if we detect an RTC
boolean onceHadAnRTC = false;

// ----------------- Motion detector -------------------

unsigned long pirLastSeen = 0;
boolean pirInstalled = false;
int pirConsecutiveCounts = 300;  // set with a high value to stop the PIR being falsely detected the first time round the loop
boolean pirStatus = false;

// --------------------- Blanking ----------------------

boolean blanked = false;
byte blankSuppressStep = 0;    // The press we are on: 1 press = suppress for 1 min, 2 press = 1 hour, 3 = 1 day
unsigned long blankSuppressedMillis = 0;   // The end time of the blanking, 0 if we are not suppressed
unsigned long blankSuppressedSelectionTimoutMillis = 0;   // Used for determining the end of the blanking period selection timeout
boolean blankTubes = false;
boolean blankLEDs = false;

// --------------- Ambient light dimming ---------------

double sensorLDRSmoothed = 0;
double sensorFactor = (double)SENSOR_SENSIT_DEFAULT / 100.0;
int ldrValue = 0;
int lastLDRValue = 0;

boolean blinkState;

// ------------------- LED management ------------------

boolean upOrDown;

// Used to control the separator LEDs
boolean led1State;
boolean led2State;
boolean ledLState;
boolean ledRState;

// Strategy 3
int changeSteps = 0;
byte currentColour = 0;

#endif
