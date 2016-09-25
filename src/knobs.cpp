/* Transmit the value of some knobs connected to the Carloop as CAN messages
 */

#include "application.h"
#include "carloop.h"

SYSTEM_THREAD(ENABLED);

void setupKnobs();
void readKnobs();
void readKnob(unsigned i);
void normalizeKnob(unsigned i);
void printKnobs();
void transmitCAN();

Carloop<CarloopRevision2> carloop;

/* Connect potentiometer of each knob to powerPin, groundPin and the
 * appropriate knobPin.
 * Run the program and adjust knobValueLow and knobValueHigh to get 100%
 * when the knob is at each end stop. If values are reversed (100% for
 * low stop), reserve powerPin and groundPin.
 */
const unsigned knobCount = 3;
const int powerPin = A2;
const int groundPin = A3;
const int knobPin[knobCount] = { A4, A5, A6 };
uint16_t knobValueRaw[knobCount] = { 0 }; // 3.3V = 4096
uint16_t knobValueLow[knobCount] = { 30, 30, 30 };
uint16_t knobValueHigh[knobCount] = { 4060, 4060, 4060 };
uint16_t knobPercent[knobCount] = { 0 }; // 100% = 32768
const uint16_t knob100Percent = 32768;

/* every
 * Helper than runs a function at a regular millisecond interval
 */
template <typename Fn>
void every(unsigned long intervalMillis, Fn fn) {
  static unsigned long last = 0;
  if (millis() - last > intervalMillis) {
    last = millis();
    fn();
  }
}

void setup() {
  Serial.begin(9600);
  setupKnobs();
  carloop.begin();
}

void setupKnobs() {
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, HIGH);

  pinMode(groundPin, OUTPUT);
  digitalWrite(groundPin, LOW);

  for (unsigned i = 0; i < knobCount; i++) {
    pinMode(knobPin[i], INPUT);
  }
}

void loop() {
  readKnobs();
  printKnobs();
  transmitCAN();
}

void readKnobs() {
  for (unsigned i = 0; i < knobCount; i++) {
    readKnob(i);
    normalizeKnob(i);
  }
}

void readKnob(unsigned i) {
  knobValueRaw[i] = analogRead(knobPin[i]);
}

/* normalizeKnob
 * Scale the raw ADC count between the low and high value, normalize
 * to a percentage value and limit between 0% and 100%.
 */
void normalizeKnob(unsigned i) {
  uint16_t range = knobValueHigh[i] - knobValueLow[i];
  int32_t percent = (int32_t)(knobValueRaw[i] - knobValueLow[i]) * knob100Percent / range;
  knobPercent[i] = (percent < 0) ? 0 : (percent > knob100Percent) ? knob100Percent : percent;
}

void printKnobs() {
  every(200, [] {
    for (unsigned i = 0; i < knobCount; i++) {
      Serial.printf("%d: %4d adc, %3.1f%%  ", i, knobValueRaw[i], knobPercent[i] * 100.0/32768.0);
    }
    Serial.println("");
  });
}

/* transmitCAN
 * Send CAN messages with the values of the knobs at regular intervals
 * Put multiple every(interval, ...) statements to send multiple CAN
 * messages at different intervals
 */
void transmitCAN() {
  every(100, [] {
    CANMessage message;

    message.id = 0x110;
    message.len = 3;
    message.data[0] = knobPercent[0] * 255 / 32768;
    message.data[1] = knobPercent[1] * 255 / 32768;
    message.data[2] = knobPercent[2] * 255 / 32768;

    carloop.can().transmit(message);
  });
}
