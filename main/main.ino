#include "ina.h"
// no clue what it's supposed to be
#define BUTTON D8
#define BATT_CHECK A0
#define MCU_SET D3
#define MCU_RELAY_EN D2
#define MCU_CHRG_STAT A7
#define MCU_CHRG_EN D13

typedef enum {idle, ready, error, testCharge, testDischarge, finish} state_t;
state_t state;

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

bool buttonPressed(void) {
  // active high
  return digitalRead(BUTTON);
}

unsigned char battCheck(void) {
  int voltage = analogRead(BATT_CHECK);
  if(voltage == 0){
    return 1; // battery not inserted
  }
  else{
    return 0; // battery inserted (may be inserted correctly OR may be backwards)
  }
}

state_t idleState(void) {
  Serial.println("Insert Battery");

  if (buttonPressed() && battCheck() == 0)
    return ready;
  else if (buttonPressed() && battCheck() == 1)
    return error;

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
  if(battCheck() != 0){
    return error;
  }
  // Enable charging
  digitalWrite(MCU_RELAY_EN, 0);
  digitalWrite(MCU_CHRG_EN, 1);
  // stop charging when MCU_CHRG_STAT is 0
  while(analogRead(MCU_CHRG_STAT) != 0){
    
  }
  // Read status of Orange LED
 
  // return error;
  // return finish; 
  return testCharge;
}
state_t testDischargeState() {
  // set constant current (use MCU_SET voltage)
  analogWrite(MCU_SET, 0); // change this value
  // check battery status (BAT_CHECK)
  if(battCheck() != 0){
    return error;
  }
  // switch relay to discharge
  // monitor battery voltage
  // monitor thermistor temperatures
  // timer
  // stop test (switch off relay) when minimum voltage is reached

  // return error;
  // return finish;
  return testDischarge;
}
state_t finishState() {
  // Stop discharging, isolate cell
  // Stop collecting data, close/store csv file
  // Display End messsage and/or ending parameters (V, time, etc)

  // return idle;
  return finish;
}
