#include "ina.h"
#include <math.h>
#define BUTTON 8
#define BATT_CHECK 0
#define MCU_SET 3
#define MCU_RELAY_EN 2
#define MCU_CHRG_STAT 7
#define MCU_CHRG_EN 13

#define THERM_CHECKS 10
#define THERM_1 A1
#define THERM_2 A2
#define THERM_3 A3

#define ADC_SCALE (5.0 / 1023.0)
#define ANALOG_WRITE_SCALE (5.0 / 255.0)

#define IDLE_BATT_VOLTAGE_LOW 0
#define IDLE_BATT_VOLTAGE_HIGH 5
#define CHARGE_BATT_VOLTAGE_LOW 0
#define CHARGE_BATT_VOLTAGE_HIGH 5
#define DISCHARGE_BATT_VOLTAGE_LOW 0
#define DISCHARGE_BATT_VOLTAGE_HIGH 5

#define _AREAD(pin) ((float)(analogRead(pin)) * ADC_SCALE)

typedef enum {idle, ready, error, testCharge, testDischarge, finish} state_t;
state_t state;
float shunt = 0;
float operating_current = 0;
float voltage = 0;

void setup() {
  pinMode(BUTTON, INPUT);
  pinMode(BATT_CHECK, INPUT);
  pinMode(MCU_SET, OUTPUT);
  pinMode(MCU_RELAY_EN, OUTPUT);
  pinMode(MCU_CHRG_STAT, INPUT);
  pinMode(MCU_CHRG_EN, OUTPUT);

  Serial.begin(115200);
  while(!Serial) {}
  Serial.println("Serial Connection established...");
  ina226_begin();
  shunt = calibrate_shunt();
  state = idle;
}

void loop() {
  // put your main code here, to run repeatedly:
  switch (state) {
    case idle:
      state = idleState();
      break;
    case ready:
      state = readyState();
      break;
    case error:
      state = errorState();
      break;
    case testCharge:
      state = testChargeState();
      break;
    case testDischarge:
      state = testDischargeState();
      break;
    case finish:
      state = finishState();
      break;      
  }
}

void set_operating_voltage(float voltage) {
  int val;

  val = round(voltage / ANALOG_WRITE_SCALE);

  analogWrite(MCU_SET, val);
}

float calibrate_shunt() {
  float current;

  set_operating_voltage(1.0);
  ina226_read(NULL, &current, NULL);

  shunt = 1.0 / current;
}

bool buttonPressed(void) {
  // active high
  return digitalRead(BUTTON);
}

unsigned char battCheck(float lower_bound, float upper_bound) {
  voltage = _AREAD(BATT_CHECK);
  if (voltage < lower_bound || voltage > upper_bound ) {
    return 1;
  }
  return 0;
}

state_t idleState(void) {
  Serial.println("Insert Battery");

  if (battCheck(IDLE_BATT_VOLTAGE_LOW, IDLE_BATT_VOLTAGE_HIGH))
    return error;
  
  if (buttonPressed())
    return ready;

  return idle;
}
state_t readyState() {
  // Set parameters
  Serial.println("Ready for Test");

  // return testing;
  return ready;
}
state_t errorState() {
  // Stop charge/Discharge. MCU_SET
  // Isolate Battery (switch relay). MCU_RELAY_EN
  analogWrite(MCU_SET, 0); 
  digitalWrite(MCU_RELAY_EN, 0);
  
  // Display error message
  Serial.println("error");
  
  return error;
}
state_t testChargeState(){
  // Check battery
  if(battCheck(CHARGE_BATT_VOLTAGE_LOW, CHARGE_BATT_VOLTAGE_HIGH) != 0){
    return error;
  }
  // Enable charging
  digitalWrite(MCU_RELAY_EN, 0);
  digitalWrite(MCU_CHRG_EN, 1);
  // stop charging when MCU_CHRG_STAT is 0
  while(analogRead(MCU_CHRG_STAT) != 0){
    
  }
  digitalWrite(MCU_CHRG_EN, 0);
  // Read status of Orange LED
 
  // return error;
  // return finish; 
  return testCharge;
}
state_t testDischargeState() {
  // set constant current (use MCU_SET voltage)
  set_operating_voltage(operating_current * shunt);
  // check battery status (BATT_CHECK)
  if(battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH)) {
    return error;
  }
  // switch relay to discharge
  digitalWrite(MCU_RELAY_EN, 1);
  // timer
  int timer = 0;
  // monitor battery voltage
  // stop test (switch off relay) when minimum voltage is reached
  while (timer++ < THERM_CHECKS && battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH)) {
    Serial.print(voltage);
    Serial.println(" volts");
  // monitor thermistor temperatures
    Serial.println("| Thermistor temps:");
    Serial.print("| 1) ");
    Serial.println(_AREAD(THERM_1));
    Serial.print("| 2) ");
    Serial.println(_AREAD(THERM_2));
    Serial.print("| 3) ");
    Serial.println(_AREAD(THERM_3));
    delay(1000);
  }
  digitalWrite(MCU_RELAY_EN, 0);

  if (timer < THERM_CHECKS)
    return error;
  return finish;
}
state_t finishState() {
  // Stop discharging, isolate cell
  // Stop collecting data, close/store csv file
  // Display End messsage and/or ending parameters (V, time, etc)

  // return idle;
  return finish;
}
