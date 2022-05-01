#include <PID_v1.h>

#include "ina.h"
#include <math.h>
#include <string.h>

#define TESTER_ID 3

#define BUTTON 8
#define BATT_CHECK 0
#define MCU_SET 3
#define MCU_RELAY_EN 2
#define MCU_CHRG_STAT 7
#define MCU_CHRG_EN 13
#define LED_GREEN 5
#define LED_RED 6

#define THERM_1 A1  // Battery
#define THERM_2 A2  // MOSFET
#define THERM_3 A3  // Resistor

#define ADC_SCALE (5.0 / 1023.0)
#define ANALOG_WRITE_SCALE (5.0 / 255.0)
#define IDLE_BATT_VOLTAGE_LOW 2.3
#define IDLE_BATT_VOLTAGE_HIGH 3.8
#define CHARGE_BATT_VOLTAGE_LOW 2.3
#define CHARGE_BATT_VOLTAGE_HIGH 3.8
#define DISCHARGE_BATT_VOLTAGE_LOW 2.3
#define DISCHARGE_BATT_VOLTAGE_HIGH 3.8
#define DISCHARGE_TIME 18000000 // 5 mins

#define CURR_SETPOINT 1500 //mA

#define _AREAD(pin) ((float)(analogRead(pin)) * ADC_SCALE)
typedef enum {idle, ready, error, testCharge, testDischarge, finish} state_t;
state_t state;
float shunt = 0;
double operating_current = 0;  //mA
float voltage = 0;
double Setpoint, Input, Output; //mA
float ina_voltage, ina_current, ina_shunt_voltage, current;
//using a PID control loop in FW to fix what Kevin couldn't do in HW
PID CC_PID(&Input, &Output, &Setpoint,0.000001,0.0001,0.0, DIRECT);

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
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  TCCR2B = TCCR2B & B11111000 | B00000001;
  // increase PWM frequency for analogWrite functions to 31.4kHz

  Serial.begin(115200);
  while(!Serial) {}
  state = idle;
  ina226_begin();
  shunt = calibrate_shunt();
}

void loop() {
//   put your main code here, to run repeatedly:
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

float read_therm(int therm_pin) {
  float voltage = _AREAD(therm_pin);
  float resistance = (1000 * voltage) / (5 - voltage); // voltage divider
  float t_inv = (1.0/298.15) + (1.0/3575.0) * log(resistance / 10000.0); // B - parameter Steinhart equation
  
  return (1 / t_inv) - 273.15;
}

void set_operating_voltage(float v) {
  int val;

  val = round(v / ANALOG_WRITE_SCALE);

  analogWrite(MCU_SET, val);
  //Serial.print("Operating voltage set:" );
  //Serial.println(val);
}


void print_csv_data() {
  Serial.print(ina_voltage, 5); // TODO Replace 10 with whatever precision we want
  Serial.print(",");
  Serial.print(current, 5); // TODO Replace 10 with whatever precision we want
  Serial.print(",");
  Serial.print(ina_shunt_voltage, 5); // TODO Replace 10 with whatever precision we want
  Serial.print(",");
  Serial.print(read_therm(THERM_1), 2); // TODO Replace 10 with whatever precision we want
  Serial.print(",");
  Serial.print(read_therm(THERM_2), 2); // TODO Replace 10 with whatever precision we want
  Serial.print(",");
  Serial.println(read_therm(THERM_3), 2); // TODO Replace 10 with whatever precision we want
  //Serial.println("C");
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
  if (digitalRead(MCU_RELAY_EN)) {
    ina226_read(&voltage, NULL, NULL);
  }
  else {
    voltage = _AREAD(BATT_CHECK);
  }
  if (voltage < lower_bound || voltage > upper_bound ) {
    return 1;
  }
  return 0;
}

state_t idleState(void) {
  calibrate_shunt();
  if (battCheck(-1, IDLE_BATT_VOLTAGE_HIGH)) {
    Serial.print("voltage measured: ");
    Serial.println(voltage);
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
  // Set parameters
  if (battCheck(0, 3.8)) {
      Serial.print("error: failed battCheck with ");
      Serial.println(voltage);
      return error;
  }

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
    operating_current = String(val).toDouble() * 1000;
    if (operating_current > 0.0) {
      Serial.println("charge");
      delay(500); // Delay so that the app doesn't read this and potential error in next state as one serial message
      Setpoint = operating_current;
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
  digitalWrite(LED_RED, 0);
  
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
  // Check battery
  if(battCheck(CHARGE_BATT_VOLTAGE_LOW, CHARGE_BATT_VOLTAGE_HIGH)){
    Serial.println("error");
    return error;
  }
  // Enable charging
  digitalWrite(MCU_RELAY_EN, 0);
  digitalWrite(LED_RED, 0);
  digitalWrite(MCU_CHRG_EN, 1);
  digitalWrite(LED_GREEN, 1);
  delay(1000);
  // stop charging when MCU_CHRG_STAT is 0
  // by reading status of Orange LED
  unsigned long timer = millis();
  while(!buttonPressed() && (_AREAD(MCU_CHRG_STAT) < 3.5)) { 
    if(battCheck(CHARGE_BATT_VOLTAGE_LOW, CHARGE_BATT_VOLTAGE_HIGH)){
      Serial.print("error: failed battCheck with ");
      Serial.println(voltage);
      return error;
    }
    // this causes errors, not sure why
    if (millis() - timer > 10000) {
      timer = millis();
      Serial.println(voltage);
    }
  }
  
  digitalWrite(MCU_CHRG_EN, 0);
  digitalWrite(LED_GREEN, 0);
  delay(500);
  
  if (_AREAD(MCU_CHRG_STAT) < 3.5) {
    Serial.println("error: charge stat still high on charge exit");
    return error;
  }
  Serial.println("discharge");
  delay(500); // Delay so that the app doesn't read this and potential error in next state as one serial message
  return testDischarge;
}
state_t testDischargeState() {
  ina226_reset();
  // set constant current (use MCU_SET voltage)
  //set_operating_voltage(operating_current * shunt);
  //set_operating_voltage(0.81);
  Output = 0.8;
  // switch relay to discharge
  digitalWrite(MCU_RELAY_EN, 1);
  digitalWrite(LED_RED, 1);
  delay(500);
  // check battery status (BATT_CHECK)
  if(battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH)) {
    Serial.print("voltage measured: ");
    Serial.println(voltage);
    Serial.println("error: failed battCheck");
    digitalWrite(LED_RED, 0);
    return error;
  }

  CC_PID.SetMode(AUTOMATIC);
  // timer
  // monitor battery voltage
  // stop test (switch off relay) when minimum voltage is reached
  unsigned long loop_time = millis();
  unsigned long msg_time = loop_time;
  unsigned long curr_time;
  while (!buttonPressed() && ((curr_time = millis())-loop_time < DISCHARGE_TIME && !battCheck(DISCHARGE_BATT_VOLTAGE_LOW, DISCHARGE_BATT_VOLTAGE_HIGH))) {
    ina226_read(&ina_voltage, &ina_current, &ina_shunt_voltage);
    current = ina_shunt_voltage/SHUNT_RES;
    Input = double(current);
    CC_PID.Compute();
    set_operating_voltage(float(Output));
    if (curr_time-msg_time > 600) {
      print_csv_data();
      msg_time = curr_time;
    }
  }
  digitalWrite(MCU_RELAY_EN, 0);
  digitalWrite(LED_RED, 0);

  if (voltage >= DISCHARGE_BATT_VOLTAGE_HIGH) {
    Serial.println("error");
    digitalWrite(LED_RED, 0);
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
  digitalWrite(LED_RED, 0);

  delay(1000);

  // TODO Note: The app automatically transitions into the idle state, so any time spent here in the finish state won't be indicated on the app (though currently, the app can't effectively display that the tester is in the finish state anyway)
  //            It might be worth using the LEDs on the board to show that the tester is in the finish state
  //while (battCheck(IDLE_BATT_VOLTAGE_LOW, IDLE_BATT_VOLTAGE_LOW)) {
    //delay(1000);
  //}
  return idle;
}
