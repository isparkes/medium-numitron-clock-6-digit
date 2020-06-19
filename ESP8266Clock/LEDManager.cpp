#include "LEDManager.h"

// DIGIT_COUNT Backlights and DIGIT_COUNT underlights
static NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> leds(DIGIT_COUNT * 2, LED_DOUT);

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
void LEDManager::setLDRValue(unsigned int ldrValue)
{
  if (cc->useBLDim) {
    // calculate the PWM factor, goes between current_config.minDim% and 100%
    _ldrDimFactor = (float) (1023 - ldrValue) / (float) 1023.0;
  }
}

// ************************************************************
// Set the pulse current value
// ************************************************************
void LEDManager::setPulseValue(unsigned int secsDelta)
{
  if (cc->useBLPulse) {
    // Calculate the brightness factor based on the "pulse"
    _pwmFactor = (float) secsDelta / (float) 1000.0;
  }
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
    ledRb[i] = red;
    ledGb[i] = green;
    ledBb[i] = blue;
  }
}

// ************************************************************
// Set under light LEDs to the same colour
// ************************************************************
void LEDManager::setUnderlightLEDs(byte red, byte green, byte blue) {
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
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
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
    RgbColor color(ledRb[i], ledGb[i], ledBb[i]);
    leds.SetPixelColor(i, color);
  }
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
    RgbColor color(ledRu[DIGIT_COUNT - 1 - i], ledGu[DIGIT_COUNT - 1 - i], ledBu[DIGIT_COUNT - 1 - i]);
    leds.SetPixelColor(i + DIGIT_COUNT, color);
  }
  leds.Show();
}

// ************************************************************
// Process the options and create a new buffer
// ************************************************************
void LEDManager::processLedStatus() {
  // -------------------------------- Backlights / Underlights -------------------------------

  if (_blanked) {
    setBacklightLEDs( getLEDAdjustedBL(0),
                      getLEDAdjustedBL(0),
                      getLEDAdjustedBL(0));
    setUnderlightLEDs(getLEDAdjustedUL(0),
                      getLEDAdjustedUL(0),
                      getLEDAdjustedUL(0));
  } else {
    switch (cc->backlightMode) {
      case BACKLIGHT_FIXED: {
          setBacklightLEDs( getLEDAdjustedBL(rgb_backlight_curve[cc->redCnl]),
                            getLEDAdjustedBL(rgb_backlight_curve[cc->grnCnl]),
                            getLEDAdjustedBL(rgb_backlight_curve[cc->bluCnl]));
          setUnderlightLEDs(getLEDAdjustedUL(rgb_backlight_curve[cc->redCnl]),
                            getLEDAdjustedUL(rgb_backlight_curve[cc->grnCnl]),
                            getLEDAdjustedUL(rgb_backlight_curve[cc->bluCnl]));
          break;
        }
      case BACKLIGHT_CYCLE: {
          cycleColours3(colors);
          setBacklightLEDs( getLEDAdjustedBL(colors[0]),
                            getLEDAdjustedBL(colors[1]),
                            getLEDAdjustedBL(colors[2]));
          setUnderlightLEDs(getLEDAdjustedUL(colors[0]),
                            getLEDAdjustedUL(colors[1]),
                            getLEDAdjustedUL(colors[2]));
          break;
        }
      case BACKLIGHT_COLOUR_TIME: {
          if (!_syncColourTime) {
            for (byte i = 0 ; i < DIGIT_COUNT ; i++) {
              byte numVal = OutputManager::Instance().getNumberArrayIndexedValue(i);
              ledRb[i] = getLEDAdjustedBL(colourTimeR[numVal]);
              ledGb[i] = getLEDAdjustedBL(colourTimeG[numVal]);
              ledBb[i] = getLEDAdjustedBL(colourTimeB[numVal]);
              ledRu[i] = getLEDAdjustedUL(colourTimeR[numVal]);
              ledGu[i] = getLEDAdjustedUL(colourTimeG[numVal]);
              ledBu[i] = getLEDAdjustedUL(colourTimeB[numVal]);
            }
          }
          break;
        }
      case BACKLIGHT_DAY_OF_WEEK: {
          setBacklightLEDs( getLEDAdjustedBL(dayOfWeekR[_dow]),
                            getLEDAdjustedBL(dayOfWeekG[_dow]),
                            getLEDAdjustedBL(dayOfWeekB[_dow]));
          setUnderlightLEDs(getLEDAdjustedUL(dayOfWeekR[_dow]),
                            getLEDAdjustedUL(dayOfWeekG[_dow]),
                            getLEDAdjustedUL(dayOfWeekB[_dow]));
          break;
        }
      case BACKLIGHT_SWEEP: {
	  // Synchonize the phase with the change of seconds.
	  int sec = second();
	  if (_lastSecond != sec) {
	    _lastSecond = sec;
	    _lastMillis = millis();
	  }
	  float phase = (float)((millis() - _lastMillis) % 1000) / 1000.0;

	  // Select an interesting new color after old color is swept out.
	  // Alternate white and the color selection.
	  if (phase < _lastPhase) {
	    for (int i = 0; i < 3; i++) {
	      _oldColor[i] = _newColor[i];
	    }

	    if (_white++ % 2) {			    // White
	      _newColor[_missingColor] = 0xff;
	      _newColor[_saturatedColor] = 0xff;
	      _newColor[_variableColor] = 0xff;
	    } else {				    // Pick a new color that lies
	      int s = _variableColor;		    // along the rim of the color wheel.
	      _variableColor = _missingColor;
	      _missingColor = _saturatedColor;
	      _saturatedColor = s;
	      if (random(2) == 1) {
		s = _missingColor;
		_missingColor = _variableColor;
		_variableColor = s;
	      }
	      _newColor[_missingColor] = 0;
	      _newColor[_saturatedColor] = 0xff;
	      _newColor[_variableColor] = random(257);
	    }
	  }
	  _lastPhase = phase;

	  // Blend the colors of each backlight corresponding to the phase.
	  for (int i = 0; i < DIGIT_COUNT; i++) {
	    float level;

	    int n, backForth = 0;				// Future config option
	    if (backForth == 1)
	      n = (_white % 2) ? i : DIGIT_COUNT - i - 1;	// Back-and-forth
	    else
	      n = i;						// Left-to-right
	    level = (phase - (float)n / DIGIT_COUNT) * DIGIT_COUNT;
	    if (level < 0)
	      level = 0;
	    if (level > 1)
	      level = 1;

	    RgbColor result;
	    result = RgbColor::LinearBlend(RgbColor(_oldColor[0], _oldColor[1], _oldColor[2]),
					   RgbColor(_newColor[0], _newColor[1], _newColor[2]),
					   level);
	    ledRb[i] = getLEDAdjustedBL(result.R);
	    ledGb[i] = getLEDAdjustedBL(result.G);
	    ledBb[i] = getLEDAdjustedBL(result.B);
	    ledRu[i] = getLEDAdjustedBL(result.R);
	    ledGu[i] = getLEDAdjustedBL(result.G);
	    ledBu[i] = getLEDAdjustedBL(result.B);
	  }
	  break;
	}
      case BACKLIGHT_MOON_PHASE: {
	  // Illuminate a fraction of the backlights corresponding to the phase
	  // of the moon.  We go from right to left as seen in the northern hemisphere.

	  // Once per second processing.
	  int s = second();
	  if (_lastSecond != s) {
	    _lastSecond = s;

	    // Update sun rise/set & moon phase data.
	    if (s == 14) {
	      // Because of inconsistent approaches to Arduino time, we have to
	      // figure out the difference between whatever is residing in now()
	      // and GMT, then add and subtract it to convert to and from GMT.
	      // The value of time() seems even more inscrutable and less useful.
	      time_t t = now();
	      struct tm gmt_tm = *gmtime(&t);
	      gmt_tm.tm_isdst = -1;
	      time_t gmtOffset = mktime(&gmt_tm) - t;

	      sunRise.calculate(cc->latitude, cc->longitude, t + gmtOffset);

	      // Convert returned values from UTC to local time zone.
	      sunRise.queryTime -= gmtOffset;
	      sunRise.riseTime -= gmtOffset;
	      sunRise.setTime -= gmtOffset;

	      // Round to nearest minute.
	      sunRise.riseTime = sunRise.riseTime / 60 * 60 +
				(sunRise.riseTime % 60 < 30 ? 0 : 60);
	      sunRise.setTime = sunRise.setTime / 60 * 60 +
			       (sunRise.setTime % 60 < 30 ? 0 : 60);

	      // Update the moon phase data.
	      moonPhase.calculate(now());

	      // Illuminate the level of each backlight corresponding to the moon phase.
	      float phase = moonPhase.phase;
	      for (int i = 0; i < DIGIT_COUNT; i++) {
		int n = DIGIT_COUNT - i - 1;
		if (phase <= 0.5)
		  _levels[i] = (2 * phase - (float)n / DIGIT_COUNT) * DIGIT_COUNT;
		else
		  _levels[i] = (2 * (1.0 - phase) - (float)i / DIGIT_COUNT) * DIGIT_COUNT;
		if (_levels[i] < 0)
		  _levels[i] = 0;
		if (_levels[i] > 1)
		  _levels[i] = 1;
	      }
	    }

	    // Transient clouds sweep across the moon.
	    //  Clouds have abrupt edges.
	    //
	    //  cloudLocation (left -> right, indicates leading edge of cloud):
	    //    Range: 0 - 7
	    //  cloudSize (size of cloud in digits):
	    //    Range: 1 - 6
	    //  cloudWxTimer (time until next weather change in minutes):
	    //    Range: 1 - 30
	    //  Weather:
	    //   cloudSpeed (digits/sec):
	    //    Range: 0.25 - 4
	    //   cloudDensity (probability of a new cloud per second):
	    //    Range: 5 - 30

	    // Change weather (cloud speed and direction, number of clouds)
	    if (_cloudWxTimer-- <= 0) {
	      if (!_cloudSize) {				  // Wait for cloud to pass.
		_cloudWxTimer = random(1, 31) * 60;	  // Weather changes
		float f = 1 + random(0, 31) / 10.0;	  // Distribute speeds nicely.
		f = random(0, 2) == 1 ? f : 1 / f;
		_cloudSpeed = f * (random(0, 2) == 1 ? 1 : -1);
		_cloudDensity = random(5, 31);		  // Probability of cloud / sec.
	      }
	    }

	    // New cloud
	    if (_cloudSize == 0 && random(_cloudDensity) == 1) {
	      _cloudSize = (float)random(10, 61) / 10.0;
	      _cloudLocation = _cloudSpeed > 0 ? 0 : 7;
	      _cloudMillis = millis();
	    }

	    // Delete cloud
	    if (_cloudSize &&
		_cloudLocation < 0 - _cloudSize || _cloudLocation > 7 + _cloudSize)
	      _cloudSize = 0;
	  }

	  // Move cloud
	  if (_cloudSize) {
	    unsigned long newCloudMillis = millis();
	    _cloudLocation += _cloudSpeed * (newCloudMillis - _cloudMillis) / 1000.0;
	    _cloudMillis = newCloudMillis;
	  }

	  // Calculate cloud opacity per digit.
	  float cloudLevels[DIGIT_COUNT];
	  if (_cloudSize) {
	    float hiRange, loRange;
	    if (_cloudSpeed > 0) {
	      hiRange = _cloudLocation;
	      loRange = _cloudLocation - _cloudSize;
	    } else {
	      loRange = _cloudLocation;
	      hiRange = _cloudLocation + _cloudSize;
	    }
	    for (int i = 0; i < DIGIT_COUNT; i++) {
	      if (hiRange >= i + 1 && loRange <= i)
		cloudLevels[i] = 1;
	      else if (hiRange <= i || loRange >= i + 1)
		cloudLevels[i] = 0;
	      else if (hiRange >= i + 1)
		cloudLevels[i] = (i + 1) - loRange;
	      else
		cloudLevels[i] = hiRange - i;
	    }
	  }

	  // Blend the night or day background color with the moon color
	  // according the the levels determined above.
	  for (int i = 0; i < DIGIT_COUNT; i++) {
	    RgbColor result;
	    if (sunRise.isVisible) {
	      result = RgbColor::LinearBlend(RgbColor(DAY_SKY_COLOR),
						      RgbColor(MOON_COLOR),
						      _levels[i]);
	    } else {
	      result = RgbColor::LinearBlend(RgbColor(NIGHT_SKY_COLOR),
						      RgbColor(MOON_COLOR),
						      _levels[i]);
	    }

	    // Add cloud cover
	    if (_cloudSize) {
	      if (sunRise.isVisible)
		result = RgbColor::LinearBlend(result,
					       RgbColor(DAY_CLOUD_COLOR),
					       cloudLevels[i]);
	      else
		result = RgbColor::LinearBlend(result,
					       RgbColor(NIGHT_CLOUD_COLOR),
					       cloudLevels[i]);
	    }

	    ledRb[i] = getLEDAdjustedBL(result.R);
	    ledGb[i] = getLEDAdjustedBL(result.G);
	    ledBb[i] = getLEDAdjustedBL(result.B);
	    ledRu[i] = getLEDAdjustedBL(result.R);
	    ledGu[i] = getLEDAdjustedBL(result.G);
	    ledBu[i] = getLEDAdjustedBL(result.B);
	  }
          break;
      }
    }
  }

  outputLEDBuffer();
}

// ************************************************************
// output a PWM LED channel, adjusting for dimming, PWM
// and user back light brightness
// ************************************************************
byte LEDManager::getLEDAdjustedBL(byte rawValue) {
  byte dimmedPWMVal;
  if (cc->useBLDim) {
    if (cc->useBLPulse) {
      dimmedPWMVal = (byte)(rawValue * _pwmFactor * _backlightDim * _ldrDimFactor);
    } else {
      dimmedPWMVal = (byte)(rawValue * _backlightDim * _ldrDimFactor);
    }
  } else {
    if (cc->useBLPulse) {
      dimmedPWMVal = (byte)(rawValue * _pwmFactor * _backlightDim);
    } else {
      dimmedPWMVal = (byte)(rawValue * _backlightDim);
    }
  }
  return dim_curve[dimmedPWMVal];
}

// ************************************************************
// output a PWM LED channel, adjusting for dimming, PWM
// and user under light brightness
// ************************************************************
byte LEDManager::getLEDAdjustedUL(byte rawValue) {
  byte dimmedPWMVal;
  if (cc->useBLDim) {
    if (cc->useBLPulse) {
      dimmedPWMVal = (byte)(rawValue * _pwmFactor * _underlightDim * _ldrDimFactor);
    } else {
      dimmedPWMVal = (byte)(rawValue * _underlightDim * _ldrDimFactor);
    }
  } else {
    if (cc->useBLPulse) {
      dimmedPWMVal = (byte)(rawValue * _pwmFactor * _underlightDim);
    } else {
      dimmedPWMVal = (byte)(rawValue * _underlightDim);
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
  for (int i = 0 ; i < DIGIT_COUNT ; i++) {
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
