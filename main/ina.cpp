#include <Wire.h>
#include <math.h>

#define INA226_RESET 0x8000

#define INA_I2C_ADDR    0x40
#define INA226_REG_CONFIGURATION    0x00
#define INA226_REG_SHUNT_VOLTAGE    0x01
#define INA226_REG_BUS_VOLTAGE      0x02
#define INA226_REG_POWER            0x03
#define INA226_REG_CURRENT          0x04
#define INA226_REG_CALIBRATION      0x05
#define INA226_REG_MASK_ENABLE      0x06
#define INA226_REG_ALERT_LIMIT      0x07
#define INA226_REG_MANUFACTURER     0xFE
#define INA226_REG_DIE_ID           0xFF

#define INA226_TIME_01MS    0 /* 140us */
#define INA226_TIME_02MS    1 /* 204us */
#define INA226_TIME_03MS    2 /* 332us */
#define INA226_TIME_05MS    3 /* 588us */
#define INA226_TIME_1MS     4 /* 1.1ms */
#define INA226_TIME_2MS     5 /* 2.115ms */
#define INA226_TIME_4MS     6 /* 4.156ms */
#define INA226_TIME_8MS     7 /* 8.244ms */

#define INA226_AVERAGES_1	     0
#define INA226_AVERAGES_4	     1
#define INA226_AVERAGES_16	     2
#define INA226_AVERAGES_64	     3
#define INA226_AVERAGES_128	     4
#define INA226_AVERAGES_256	     5
#define INA226_AVERAGES_512	     6
#define INA226_AVERAGES_1024     7

#define INA226_MODE_OFF 0
#define INA226_MODE_SHUNT_TRIGGERED 1
#define INA226_MODE_BUS_TRIGGERED   2
#define INA226_MODE_SHUNT_BUS_TRIGGERED 3
#define INA226_MODE_OFF2    4
#define INA226_MODE_SHUNT_CONTINUOUS    5
#define INA226_MODE_BUS_CONTINUOUS  6
#define INA226_MODE_SHUNT_BUS_CONTINUOUS    7

float current_lsb;
uint16_t config;

void i2c_write16(uint8_t addr, uint8_t reg, uint16_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write((uint8_t *)&val, 2);
    Wire.endTransmission();
}

uint16_t i2c_read16(uint8_t addr, uint8_t reg) {
    uint16_t ret;
    
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(INA_I2C_ADDR, 2);

    if (Wire.available() <= 2) {
        ret = Wire.read() << 8;
        ret |= Wire.read();
    }

    return ret;
}

void ina226_calibrate(float r_shunt, float max_current) {
    current_lsb = max_current / (1 << 15);
	float calib = 0.00512 / (current_lsb * r_shunt);
	uint16_t calib_reg = (uint16_t) floorf(calib);
	current_lsb = 0.00512 / (r_shunt * calib_reg);

    i2c_write16(INA_I2C_ADDR, INA226_REG_CALIBRATION, calib_reg);
}

void ina226_configure(uint8_t bus, uint8_t shunt, uint8_t average, uint8_t mode){
    config = (average<<9) | (bus<<6) | (shunt<<3) | mode;

    i2c_write16(INA_I2C_ADDR, INA226_REG_CONFIGURATION, config);
}

void ina226_read(float *voltage, float *current, float *power, float *shunt_voltage) {
    uint16_t voltage_reg;
    int16_t current_reg, power_reg, shunt_reg;

    voltage_reg = i2c_read16(INA_I2C_ADDR, INA226_REG_BUS_VOLTAGE);
    current_reg = i2c_read16(INA_I2C_ADDR, INA226_REG_CURRENT);
    power_reg = i2c_read16(INA_I2C_ADDR, INA226_REG_POWER);
    shunt_reg = i2c_read16(INA_I2C_ADDR, INA226_REG_SHUNT_VOLTAGE);

    *voltage = (float) voltage_reg * .00125;
    *current = (float) current_reg * 1000.0 * current_lsb;
    *power = (float) power_reg * 25000.0 * current_lsb;
    *shunt_voltage = (float) shunt_reg * .00235;
}


void ina226_reset() {
    i2c_write16(INA_I2C_ADDR, INA226_REG_CONFIGURATION, config = INA226_RESET);

    ina226_calibrate(.1, 1.0);
    ina226_configure(INA226_TIME_8MS, INA226_TIME_8MS, INA226_AVERAGES_16, INA226_MODE_SHUNT_CONTINUOUS);   
}

void ina226_begin() {
    Wire.begin();
    ina226_calibrate(.1, 1.0);
    ina226_configure(INA226_TIME_8MS, INA226_TIME_8MS, INA226_AVERAGES_16, INA226_MODE_SHUNT_CONTINUOUS);
}