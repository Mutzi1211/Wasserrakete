#include <Rakete_utility.h>





void setup()
{
  Serial.begin(115200);

  init();

  bme_active = bme.init();

  icm_active = icm.init();

  lipo_active = lipo.init();

  oled_active = display.init();

  servo_active = servo.init();

  initWebserver();

  initRecording();

  display.show("Launch -> ");
}


void loop()
{
  if ((micros() - nextMicros) >= 0)
  {
    nextMicros += PUBLISH_INTERVAL * 1000;
    // wird all 100 ms ausgefuehrt

    record();

    if (check_parachute())
    {
      deploy_parachute();
    }
  }

  if (parachute_progress && millis() - last_parachute > 1000)
  {
    // 1 Sekunde nach dem auslösen
    stop_parachute();
  }

  if (parachute_deployed && millis() - last_parachute > 10000)
  {
    // 10 Sekunden nach dem auslösen
    reset();
  
  }
}
