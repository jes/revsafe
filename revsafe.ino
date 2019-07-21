/* JSR Rev-Safe
 *  
 *  Set "Processor" to "ATmega328P (Old Bootloader)"
 *  Change redline by editing max_rpm variable
 *  Serial port is 115200 baud
 *  Use "Serial Plotter" to live-view engine RPM
 *  
 *  3-pin waterproof plug:
 *    Red wire - 12v
 *    Black wire - ground
 *    White wire - ground path for kill switch
 *  
 *  Audio jack:
 *    Red wire - supplies 5v to hall sensor
 *    Uninsulated wire - ground
 *    White wire - signal from hall sensor
 *  
 *  Spade connector:
 *    Connect to kill switch wire
 */

#include <avr/sleep.h>

const int sensor_pin = 2; // only pins 2 & 3 support interrupts
const int killswitch_pin = 3;
const int max_rpm = 4800;
const unsigned long print_every = 100; // ms
const unsigned long sleep_after = 10000; // ms
volatile unsigned long prev_trigger = 0;
volatile unsigned long last_trigger = 0;
volatile unsigned long last_trigger_ms = 0;

void setup() {
  pinMode(killswitch_pin, OUTPUT);
  pinMode(sensor_pin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensor_pin), sensor_trigger, FALLING);
  Serial.begin(115200);
}

void sensor_trigger(void) {
  prev_trigger = last_trigger;
  last_trigger = micros();
  last_trigger_ms = millis();
}

long rpm() {
  static long last_rpm;
  unsigned long now = micros();
  
  // cope with micros() wrap-around
  if (last_trigger > now)
    last_trigger = 0;
  if (prev_trigger > last_trigger)
    prev_trigger = 0;
    
  unsigned long elapsed_us = last_trigger - prev_trigger;
  if (elapsed_us == 0)
    return last_rpm;

  // rpm is less than half what it was last time: just assume we've stopped
  if (now - last_trigger > elapsed_us*2) {
    last_rpm = 0;
    return 0;
  }
  
  // rpm is slowing down, so just linearly drop the estimate until we see the magnet again
  if (now - last_trigger > elapsed_us)
    elapsed_us = now - last_trigger;
  
  // revs-per-min = mins-per-us / us-per-rev
  last_rpm = 60000000 / elapsed_us;

  return last_rpm;
}

void kill() {
  killstate(HIGH);
}

void revive() {
  killstate(LOW);
}

void killstate(int state) {
  static int kill_state = 42; // neither HIGH nor LOW
  if (kill_state == state)
    return;
  digitalWrite(killswitch_pin, state);
  kill_state = state;
}

// https://thekurks.net/blog/2018/1/24/guide-to-arduino-sleep-mode
void sleep() {
  sleep_enable();
  attachInterrupt(digitalPinToInterrupt(sensor_pin), wakeup, FALLING);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_cpu();
}

void wakeup() {
  sleep_disable();
  last_trigger_ms = millis(); // don't immediately go back to sleep
  attachInterrupt(digitalPinToInterrupt(sensor_pin), sensor_trigger, FALLING);
}

void loop() {
  static unsigned long last_print = 0;
  
  if (millis() > last_print + print_every) {
    last_print = millis();
    Serial.println(rpm());
  }
  
  if (rpm() > max_rpm)
    kill();
  else
    revive();

  if (millis() > last_trigger_ms + sleep_after)
    sleep();
}
