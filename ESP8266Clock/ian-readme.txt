Tonight's festivities consisted of trying to compensate for the Numitrons'
"assymetrical" behavior, i.e. segments turn off faster than they turn on. I
think I accomplished that, with some reservations.

I grabbed a convenient switch on the UI to use for A-B comparison of
"original" and "Ty" display handling. Set Backlight to "Fixed RGB backlight,
no dimming" to see "Ty way", any other backlight setting for "original way".

It's a dirty hack right now for proof of concept. I can make it nice but I
don't want to put any more time into it until I hear from you. For all I know
you may hate it!

Starts at Line 1522 in ESP8266Clock.ino

Pro:
 - I think it looks clean

Con:
 - Adds code that is processed on every iteration of main loop. (However, it
   does not seem to affect impressions per minute - still 95 - 97)
 - Haven't even considered what effect this may have on all the other Modes.
 - It throws the display back out of sync with the LEDs but this can be fixed.
 - It's sort of timing dependant and for all I know a different batch of tubes
   may have a different "time constant".
