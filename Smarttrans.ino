#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <math.h>

// =====================================
// WIFI SETTINGS
// =====================================
const char* ssid = "nikita09";
const char* password = "12345678";

// =====================================
// SENSOR PINS
// =====================================
#define DHT_PIN 4
#define FLOAT_PIN 27
#define VOLTAGE_PIN 34
#define CURRENT_PIN 35

#define DHT_TYPE DHT11

// =====================================
// SENSOR OBJECTS
// =====================================
DHT dht(DHT_PIN, DHT_TYPE);
WebServer server(80);

// =====================================
// SENSOR VALUES
// =====================================
float temperature = NAN;
float voltage = NAN;
float current = NAN;

bool oilNormal = false;

// =====================================
// ADC SETTINGS
// =====================================
const float ADC_REFERENCE = 3.3;
const int ADC_MAX_VALUE = 4095;

// Calibration factors must be determined
// using a suitable reference instrument.
float voltageCalibration = 1.0;

// Example for ACS712 20A model only.
// Confirm the actual model before calibration.
float currentSensitivity = 0.100;

// =====================================
// DASHBOARD HTML
// =====================================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="3">

<title>Smart Transformer Monitoring</title>

<style>
body {
  font-family: Arial, sans-serif;
  background: #eef2f7;
  margin: 0;
  text-align: center;
}

header {
  background: #12345a;
  color: white;
  padding: 22px 10px;
}

.container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(145px, 1fr));
  gap: 16px;
  padding: 20px;
  max-width: 900px;
  margin: auto;
}

.card {
  background: white;
  padding: 22px 10px;
  border-radius: 12px;
  box-shadow: 0 3px 10px #0002;
}

.label {
  font-size: 17px;
  color: #444;
}

.value {
  font-size: 26px;
  font-weight: bold;
  color: #087e8b;
}

.status {
  background: white;
  margin: 10px 18px;
  padding: 18px;
  border-radius: 12px;
}

footer {
  padding: 15px;
  color: #555;
}
</style>
</head>

<body>

<header>
<h2>Smart Transformer</h2>
<p>Health Monitoring System Using IoT</p>
</header>

<div class="container">

<div class="card">
<div class="label">Temperature</div>
<p class="value">%TEMP%</p>
</div>

<div class="card">
<div class="label">Oil Level</div>
<p class="value">%OIL%</p>
</div>

<div class="card">
<div class="label">Voltage</div>
<p class="value">%VOLT%</p>
</div>

<div class="card">
<div class="label">Current</div>
<p class="value">%CURR%</p>
</div>

</div>

<div class="status">
<h3>Transformer Monitoring Status</h3>
<p>%STATUS%</p>
</div>

<footer>
Smart Transformer Health Monitoring System Using IoT
<p>Updates every 3 seconds</p>
</footer>

</body>
</html>
)rawliteral";

// =====================================
// READ AC RMS AT ANALOG INPUT
// =====================================
float readACRMS(int pin) {

  const int samples = 1000;

  double sum = 0;
  double sumSquares = 0;

  for (int i = 0; i < samples; i++) {

    float sampleVoltage =
      analogRead(pin) * ADC_REFERENCE / ADC_MAX_VALUE;

    sum += sampleVoltage;
    sumSquares += (double)sampleVoltage * sampleVoltage;

    delayMicroseconds(200);
  }

  double mean = sum / samples;

  double variance =
    (sumSquares / samples) - (mean * mean);

  if (variance < 0) {
    variance = 0;
  }

  return sqrt(variance);
}

// =====================================
// READ VOLTAGE
// =====================================
float readVoltage() {

  float sensorRMS = readACRMS(VOLTAGE_PIN);

  // Calibration required against a known
  // AC voltage reference.
  return sensorRMS * voltageCalibration;
}

// =====================================
// READ CURRENT
// =====================================
float readCurrent() {

  float sensorRMS = readACRMS(CURRENT_PIN);

  // Approximate AC current.
  // Calibration depends on ACS712 model
  // and its output characteristics.
  float amps = sensorRMS / currentSensitivity;

  return amps;
}

// =====================================
// UPDATE SENSORS
// =====================================
void updateSensors() {

  // DHT11 temperature
  float t = dht.readTemperature();

  if (!isnan(t)) {
    temperature = t;
  }

  // Float sensor
  // Assumes the switch connects GPIO 27 to GND
  // when the oil level is in the normal state.
  oilNormal = (digitalRead(FLOAT_PIN) == LOW);

  // Voltage and current
  voltage = readVoltage();
  current = readCurrent();

  // Serial Monitor debugging
  Serial.print("Temperature: ");
  Serial.println(temperature);

  Serial.print("Oil Normal: ");
  Serial.println(oilNormal ? "YES" : "NO");

  Serial.print("Voltage reading: ");
  Serial.println(voltage, 3);

  Serial.print("Current reading: ");
  Serial.println(current, 3);

  Serial.println("----------------------");
}

// =====================================
// DASHBOARD HANDLER
// =====================================
void handleRoot() {

  updateSensors();

  String page = webpage;

  if (isnan(temperature)) {
    page.replace("%TEMP%", "Sensor Error");
  } else {
    page.replace("%TEMP%", String(temperature, 1) + " &deg;C");
  }

  page.replace("%OIL%", oilNormal ? "Normal" : "Low");

  if (isnan(voltage)) {
    page.replace("%VOLT%", "Sensor Error");
  } else {
    page.replace("%VOLT%", String(voltage, 2) + " V");
  }

  if (isnan(current)) {
    page.replace("%CURR%", "Sensor Error");
  } else {
    page.replace("%CURR%", String(current, 3) + " A");
  }

  String status = "Monitoring";

  if (isnan(temperature)) {
    status = "WARNING: Check DHT11 sensor";
  }
  else if (temperature >= 60 || !oilNormal) {
    status = "WARNING: Check temperature/oil level";
  }

  page.replace("%STATUS%", status);

  server.send(200, "text/html", page);
}

// =====================================
// SETUP
// =====================================
void setup() {

  Serial.begin(115200);

  // Initialize sensors
  dht.begin();

  pinMode(FLOAT_PIN, INPUT_PULLUP);

  analogReadResolution(12);

  analogSetPinAttenuation(VOLTAGE_PIN, ADC_11db);
  analogSetPinAttenuation(CURRENT_PIN, ADC_11db);

  // Connect Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi");

  unsigned long startTime = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - startTime < 60000) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println();
    Serial.println("Wi-Fi connection failed.");
    Serial.println("Check Wi-Fi name and password.");
    return;
  }

  Serial.println();
  Serial.println("Wi-Fi connected!");

  Serial.print("Dashboard IP: http://");
  Serial.println(WiFi.localIP());

  // Start web server
  server.on("/", HTTP_GET, handleRoot);
  server.begin();

  Serial.println("Web server started!");
}

// =====================================
// LOOP
// =====================================
void loop() {

  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient();
  }
}