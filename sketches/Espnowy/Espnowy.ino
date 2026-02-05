#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi Credentials
const char* WIFI_SSID = "Redmi";        // ZMIEŃ NA SWOJĄ SIEĆ!
const char* WIFI_PASSWORD = "11111111"; // ZMIEŃ NA SWOJE HASŁO!

// Communication
const int WEBSOCKET_PORT = 81;
const int SERIAL_BAUDRATE = 115200;

// Motor pins (28BYJ-48 + ULN2003)
const int MOTOR_LEFT_PINS[4] = {25, 26, 27, 14};   // IN1-IN4
const int MOTOR_RIGHT_PINS[4] = {32, 33, 12, 13};  // IN1-IN4

// Sensor pins (HC-SR04) - TYLKO DWA CZUJNIKI pod kątem ±15°
const int SENSOR_LEFT_TRIG = 4;
const int SENSOR_LEFT_ECHO = 5;
const int SENSOR_RIGHT_TRIG = 18;
const int SENSOR_RIGHT_ECHO = 19;

// Battery monitor
const int BATTERY_PIN = 34;  // ADC pin
const float VOLTAGE_DIVIDER_RATIO = 5.95;  // Zmierzony przelicznik: V_bateria = V_ADC × 5.95

// Motor specifications (28BYJ-48)
const int STEPS_PER_REV = 4096;  // 64 internal steps * 64 gear ratio
const int STEP_DELAY_US = 1000;  // Microseconds between steps (adjust for speed)

// Half-step sequence for 28BYJ-48
const int STEP_SEQUENCE[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
};

// Timing
const unsigned long SENSOR_UPDATE_MS = 50;   // 20 Hz sensor updates
const unsigned long WATCHDOG_TIMEOUT_MS = 2000;  // 2 seconds

// Battery thresholds
const float BATTERY_MIN_V = 6.0;
const float BATTERY_WARNING_V = 6.8;
const float BATTERY_CRITICAL_V = 6.4;

// Sensor mounting angle (degrees from vehicle axis)
const float SENSOR_ANGLE = 15.0;  // ±15° from center axis

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

WebSocketsServer webSocket(WEBSOCKET_PORT);

// Motor state
int motorLeftStep = 0;
int motorRightStep = 0;
long stepsLeftCount = 0;
long stepsRightCount = 0;

// Current command
String currentAction = "STOP";
int speedLeft = 0;
int speedRight = 0;

// Sensor data - TYLKO LEWY I PRAWY
float distFront = 400.0;  // Obliczane z lewego i prawego
float distLeft = 400.0;
float distRight = 400.0;
float batteryVoltage = 7.4;
int batteryPercent = 100;

// Watchdog
unsigned long lastCommandTime = 0;
bool emergencyStop = false;

// Timing
unsigned long lastSensorUpdate = 0;


// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Serial communication
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println("\n\n================================");
  Serial.println("SWARM ROBOT ESP32 v2.2");
  Serial.println("2 SENSORS CONFIG (±15°)");
  Serial.println("================================\n");

  // Configure motor pins
  Serial.print("Initializing motors... ");
  for (int i = 0; i < 4; i++) {
    pinMode(MOTOR_LEFT_PINS[i], OUTPUT);
    pinMode(MOTOR_RIGHT_PINS[i], OUTPUT);
    digitalWrite(MOTOR_LEFT_PINS[i], LOW);
    digitalWrite(MOTOR_RIGHT_PINS[i], LOW);
  }
  Serial.println("OK");

  // Configure sensor pins - TYLKO DWA CZUJNIKI
  Serial.print("Initializing sensors (2x)... ");
  pinMode(SENSOR_LEFT_TRIG, OUTPUT);
  pinMode(SENSOR_LEFT_ECHO, INPUT);
  pinMode(SENSOR_RIGHT_TRIG, OUTPUT);
  pinMode(SENSOR_RIGHT_ECHO, INPUT);
  Serial.println("OK");

  // Configure battery monitor
  pinMode(BATTERY_PIN, INPUT);

  // Connect to WiFi
  Serial.print("Connecting to WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n\n✅ WiFi CONNECTED!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");

    // Start WebSocket server
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);
    Serial.print("WebSocket server started on port ");
    Serial.println(WEBSOCKET_PORT);

  } else {
    Serial.println("\n\n⚠️  WiFi connection FAILED!");
    Serial.println("Robot will work in SERIAL-ONLY mode");
  }

  Serial.println("\n✅ SWARM Robot ready!");
  Serial.println("Config: 2 sensors at ±15°");
  Serial.println("Waiting for commands...\n");

  lastCommandTime = millis();
}


// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Handle WebSocket events
  webSocket.loop();

  // Check for serial commands
  if (Serial.available()) {
    processSerialCommand();
  }

  // Update sensors periodically
  if (millis() - lastSensorUpdate >= SENSOR_UPDATE_MS) {
    updateSensors();
    sendSensorData();
    lastSensorUpdate = millis();
  }

  // Watchdog timer - stop if no command for 2 seconds
  if (millis() - lastCommandTime > WATCHDOG_TIMEOUT_MS && !emergencyStop) {
    emergencyStop = true;
    stopMotors();
    sendAlert("TIMEOUT", "No command received for 2s - Emergency stop");
  }

  // Execute current action
  if (!emergencyStop) {
    executeAction();
  }

  // Check battery
  checkBattery();

  delay(1);  // Small delay for stability
}


// ============================================================================
// WEBSOCKET EVENT HANDLER
// ============================================================================

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WS] Client #%u disconnected\n", num);
      break;

    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WS] Client #%u connected from %s\n", num, ip.toString().c_str());

        // Send welcome message
        StaticJsonDocument<256> doc;
        doc["type"] = "status";
        doc["message"] = "ESP32 Connected - 2 Sensors Config";
        doc["version"] = "2.2";
        doc["ip"] = WiFi.localIP().toString();
        doc["sensors"] = 2;

        String output;
        serializeJson(doc, output);
        webSocket.sendTXT(num, output);
      }
      break;

    case WStype_TEXT:
      {
        // Parse JSON command
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (error) {
          Serial.print("[WS] JSON parse error: ");
          Serial.println(error.c_str());
          return;
        }

        // Process command
        String type = doc["type"] | "";

        if (type == "command") {
          String action = doc["action"] | "STOP";
          int spdL = doc["speed_left"] | 0;
          int spdR = doc["speed_right"] | 0;

          processCommand(action, spdL, spdR);

          // Send acknowledgment
          StaticJsonDocument<128> ack;
          ack["type"] = "ack";
          ack["action"] = action;

          String ackStr;
          serializeJson(ack, ackStr);
          webSocket.sendTXT(num, ackStr);

        } else if (type == "ping") {
          // Respond to ping
          StaticJsonDocument<64> pong;
          pong["cmd"] = "pong";

          String pongStr;
          serializeJson(pong, pongStr);
          webSocket.sendTXT(num, pongStr);
        }
      }
      break;
  }
}


// ============================================================================
// SERIAL COMMAND PROCESSING
// ============================================================================

void processSerialCommand() {
  String line = Serial.readStringUntil('\n');
  line.trim();

  if (line.length() == 0) return;

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, line);

  if (error) {
    Serial.print("[SERIAL] JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  // Process command
  String type = doc["type"] | "";

  if (type == "command") {
    String action = doc["action"] | "STOP";
    int spdL = doc["speed_left"] | 0;
    int spdR = doc["speed_right"] | 0;

    processCommand(action, spdL, spdR);
  }
}


// ============================================================================
// COMMAND PROCESSING
// ============================================================================

void processCommand(String action, int spdL, int spdR) {
  currentAction = action;
  speedLeft = constrain(spdL, -150, 150);
  speedRight = constrain(spdR, -150, 150);

  lastCommandTime = millis();
  emergencyStop = false;

  Serial.print("[CMD] ");
  Serial.print(action);
  Serial.print(" L=");
  Serial.print(speedLeft);
  Serial.print(" R=");
  Serial.println(speedRight);
}


// ============================================================================
// ACTION EXECUTION
// ============================================================================

void executeAction() {
  if (currentAction == "STOP") {
    stopMotors();

  } else if (currentAction == "FORWARD") {
    setMotorSpeed(speedLeft, speedRight);

  } else if (currentAction == "REVERSE") {
    setMotorSpeed(-abs(speedLeft), -abs(speedRight));

  } else if (currentAction == "TURN_LEFT") {
    // Left slower, right faster
    setMotorSpeed(speedLeft, speedRight);

  } else if (currentAction == "TURN_RIGHT") {
    // Right slower, left faster
    setMotorSpeed(speedLeft, speedRight);

  } else if (currentAction == "ESCAPE") {
    // Rotate in place (one forward, one reverse)
    setMotorSpeed(speedLeft, speedRight);
  }
}


// ============================================================================
// MOTOR CONTROL
// ============================================================================

void setMotorSpeed(int leftSpeed, int rightSpeed) {
  // Left motor
  if (leftSpeed > 0) {
    stepMotor(MOTOR_LEFT_PINS, motorLeftStep, 1);
    stepsLeftCount++;
  } else if (leftSpeed < 0) {
    stepMotor(MOTOR_LEFT_PINS, motorLeftStep, -1);
    stepsLeftCount--;
  }

  // Right motor
  if (rightSpeed > 0) {
    stepMotor(MOTOR_RIGHT_PINS, motorRightStep, 1);
    stepsRightCount++;
  } else if (rightSpeed < 0) {
    stepMotor(MOTOR_RIGHT_PINS, motorRightStep, -1);
    stepsRightCount--;
  }

  // Speed control via delay
  int avgSpeed = (abs(leftSpeed) + abs(rightSpeed)) / 2;
  int delayTime = map(avgSpeed, 0, 150, 2000, 500);  // Faster = shorter delay
  delayMicroseconds(delayTime);
}


void stepMotor(const int pins[4], int &currentStep, int direction) {
  // Update step position
  currentStep += direction;
  if (currentStep >= 8) currentStep = 0;
  if (currentStep < 0) currentStep = 7;

  // Apply step sequence
  for (int i = 0; i < 4; i++) {
    digitalWrite(pins[i], STEP_SEQUENCE[currentStep][i]);
  }
}


void stopMotors() {
  // Turn off all motor coils
  for (int i = 0; i < 4; i++) {
    digitalWrite(MOTOR_LEFT_PINS[i], LOW);
    digitalWrite(MOTOR_RIGHT_PINS[i], LOW);
  }
}


// ============================================================================
// SENSOR READING - DWA CZUJNIKI POD KĄTEM ±15°
// ============================================================================

void updateSensors() {
  // Czytaj oba czujniki z opóźnieniem między nimi
  distLeft = readUltrasonic(SENSOR_LEFT_TRIG, SENSOR_LEFT_ECHO);
  delay(50);  // 50ms przerwy między czujnikami - zapobiega interferencji

  distRight = readUltrasonic(SENSOR_RIGHT_TRIG, SENSOR_RIGHT_ECHO);

  // Oblicz distFront na podstawie lewego i prawego
  // Czujniki pod kątem ±15°, więc odległość "prosto" jest trochę większa
  // Używamy cos(15°) ≈ 0.966 jako korekcji geometrycznej

  float leftProjected = distLeft * 0.966;   // Projekcja lewego na oś przód
  float rightProjected = distRight * 0.966; // Projekcja prawego na oś przód

  // distFront = minimum z obu projekcji (najbliższa przeszkoda)
  distFront = fminf(leftProjected, rightProjected);

  // Jeśli oba czujniki pokazują timeout (400), zostaw 400
  if (distLeft >= 400 && distRight >= 400) {
    distFront = 400.0;
  }

  // Read battery
  int adcValue = analogRead(BATTERY_PIN);
  batteryVoltage = (adcValue / 4095.0) * 3.3 * VOLTAGE_DIVIDER_RATIO;

  // Calculate percentage (simple linear 6.0V-8.4V)
  batteryPercent = constrain(map(batteryVoltage * 10, 60, 84, 0, 100), 0, 100);
}


float readUltrasonic(int trigPin, int echoPin) {
  // Send pulse
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Read echo
  long duration = pulseIn(echoPin, HIGH, 30000);  // 30ms timeout

  if (duration == 0) {
    return 400.0;  // Max range if no echo
  }

  // Calculate distance (speed of sound = 343 m/s)
  float distance = ((float)duration * 0.0343f) / 2.0f;  // cm

  return constrain(distance, 20.0f, 400.0f);
}


// ============================================================================
// DATA TRANSMISSION
// ============================================================================

void sendSensorData() {
  StaticJsonDocument<512> doc;

  doc["type"] = "sensors";
  doc["dist_front"] = round(distFront * 10) / 10.0;  // 1 decimal
  doc["dist_left"] = round(distLeft * 10) / 10.0;
  doc["dist_right"] = round(distRight * 10) / 10.0;
  doc["battery_v"] = round(batteryVoltage * 100) / 100.0;  // 2 decimals
  doc["battery_pct"] = batteryPercent;
  doc["steps_l"] = stepsLeftCount;
  doc["steps_r"] = stepsRightCount;
  doc["action"] = currentAction;
  doc["emergency"] = emergencyStop;
  doc["sensor_config"] = "2x_15deg";  // Informacja o konfiguracji

  String output;
  serializeJson(doc, output);

  // Send via WebSocket
  webSocket.broadcastTXT(output);

  // Send via Serial
  Serial.println(output);
}


void sendAlert(String level, String message) {
  StaticJsonDocument<256> doc;

  doc["type"] = "alert";
  doc["level"] = level;
  doc["message"] = message;
  doc["timestamp"] = millis();

  String output;
  serializeJson(doc, output);

  webSocket.broadcastTXT(output);
  Serial.println(output);
}


// ============================================================================
// BATTERY MONITORING
// ============================================================================

void checkBattery() {
  static unsigned long lastBatteryCheck = 0;
  static bool warningShown = false;

  if (millis() - lastBatteryCheck < 5000) return;  // Check every 5s
  lastBatteryCheck = millis();

  if (batteryVoltage < BATTERY_CRITICAL_V) {
    if (!emergencyStop) {
      emergencyStop = true;
      stopMotors();
      sendAlert("CRITICAL", "Battery critical! " + String(batteryVoltage, 2) + "V");
    }
  } else if (batteryVoltage < BATTERY_WARNING_V && !warningShown) {
    sendAlert("WARNING", "Battery low: " + String(batteryVoltage, 2) + "V");
    warningShown = true;
  } else if (batteryVoltage >= BATTERY_WARNING_V) {
    warningShown = false;
  }
}


// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void printStatus() {
  Serial.println("\n--- SWARM STATUS ---");
  Serial.print("WiFi: ");
  Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Battery: ");
  Serial.print(batteryVoltage, 2);
  Serial.print("V (");
  Serial.print(batteryPercent);
  Serial.println("%)");
  Serial.print("Sensors: L=");
  Serial.print(distLeft, 1);
  Serial.print(" F=");
  Serial.print(distFront, 1);
  Serial.print(" R=");
  Serial.println(distRight, 1);
  Serial.print("Steps: L=");
  Serial.print(stepsLeftCount);
  Serial.print(" R=");
  Serial.println(stepsRightCount);
  Serial.println("-------------------\n");
}
