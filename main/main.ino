#include "ina.h"
#include <math.h>
#include <string.h>

#define TESTER_ID 0

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
#define DISCHARGE_TIME 1500000

#define _AREAD(pin) ((float)(analogRead(pin)) * ADC_SCALE)
typedef enum {idle, ready, error, testCharge, testDischarge, finish} state_t;
state_t state;
float shunt = 0;
float operating_current = 0;
float voltage = 0;
inline void _LOG(char str[]) {
  switch(state) {
    case idle:
      Serial.print("IDLE: ");
      break;
    case ready:
      Serial.print("READY: ");
      break;
    case error:
      Serial.print("ERROR: ");
      break;
    case testCharge:
      Serial.print("TEST CHARGE: ");
      break;
    case testDischarge:
      Serial.print("TEST DISCHARGE: ");
      break;
    case finish:
      Serial.print("FINISH: ");
      break;
  }
  Serial.println(str);
}

void setup() {
  pinMode(BUTTON, INPUT);
  pinMode(BATT_CHECK, INPUT);
  pinMode(MCU_SET, OUTPUT);
  pinMode(MCU_RELAY_EN, OUTPUT);
  pinMode(MCU_CHRG_STAT, INPUT);
  pinMode(MCU_CHRG_EN, OUTPUT);

  Serial.begin(115200);
  while(!Serial) {}
  state = idle;
  ina226_begin();
  shunt = calibrate_shunt();
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
  calibrate_shunt();
  if (battCheck(IDLE_BATT_VOLTAGE_LOW, IDLE_BATT_VOLTAGE_HIGH)) {
    Serial.println("error:idle");
    return error;
  }

  // receive/send ping from/to controller
  if (Serial.available() > 0) {
    String message = Serial.readString();
    if (message.indexOf("tester") != -1) {
      Serial.print("yes,");
      Serial.print(TESTER_ID);
    }
  }
  
  if (voltage && buttonPressed()) {
    Serial.println("ready");
    return ready;
  }

  return idle;
}
state_t readyState() {
  calibrate_shunt();
  // Set parameters

  // starttest,current
  if (Serial.available() > 0) {
    String resp = Serial.readStringUntil('\0');
    char cpy[resp.length()];
    strcpy(cpy, resp.c_str());
    char *val = strtok(cpy, ",");
    if (strcmp(val, "starttest") != 0) {
      return ready;
    }
    
    val = strtok(NULL, ",");
    operating_current = String(val).toFloat();
    if (operating_current > 0.0) {
      Serial.println("charge");
      delay(500); // Delay so that the app doesn't read this and potential error in next state as one serial message
      return testCharge;
    }
    Serial.println("error");
    return error;
  }

  return ready;
}
state_t errorState() {
  // Stop charge/Discharge. MCU_SET
  // Isolate Battery (switch relay). MCU_RELAY_EN
  set_operating_voltage(0);
  digitalWrite(MCU_RELAY_EN, 0);
  
  // Display error message
  if (Serial.available() > 0) {
    String resp = Serial.readStringUntil('\n');
    if (resp.substring(0, 7) == "resume:") {
      if (resp.substring(7) == "idle") {
        Serial.println("idle");
        return idle;
      }
      if (resp.substring(7) == "ready") {
        Serial.println("ready");
        return ready;
      }
      if (resp.substring(7) == "charge") {
        Serial.println("charge");
        return testCharge;
      }
      if (resp.substring(7) == "discharge") {
        Serial.println("discharge");
        return testDischarge;
      }
    }
  }
  
  return error;
}
state_t testChargeState(){
  calibrate_shunt(); //is this allowed?
  // Check battery
  if(battCheck(CHARGE_BATT_VOLTAGE_LOW, CHARGE_BATT_VOLTAGE_HIGH)){
    Serial.println("error");
    return error;
  }
  // Enable charging
  digitalWrite(MCU_RELAY_EN, 0);
  digitalWrite(MCU_CHRG_EN, 1);
  // stop charging when MCU_CHRG_STAT is 0
  // by reading status of Orange LED
  while(analogRead(MCU_CHRG_STAT)) {
    if(battCheck(CHARGE_BATT_VOLTAGE_LOW, CHARGE_BATT_VOLTAGE_HIGH)){
      Serial.println("error");
      return error;
    }
  }
  
  digitalWrite(MCU_CHRG_EN, 0);
  
  if (analogRead(MCU_CHRG_STAT)) {
    Serial.println("error");
    return error;
  }
  Serial.println("discharge");
  delay(500); // Delay so that the app doesn't read this and potential error in next state as one serial message
  return testDischarge;
}
state_t testDischargeState() {
  // set constant current (use MCU_SET voltage)
  set_operating_voltage(operating_current * shunt);
  // check battery status (BATT_CHECK)
  if(battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH)) {
    Serial.println("error");
    return error;
  }
  // switch relay to discharge
  digitalWrite(MCU_RELAY_EN, 1);
  // timer
  // monitor battery voltage
  // stop test (switch off relay) when minimum voltage is reached
  unsigned long loop_time = millis();
  unsigned long msg_time = loop_time;
  unsigned long curr_time;
  while ((curr_time = millis())-loop_time < DISCHARGE_TIME && battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH)) {
    if (curr_time-msg_time > 60000) {
      Serial.print(voltage, 10); // TODO Replace 10 with whatever precision we want
      Serial.print(",");
      Serial.print(_AREAD(THERM_1), 10); // TODO Replace 10 with whatever precision we want
      Serial.print(",");
      Serial.print(_AREAD(THERM_2), 10); // TODO Replace 10 with whatever precision we want
      Serial.print(",");
      Serial.println(_AREAD(THERM_3), 10); // TODO Replace 10 with whatever precision we want
      
      msg_time = curr_time;
    }
  }
  digitalWrite(MCU_RELAY_EN, 0);

  if (voltage >= DISCHARGE_BATT_VOLTAGE_HIGH) {
    Serial.println("error");
    return error;
  }

  delay(500); // Delay to prevent finish from being read as part of the last line of the csv file
  Serial.println("finish");
  return finish;
}
state_t finishState() {
  // Stop discharging, isolate cell
  set_operating_voltage(0);
  digitalWrite(MCU_RELAY_EN, 0);

  // TODO Note: The app automatically transitions into the idle state, so any time spent here in the finish state won't be indicated on the app (though currently, the app can't effectively display that the tester is in the finish state anyway)
  //            It might be worth using the LEDs on the board to show that the tester is in the finish state
  while (battCheck(IDLE_BATT_VOLTAGE_LOW, IDLE_BATT_VOLTAGE_LOW)) {
    delay(1000);
  }
  return idle;
}
