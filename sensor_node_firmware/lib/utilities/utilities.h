#ifndef UTILITIES_H
#define UTILITIES_H

#include <WiFi.h>
#include "SPIFFS.h"


//Variables to save values from HTML form
struct WifiSettings {
  String ssid;
  String pass;
  String ip;
  String gateway;
};

// Wifi utilities
bool getSSID(char *buffer, size_t bufferSize);
int getRSSI(void);
int mapSignalStrength(int rssi, bool returnRawDb);

// SPIFFS utilities
// Initialize SPIFFS
void initSPIFFS(); 

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path);

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message);

#endif // UTILITIES_H