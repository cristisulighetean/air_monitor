
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>

#include "utilities.h"
#include "JSONHelper.h"

// Sensor libraries
#include <Wire.h>
#include "SparkFunBME280.h"
#include "SparkFun_SGP30_Arduino_Library.h"

// I2C sensor communication lines
#define I2C_SDA 0
#define I2C_SCL 1

struct SensorData {
  float temp;
  float press;
  uint16_t eCO2;
  uint16_t tVOC;
};

SGP30 sgp30_sensor;
BME280 bme280_sensor;
SensorData sensorData;
WifiSettings wifiSettings;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

// Search for parameter in HTTP POST request
const char* PARAM_SSID = "ssid";
const char* PARAM_PASS = "pass";
const char* PARAM_IP = "ip";
const char* PARAM_GATEWAY = "gateway";

// Sensor node identifier
String sensor_identifier;
const char* sensorIdentifierPath = "/identifier.txt";

// File paths to save input values permanently
const char* ssidPath = "/ssid.txt";
const char* passPath = "/pass.txt";
const char* ipPath = "/ip.txt";
const char* gatewayPath = "/gateway.txt";

IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
IPAddress subnet(255, 255, 0, 0);

// Interval to wait for Wi-Fi connection (milliseconds)
const long interval = 10000;  

// Initialize WiFi
bool initializeWifi() {
  // Check if SSID & IP address are valid
  if(wifiSettings.ssid == "" || wifiSettings.ip == ""){
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  // Lunch Wifi in Station mode
  WiFi.mode(WIFI_STA);
  localIP.fromString(wifiSettings.ip.c_str());
  localGateway.fromString(wifiSettings.gateway.c_str());

  if (!WiFi.config(localIP, localGateway, subnet)){
    Serial.println("STA Failed to configure");
    return false;
  }

  WiFi.begin(wifiSettings.ssid.c_str(), wifiSettings.pass.c_str());
  Serial.println("Connecting to WiFi...");

  // Timeout for wifi connection
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED) {
    unsigned long currentAttemptTime = millis();
    if (currentAttemptTime - startAttemptTime >= interval) {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.println("Connected successfully.");
  Serial.print("Connected to: ");
  Serial.println(WiFi.localIP());
  return true;
}

void restartSensorNode() {
  Serial.println("---------------- Restarting MCU ----------------");
  delay(5000);
  ESP.restart();
}

bool onReset() {
  if (SPIFFS.begin(true)) {
    SPIFFS.remove("/ssid.txt");
    SPIFFS.remove("/pass.txt");
    SPIFFS.remove("/ip.txt");
    SPIFFS.remove("/gateway.txt");
    SPIFFS.end();
    return true;
  }
  return false;
}

// ----------------------------------------------------------------

void sensorReadData(void * parameter) {
   for (;;) {
    // BME280 readings
    sensorData.temp = bme280_sensor.readTempC();
    sensorData.press = bme280_sensor.readFloatPressure();

    // SGP30 readings
    sgp30_sensor.measureAirQuality();
    sensorData.eCO2 = sgp30_sensor.CO2;
    sensorData.tVOC = sgp30_sensor.TVOC;

    char data_result[40];
    sprintf(data_result, 
      "%.02ft %.02fp %dco2 %dtvoc", 
      sensorData.temp, 
      sensorData.press, 
      sensorData.eCO2, 
      sensorData.tVOC);

    Serial.println("Read sensor data");
    Serial.println(data_result);
 
    vTaskDelay(30000 / portTICK_PERIOD_MS);
   }
}

void sensorReadDataTask() {    
  xTaskCreate(     
  sensorReadData,      
  "Read sensor data",      
  5000,      
  nullptr,      
  1,     
  nullptr     
  );     
}

bool sensorInit() {
  // Setup I2C interface
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);
  
  // SGP30 init
  if (!sgp30_sensor.begin()) {
    Serial.println("No SGP30 Detected. Check connections.");
    return false;
  }

  sgp30_sensor.initAirQuality();
  delay(1000);

  // BMP280 init
  bme280_sensor.settings.I2CAddress = 0x76;
  if (!bme280_sensor.beginI2C()) {
    Serial.println("No BMP280 Detected. Check connections.");
    return false;
  }
  // BME
  bme280_sensor.setMode(MODE_NORMAL); //MODE_SLEEP, MODE_FORCED, MODE_NORMAL is valid. See 3.3
  delay(1000);

  return true;
}

// ----------------------------------------------------------------

void setup() {
  // Serial port for debugging purposes
  Serial.begin(115200);

  initSPIFFS();
  
  // Load values saved in SPIFFS
  wifiSettings.ssid = readFile(SPIFFS, ssidPath);
  wifiSettings.pass = readFile(SPIFFS, passPath);
  wifiSettings.ip = readFile(SPIFFS, ipPath);
  wifiSettings.gateway = readFile (SPIFFS, gatewayPath);

  Serial.println(wifiSettings.ssid);
  Serial.println(wifiSettings.pass);
  Serial.println(wifiSettings.ip);
  Serial.println(wifiSettings.gateway);

  // Get Sensor Node identfier
  sensor_identifier = readFile(SPIFFS, sensorIdentifierPath);

  if(initializeWifi()) {
    // Endpoints
    // ----------------------------------------------------------------
    // Initialize sensors
    if (sensorInit()) {
      Serial.println("Sensor initialization successful");
      sensorReadDataTask();

      // Endpoint to read sensor data
      server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){

        // --------------------------------
        // Wifi network data 
        char ssidBuffer[50];
        getSSID(ssidBuffer, sizeof(ssidBuffer));
        int signalStrength = mapSignalStrength(getRSSI(), false);
        // Sensor identifier
        const char* identifierChar = sensor_identifier.c_str();

        // --------------------------------
        char jsonBuffer[300];
        createSensorJson(identifierChar, 
          sensorData.temp, 
          sensorData.press, 
          sensorData.tVOC, 
          sensorData.eCO2, 
          ssidBuffer, 
          signalStrength, 
          jsonBuffer, 
          sizeof(jsonBuffer));

        Serial.println(jsonBuffer);

        request->send(200, "application/json", jsonBuffer);
      });

      // Route for root / web page
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html", false);
      });

      // Status endpoint
      server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        const char* SENSOR_ONLINE = "sensor_online";
        request->send(200, "text/plain", SENSOR_ONLINE);
      });

    }

    else {
      // Sensors failed to initialize
      Serial.println("Sensor initialization failed");

      // Route for root / web page
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/indexFailed.html", "text/html", false);
      });

      // Status endpoint
      server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
        const char* SENSOR_INITIALATION_FAILED = "sensor_initialization_failed";
        request->send(200, "text/plain", SENSOR_INITIALATION_FAILED);
      });
    }


    // Endpoint to reset MCU (remove network configurations)
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request){
      if (onReset()) {
        request->send(200, "text/plain", "Network configuration files deleted. Restart the device.");
        restartSensorNode();
      }
      else {
        const char* SPIFFS_FS_ACCESS_ERROR = "Faile to access the file system";
        request->send(500, "text/plain", SPIFFS_FS_ACCESS_ERROR);
      }
    });

    // Endpoint to restart Server node
    server.on("/restart", HTTP_GET, [](AsyncWebServerRequest *request){
      const char* SENSOR_RESTART = "Restarting the Sensor node...";
      request->send(200, "text/plain", SENSOR_RESTART);
      restartSensorNode();
    });

    server.serveStatic("/", SPIFFS, "/");

    server.begin();
  }
  else {
    // Lunch Sensor node in AP mode
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting Wifi into AP mode (Access Point)");
    // nullptr sets an open Access Point
    WiFi.softAP("Sensor Node Commisioning", nullptr);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP); 

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(SPIFFS, "/wifiManager.html", "text/html");
    });
    
    server.serveStatic("/", SPIFFS, "/");
    
    // Store the Network parameters
    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_SSID) {
            wifiSettings.ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(wifiSettings.ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, wifiSettings.ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_PASS) {
            wifiSettings.pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(wifiSettings.pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, wifiSettings.pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_IP) {
            wifiSettings.ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(wifiSettings.ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, wifiSettings.ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_GATEWAY) {
            wifiSettings.gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(wifiSettings.gateway);
            // Write file to save value
            writeFile(SPIFFS, gatewayPath, wifiSettings.gateway.c_str());
          }
          //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
        }
      }
      const char* MESSAGE_TEMPLATE = "Done. Sensor node will restart, connect to your router and go to IP address: %s";
      // Buffer to store the final message
      char messageBuffer[128];
      // Use snprintf to format the message with the IP address
      snprintf(messageBuffer, sizeof(messageBuffer), MESSAGE_TEMPLATE, wifiSettings.ip.c_str());

      request->send(200, "text/plain", messageBuffer);
      delay(3000);
      ESP.restart();
    });
    server.begin();
  }
}

void loop() {
}
