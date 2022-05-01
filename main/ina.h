#include <stdint.h>

#define SHUNT_RES 0.0195

void ina226_read(float *voltage, float *current, float *shunt_voltage);
void ina226_reset();
void ina226_begin();
