#include <Arduino.h>

/* ================= CONFIG ================= */
#define NODE_ID 1
#define BAUDRATE 115200

#define CMD_FRAME_SIZE 8
#define TELE_FRAME_SIZE 9

#define SOF 0xAA
#define EOF_ 0x55

#define TELE_SOF 0xAB
#define TELE_EOF 0x54

#define FLAG_STOP  0x01
#define FLAG_EMERG 0x02

/* ================= PINY ================= */
// Left Stepper
#define L1 19
#define L2 21
#define L3 22
#define L4 23

// Right Stepper
#define R1 16
#define R2 17
#define R3 5
#define R4 18

/* ================= STEPPER SEQUENCE ================= */
const uint8_t SEQ[8][4] = {
  {1,0,0,0},{1,1,0,0},{0,1,0,0},{0,1,1,0},
  {0,0,1,0},{0,0,1,1},{0,0,0,1},{1,0,0,1}
};

/* ================= STATE ================= */
volatile int8_t targetL = 0;
volatile int8_t targetR = 0;
int stepL = 0;
int stepR = 0;
unsigned long lastStepL = 0;
unsigned long lastStepR = 0;
unsigned long lastCmdTime = 0;

/* ================= CRC8 ================= */
uint8_t crc8(const uint8_t* data, uint8_t len){
  uint8_t crc=0;
  while(len--){
    uint8_t inbyte = *data++;
    for(uint8_t i=8;i;i--){
      uint8_t mix=(crc^inbyte)&1;
      crc>>=1;
      if(mix) crc^=0x8C;
      inbyte>>=1;
    }
  }
  return crc;
}

/* ================= UTILS ================= */
int speedToDelay(int s){
  s = constrain(abs(s),0,127);
  return map(s,0,127,3000,800);
}
void setL(int p){ p=(p+8)%8; digitalWrite(L1,SEQ[p][0]); digitalWrite(L2,SEQ[p][1]); digitalWrite(L3,SEQ[p][2]); digitalWrite(L4,SEQ[p][3]); }
void setR(int p){ p=(p+8)%8; digitalWrite(R1,SEQ[p][0]); digitalWrite(R2,SEQ[p][1]); digitalWrite(R3,SEQ[p][2]); digitalWrite(R4,SEQ[p][3]); }

/* ================= UPDATE MOTORS ================= */
void updateMotorsRT(){
  unsigned long now = micros();
  if(targetL!=0 && now-lastStepL>=speedToDelay(targetL)){ stepL+=(targetL>0?1:-1); setL(stepL); lastStepL=now; }
  if(targetR!=0 && now-lastStepR>=speedToDelay(targetR)){ stepR-=(targetR>0?1:-1); setR(stepR); lastStepR=now; }
}

/* ================= PARSER CMD ================= */
void handleBinaryRX(){
  static uint8_t buf[CMD_FRAME_SIZE];
  static uint8_t idx=0;
  while(Serial.available()){
    uint8_t b = Serial.read();
    if(idx==0 && b!=SOF) continue;
    buf[idx++]=b;
    if(idx==CMD_FRAME_SIZE){
      idx=0;
      if(buf[7]!=EOF_) return;
      if(buf[1]!=NODE_ID && buf[1]!=0xFF) return;
      if(crc8(buf,6)!=buf[6]) return;

      int8_t l=(int8_t)buf[2];
      int8_t r=(int8_t)buf[3];
      uint8_t flags=buf[4];
      lastCmdTime=millis();

      if(flags & FLAG_STOP){ targetL=0; targetR=0; return; }
      if(flags & FLAG_EMERG){ targetL=0; targetR=0; return; }

      targetL=constrain(l,-127,127);
      targetR=constrain(r,-127,127);
    }
  }
}

/* ================= TELEMETRIA ================= */
void sendTelemetry(){
  uint8_t frame[TELE_FRAME_SIZE];
  frame[0]=TELE_SOF;
  frame[1]=NODE_ID;
  frame[2]=constrain(stepL&0xFF,0,127); // przykładowo używamy pozycji jako odległość
  frame[3]=constrain(stepR&0xFF,0,127);
  frame[4]=64; // battery %
  frame[5]=0;  // state
  frame[6]=millis()/1000 & 0xFF; // seq
  frame[7]=crc8(frame,7);
  frame[8]=TELE_EOF;
  Serial.write(frame,TELE_FRAME_SIZE);
}

/* ================= WATCHDOG ================= */
void commWatchdog(){ if(millis()-lastCmdTime>300){ targetL=0; targetR=0; } }

/* ================= SETUP ================= */
void setup(){
  Serial.begin(BAUDRATE);
  pinMode(L1,OUTPUT); pinMode(L2,OUTPUT); pinMode(L3,OUTPUT); pinMode(L4,OUTPUT);
  pinMode(R1,OUTPUT); pinMode(R2,OUTPUT); pinMode(R3,OUTPUT); pinMode(R4,OUTPUT);
  lastCmdTime=millis();
}

/* ================= LOOP ================= */
void loop(){
  handleBinaryRX();
  updateMotorsRT();
  commWatchdog();
  static unsigned long lastTele=0;
  if(millis()-lastTele>100){ sendTelemetry(); lastTele=millis(); }
}
