/**
   Test of parallel shifting out of two bytes at once, using two data lines.
*/

const int dataPin1 = 12;   //Outputs the byte to transfer
const int dataPin2 = 14;   //Outputs the byte to transfer
const int loadPin = 13;    //Controls the internal transference of data in SN74HC595 internal registers
const int clockPin = 15;   //Generates the clock signal to control the transference of data

bool ledState = false;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(dataPin1, OUTPUT);
  pinMode(dataPin2, OUTPUT);
  pinMode(loadPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
}

void loop() {
  byte data1 = B00000001;
  byte data2 = B00000001;

  shiftOutDual(loadPin, dataPin1, dataPin2, clockPin, MSBFIRST, data1, data2);

  data1 = data1 << 1;
  if (data1 == 0) data1 = 1;
  data2 = data1;

  ledState = !ledState;
  digitalWrite(LED_BUILTIN, ledState);
  delay(100);
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
    digitalWrite(_loadPin, HIGH);
  }
}

