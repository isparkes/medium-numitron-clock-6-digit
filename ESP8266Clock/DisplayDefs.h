#ifndef DisplayDefs_h
#define DisplayDefs_h

#define DIAGS_START       0
#define DIAGS_SPIFFS      1
#define DIAGS_WIFI        2
#define DIAGS_NTP         3
#define DIAGS_RTC         4
#define DIAGS_DEBUG       5

// added to allow display to show when we enter AP MODE - not a Diags step!
#define AP_MODE           6

#define LED_MODE_MIN        0
#define LED_RAILROAD        0
#define LED_RAILROAD_X      1
#define LED_BLINK_SLOW      2
#define LED_BLINK_FAST      3
#define LED_BLINK_DBL       4
#define LED_LUNAR	    5
#define LED_ON              6
#define LED_OFF             7
#define LED_BLINK_DEFAULT   LED_RAILROAD
#define LED_MODE_MAX        7

#define STATUS_LED_MODE_MIN    0
#define STATUS_LED_MODE_NTP    0
#define STATUS_LED_MODE_ONLINE 1
#define STATUS_LED_MODE_AM_PM  2
#define STATUS_LED_MODE_PIR    3
#define STATUS_LED_MODE_OFF    4
#define STATUS_LED_MODE_ON     5
#define STATUS_LED_MODE_MAX    5

#endif
