/**
 * SWARM ESP32 Tricycle Robot - UNIFIED 3 MODES
 *
 * COMMUNICATION MODES:
 * 1. WiFi Access Point (default)
 * 2. WiFi Client (connects to router)
 * 3. USB Serial (for Android OTG)
 *
 * MODE SELECTION (at startup):
 * - Hold button on GPIO 0 = WiFi Client mode
 * - Normal boot = WiFi AP mode
 * - No WiFi networks = USB Serial mode (fallback)
 *
 * BIBLIOTEKI:
 * - ArduinoJson 7.x
 * - WebSockets 2.x
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ============================================
// MODE SELECTION
// ============================================

enum CommMode {
  MODE_AP,        // Access Point
  MODE_CLIENT,    // WiFi Client
  MODE_USB        // USB Serial only
};

CommMode currentMode = MODE_AP;

// ============================================
// WIFI CONFIGURATION
// ============================================

// AP Mode
#define AP_SSID "SWARM_ROBOT"
#define AP_PASSWORD "swarm2026"
#define AP_CHANNEL 6

// Client Mode (saved networks)
#define MAX_NETWORKS 5
struct WiFiNetwork {
  char ssid[32];
  char password[64];
};

WiFiNetwork savedNetworks[MAX_NETWORKS] = {
  {"OPPO", "11111111"},
  {"Redmi", "11111111"},
  {"", ""},
  {"", ""},
  {"", ""}
};

// AP IP
IPAddress ap_local_IP(192, 168, 4, 1);
IPAddress ap_gateway(192, 168, 4, 1);
IPAddress ap_subnet(255, 255, 255, 0);

// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(81);
Preferences preferences;

// ============================================
// PINS
// ============================================

// Mode selection button
#define MODE_BUTTON 0  // BOOT button on DevKit

// Sensors
#define TRIG_LEFT 12
#define ECHO_LEFT 14
#define TRIG_RIGHT 27
#define ECHO_RIGHT 26

// Steppers
#define STEP_L_IN1 19
#define STEP_L_IN2 21
#define STEP_L_IN3 22
#define STEP_L_IN4 23

#define STEP_R_IN1 16
#define STEP_R_IN2 17
#define STEP_R_IN3 5
#define STEP_R_IN4 18

#define BATTERY_ADC 34

// ============================================
// ROBOT SPECS
// ============================================

#define WHEEL_DIAMETER_MM 65.0
#define STEPS_PER_REV 2048
#define STEPS_PER_MM (STEPS_PER_REV / (WHEEL_DIAMETER_MM * PI))

#define STEP_DELAY_MIN_US 800
#define STEP_DELAY_MAX_US 3000

#define MAX_DISTANCE_CM 400
#define SOUND_SPEED 0.0343f

#define BATT_FULL_V 8.4
#define BATT_CRITICAL_V 6.4
#define ADC_DIVIDER_RATIO 2.0
#define ADC_REF_VOLTAGE 3.3
#define ADC_RESOLUTION 4095.0

// ============================================
// STEPPER SEQUENCE
// ============================================

const int STEP_SEQ[8][4] = {
  {1,0,0,0}, {1,1,0,0}, {0,1,0,0}, {0,1,1,0},
  {0,0,1,0}, {0,0,1,1}, {0,0,0,1}, {1,0,0,1}
};

// ============================================
// GLOBALS
// ============================================

unsigned long lastSensorRead = 0;
float distLeft = 0, distRight = 0;
float batteryVoltage = 0;
int batteryPercent = 0;

int stepPosL = 0, stepPosR = 0;
String currentAction = "STOP";
int currentSpeedL = 0, currentSpeedR = 0;

bool wsConnected = false;
uint8_t wsClientNum = 0;
bool emergencyStop = false;

// USB Serial mode
bool usbMode = false;

// ============================================
// SETUP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n========================================");
  Serial.println("SWARM ESP32 - UNIFIED 3 MODES v4.0");
  Serial.println("========================================");

  pinMode(MODE_BUTTON, INPUT_PULLUP);

  initPins();
  stopMotors();
  readBattery();

  // Detect mode
  currentMode = detectMode();

  // Start communication based on mode
  switch (currentMode) {
    case MODE_AP:
      Serial.println("\n[MODE] Access Point");
      startAccessPoint();
      webSocket.begin();
      webSocket.onEvent(onWebSocketEvent);
      break;

    case MODE_CLIENT:
      Serial.println("\n[MODE] WiFi Client");
      loadSettings();
      connectWiFiClient();
      webSocket.begin();
      webSocket.onEvent(onWebSocketEvent);
      break;

    case MODE_USB:
      Serial.println("\n[MODE] USB Serial");
      usbMode = true;
      break;
  }

  Serial.println("========================================");
  Serial.println("READY");
  Serial.println("========================================");

  sendStatus();
}

void initPins() {
  pinMode(TRIG_LEFT, OUTPUT);
  pinMode(ECHO_LEFT, INPUT);
  pinMode(TRIG_RIGHT, OUTPUT);
  pinMode(ECHO_RIGHT, INPUT);

  pinMode(STEP_L_IN1, OUTPUT);
  pinMode(STEP_L_IN2, OUTPUT);
  pinMode(STEP_L_IN3, OUTPUT);
  pinMode(STEP_L_IN4, OUTPUT);

  pinMode(STEP_R_IN1, OUTPUT);
  pinMode(STEP_R_IN2, OUTPUT);
  pinMode(STEP_R_IN3, OUTPUT);
  pinMode(STEP_R_IN4, OUTPUT);

  pinMode(BATTERY_ADC, INPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

// ============================================
// MODE DETECTION
// ============================================

CommMode detectMode() {
  // Check if BOOT button pressed during startup
  if (digitalRead(MODE_BUTTON) == LOW) {
    Serial.println("BOOT button pressed - WiFi Client mode");
    delay(1000);
    return MODE_CLIENT;
  }

  // Try AP mode first
  Serial.println("Auto-detecting mode...");

  // Check if any WiFi networks saved
  preferences.begin("swarm", true);
  String ssid0 = preferences.getString("ssid0", "");
  preferences.end();

  if (ssid0.length() > 0) {
    Serial.println("Saved networks found - trying Client mode");
    return MODE_CLIENT;
  }

  // Default: AP mode
  Serial.println("No saved networks - using AP mode");
  return MODE_AP;
}

// ============================================
// WIFI ACCESS POINT
// ============================================

void startAccessPoint() {
  if (!WiFi.softAPConfig(ap_local_IP, ap_gateway, ap_subnet)) {
    Serial.println("AP Config Failed!");
    return;
  }

  bool success = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, 4);

  if (success) {
    IPAddress ip = WiFi.softAPIP();
    Serial.println("========================================");
    Serial.println("*** ACCESS POINT ACTIVE ***");
    Serial.printf("SSID:     %s\n", AP_SSID);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.printf("IP:       %s\n", ip.toString().c_str());
    Serial.println("========================================");
  } else {
    Serial.println("AP Start Failed - falling back to USB mode");
    currentMode = MODE_USB;
    usbMode = true;
  }
}

// ============================================
// WIFI CLIENT
// ============================================

void loadSettings() {
  preferences.begin("swarm", true);

  for (int i = 0; i < MAX_NETWORKS; i++) {
    String ssidKey = "ssid" + String(i);
    String passKey = "pass" + String(i);

    String ssid = preferences.getString(ssidKey.c_str(), "");
    String pass = preferences.getString(passKey.c_str(), "");

    if (ssid.length() > 0) {
      ssid.toCharArray(savedNetworks[i].ssid, 32);
      pass.toCharArray(savedNetworks[i].password, 64);
    }
  }

  preferences.end();
}

void connectWiFiClient() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  int numNetworks = WiFi.scanNetworks();
  Serial.printf("Found %d networks\n", numNetworks);

  for (int slot = 0; slot < MAX_NETWORKS; slot++) {
    if (strlen(savedNetworks[slot].ssid) == 0) continue;

    for (int i = 0; i < numNetworks; i++) {
      if (WiFi.SSID(i) == savedNetworks[slot].ssid) {
        Serial.printf("Connecting to: %s\n", savedNetworks[slot].ssid);

        WiFi.begin(savedNetworks[slot].ssid, savedNetworks[slot].password);

        int timeout = 30;
        while (WiFi.status() != WL_CONNECTED && timeout > 0) {
          delay(500);
          Serial.print(".");
          timeout--;
        }

        if (WiFi.status() == WL_CONNECTED) {
          Serial.printf("\n*** CONNECTED! IP: %s ***\n",
                       WiFi.localIP().toString().c_str());
          return;
        }
      }
    }
  }

  Serial.println("\nWiFi connection failed - falling back to USB mode");
  currentMode = MODE_USB;
  usbMode = true;
}

// ============================================
// WEBSOCKET
// ============================================

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client %u disconnected\n", num);
      if (num == wsClientNum) {
        wsConnected = false;
        stopMotors();
      }
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WS] Client %u connected: %s\n", num, ip.toString().c_str());
        wsConnected = true;
        wsClientNum = num;
        sendStatus();
      }
      break;

    case WStype_TEXT:
      processCommand(String((char*)payload));
      break;
  }
}

// ============================================
// SENSORS
// ============================================

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return MAX_DISTANCE_CM;

  return fminf((float)duration * SOUND_SPEED / 2.0f, (float)MAX_DISTANCE_CM);
}

void readAllSensors() {
  distLeft = readDistance(TRIG_LEFT, ECHO_LEFT);
  distRight = readDistance(TRIG_RIGHT, ECHO_RIGHT);
}

void readBattery() {
  int adcRaw = analogRead(BATTERY_ADC);
  float adcV = (adcRaw / ADC_RESOLUTION) * ADC_REF_VOLTAGE;
  batteryVoltage = adcV * ADC_DIVIDER_RATIO;
  batteryVoltage = constrain(batteryVoltage, BATT_CRITICAL_V, BATT_FULL_V);

  batteryPercent = map((int)(batteryVoltage * 100),
                       (int)(BATT_CRITICAL_V * 100),
                       (int)(BATT_FULL_V * 100),
                       0, 100);
  batteryPercent = constrain(batteryPercent, 0, 100);
}

// ============================================
// JSON COMMUNICATION
// ============================================

void sendJSON(JsonDocument& doc) {
  String json;
  serializeJson(doc, json);

  // Send via WebSocket (if connected)
  if (wsConnected && !usbMode) {
    webSocket.sendTXT(wsClientNum, json);
  }

  // Always send via Serial (for USB mode and debugging)
  Serial.println(json);
}

void sendSensorData() {
  StaticJsonDocument<300> doc;

  doc["type"] = "sensors";
  doc["dist_left"] = round(distLeft * 10) / 10.0;
  doc["dist_right"] = round(distRight * 10) / 10.0;
  doc["dist_front"] = round((distLeft + distRight) / 2.0 * 10) / 10.0;
  doc["battery_v"] = round(batteryVoltage * 100) / 100.0;
  doc["battery_pct"] = batteryPercent;
  doc["action"] = currentAction;
  doc["speed_left"] = currentSpeedL;
  doc["speed_right"] = currentSpeedR;

  sendJSON(doc);
}

void sendStatus() {
  StaticJsonDocument<300> doc;
  doc["type"] = "status";

  switch (currentMode) {
    case MODE_AP:
      doc["mode"] = "AP";
      doc["ssid"] = AP_SSID;
      doc["ip"] = WiFi.softAPIP().toString();
      break;

    case MODE_CLIENT:
      doc["mode"] = "CLIENT";
      doc["ssid"] = WiFi.SSID();
      doc["ip"] = WiFi.localIP().toString();
      break;

    case MODE_USB:
      doc["mode"] = "USB";
      doc["ip"] = "serial";
      break;
  }

  doc["battery_v"] = round(batteryVoltage * 100) / 100.0;
  doc["uptime"] = millis() / 1000;

  sendJSON(doc);
}

void sendAlert(const char* message) {
  StaticJsonDocument<200> doc;
  doc["type"] = "alert";
  doc["message"] = message;
  doc["battery_v"] = batteryVoltage;
  doc["dist_left"] = distLeft;
  doc["dist_right"] = distRight;
  sendJSON(doc);
}

// ============================================
// STEPPER CONTROL
// ============================================

void setStepperL(int step) {
  step = ((step % 8) + 8) % 8;
  digitalWrite(STEP_L_IN1, STEP_SEQ[step][0]);
  digitalWrite(STEP_L_IN2, STEP_SEQ[step][1]);
  digitalWrite(STEP_L_IN3, STEP_SEQ[step][2]);
  digitalWrite(STEP_L_IN4, STEP_SEQ[step][3]);
}

void setStepperR(int step) {
  step = ((step % 8) + 8) % 8;
  digitalWrite(STEP_R_IN1, STEP_SEQ[step][0]);
  digitalWrite(STEP_R_IN2, STEP_SEQ[step][1]);
  digitalWrite(STEP_R_IN3, STEP_SEQ[step][2]);
  digitalWrite(STEP_R_IN4, STEP_SEQ[step][3]);
}

void disableSteppers() {
  digitalWrite(STEP_L_IN1, LOW);
  digitalWrite(STEP_L_IN2, LOW);
  digitalWrite(STEP_L_IN3, LOW);
  digitalWrite(STEP_L_IN4, LOW);
  digitalWrite(STEP_R_IN1, LOW);
  digitalWrite(STEP_R_IN2, LOW);
  digitalWrite(STEP_R_IN3, LOW);
  digitalWrite(STEP_R_IN4, LOW);
}

void stopMotors() {
  disableSteppers();
  currentAction = "STOP";
  currentSpeedL = 0;
  currentSpeedR = 0;
  emergencyStop = false;
}

int speedToDelay(int speed) {
  speed = constrain(abs(speed), 0, 150);
  return map(speed, 0, 150, STEP_DELAY_MAX_US, STEP_DELAY_MIN_US);
}

void executeDifferentialDrive(int speedL, int speedR, int durationMs) {
  currentSpeedL = speedL;
  currentSpeedR = speedR;

  if (speedL == 0 && speedR == 0) {
    stopMotors();
    return;
  }

  int dirL = (speedL >= 0) ? 1 : -1;
  int dirR = (speedR >= 0) ? 1 : -1;

  int delayL = speedToDelay(speedL);
  int delayR = speedToDelay(speedR);

  unsigned long startTime = millis();
  unsigned long lastStepL = 0;
  unsigned long lastStepR = 0;

  while (millis() - startTime < durationMs) {
    unsigned long now = micros();

    if (speedL != 0 && now - lastStepL >= delayL) {
      stepPosL += dirL;
      setStepperL(stepPosL);
      lastStepL = now;
    }

    if (speedR != 0 && now - lastStepR >= delayR) {
      stepPosR -= dirR;
      setStepperR(stepPosR);
      lastStepR = now;
    }

    if ((millis() - startTime) % 50 == 0) {
      if (!usbMode) webSocket.loop();
      readAllSensors();

      if (distLeft < 5.0 || distRight < 5.0) {
        emergencyStop = true;
        stopMotors();
        sendAlert("COLLISION_IMMINENT");
        return;
      }
    }

    if (emergencyStop) {
      stopMotors();
      return;
    }

    delayMicroseconds(100);
  }

  disableSteppers();
}

// ============================================
// COMMAND PROCESSING
// ============================================

void processCommand(String json) {
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, json);

  if (err) {
    Serial.printf("[ERROR] JSON: %s\n", err.c_str());
    return;
  }

  // Ping/Pong
  if (doc.containsKey("cmd")) {
    String cmd = doc["cmd"].as<String>();
    if (cmd == "ping") {
      StaticJsonDocument<100> resp;
      resp["cmd"] = "pong";
      resp["timestamp"] = millis();
      sendJSON(resp);
      return;
    }
  }

  String type = doc["type"].as<String>();

  if (type == "command") {
    String action = doc["action"].as<String>();
    int speedL = doc["speed_left"] | 100;
    int speedR = doc["speed_right"] | 100;

    currentAction = action;
    emergencyStop = false;

    Serial.printf("[CMD] %s (L=%d, R=%d)\n", action.c_str(), speedL, speedR);

    if (action == "STOP") {
      stopMotors();
    } else if (action == "FORWARD") {
      executeDifferentialDrive(speedL, speedR, 500);
    } else if (action == "TURN_LEFT") {
      executeDifferentialDrive(speedL, speedR, 300);
    } else if (action == "TURN_RIGHT") {
      executeDifferentialDrive(speedL, speedR, 300);
    } else if (action == "ESCAPE") {
      executeDifferentialDrive(-speedL, speedR, 400);
    } else {
      executeDifferentialDrive(speedL, speedR, 300);
    }

    sendSensorData();

  } else if (type == "get_status") {
    sendStatus();
  } else if (type == "emergency_stop") {
    emergencyStop = true;
    stopMotors();
    sendAlert("EMERGENCY_STOP");
  }
}

// ============================================
// SAFETY
// ============================================

void checkBattery() {
  if (batteryVoltage < BATT_CRITICAL_V) {
    emergencyStop = true;
    stopMotors();
    sendAlert("BATTERY_CRITICAL");
  }
}

void checkObstacles() {
  if ((distLeft < 5.0 || distRight < 5.0) &&
      currentAction != "STOP" &&
      currentSpeedL != 0 && currentSpeedR != 0) {
    emergencyStop = true;
    stopMotors();
    sendAlert("OBSTACLE_CRITICAL");
  }
}

// ============================================
// MAIN LOOP
// ============================================

void loop() {
  // WebSocket loop (only if not USB mode)
  if (!usbMode) {
    webSocket.loop();
  }

  unsigned long currentMillis = millis();

  // Read sensors
  if (currentMillis - lastSensorRead >= 100) {
    readAllSensors();
    lastSensorRead = currentMillis;

    static int readCount = 0;
    if (++readCount >= 10) {
      readBattery();
      checkBattery();
      readCount = 0;
    }

    checkObstacles();

    // Send sensors (WebSocket or Serial)
    if (wsConnected || usbMode) {
      sendSensorData();
    }
  }

  // Serial commands (works in all modes)
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      processCommand(line);
    }
  }

  delay(1);
}
