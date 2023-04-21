#include "JSONHelper.h"

void floatToCharRounded(float number, char* buffer, size_t bufferSize) {
  snprintf(buffer, bufferSize, "%.2f", number);
}

void createSensorJson(
  const char *sensor_node_name, 
  float temp, 
  float pressure,
  uint16_t tvoc, 
  uint16_t eco2, 
  char *wifi_network, 
  int wifi_signal, 
  char *jsonBuffer, 
  size_t jsonBufferSize)
{
  StaticJsonDocument<300> doc;

  char tempBuffer[20];
  char pressureBuffer[20];

  floatToCharRounded(temp, tempBuffer, sizeof(tempBuffer));
  floatToCharRounded(pressure, pressureBuffer, sizeof(pressureBuffer));

  doc["name"] = sensor_node_name;

  JsonObject tempObj = doc.createNestedObject("temp");
  tempObj["data"] = tempBuffer;
  tempObj["unit"] = "°C";

  JsonObject pressureObj = doc.createNestedObject("pressure");
  pressureObj["data"] = pressureBuffer;
  pressureObj["unit"] = "hPa";

  JsonObject tvocObj = doc.createNestedObject("tvoc");
  tvocObj["data"] = tvoc;
  tvocObj["unit"] = "ppb";

  JsonObject eco2Obj = doc.createNestedObject("eco2");
  eco2Obj["data"] = eco2;
  eco2Obj["unit"] = "ppm";

  JsonObject wifiObj = doc.createNestedObject("wifi");
  wifiObj["network"] = wifi_network;
  wifiObj["signal"] = wifi_signal;

  serializeJson(doc, jsonBuffer, jsonBufferSize);
}
