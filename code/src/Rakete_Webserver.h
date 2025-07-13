
#pragma once

#include <Arduino.h>
#include <AsyncEventSource.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

#include <WiFi.h>

class Rakete_Webserver {

public:
  Rakete_Webserver();
  void init();
  void setAngle(int angle);
  bool saveJsonListing(const char *outputPath);
  void listFilesRecursiveJson(File dir, File &outFile, const String &path);

private:
  AsyncWebServer server;
  AsyncEventSource events;
  

  const char *index_html = "/index.html";
};
