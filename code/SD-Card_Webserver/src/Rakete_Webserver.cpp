
#include "Rakete_Webserver.h"
#include "Arduino.h"

Rakete_Webserver::Rakete_Webserver()
    : server(80), events("/events") {}

void Rakete_Webserver::init() {

  server.addHandler(&events);

  server.on("/list.json", HTTP_GET, [this](AsyncWebServerRequest *request) {
    this->saveJsonListing("/list.json");
    request->send(SD, "/list.json", "application/json");
  });

  server.on(
      "/upload", HTTP_POST,
      [this](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "Upload abgeschlossen");
      },
      [this](AsyncWebServerRequest *request, String filename, size_t index,
             uint8_t *data, size_t len, bool final) {
        static File uploadFile;

        if (index == 0) {
          String path = "/" + filename;
          if (SD.exists(path))
            SD.remove(path);
          uploadFile = SD.open(path, FILE_WRITE);
        }

        if (uploadFile) {
          uploadFile.write(data, len);
        }

        if (final && uploadFile) {
          uploadFile.close();
        }

        this->saveJsonListing("/list.json");
      });

  server.on("/delete", HTTP_DELETE, [this](AsyncWebServerRequest *request) {
    if (!request->hasParam("path")) {
      return request->send(400, "text/plain", "Fehlender Parameter: path");
    }
    String path = request->getParam("path")->value();
    if (!path.startsWith("/")) {
      path = "/" + path;
    }

    if (SD.exists(path)) {
      if (SD.remove(path)) {
        this->saveJsonListing("/list.json");
        request->send(200, "text/plain", "Datei gelöscht");
      } else {
        request->send(500, "text/plain", "Löschen fehlgeschlagen");
      }
    } else {
      request->send(404, "text/plain", "Datei nicht gefunden");
    }
  });

  server.onNotFound([this](AsyncWebServerRequest *request) {
    String path = request->url();
    printf("Anfrage für Pfad: %s\n", path.c_str());

    this->saveJsonListing("/list.json");

    if (path == "/") {
      request->redirect("/index_html");
      return;
    }

    if (path.indexOf('.') == 0) {
      request->send(404, "text/plain", "404: File Not Found");
      return;
    }

    if (!SD.exists(path)) {
      request->send(404, "text/plain", "404: File Not Found");
      return;
    }

    File file = SD.open(path, FILE_READ);
    if (file) {

      String contentType = "text/plain";
      if (path.endsWith(".html"))
        contentType = "text/html";
      else if (path.endsWith(".css"))
        contentType = "text/css";
      else if (path.endsWith(".js"))
        contentType = "application/javascript";
      else if (path.endsWith(".png"))
        contentType = "image/png";
      else if (path.endsWith(".jpg") || path.endsWith(".jpeg"))
        contentType = "image/jpeg";
      else if (path.endsWith(".gif"))
        contentType = "image/gif";
      else if (path.endsWith(".ico"))
        contentType = "image/x-icon";
      else if (path.endsWith(".xml"))
        contentType = "text/xml";
      else if (path.endsWith(".pdf"))
        contentType = "application/pdf";

      AsyncWebServerResponse *response =
          request->beginResponse(SD, path, contentType);
      request->send(response);
      file.close();
    } else {
      request->send(500, "text/plain", "500: Fehler beim Öffnen der Datei");
    }
  });

  server.begin();
  Serial.println("Webserver gestartet");
}

bool Rakete_Webserver::saveJsonListing(const char *outputPath) {
  File root = SD.open("/");
  if (!root || !root.isDirectory()) {
    return false;
  }

  File jsonFile = SD.open(outputPath, FILE_WRITE);
  if (!jsonFile) {
    root.close();
    return false;
  }

  jsonFile.print("[");
  this->listFilesRecursiveJson(root, jsonFile, "");
  jsonFile.print("]");

  jsonFile.close();
  root.close();
  return true;
}

void Rakete_Webserver::listFilesRecursiveJson(File dir, File &outFile,
                                              const String &path) {
  bool firstEntry = true;

  // Stelle sicher, dass wir am Verzeichnisanfang lesen
  dir.rewindDirectory();

  while (true) {
    File entry = dir.openNextFile();
    if (!entry)
      break;

    // Relativen Namen extrahieren
    String name = String(entry.name()).substring(path.length());

    // Skip hidden files/folders (Name beginnt mit '.')
    if (name.length() > 0 && name.charAt(0) == '.') {
      entry.close();
      continue;
    }

    // Nur zwischen den Einträgen Komma drucken
    if (!firstEntry) {
      outFile.print(",");
    }
    firstEntry = false;

    // JSON-Objekt starten
    outFile.print("{\"name\":\"");
    outFile.print(name);
    outFile.print("\",\"type\":\"");
    outFile.print(entry.isDirectory() ? "folder" : "file");
    outFile.print("\"");

    // Bei Ordnern rekursiv in children
    if (entry.isDirectory()) {
      outFile.print(",\"children\":[");
      String newPath = String(entry.name()) + "/";

      // Unterordner frisch öffnen, damit openNextFile() neu startet
      File subDir = SD.open(entry.name());
      if (subDir && subDir.isDirectory()) {
        listFilesRecursiveJson(subDir, outFile, newPath);
        subDir.close();
      }

      outFile.print("]");
    }

    // Objekt beenden
    outFile.print("}");
    entry.close();
  }
}
