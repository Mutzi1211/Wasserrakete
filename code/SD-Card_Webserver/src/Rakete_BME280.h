
#pragma once

#include <Wire.h>
#include <SparkFunBME280.h>

class Rakete_BME280 {

    public:
    Rakete_BME280();

    bool init();
    float getTemperatur();
    float getHumidity();
    float getPressure();
    float getAltitude();

private:
    BME280 bme280;
    
};
