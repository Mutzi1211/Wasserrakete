#include <Rakete_utility.h>

void setup()
{
  Serial.begin(115200);

  init();


  
  initWebserver();

  display.show("Launch -> ");
}


void loop()
{
  if ((micros() - nextMicros) >= 0)
  {
    nextMicros += PUBLISH_INTERVAL * 1000;
    // wird all 100 ms ausgefuehrt

    
  }

  if (parachute_progress && millis() - last_parachute > 1000)
  {
    // 1 Sekunde nach dem auslösen
   

  }

  if (parachute_deployed && millis() - last_parachute > 10000)
  {
    // 10 Sekunden nach dem auslösen
   
  
  }
}
