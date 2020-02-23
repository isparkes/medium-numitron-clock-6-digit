#include "LEDManager.h"

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(DIGIT_COUNT, LED_DOUT);

void LEDManager::setUp()
{
  // Set up the LED output
  leds.Begin();
}

// ************************************************************
// recalculae "slow moving" parameters
// ************************************************************
void LEDManager::recalculateVariables() {
  _backlightDim = (float) current_config.backlightDimFactor / (float) 100;
  _underlightDim = (float) current_config.extDimFactor / (float) 100;
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
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
    ledR[i] = red;
    ledG[i] = green;
    ledB[i] = blue;
  }
}

// ************************************************************
// Set under light LEDs to the same colour
// ************************************************************
//void LEDManager::setUnderlightLEDs(byte red, byte green, byte blue) {
//  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
//    ledR[i + DIGIT_COUNT] = red;
//    ledG[i + DIGIT_COUNT] = green;
//    ledB[i + DIGIT_COUNT] = blue;
//  }
//}

// ************************************************************
// Put the led buffers out
// ************************************************************
void LEDManager::outputLEDBuffer() {
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
    int idx = (DIGIT_COUNT - 1) - i;
    RgbColor color(ledR[i], ledG[i], ledB[i]);
    leds.SetPixelColor(idx, color);
  }
  leds.Show();
}

// ************************************************************
// Process the options and create a new buffer
// ************************************************************
void LEDManager::processLedStatus() {
  // -------------------------------- Backlights / Underlights -------------------------------

  if (1 == 2) {
    setBacklightLEDs( getLEDAdjusted(0, NO_DIM, NO_PULSE, BACKLIGHT),
                      getLEDAdjusted(0, NO_DIM, NO_PULSE, BACKLIGHT),
                      getLEDAdjusted(0, NO_DIM, NO_PULSE, BACKLIGHT));
//    setUnderlightLEDs(getLEDAdjusted(0, NO_DIM, NO_PULSE, UNDERLIGHT),
//                      getLEDAdjusted(0, NO_DIM, NO_PULSE, UNDERLIGHT),
//                      getLEDAdjusted(0, NO_DIM, NO_PULSE, UNDERLIGHT));
  } else {
    switch (current_config.backlightMode) {
      case BACKLIGHT_FIXED: {
          setBacklightLEDs( getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], NO_DIM, NO_PULSE, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], NO_DIM, NO_PULSE, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], NO_DIM, NO_PULSE, BACKLIGHT));
//          setUnderlightLEDs(getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], NO_DIM, NO_PULSE, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], NO_DIM, NO_PULSE, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], NO_DIM, NO_PULSE, UNDERLIGHT));
          break;
        }
      case BACKLIGHT_PULSE: {
          setBacklightLEDs( getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], NO_DIM, PULSED, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], NO_DIM, PULSED, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], NO_DIM, PULSED, BACKLIGHT));
//          setUnderlightLEDs(getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], NO_DIM, PULSED, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], NO_DIM, PULSED, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], NO_DIM, PULSED, UNDERLIGHT));
          break;
        }
      case BACKLIGHT_FIXED_DIM: {
          setBacklightLEDs( getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], DIMMED, NO_PULSE, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], DIMMED, NO_PULSE, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], DIMMED, NO_PULSE, BACKLIGHT));
//          setUnderlightLEDs(getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], DIMMED, NO_PULSE, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], DIMMED, NO_PULSE, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], DIMMED, NO_PULSE, UNDERLIGHT));
          break;
        }
      case BACKLIGHT_PULSE_DIM: {
          setBacklightLEDs( getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], DIMMED, PULSED, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], DIMMED, PULSED, BACKLIGHT),
                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], DIMMED, PULSED, BACKLIGHT));
//          setUnderlightLEDs(getLEDAdjusted(rgb_backlight_curve[current_config.redCnl], DIMMED, PULSED, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.grnCnl], DIMMED, PULSED, UNDERLIGHT),
//                            getLEDAdjusted(rgb_backlight_curve[current_config.bluCnl], DIMMED, PULSED, UNDERLIGHT));
          break;
        }
      case BACKLIGHT_CYCLE: {
          cycleColours3(colors);
          setBacklightLEDs( getLEDAdjusted(colors[0], NO_DIM, NO_PULSE, BACKLIGHT),
                            getLEDAdjusted(colors[1], NO_DIM, NO_PULSE, BACKLIGHT),
                            getLEDAdjusted(colors[2], NO_DIM, NO_PULSE, BACKLIGHT));
//          setUnderlightLEDs(getLEDAdjusted(colors[0], NO_DIM, NO_PULSE, UNDERLIGHT),
//                            getLEDAdjusted(colors[1], NO_DIM, NO_PULSE, UNDERLIGHT),
//                            getLEDAdjusted(colors[2], NO_DIM, NO_PULSE, UNDERLIGHT));
          break;
        }
      case BACKLIGHT_CYCLE_DIM: {
          cycleColours3(colors);
          setBacklightLEDs( getLEDAdjusted(colors[0], NO_DIM, PULSED, BACKLIGHT),
                            getLEDAdjusted(colors[1], NO_DIM, PULSED, BACKLIGHT),
                            getLEDAdjusted(colors[2], NO_DIM, PULSED, BACKLIGHT));
//          setUnderlightLEDs(getLEDAdjusted(colors[0], NO_DIM, PULSED, UNDERLIGHT),
//                            getLEDAdjusted(colors[1], NO_DIM, PULSED, UNDERLIGHT),
//                            getLEDAdjusted(colors[2], NO_DIM, PULSED, UNDERLIGHT));
          break;
        }
      case BACKLIGHT_COLOUR_TIME: {
          if (!_syncColourTime) {
            for (int i = 0 ; i < DIGIT_COUNT ; i++) {
              ledR[DIGIT_COUNT - 1 - i] = getLEDAdjusted(colourTimeR[numberArray[i]], NO_DIM, NO_PULSE, BACKLIGHT);
              ledG[DIGIT_COUNT - 1 - i] = getLEDAdjusted(colourTimeG[numberArray[i]], NO_DIM, NO_PULSE, BACKLIGHT);
              ledB[DIGIT_COUNT - 1 - i] = getLEDAdjusted(colourTimeB[numberArray[i]], NO_DIM, NO_PULSE, BACKLIGHT);
//              ledR[DIGIT_COUNT + 5 - i] = getLEDAdjusted(colourTimeR[numberArray[i]], NO_DIM, NO_PULSE, UNDERLIGHT);
//              ledG[DIGIT_COUNT + 5 - i] = getLEDAdjusted(colourTimeG[numberArray[i]], NO_DIM, NO_PULSE, UNDERLIGHT);
//              ledB[DIGIT_COUNT + 5 - i] = getLEDAdjusted(colourTimeB[numberArray[i]], NO_DIM, NO_PULSE, UNDERLIGHT);
            }
          }
          break;
        }
      case BACKLIGHT_COLOUR_TIME_DIM: {
          for (int i = 0 ; i < DIGIT_COUNT ; i++) {
              ledR[DIGIT_COUNT - 1 - i] = getLEDAdjusted(colourTimeR[numberArray[i]], DIMMED, NO_PULSE, BACKLIGHT);
              ledG[DIGIT_COUNT - 1 - i] = getLEDAdjusted(colourTimeG[numberArray[i]], DIMMED, NO_PULSE, BACKLIGHT);
              ledB[DIGIT_COUNT - 1 - i] = getLEDAdjusted(colourTimeB[numberArray[i]], DIMMED, NO_PULSE, BACKLIGHT);
//              ledR[DIGIT_COUNT + 5 - i] = getLEDAdjusted(colourTimeR[numberArray[i]], DIMMED, NO_PULSE, UNDERLIGHT);
//              ledG[DIGIT_COUNT + 5 - i] = getLEDAdjusted(colourTimeG[numberArray[i]], DIMMED, NO_PULSE, UNDERLIGHT);
//              ledB[DIGIT_COUNT + 5 - i] = getLEDAdjusted(colourTimeB[numberArray[i]], DIMMED, NO_PULSE, UNDERLIGHT);
          }
        }
    }
  }

  outputLEDBuffer();
}

// ************************************************************
// output a PWM LED channel, adjusting for dimming and PWM
// brightness:
// ************************************************************
byte LEDManager::getLEDAdjusted(byte rawValue, boolean dimmed, boolean usePulse, boolean backOrUnder) {
  byte dimmedPWMVal;
  if (dimmed) {
    if (usePulse) {
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
    if (usePulse) {
      if (!backOrUnder) {
        dimmedPWMVal = (byte)(rawValue * _pwmFactor * _backlightDim);
      } else {
        dimmedPWMVal = (byte)(rawValue * _pwmFactor * _underlightDim);
      }
    } else {
      if (!backOrUnder) {
        dimmedPWMVal = (byte)(rawValue * _backlightDim);
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
  if (_cycleCount > current_config.cycleSpeed) {
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
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
    if (i > stepNumber) {
      ledR[i] = ledG[i] = ledB[i] = 0x1f;
    } else if (i == stepNumber) {
      if (state == STATUS_RED) {
        ledR[i] = 0xff;
        ledG[i] = 0;
        ledB[i] = 0;
      } else if (state == STATUS_YELLOW) {
        ledR[i] = 0xff;
        ledG[i] = 0x7f;
        ledB[i] = 0x0f;
      } else if (state == STATUS_GREEN) {
        ledR[i] = 0;
        ledG[i] = 0xff;
        ledB[i] = 0;
      } else if (state == STATUS_BLUE) {
        ledR[i] = 0;
        ledG[i] = 0;
        ledB[i] = 0xff;
      }
    }
  }
  outputLEDBuffer();
}
