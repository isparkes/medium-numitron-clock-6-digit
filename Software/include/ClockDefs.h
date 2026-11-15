//**********************************************************************************
//* Definitions for the basic functions of the clock                               *
//**********************************************************************************

#ifndef ClockDefs_h
#define ClockDefs_h

// ************************** Pin Allocations *************************

#define pirPin      2     // D4
#define LDRPin      A0    // ADC input

// -------------------------------------------------------------------------------
// Software version shown in config menu
#define SOFTWARE_VERSION      7

// -------------------------------------------------------------------------------
#define SENSOR_DARK_MIN     100
#define SENSOR_DARK_MAX     600
#define SENSOR_DARK_DEFAULT 450

#define SENSOR_BRIGHT_MIN     600
#define SENSOR_BRIGHT_MAX     1023
#define SENSOR_BRIGHT_DEFAULT 1023

#define SENSOR_FACTOR_MIN     50
#define SENSOR_FACTOR_MAX     200
#define SENSOR_FACTOR_DEFAULT 100

#define SENSOR_SMOOTH_READINGS_DEFAULT 500  // Speed at which the brighness adapts to changes

#define SET_DIM_DEFAULT 1023

// -------------------------------------------------------------------------------
#define SECS_MAX  60
#define MINS_MAX  60
#define HOURS_MAX 24

// -------------------------------------------------------------------------------
// Clock modes - normal running is MODE_TIME, other modes accessed by a middle length ( 1S < press < 2S ) button press
#define MODE_MIN                        0
#define MODE_TIME                       0

// -------------------------------------------------------------------------------
// Time setting, need all six digits, so no flashing mode indicator
#define MODE_HOURS_SET                  1
#define MODE_MINS_SET                   2
#define MODE_SECS_SET                   3
#define MODE_DAYS_SET                   4
#define MODE_MONTHS_SET                 5
#define MODE_YEARS_SET                  6

#define MODE_12_24                      7  // 0 = 24, 1 = 12
#define HOUR_MODE_DEFAULT               false
#define MODE_LEAD_BLANK                 8  // 1 = blanked
#define LEAD_BLANK_DEFAULT              false
#define MODE_DATE_FORMAT                9  // 
#define MODE_DAY_BLANKING               10 // 
#define MODE_HR_BLNK_START              11 // skipped if not using hour blanking
#define MODE_HR_BLNK_END                12 // skipped if not using hour blanking

#define MODE_BLANK_MODE                 13 // 
#define MODE_BLANK_MODE_DEFAULT         BLANK_MODE_BOTH

#define MODE_USE_LDR                    14 // 1 = use LDR, 0 = don't (and have 100% brightness)
#define MODE_USE_LDR_DEFAULT            true

#define MODE_LDR_THRSH                  15 // 
#define MODE_LDR_SENS                   16 // 

#define MODE_SLOTS_MODE                 17 // 
#define MODE_SLOTS_FREQ                 18 // 

#define MODE_LED_BLINK                  19 // 

#define MODE_PIR_TIMEOUT_UP             20 // 
#define MODE_PIR_TIMEOUT_DOWN           21 // 

#define MODE_BACKLIGHT_MODE             22 // 
#define MODE_RED_CNL                    23 // 
#define MODE_GRN_CNL                    24 // 
#define MODE_BLU_CNL                    25 // 
#define MODE_CYCLE_SPEED                26 // 

#define MODE_BL_DIM                     27 // 
#define MODE_UL_DIM                     28 // 

#define MODE_SEP_DIM                    29 // 

#define MODE_L_IND_MODE                 30 // 
#define MODE_R_IND_MODE                 31 // 
#define MODE_DO_NOT_DIM_INDS            32 // 

// Use the PIR pin pullup resistor
#define MODE_USE_PIR_PULLUP             33 // 1 = use Pullup
#define USE_PIR_PULLUP_DEFAULT          true

#define MODE_PREHEAT                    34 // 

// Software Version
#define MODE_VERSION                    35 // 

// Tube test - all six digits, so no flashing mode indicator
#define MODE_TUBE_TEST                  36

#define MODE_MAX                        36

#define TIME_SET_MODE_DUR_MS            10000

// -------------------------------------------------------------------------------
// Temporary display modes - accessed by a short press ( < 1S ) on the button when in MODE_TIME
#define TEMP_MODE_MIN                   0
#define TEMP_MODE_DATE                  0 // Display the date for 5 S
#define TEMP_MODE_LDR                   1 // Display the normalised LDR reading for 5S, returns a value from 100 (dark) to 999 (bright)
#define TEMP_MODE_VERSION               2 // Display the version for 5S
#define TEMP_IP_ADDR12                  3 // IP xxx.yyy.zzz.aaa: xxx.yyy
#define TEMP_IP_ADDR34                  4 // IP xxx.yyy.zzz.aaa: zzz.aaa
#define TEMP_IMPR                       5 // number of impressions per second
#define TEMP_ESP_ID                     6 // ESP ID for mdns
#define TEMP_MODE_MAX                   6

#define TEMP_DISPLAY_MODE_DUR_MS        5000

// -------------------------------------------------------------------------------
#define DATE_FORMAT_MIN                 0
#define DATE_FORMAT_YYMMDD              0
#define DATE_FORMAT_MMDDYY              1
#define DATE_FORMAT_DDMMYY              2
#define DATE_FORMAT_MAX                 2
#define DATE_FORMAT_DEFAULT             2

// -------------------------------------------------------------------------------
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

// -------------------------------------------------------------------------------
#define BLANK_MODE_MIN                  0
#define BLANK_MODE_TUBES                0  // Use blanking for tubes only 
#define BLANK_MODE_LEDS                 1  // Use blanking for LEDs only
#define BLANK_MODE_BOTH                 2  // Use blanking for tubes and LEDs
#define BLANK_MODE_MAX                  2
#define BLANK_MODE_DEFAULT              BLANK_MODE_BOTH

// -------------------------------------------------------------------------------
#define PIR_TIMEOUT_MIN                 60    // 1 minute in seconds
#define PIR_TIMEOUT_MAX                 3600  // 1 hour in seconds
#define PIR_TIMEOUT_DEFAULT             300   // 5 minutes in seconds

// -------------------------------------------------------------------------------
#define USE_LDR_DEFAULT                 true

// -------------------------------------------------------------------------------
#define SLOTS_MODE_MIN                  0
#define SLOTS_MODE_WIPE_WIPE            0   // use slots effect "wipe in, wipe out"
#define SLOTS_MODE_BANG_BANG            1   // use slots effect "bang in, bang out"
#define SLOTS_MODE_MAX                  1
#define SLOTS_MODE_DEFAULT              SLOTS_MODE_WIPE_WIPE

#define SLOTS_FREQ_MIN                  0
#define SLOTS_FREQ_OFF                  0   // do not use slots effect
#define SLOTS_FREQ_1_MIN                1   // use slots effect every 1 minute
#define SLOTS_FREQ_5_MIN                2   // use slots effect every 5 minutes
#define SLOTS_FREQ_10_MIN               3   // use slots effect every 10 minutes
#define SLOTS_FREQ_MAX                  3
#define SLOTS_FREQ_DEFAULT              SLOTS_FREQ_1_MIN

// -------------------------------------------------------------------------------
// RTC address
#define RTC_I2C_ADDRESS                 0x68

// -------------------------------------------------------------------------------
#define DO_NOT_APPLY_LEAD_0_BLANK       false
#define APPLY_LEAD_0_BLANK              true

// -------------------------------------------------------------------------------
#define DIGIT_COUNT                     6

// -------------------------------------------------------------------------------
#define BUILTIN_LED_PIN                 1

// -------------------------------------------------------------------------------
#define CONFIG_PORTAL_TIMEOUT           60

#define DIAGS_DELAY_TIME                500

// -------------------------------------------------------------------------------
#define SYNC_HOURS 3
#define SYNC_MINS 4
#define SYNC_SECS 5
#define SYNC_DAY 2
#define SYNC_MONTH 1
#define SYNC_YEAR 0

#endif
