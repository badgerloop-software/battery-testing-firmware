typedef enum {idle, ready, error, testing, finish} state_t;
state_t currState;

void setup() {
  // put your setup code here, to run once:
  currState = idle;
}

void loop() {
  // put your main code here, to run repeatedly:
  switch (currState) {
    case idle:
      currState = idleState();
      break;
    case ready:
      currState = readyState();
      break;
    case error:
      currState = errorState();
      break;
    case testing:
      currState = testingState();
      break;
    case finish:
      currState = finishState();
      break;      

  }
}
state_t idleState() {
  // Display "Insert Battery"

  // return ready;
  // return error;
  return idle;
}
state_t readyState() {
  // Set parameters
  // Display "Ready for Test"

  // return testing;
  return ready;
}
state_t errorState() {
  // Stop charge/Discharge
  // Isolate Battery (switch relay)
  // Display error message
 
  return error;
}
state_t testingState() {
  // Charge/discharge
  // Monitor/display parameters
  // Display "Test in Progress" and estimated time left

  // return error;
  // return finish;
  return testing;
}
state_t finishState() {
  // Stop discharging, isolate cell
  // Stop collecting data, close/store csv file
  // Display End messsage and/or ending parameters (V, time, etc)

  // return idle;
  return finish;
}
