#include "LEDManager.h"

// 6 Backlights and 6 underlights
static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(PIXEL_COUNT * 2, LED_DOUT);

void LEDManager::setUp()
{
  // Set up the LED output
  leds.Begin();
}

// ************************************************************
// recalculate "slow moving" parameters
// ************************************************************
void LEDManager::recalculateVariables() {
  _backlightDim = (float) cc->backlightDimFactor / (float) 100;
  _underlightDim = (float) cc->extDimFactor / (float) 100;
}

// ************************************************************
// Set the LDR dimming value
// ************************************************************
void LEDManager::setLDRValue(int ldrValue)
{
  if (_ldrValue != ldrValue) {
    _ldrValue = ldrValue;

    // calculate the PWM factor, goes between current_config.minDim% and 100%
    _ldrDimFactor = (float) (1023 - _ldrValue) / (float) 1023.0;
  }
}

// ************************************************************
// Set the pulse current value
// ************************************************************
void LEDManager::setPulseValue(int secsDelta)
{
  // Calculate the brightness factor based on the "pulse"
  _pwmFactor = (float) secsDelta / (float) 1000.0;
}

// ************************************************************
// Set blank status
// ************************************************************
void LEDManager::setBlanked(boolean blanked)
{
  _blanked = blanked;
}

// ************************************************************
// Set back light LEDs to the same colour
// ************************************************************
void LEDManager::setBacklightLEDs(byte red, byte green, byte blue) {
  for (int i = 0 ; i < PIXEL_COUNT ; i++) {
    ledRb[i] = red;
    ledGb[i] = green;
    ledBb[i] = blue;
  }
}

// ************************************************************
// Set under light LEDs to the same colour
// ************************************************************
void LEDManager::setUnderlightLEDs(byte red, byte green, byte blue) {
  for (int i = 0 ; i < PIXEL_COUNT ; i++) {
    ledRu[i] = red;
    ledGu[i] = green;
    ledBu[i] = blue;
  }
}

// ************************************************************
// Set day of week for the 'day of week' backlight mode
// ************************************************************
void LEDManager::setDayOfWeek(byte dow) {
  _dow = dow-1;
}

// ************************************************************
// Put the led buffers out
// ************************************************************
void LEDManager::outputLEDBuffer() {
  for (int i = 0 ; i < PIXEL_COUNT ; i++) {
    RgbColor color(ledRb[i], ledGb[i], ledBb[i]);
    leds.SetPixelColor(i, color);
  }
  for (int i = 0 ; i < PIXEL_COUNT ; i++) {
    RgbColor color(ledRu[i], ledGu[i], ledBu[i]);
    leds.SetPixelColor(i + 6, color);
  }
  leds.Show();
}

// ************************************************************
// Process the options and create a new buffer
// ************************************************************
void LEDManager::processLedStatus() {
  // -------------------------------- Backlights / Underlights -------------------------------

  if (_blanked) {
    setBacklightLEDs( getLEDAdjusted(0, BACKLIGHT),
                      getLEDAdjusted(0, BACKLIGHT),
                      getLEDAdjusted(0, BACKLIGHT));
    setUnderlightLEDs(getLEDAdjusted(0, UNDERLIGHT),
                      getLEDAdjusted(0, UNDERLIGHT),
                      getLEDAdjusted(0, UNDERLIGHT));
  } else {
    switch (cc->backlightMode) {
      case BACKLIGHT_FIXED: {
          setBacklightLEDs( getLEDAdjusted(rgb_backlight_curve[cc->redCnl], BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[cc->grnCnl], BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[cc->bluCnl], BACKLIGHT));
          setUnderlightLEDs(getLEDAdjusted(rgb_backlight_curve[cc->redCnl], UNDERLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[cc->grnCnl], UNDERLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[cc->bluCnl], UNDERLIGHT));
          break;
        }
      case BACKLIGHT_CYCLE: {
          cycleColours3(colors);
          setBacklightLEDs( getLEDAdjusted(colors[0], BACKLIGHT),
                            getLEDAdjusted(colors[1], BACKLIGHT),
                            getLEDAdjusted(colors[2], BACKLIGHT));
          setUnderlightLEDs(getLEDAdjusted(colors[0], UNDERLIGHT),
                            getLEDAdjusted(colors[1], UNDERLIGHT),
                            getLEDAdjusted(colors[2], UNDERLIGHT));
          break;
        }
      case BACKLIGHT_COLOUR_TIME: {
          if (!_syncColourTime) {
            for (int i = 0 ; i < PIXEL_COUNT ; i++) {
              ledRb[i] = getLEDAdjusted(colourTimeR[numberArray[i]], BACKLIGHT);
              ledGb[i] = getLEDAdjusted(colourTimeG[numberArray[i]], BACKLIGHT);
              ledBb[i] = getLEDAdjusted(colourTimeB[numberArray[i]], BACKLIGHT);
              ledRu[i] = getLEDAdjusted(colourTimeR[numberArray[i]], UNDERLIGHT);
              ledGu[i] = getLEDAdjusted(colourTimeG[numberArray[i]], UNDERLIGHT);
              ledBu[i] = getLEDAdjusted(colourTimeB[numberArray[i]], UNDERLIGHT);
            }
          }
          break;
        }
      case BACKLIGHT_DAY_OF_WEEK: {
          setBacklightLEDs( getLEDAdjusted(dayOfWeekR[_dow], BACKLIGHT),
                            getLEDAdjusted(dayOfWeekG[_dow], BACKLIGHT),
                            getLEDAdjusted(dayOfWeekB[_dow], BACKLIGHT));
          setUnderlightLEDs(getLEDAdjusted(dayOfWeekR[_dow], UNDERLIGHT),
                            getLEDAdjusted(dayOfWeekG[_dow], UNDERLIGHT),
                            getLEDAdjusted(dayOfWeekB[_dow], UNDERLIGHT));
          break;
        }
    }
  }

  outputLEDBuffer();
}

// ************************************************************
// output a PWM LED channel, adjusting for dimming and PWM
// brightness:
// ************************************************************
byte LEDManager::getLEDAdjusted(byte rawValue, boolean backOrUnder) {
  byte dimmedPWMVal;
  float floatRaw = (float) rawValue;
  if (cc->useBLDim) {
    if (cc->useBLPulse) {
      if (!backOrUnder) {
        dimmedPWMVal = (byte)(rawValue * _pwmFactor * _backlightDim * _ldrDimFactor);
      } else {
        dimmedPWMVal = (byte)(rawValue * _pwmFactor * _underlightDim * _ldrDimFactor);
      }
    } else {
      if (!backOrUnder) {
        dimmedPWMVal = (byte)(rawValue * _backlightDim * _ldrDimFactor);
      } else {
        dimmedPWMVal = (byte)(rawValue * _underlightDim * _ldrDimFactor);
      }
    }
  } else {
    if (cc->useBLPulse) {
      if (!backOrUnder) {
        dimmedPWMVal = (byte)(rawValue * _pwmFactor * _backlightDim);
      } else {
        dimmedPWMVal = (byte)(rawValue * _pwmFactor * _underlightDim);
      }
    } else {
      if (!backOrUnder) {
        dimmedPWMVal = (byte)(floatRaw * _backlightDim);
      } else {
        dimmedPWMVal = (byte)(rawValue * _underlightDim);
      }
    }
  }
  return dim_curve[dimmedPWMVal];
}

// ************************************************************
// Colour cycling 3: one colour dominates
// ************************************************************
void LEDManager::cycleColours3(int colors[3]) {
  _cycleCount++;
  if (_cycleCount > cc->cycleSpeed) {
    _cycleCount = 0;

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

void LEDManager::setSyncColourTime(boolean value) {
  _syncColourTime = value;
}


// ************************************************************
// Set the diagnostic LED colour - progressively setting the
// LEDs to dignostic colours
// ************************************************************
void LEDManager::setDiagnosticLED(byte stepNumber, byte state) {
  for (int i = 0 ; i < PIXEL_COUNT ; i++) {
    if (i > stepNumber) {
      ledRb[i] = ledGb[i] = ledBb[i] = ledRu[i] = ledGu[i] = ledBu[i] = 0x1f;
    } else if (i == stepNumber) {
      if (state == STATUS_RED) {
        ledRb[i] = 0xff;
        ledGb[i] = 0;
        ledBb[i] = 0;
        ledRu[i] = 0xff;
        ledGu[i] = 0;
        ledBu[i] = 0;
      } else if (state == STATUS_YELLOW) {
        ledRb[i] = 0xff;
        ledGb[i] = 0x7f;
        ledBb[i] = 0x0f;
        ledRu[i] = 0xff;
        ledGu[i] = 0x7f;
        ledBu[i] = 0x0f;
      } else if (state == STATUS_GREEN) {
        ledRb[i] = 0;
        ledGb[i] = 0xff;
        ledBb[i] = 0;
        ledRu[i] = 0;
        ledGu[i] = 0xff;
        ledBu[i] = 0;
      } else if (state == STATUS_BLUE) {
        ledRb[i] = 0;
        ledGb[i] = 0;
        ledBb[i] = 0xff;
        ledRu[i] = 0;
        ledGu[i] = 0;
        ledBu[i] = 0xff;
      }
    }
  }
  outputLEDBuffer();
}
