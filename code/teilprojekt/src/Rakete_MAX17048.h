
#pragma once

#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <Wire.h>

class Rakete_MAX17048 {

public:
  Rakete_MAX17048();

  bool init();
  float getVoltage();
  float getSOC();

private:
  SFE_MAX1704X lipo;
};
