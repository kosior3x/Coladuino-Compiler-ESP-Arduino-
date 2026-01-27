/**
 * SWARM ESP32 Tricycle Robot - ACCESS POINT MODE
 * ESP32 DevKit V1
 * 
 * ESP32 tworzy własną sieć WiFi:
 * - SSID: SWARM_ROBOT
 * - Password: swarm2026
 * - IP: 192.168.4.1
 * - WebSocket: port 81
 * 
 * BIBLIOTEKI:
 * - ArduinoJson 7.x
 * - WebSockets 2.x (by Markus Sattler)
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ============================================
// ACCESS POINT CONFIGURATION
// ============================================

#define AP_SSID "SWARM_ROBOT"
#define AP_PASSWORD "swarm2026"
#define AP_CHANNEL 6
#define AP_MAX_CONNECTIONS 4

// Fixed AP IP (standard)
IPAddress local_IP(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);

// WebSocket server on port 81
WebSocketsServer webSocket = WebSocketsServer(81);

// ============================================
// PIN DEFINITIONS
// ============================================

// HC-SR04 Sensors
#define TRIG_LEFT 12
#define ECHO_LEFT 14
#define TRIG_RIGHT 27
#define ECHO_RIGHT 26

// Stepper LEFT (28BYJ-48)
#define STEP_L_IN1 19
#define STEP_L_IN2 21
#define STEP_L_IN3 22
#define STEP_L_IN4 23

// Stepper RIGHT (28BYJ-48)
#define STEP_R_IN1 16
#define STEP_R_IN2 17
#define STEP_R_IN3 5
#define STEP_R_IN4 18

// Battery
#define BATTERY_ADC 34

// ============================================
// ROBOT SPECS
// ============================================

#define WHEEL_DIAMETER_MM 65.0
#define WHEEL_CIRCUMFERENCE_MM (WHEEL_DIAMETER_MM * PI)
#define STEPS_PER_REV 2048
#define STEPS_PER_MM (STEPS_PER_REV / WHEEL_CIRCUMFERENCE_MM)

#define STEP_DELAY_MIN_US 800
#define STEP_DELAY_MAX_US 3000

#define MAX_DISTANCE_CM 400
#define SOUND_SPEED 0.0343

#define BATT_FULL_V 8.4
#define BATT_CRITICAL_V 6.4
#define ADC_DIVIDER_RATIO 2.0
#define ADC_REF_VOLTAGE 3.3
#define ADC_RESOLUTION 4095.0

// ============================================
// STEPPER SEQUENCE
// ============================================

const int STEP_SEQ[8][4] = {
  {1, 0, 0, 0},
  {1, 1, 0, 0},
  {0, 1, 0, 0},
  {0, 1, 1, 0},
  {0, 0, 1, 0},
  {0, 0, 1, 1},
  {0, 0, 0, 1},
  {1, 0, 0, 1}
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
int currentSpeedL = 0;
int currentSpeedR = 0;

bool apActive = false;
bool wsConnected = false;
uint8_t wsClientNum = 0;
int connectedClients = 0;

bool emergencyStop = false;

// ============================================
// SETUP
// ============================================

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n========================================");
  Serial.println("SWARM ESP32 - ACCESS POINT MODE v3.0");
  Serial.println("========================================");
  
  initPins();
  stopMotors();
  readBattery();
  
  // Start Access Point
  startAccessPoint();
  
  // Start WebSocket
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  
  Serial.println("========================================");
  Serial.println("READY - Waiting for clients");
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
// ACCESS POINT
// ============================================

void startAccessPoint() {
  Serial.println("\nStarting Access Point...");
  
  // Configure AP with fixed IP
  if (!WiFi.softAPConfig(local_IP, gateway, subnet)) {
    Serial.println("AP Config Failed!");
    return;
  }
  
  // Start AP
  bool success = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CONNECTIONS);
  
  if (success) {
    apActive = true;
    IPAddress ip = WiFi.softAPIP();
    
    Serial.println("========================================");
    Serial.println("*** ACCESS POINT ACTIVE ***");
    Serial.printf("SSID:     %s\n", AP_SSID);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.printf("IP:       %s\n", ip.toString().c_str());
    Serial.printf("Channel:  %d\n", AP_CHANNEL);
    Serial.println("========================================");
    Serial.println("\nConnect to this network to control robot!");
    Serial.println("WebSocket will be available at ws://192.168.4.1:81");
  } else {
    Serial.println("AP Start Failed!");
    apActive = false;
  }
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
      connectedClients = WiFi.softAPgetStationNum();
      Serial.printf("Connected clients: %d\n", connectedClients);
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[WS] Client %u connected from %s\n", num, ip.toString().c_str());
        wsConnected = true;
        wsClientNum = num;
        connectedClients = WiFi.softAPgetStationNum();
        Serial.printf("Connected clients: %d\n", connectedClients);
        sendStatus();
      }
      break;
      
    case WStype_TEXT:
      {
        String msg = String((char*)payload);
        processCommand(msg);
      }
      break;
      
    default:
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
  
  float distance = duration * SOUND_SPEED / 2.0;
  return min(distance, (float)MAX_DISTANCE_CM);
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
  
  if (wsConnected) {
    webSocket.sendTXT(wsClientNum, json);
  }
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
  doc["clients"] = connectedClients;
  
  sendJSON(doc);
}

void sendStatus() {
  StaticJsonDocument<300> doc;
  doc["type"] = "status";
  doc["mode"] = "AP";
  doc["ssid"] = AP_SSID;
  doc["ip"] = WiFi.softAPIP().toString();
  doc["clients"] = connectedClients;
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
      webSocket.loop();
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
      
    } else if (action.startsWith("DANGER") || action.startsWith("WARNING")) {
      executeDifferentialDrive(speedL, speedR, 200);
      
    } else if (action.startsWith("CHAOS")) {
      executeDifferentialDrive(speedL, speedR, 300);
      
    } else if (action.startsWith("SEEK") || action.startsWith("ACTIVE")) {
      executeDifferentialDrive(speedL, speedR, 400);
      
    } else if (action.startsWith("CORRIDOR") || action.startsWith("DRIFT")) {
      executeDifferentialDrive(speedL, speedR, 250);
      
    } else if (action.startsWith("CRITICAL")) {
      executeDifferentialDrive(speedL, speedR, 200);
      
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
  webSocket.loop();
  
  unsigned long currentMillis = millis();
  
  // Update connected clients count
  static unsigned long lastClientCheck = 0;
  if (currentMillis - lastClientCheck >= 2000) {
    int clients = WiFi.softAPgetStationNum();
    if (clients != connectedClients) {
      connectedClients = clients;
      Serial.printf("Connected clients: %d\n", connectedClients);
    }
    lastClientCheck = currentMillis;
  }
  
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
    
    if (wsConnected) {
      sendSensorData();
    }
  }
  
  // Serial commands
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      processCommand(line);
    }
  }
  
  delay(1);
}
