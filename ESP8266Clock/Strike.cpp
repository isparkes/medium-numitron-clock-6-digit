// Implement Strikes and Alarms for Ian Sparkes' DA-2000 Numitron Clock.
//
// Most of the code will work with any chime/bell/system that can be controlled
// via a single GPIO pin.  This was orginally developed for use with the chime from
// Now & Zen's 'Digital Zen Alarm Clock, version 2.0'.

#include "Arduino.h"
#include "TimeLib.h"
#include "Strike.h"

// Add a new event to the list of strike events.
strikeEvent *
Strike::addEvent(TimeElements *Event,
		 byte strikePattern, byte eventRepeat, byte eventActive) {
  strikeEvent *s = new strikeEvent;

  // Validate the input arguments for web interface.
  // Event->Year is always valid
  if ((Event->Month != WILD && (Event->Month < 1 || Event->Month > 12)) ||
      (Event->Day != WILD && (Event->Day < 1 || Event->Day > 31)) ||
      (Event->Wday != WILD && (Event->Wday < 1 || Event->Wday > 7)) ||
      (Event->Hour != WILD && Event->Hour > 23) ||
      (Event->Minute != WILD && Event->Minute > 59) ||
      (Event->Second != WILD && Event->Second > 59) ||
      (strikePattern < STRIKE_MIN || strikePattern > STRIKE_MAX) ||
      (eventRepeat != EVENT_REPEAT && eventRepeat != EVENT_NOREPEAT) ||
      (eventActive != EVENT_ACTIVE && eventActive != EVENT_INACTIVE))
    return nullptr;

  s->nextEvent = nullptr;
  s->prevEvent = nullptr;

  // Link the new event into the list.
  if (strikeEvents != nullptr) {
    s->nextEvent = strikeEvents;
    strikeEvents->prevEvent = s;
  }
  strikeEvents = s;

  s->Event = *Event;
  s->strikePattern = strikePattern;
  s->eventRepeat = eventRepeat;
  s->eventActive = eventActive;

  // Recalculate next event.
  scheduleEvent();

  return strikeEvents;
}

// Remove an event from the list of strike events.
void
Strike::deleteEvent(strikeEvent *strikeEvent) {

  // Validate that the argument is a pointer to a current strikeEvent.
  // Passing pointers through the web interface requires diligence.
  boolean validated = false;
  for (class strikeEvent *e = strikeEvents; e; e = e->nextEvent) {
    if (strikeEvent == e) {
      validated = true;
      break;
    }
  }
  if (!validated)
    return;

  // If this is the only event, delete the list.
  if (strikeEvent->prevEvent == nullptr && strikeEvent->nextEvent == nullptr)
    strikeEvents = nullptr;

  // If this is the last event, make the previous event the last.
  else if (strikeEvent->nextEvent == nullptr)
    strikeEvent->prevEvent->nextEvent = nullptr;

  // If this is the first event, make the next event the first.
  else if (strikeEvent->prevEvent == nullptr) {
    strikeEvents = strikeEvent->nextEvent;
    strikeEvent->nextEvent->prevEvent = nullptr;
  } else {
  // Otherwise just delete it from the list.
    strikeEvent->prevEvent->nextEvent = strikeEvent->nextEvent;
    strikeEvent->nextEvent->prevEvent = strikeEvent->prevEvent;
  }

  delete strikeEvent;

  // Recalculate next event.
  scheduleEvent();

  return;
}

// Executed each hour and after every event completes, pick the next event
// and make it active.  Only consider events scheduled within the current
// or next hour.  Sets the state variable nextEventMillis.
void
Strike::scheduleEvent() {
  strikeEvent *c = nullptr;
  unsigned int millisToNearestEvent = -1;   // Largest unsigned int

  // Get a fixed reference time for all further operations.
  // We add one because the beginning of the specified second must be in the
  // future and we are already partially through the current second.
  // We add CHIME_CHARGE_MILLIS because we can't operate the striker
  // without that delay.
  time_t t = now() + ((CHIME_CHARGE_MILLIS / 1000) + 1) + 1;

  // Find the next event.
  for (strikeEvent *e = strikeEvents; e; e = e->nextEvent) {
    // Skip events that are inactive or more than two hours away.
    if (e->eventActive == EVENT_INACTIVE)
      continue;
    if (e->Event.Year != WILD && tmYearToCalendar(e->Event.Year) != year(t))
      continue;
    if (e->Event.Month != WILD && e->Event.Month != month(t))
      continue;
    if (e->Event.Day != WILD && e->Event.Day != day(t))
      continue;
    if (e->Event.Wday != WILD && e->Event.Wday != weekday(t))
      continue;
    if (e->Event.Hour != WILD && e->Event.Hour != hour(t) &&
	e->Event.Hour != ((hour(t) + 1) % 24))
      continue;

    // It makes no sense to have a wildcarded second, so we just treat them
    // as a zero.  This modification becomes visible in the GUI.
    if (e->Event.Second == WILD)
      e->Event.Second = 0;

    // Determine how many seconds between t and the event.  A wildcard means
    // we look for the closest possible time greater than t.
    int millisToThisEvent =
      e->Event.Hour == WILD ? 0 : ((int)e->Event.Hour - hour(t) + 24) % 24 * 60;

    millisToThisEvent += e->Event.Minute == WILD ? 0 : (int)e->Event.Minute - minute(t);
    if (millisToThisEvent < 0)
      millisToThisEvent = (millisToThisEvent + 60) % 60;
    millisToThisEvent *= 60;

    millisToThisEvent += (int)e->Event.Second - second(t);
    if (millisToThisEvent < 0)
      millisToThisEvent = (millisToThisEvent + 60) % 60;
    millisToThisEvent *= 1000;

    // If this is a nearer event, pick it.  If it is a tie, the alarm gets precedence.
    if (millisToThisEvent < millisToNearestEvent ||
	(millisToThisEvent == millisToNearestEvent &&
	 e->strikePattern == STRIKE_FIBONACCI)) {
      millisToNearestEvent = millisToThisEvent;
      c = e;
    }
  }

  // If we have found a suitable event, schedule it.  Return the excess charge time
  // we took out above so the strike is not early.
  if (c != nullptr) {
    nextEvent = c;
    nextEventMillis = millisToNearestEvent + millis() + 2000 - CHIME_CHARGE_MILLIS % 1000;
  } else {
    nextEvent = nullptr;
    nextEventMillis = 0;
  }
}

// Indicates whether sub-second timing is required
boolean
Strike::eventHiRes() {
  return eventHiResFlag;
}

// Indicates if strikes have been silenced.
boolean
Strike::silentMode() {
  return silentModeFlag;
}

// Silence an active event.
void
Strike::silence() {
  activeEvent.strikePattern = STRIKE_SILENCED;
}

// Set parameters to enable and disable strikes (but not alarms).
void
Strike::setSilencing(boolean strikeSilenced, boolean strikeLdrSilencing,
		     int strikeLdrSilenceThrsh, int ldrValue,
		     boolean strikeSilenceHours, byte strikeSilenceHourStart,
		     byte strikeSilenceHourEnd) {
		     
  // Check the manual silencing state.
  silentModeFlag = strikeSilenced ? true : false;

  // Check the LDR silencing criteria.
  if (!silentModeFlag && strikeLdrSilencing)
    silentModeFlag = ldrValue > strikeLdrSilenceThrsh ? true : false;

  // Check the fixed time silencing criteria.
  if (!silentModeFlag && strikeSilenceHours) {
    silentModeFlag = ((strikeSilenceHourStart > hour()) ^ (strikeSilenceHourEnd > hour()) ?
		      strikeSilenceHourStart < strikeSilenceHourEnd :
		      strikeSilenceHourStart > strikeSilenceHourEnd);
  }
}

// Return the number of ship's bells for the hour and half-hour.
int
shipsbells(int hour, int minute) {
  int bells;

  bells = ((hour - 1) % 4 + 1) * 2;
  if (minute > 15 && minute < 45)
    bells++;

  if (bells == 9)
    bells = 1;

  return bells;
}

// Return the milliseconds to delay until the next strike, indexed by cumulative strike
// count.
// Fibonnaci series counts and delays.
//  1    2   3   4   5    6    7    8  minute
//  1    1   2   3   5    8   13   13  per minute
//  60  60  30  20  12  7.5  4.6  4.6  seconds between
//  60  60  30  20  12    8    5    5  seconds between (actual)
//
// strike  1-2  3-4  5-7  8-12  13-20 21-46
// delay    60   30   20    12      8     5
unsigned long
fibonacciDelay(int count) {
  if (count >= 21)
    return 5000;
  if (count >= 13)
    return 8000;
  if (count >= 8)
    return 12000;
  if (count >= 5)
    return 20000;
  if (count >= 3)
    return 30000;
  return 60000;
}

// Strike state machine.
// Called once per second, steps through and executes the specified strike
// sequence.  Called every loop() if eventHiResFlag is set.
void
Strike::executeEvent(unsigned long nowMillis) {
  static unsigned long lastMillis = 0;

  // If the time changes excessively, reschedule events.
  if (abs(nowMillis - lastMillis) > 5000)	    // Five seconds
    scheduleEvent();

  lastMillis = nowMillis;

  switch (strikeState) {
    case SSTATE_IDLE:
      if (nextEvent == nullptr)			    // If no scheduled event, return.
	return;
      if (nowMillis - nextEventMillis >= 1000)	    // Wait till within a second.
	return;

      // Initialize and prepare this event to run.
      strikeCount = 0;
      activeEvent = *nextEvent;			    // Make scheduled event active.
      if (nextEvent->eventRepeat == EVENT_NOREPEAT) // Inactivate nonrepeating events.
	nextEvent->eventActive = EVENT_INACTIVE;    // Only saved to SPIFF daily.

      // Silencing option processing
      if (silentModeFlag && activeEvent.strikePattern != STRIKE_FIBONACCI) {
	activeEvent.strikePattern = STRIKE_SILENCED;
	strikeState = SSTATE_OPTIONS;		    // Skip the operational states.
	return;
      }

    case SSTATE_PRECHARGE:
      analogWriteFreq(CHIME_PUMP_FREQ);		    // Run the voltage muliplier.
      analogWrite(CHIME_GPIO, CHIME_PUMP_PWM);

      strikeState = SSTATE_CHARGING;
      eventHiResFlag = true;
      strikeTimer = nowMillis;
      return;

    case SSTATE_CHARGING:
      if (nowMillis - strikeTimer < CHIME_CHARGE_MILLIS)  // Wait until charged.
	return;

      analogWriteFreq(DEFAULT_FREQ);		    // Voltage high to trigger.
      analogWrite(CHIME_GPIO, 1024);

      strikeState = SSTATE_TRIGGER;
      strikeTimer = nowMillis;
      return;

    case SSTATE_TRIGGER:
      if (nowMillis - strikeTimer < CHIME_TRIGGER_MILLIS)  // Wait until triggered.
	return;

      analogWrite(CHIME_GPIO, 0);		    // Bring voltage to ground.

      strikeState = SSTATE_OPTIONS;
      eventHiResFlag = false;
      strikeTimer = nowMillis;
      return;

    case SSTATE_OPTIONS:		      // Implement various strike options.
	switch (activeEvent.strikePattern) {
	  case STRIKE_SINGLE:
	    break;

	  case STRIKE_HOURS:
	    if (++strikeCount >= hourFormat12())
	      break;

	    strikeState = SSTATE_PRECHARGE;
	    eventHiResFlag = true;
	    return;

	  case STRIKE_SHIPS:
	    if (++strikeCount >= shipsbells(hour(), minute()))
		break;

	    strikeState = SSTATE_PRECHARGE;
	    eventHiResFlag = true;
	    return;

	  case STRIKE_FIBONACCI:
	    if (++strikeCount >= STRIKE_FIBONACCI_END)
	      break;

	    strikeState = SSTATE_WAIT;
	    strikeTimer = nowMillis;
	    return;

	  case STRIKE_SILENCED:
	    strikeState = SSTATE_WAIT;
	    strikeTimer = nowMillis;
	    return;
	}

	// Event finished; select next event.
	strikeState = SSTATE_IDLE;
	scheduleEvent();
	return;

    case SSTATE_WAIT:			    // Provide delays between strikes.
      switch (activeEvent.strikePattern) {
	case STRIKE_FIBONACCI:
	  if (nowMillis - strikeTimer < fibonacciDelay(strikeCount) - CHIME_CHARGE_MILLIS)
	    return;

	  strikeState = SSTATE_PRECHARGE;
	  eventHiResFlag = true;
	  return;

	case STRIKE_SILENCED:		    // Avoid immediately rescheduling something.
	  if (nowMillis - strikeTimer < 3000) // Wait three seconds
	    return;

	default:			    // STRIKE_SILENCED falls through to here.
	  strikeState = SSTATE_IDLE;
	  scheduleEvent();
	  return;
      }
  }
}

// Return successive strikeEvents.
// First event is returned with nullptr as the argument.
// Last event returns nullptr.
strikeEvent *
Strike::getNextEvent(strikeEvent *strikeEvent) {
  if (strikeEvent == nullptr)
    return(strikeEvents);

  // Validate that the argument is a pointer to a current strikeEvent.
  boolean validated = false;
  for (class strikeEvent *e = strikeEvents; e; e = e->nextEvent) {
    if (strikeEvent == e) {
      validated = true;
      break;
    }
  }
  if (!validated)
    return(nullptr);

  return(strikeEvent->nextEvent);
}
