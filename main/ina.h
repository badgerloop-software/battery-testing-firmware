#include <stdint.h>

void ina226_read(float *voltage, float *current, float *power, float *shunt_voltage);
void ina226_reset();
void ina226_begin();