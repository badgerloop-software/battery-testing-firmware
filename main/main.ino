#include "ina.h"
#include <math.h>
#define BUTTON 8
#define BATT_CHECK 0
#define MCU_SET 3
#define MCU_RELAY_EN 2
#define MCU_CHRG_STAT 7
#define MCU_CHRG_EN 13

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
unsigned long start = 0;

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
      start = millis();
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

void set_operating_voltage(float v) {
  int val;

  val = round(v / ANALOG_WRITE_SCALE);

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
  set_operating_voltage(0);
  digitalWrite(MCU_RELAY_EN, 0);
  
  // Display error message
  Serial.println("error");
  
  return error;
}
state_t testChargeState(){
  // Check battery
  if(battCheck(CHARGE_BATT_VOLTAGE_LOW, CHARGE_BATT_VOLTAGE_HIGH)){
    return error;
  }
  // Enable charging
  digitalWrite(MCU_RELAY_EN, 0);
  digitalWrite(MCU_CHRG_EN, 1);
  // stop charging when MCU_CHRG_STAT is 0
  // by reading status of Orange LED
  int s = start;
  while(millis()-s < 10000 && analogRead(MCU_CHRG_STAT) != 0) {}
  digitalWrite(MCU_CHRG_EN, 0);

  return analogRead(MCU_CHRG_STAT) ? error : finish;
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
  int s = start;
  // monitor battery voltage
  // stop test (switch off relay) when minimum voltage is reached
  Serial.println("volts,therm1,therm2,therm3");
  while (millis()-start < 15000 && battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH)) {
    Serial.print(voltage);
    Serial.print(", ");
    Serial.print(_AREAD(THERM_1));
    Serial.print(", ");
    Serial.print(_AREAD(THERM_2));
    Serial.print(", ");
    Serial.println(_AREAD(THERM_3));
    delay(300);
  }
  digitalWrite(MCU_RELAY_EN, 0);

  if (voltage >= DISCHARGE_BATT_VOLTAGE_HIGH)
    return error;
  return finish;
}
state_t finishState() {
  // Stop discharging, isolate cell
  set_operating_voltage(0);
  digitalWrite(MCU_RELAY_EN, 0);
  // Stop collecting data, close/store csv file
  // Display End messsage and/or ending parameters (V, time, etc)
  Serial.println("'EOF'");
  Serial.print(voltage);
  Serial.print("v  Runtime ");
  Serial.print(millis()-start);
  Serial.println("s");

  delay(3000);
  while (battCheck(IDLE_BATT_VOLTAGE_LOW, IDLE_BATT_VOLTAGE_LOW)) {
    Serial.println("Remove battery");
    delay(1000);
  } 
  return idle;
}
