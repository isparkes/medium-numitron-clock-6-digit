/**
   Test of parallel shifting out of two bytes at once, using two data lines.
*/

#include <NeoPixelBus.h>

const uint16_t PixelCount = 6; // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 2;  // make sure to set this to the correct pin, ignored for Esp8266

#define colorSaturation 16

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(PixelCount, PixelPin);
RgbColor red(colorSaturation, 0, 0);

const int dataPin1 = 15;   // D? - Outputs the byte to transfer
const int dataPin2 = 13;   // D? - Outputs the byte to transfer
const int loadPin = 14;    // D? - Controls the internal transference of data in SN74HC595 internal registers
const int clockPin = 12;   // D? - Generates the clock signal to control the transference of data

const int LDRPin = A0;

//int latchPin = 14;  // RCLK - pin 12
//int clockPin = 12;  // SRCLK - pin 11
//int dataPin = 15;   // SER - DS - pin 14

bool ledState = false;

byte digitCnt = 0;

int dimmingTop = 2000;
int dimmingSmoothing = dimmingTop;

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
byte data2[6] = {DIGIT_0, DIGIT_0, DIGIT_0, DIGIT_0, DIGIT_0, DIGIT_0};
byte data1[6] = {1, 2, 4, 8, 16, 32};

int count = 0;

void setup() {
  Serial.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(dataPin1, OUTPUT);
  pinMode(dataPin2, OUTPUT);
  pinMode(loadPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  //
  //  digitalWrite(dataPin1, HIGH);
  //  digitalWrite(dataPin2, HIGH);
  //  digitalWrite(loadPin, HIGH);
  //  digitalWrite(clockPin, HIGH);
  //
  //  delay(1000);
  //
  //  digitalWrite(dataPin1, LOW);
  //  digitalWrite(dataPin2, LOW);
  //  digitalWrite(loadPin, LOW);
  //  digitalWrite(clockPin, LOW);
  //
  //  delay(1000);
  //
  //  digitalWrite(dataPin1, HIGH);
  //  digitalWrite(dataPin2, HIGH);
  //  digitalWrite(loadPin, HIGH);
  //  digitalWrite(clockPin, HIGH);
  //
  //  delay(1000);
  //
  //  digitalWrite(dataPin1, LOW);
  //  digitalWrite(dataPin2, LOW);
  //  digitalWrite(loadPin, LOW);
  //  digitalWrite(clockPin, LOW);
  //
  //  delay(1000);

  Serial.println("Starting");
  
    // this resets all the neopixels to an off state
    strip.Begin();
    strip.Show();
    
    // apply the color to the strip
    for (uint16_t pixel = 0; pixel < PixelCount; pixel++)
    {
        strip.SetPixelColor(pixel, red);
    }
    strip.Show();
    
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {

  int tmpCount = count;
  data2[5] = tmpCount % 10;
  tmpCount = tmpCount / 10;
  data2[4] = tmpCount % 10;
  tmpCount = tmpCount / 10;
  data2[3] = tmpCount % 10;
  tmpCount = tmpCount / 10;
  data2[2] = tmpCount % 10;
  tmpCount = tmpCount / 10;
  data2[1] = tmpCount % 10;
  tmpCount = tmpCount / 10;
  data2[0] = tmpCount % 10;
  
  for (int i = 0 ; i < 6 ; i++) {
    for (int loop = 0 ; loop < 3000 ; loop++) {
      if (loop == 200)
        shiftOutDual(loadPin, dataPin1, dataPin2, clockPin, MSBFIRST, data1[i], digitData[data2[i]]);
      if (loop == dimmingTop)
        shiftOutDual(loadPin, dataPin1, dataPin2, clockPin, MSBFIRST, 0, 0);
    }
  }
  
  count++;

  if (count > 999999) count = 0; 

  if ((count % 100) == 0) {
    dimmingTop = getLDRDimmingTop();
    // Serial.println(dimmingTop);
  }
}

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

int getLDRDimmingTop() {
    int rawLDR = analogRead(LDRPin);
    if (rawLDR < 100) rawLDR = 100;
    if (rawLDR > 1000) rawLDR = 1000;
    rawLDR = 1100 - rawLDR;

    dimmingSmoothing += ((rawLDR - dimmingSmoothing) / 5);
    return dimmingSmoothing * 2;
}

