#ifndef Strike_h
#define Strike_h

#include "Arduino.h"
#include "TimeLib.h"

/*
 * This code drives the strike mechanism of the "Now & Zen" "Digital Zen Alarm Clock",
 * version 2.0, when run from Ian Sparkes' "DA-2000 Numitron Clock".
 *
 * A small interface circuit must be added to the Digital Zen chime circuit, the
 * diagram and instructions for which can be found at ...
 *
 * It takes a few seconds to charge the strike circuit sufficiently to activate
 * the linear motor, so strikes must be scheduled in advance and cannot
 * be repeated more rapidly than the charging interval.
 */

// Strike Pattern
// Fibonacci is intended as an alarm pattern.
#define STRIKE_MIN	  1
#define STRIKE_SINGLE	  1		    // Once
#define STRIKE_HOURS	  2		    // Strike the hour
#define	STRIKE_SHIPS	  3		    // Strike Ship's Bells
#define STRIKE_FIBONACCI  4		    // Increasing Fibonacci numbers (per min)
#define	STRIKE_SILENCED	  5		    // Silenced
#define STRIKE_MAX	  5
#define STRIKE_DEFAULT	STRIKE_SINGLE

#define STRIKE_FIBONACCI_END 46		    // Stop after this many strikes

// Event Repeats
#define EVENT_REPEAT		1	    // Disable event after occurance
#define EVENT_NOREPEAT		2	    // Event remains active
#define EVENT_REPEAT_DEFAULT	EVENT_REPEAT

// Event Active/Inactive
#define	EVENT_ACTIVE	1		    // Event is active
#define EVENT_INACTIVE	2		    // Event is inactive

#define WILD  (uint8_t)-1		    // Wildcard representation in date/time

// List of scheduled events.
class strikeEvent {
  public:
    TimeElements Event;
    byte eventRepeat = EVENT_REPEAT_DEFAULT;
    byte eventActive = EVENT_INACTIVE;
    byte strikePattern = STRIKE_DEFAULT;
    strikeEvent *prevEvent = nullptr;
    strikeEvent *nextEvent = nullptr;

  private:
};

// Event processing state machine
#define	SSTATE_IDLE	  0
#define SSTATE_PRECHARGE  1
#define SSTATE_CHARGING	  2
#define SSTATE_TRIGGER	  3
#define SSTATE_OPTIONS	  4
#define SSTATE_WAIT	  5

// Manage the events and hardware associated with the chime/strike function.
class Strike {
  public:
    strikeEvent *addEvent(TimeElements *Event, byte strikePattern, byte eventRepeat,
			  byte eventActive);
    strikeEvent *getNextEvent(strikeEvent *);
    boolean deleteEvent(strikeEvent *strikeEvent);
    void executeEvent(unsigned long nowMillis);
    boolean eventHiRes();
    boolean silentMode();
    void silence();
    void scheduleEvent();
    void setSilencing(boolean strikeSilenced, boolean strikeLdrSilencing,
		      int strikeLdrSilenceThrsh, int ldrValue,
		      boolean strikeSilenceHours, byte strikeSilenceHourStart,
		      byte StrikeSilenceHourEnd);

  private:
    class strikeEvent *strikeEvents = nullptr;	// Pointer to linked list of events
    class strikeEvent *nextEvent = nullptr;	// Pointer to the next scheduled event
    class strikeEvent activeEvent;	// Contents of current event
    unsigned long nextEventMillis = 0;	// Time of next scheduled event
    boolean eventHiResFlag = false;	// Enable sub-second timing
    boolean silentModeFlag = false;	// Disable strikes (but not alarms)
    byte strikeState = SSTATE_IDLE;	// State machine current state
    unsigned long strikeTimer;		// State machine internal timer
    byte strikeCount = 0;		// Number of strikes struck
};

// GPIO/chime electronics interface configuration.
#define	CHIME_GPIO		15	// GPIO15 is present on Ian's header
#define CHIME_CHARGE_MILLIS	2500    // Voltage multiplier charge time
#define	CHIME_PUMP_FREQ		4096    // Voltage multiplier frequency
#define CHIME_PUMP_PWM		196	// Voltage multiplier PWM
#define	DEFAULT_FREQ		1024	// Default frequency
#define CHIME_TRIGGER_MILLIS	50	// Discharge trigger hold time

#endif
