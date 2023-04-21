#ifndef JSONHELPER_H
#define JSONHELPER_H

#include <ArduinoJson.h>

void floatToCharRounded(float number, char* buffer, size_t bufferSize);

void createSensorJson(
  const char *sensor_node_name, 
  float temp,
  float pressure,
  uint16_t tvoc, 
  uint16_t eco2, 
  char *wifi_network, 
  int wifi_signal, 
  char *jsonBuffer, 
  size_t jsonBufferSize
);

#endif // JSONHELPER_H
