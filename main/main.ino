// no clue what it's supposed to be
#define BUTTON A0

typedef enum {idle, ready, error, testing, finish} state_t;
state_t state;

void setup() {
  pinMode(BUTTON, INPUT);

  Serial.begin(115200);
  while(!Serial) {}
  Serial.println("Serial Connection established...");
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
    case testing:
      state = testingState();
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
  // inserted correctly
  return 0;
  // inserted backward
  return 1;
  // not inserted
  return 2;

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
