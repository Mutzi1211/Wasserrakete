
#pragma once

#include <Wire.h>
#include "ICM_20948.h"

class Rakete_ICM20948 {
public:
    Rakete_ICM20948();

    bool init();
    float getAccX();
    float getAccY();
    float getAccZ();

private:
    ICM_20948_I2C icm;
};
