#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <cstring>
#include <Arduino.h>
#include <DHT.h>
#include <Wire.h>
#include <BH1750.h>

BH1750 lightMeter;

const char* ssid = "HoanBao.Co";
const char* password = "1277212016";
const char* mqttServer = "test.mosquitto.org"; 
int port = 1883;
const char* event_name = "push_notification_2";
const char* ifttt_key = "h3TN_h-W-AGqUTDJgwj2CTvRx08zgProKvCW9VG-PL2";
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const int pinRelayPump = 18;
const int trigPin1 = 2;
const int echoPin1 = 4;
const int trigPin2 = 13;
const int echoPin2 = 14;
const int RAIN_SENSOR_PIN = 34;
const int MOIST_SENSOR_PIN = 32;
const int pinRelayLED = 5;
// Step motor
const int stepPin = 16; 
const int dirPin = 17; 
const int enPin = 23;

void wifiConnect() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");
}

void mqttConnect() {
  while(!mqttClient.connected()) {
    Serial.println("Attemping MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if(mqttClient.connect(clientId.c_str())) {
      Serial.println("connected");
      mqttClient.subscribe("21C2_N14/pump_pub");
      mqttClient.subscribe("21C2_N14/servo_pub");
      mqttClient.subscribe("21C2_N14/led_pub");
    }
    else {
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

// -----PUBLISH FUNCTIONS-----
void publishBool(String pub, String field, bool value) {
  // Create json object with values
  DynamicJsonDocument jsonDoc(200);
  jsonDoc[field] = value;

  // Stringify
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Publish
  mqttClient.publish(pub.c_str(), jsonString.c_str());
}

void publishSensorData(float humid, float rain, float light, bool ultra) {
  // Create json object with values
  DynamicJsonDocument jsonDoc(200);
  jsonDoc["humid"] = humid;
  jsonDoc["rain"] = rain;
  jsonDoc["light"] = light;
  jsonDoc["ultra"] = ultra;

  // Stringify
  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Publish
  mqttClient.publish("21C2_N14/device", jsonString.c_str());
}
// -----END OF PUBLISH FUNCTIONS-----

bool isPumping = false;

void callback(char* topic, byte* message, unsigned int length) {
  String strMsg;
  for(int i=0; i<length; i++) {
    strMsg += (char)message[i];
  }

  if(strcmp(topic, "21C2_N14/pump_pub") == 0){
    // Format: "[bool],[time]"
    // Extract status, time
    int commaIndex = strMsg.indexOf(',');
    String statusStr = strMsg.substring(0, commaIndex);
    String timeStr = strMsg.substring(commaIndex + 1);
    if (!timeStr) {
      timeStr = "10";
    }
    int timeValue = timeStr.toInt();

    if (statusStr == "true") {
      startPump(timeValue);
    }
    else if (statusStr == "false") {
      digitalWrite(pinRelayPump, HIGH); // Stop the pump
      isPumping = false;
    }
    else {
      Serial.println("Error at pump");
    }
  }

  else if (strcmp(topic, "21C2_N14/servo_pub") == 0) {
    // How many rounds the step motor spins
    const int rounds = 80;

    // Close lid
    if (strMsg == "true") {
      // Set direction: Counter-clockwise
      digitalWrite(dirPin, HIGH);

      // 1.8-degree step motor: 200 = 360 degrees
      for(int x = 0; x < 200*rounds; x++) {
        // HIGH to LOW = 1 step
        digitalWrite(stepPin,HIGH); 
        delayMicroseconds(500); 
        digitalWrite(stepPin,LOW); 
        delayMicroseconds(500); 
      }
    }
    // Open lid
    else if (strMsg == "false") {
      // Set direction: Clockwise
      digitalWrite(dirPin, LOW);

      for(int x = 0; x < 200*rounds; x++) {
        digitalWrite(stepPin,HIGH);
        delayMicroseconds(500);
        digitalWrite(stepPin,LOW);
        delayMicroseconds(500);
      }
    }
    else {
      Serial.println("Error at servo");
    }
  }

  else if (strcmp(topic, "21C2_N14/led_pub") == 0) {
    if (strMsg == "true") {
      digitalWrite(pinRelayLED, LOW);
    }
    else if (strMsg == "false") {
      digitalWrite(pinRelayLED, HIGH);
    }
    else {
      Serial.println("Error at LED");
    }
  }
}

unsigned long pumpStartTime;
unsigned long pumpDurationMillis;

void startPump(int duration){
  // If auto-checking period is smaller than pumping time, do nothing
  if (!isPumping) {
    digitalWrite(pinRelayPump, LOW); // Start the pump
    isPumping = true;
    pumpStartTime = millis();
    pumpDurationMillis = (unsigned long)duration * 1000;
  }
}

// -----READ SENSOR FUNCTIONS-----
float readHumidity() {
  float floatMoisture = 100 - (analogRead(MOIST_SENSOR_PIN) / 4095.0 * 100);
  int intMoisture = static_cast<int>(round(floatMoisture));
  return intMoisture;
}

float readRainSensor() {
  float floatRain = 100 - (analogRead(RAIN_SENSOR_PIN) / 4095.0 * 100);
  int intRain = static_cast<int>(round(floatRain));
  return intRain;
}

float readLightSensor() {
	float lux = 0;
  if (lightMeter.measurementReady()) {
    lux = round(lightMeter.readLightLevel() / 4);
    if (lux > 100) {
      lux = 100;
    }
  }
  return lux;
}

bool readUltrasonicDistance() {
  digitalWrite(trigPin1, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin1, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin1, LOW);
  long duration1 = pulseIn(echoPin1, HIGH);
  int distance1 = duration1 * 0.034 / 2;

  digitalWrite(trigPin2, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin2, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin2, LOW);
  long duration2 = pulseIn(echoPin2, HIGH);
  int distance2 = duration2 * 0.034 / 2;

  if(distance1 <= 20 || distance2 <= 20){
    return true;
  }
  return false;
}
// -----END OF READ SENSOR FUNCTIONS-----

void setup() {
  Serial.begin(9600);
  Serial.print("Connecting to WiFi");

  wifiConnect();
  mqttClient.setServer(mqttServer, port);
  mqttClient.setCallback(callback);
  mqttClient.setKeepAlive( 90 );
	Wire.begin();

  lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);

  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
  pinMode(RAIN_SENSOR_PIN, INPUT);
  pinMode(MOIST_SENSOR_PIN, INPUT);
  pinMode(pinRelayPump, OUTPUT);
  pinMode(pinRelayLED, OUTPUT);

  digitalWrite(pinRelayPump, HIGH);
  digitalWrite(pinRelayLED, HIGH);

  // Step motor setup
  pinMode(stepPin, OUTPUT); 
  pinMode(dirPin, OUTPUT);

  pinMode(enPin, OUTPUT);
  // Enable the step motor
  digitalWrite(enPin, LOW);
}

unsigned long lastReadTime = 0;
const unsigned long readInterval = 3000;
const unsigned long publishInterval = 5000;
unsigned long lastPublishTime = 0;

bool ultras = false;
float humid = 0, rain = 0, light = 0;

void loop() {
  if (!mqttClient.connected()) {
    mqttConnect();
  }
  mqttClient.loop();

  unsigned long currentTime = millis();

  // Check if it's time to read the sensor data
  if (currentTime - lastReadTime >= readInterval) {
    ultras = readUltrasonicDistance();
    humid = readHumidity();
    rain = readRainSensor();
    light = readLightSensor();

    lastReadTime = currentTime;
  }

  // Check if it's time to publish the sensor data
  if (currentTime - lastPublishTime >= publishInterval) {
    publishSensorData(humid, rain, light, ultras);

    lastPublishTime = currentTime;
  }

  // Check if it's time to stop pumping
  if (isPumping && millis() - pumpStartTime >= pumpDurationMillis) {
    digitalWrite(pinRelayPump, HIGH); // Stop the pump
    isPumping = false;
    publishBool("21C2_N14/pump_sub", "pump", false);
  }
}

