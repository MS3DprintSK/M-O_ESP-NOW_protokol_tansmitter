#include <ESP8266WiFi.h>
#include <espnow.h>
#include <Servo.h>
#include "DFRobotDFPlayerMini.h"

// --- ALLOWED MAC ADDRESS CONFIGURATION ---
uint8_t allowedMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// --- PIN CONFIGURATION ---
#define PIN_HEAD_SERVO      D8
#define PIN_STEERING_SERVO  D7
#define PIN_MOTOR_PWM       D6
#define PIN_MOTOR_IN1       D5
#define PIN_MOTOR_IN2       D0
#define PIN_POTENTIOMETER   A0

// --- CONSTANTS AND SETTINGS ---
const int HEAD_OFFSET = -5;      
const int STEERING_OFFSET = 20;   
const int HEAD_DEADZONE = 15;   
const int MOTOR_DEADZONE = 8;    
const int MAX_SPEED = 800;    
const int QUARTER_SPEED = 200; 
const int STEERING_ANGLE = 30;    
int lastVolumeVal = -1;

Servo headServo;
Servo steeringServo;
DFRobotDFPlayerMini myDFPlayer;

typedef struct struct_message {
    int id;
    int joy[4]; 
} struct_message;

struct_message incomingData;
int lastHeadPos = 90 + HEAD_OFFSET;
int lastReceivedID = -1;
int previousID = -1;

bool isShakingActive = false;
bool waitForLoop = false;
bool isSequence7Active = false;
bool isSequence3Active = false;
bool isSequence2Active = false;

unsigned long lastShakeTime = 0;
unsigned long lastLoopTime = 0;
unsigned long sequenceChangeTime = 0;
unsigned long sequence3Time = 0;
unsigned long sequence2Time = 0;

int shakeCount = 0;
int shakeDirection = 1; 
int sequence7Step = 0;
const int sequence7Tracks[] = {9, 11, 13, 16};

// --- FUNCTIONS ---

void updateVolume() {
    int rawA0 = analogRead(PIN_POTENTIOMETER);
    int newVolume = map(rawA0, 0, 1023, 0, 30);
    if (abs(newVolume - lastVolumeVal) > 1) {
        myDFPlayer.volume(newVolume);
        lastVolumeVal = newVolume;
    }
}

void processMovement() {
    int rawHead = incomingData.joy[0];      // Head joystick
    int rawSteering = incomingData.joy[2];   // Steering joystick
    
    // 1. Steering Logic (Inverted mapping, 30-degree limit)
    bool isTurning = abs(rawSteering - 90) > 5;
    int finalSteering = (!isTurning) ? (90 + STEERING_OFFSET) : (map(rawSteering, 0, 180, 90 + STEERING_ANGLE, 90 - STEERING_ANGLE) + STEERING_OFFSET);
    steeringServo.write(constrain(finalSteering, 0, 180));

    // 2. Head Movement Logic (Combined manual control + automatic look-into-turn)
    if (!isShakingActive) {
        // Calculate deflection from center for both joysticks
        int headDiff = (abs(rawHead - 90) < HEAD_DEADZONE) ? 0 : (rawHead - 90);
        
        // Automatic head movement follows steering (inverted here too to match your inverted steering)
        int steeringDiff = (abs(rawSteering - 90) < 5) ? 0 : (90 - rawSteering); 
        
        // Combine both inputs + offset
        int finalHead = 90 + HEAD_OFFSET + headDiff + steeringDiff;
        
        if (abs(finalHead - lastHeadPos) > 1) {
            headServo.write(constrain(finalHead, 0, 180));
            lastHeadPos = finalHead;
        }
    }

    // 3. Throttle logic with speed reduction to QUARTER_SPEED during turns
    if (!isSequence3Active && !isSequence2Active) {
        int motorValue = incomingData.joy[3];
        int currentMaxLimit = isTurning ? QUARTER_SPEED : MAX_SPEED;

        if (motorValue > (90 + MOTOR_DEADZONE)) {
            digitalWrite(PIN_MOTOR_IN1, HIGH); digitalWrite(PIN_MOTOR_IN2, LOW);
            analogWrite(PIN_MOTOR_PWM, map(motorValue, 90 + MOTOR_DEADZONE, 180, 0, currentMaxLimit));
        } 
        else if (motorValue < (90 - MOTOR_DEADZONE)) {
            digitalWrite(PIN_MOTOR_IN1, LOW); digitalWrite(PIN_MOTOR_IN2, HIGH);
            analogWrite(PIN_MOTOR_PWM, map(motorValue, 90 - MOTOR_DEADZONE, 0, 0, currentMaxLimit));
        } 
        else {
            digitalWrite(PIN_MOTOR_IN1, LOW); digitalWrite(PIN_MOTOR_IN2, LOW);
            analogWrite(PIN_MOTOR_PWM, 0);
        }
    }
}

void updateEffects() {
    unsigned long currentTime = millis();
    if (isShakingActive) {
        if (currentTime - lastShakeTime >= 150) { 
            lastShakeTime = currentTime;
            int center = 90 + HEAD_OFFSET;
            if (shakeDirection == 1) { headServo.write(center + 15); shakeDirection = -1; }
            else { headServo.write(center - 15); shakeDirection = 1; shakeCount++; }
            if (shakeCount >= 10) { isShakingActive = false; headServo.write(center); waitForLoop = true; lastLoopTime = currentTime; }
        }
    }
    
    if (waitForLoop && (incomingData.id == 5 || incomingData.id == 6)) {
        if (currentTime - lastLoopTime >= 4000) { myDFPlayer.playMp3Folder(7); lastLoopTime = currentTime; }
    }
    
    if (isSequence7Active && incomingData.id == 7) {
        if (currentTime - sequenceChangeTime >= 3000) {
            sequence7Step++;
            if (sequence7Step < 4) { myDFPlayer.playMp3Folder(sequence7Tracks[sequence7Step]); sequenceChangeTime = currentTime; }
            else { isSequence7Active = false; }
        }
    }
    
    if (isSequence3Active && incomingData.id == 3) {
        unsigned long elapsed = currentTime - sequence3Time;
        if (elapsed < 1000) { digitalWrite(PIN_MOTOR_IN1, HIGH); digitalWrite(PIN_MOTOR_IN2, LOW); analogWrite(PIN_MOTOR_PWM, QUARTER_SPEED); }
        else if (elapsed < 2000) { digitalWrite(PIN_MOTOR_IN1, LOW); digitalWrite(PIN_MOTOR_IN2, HIGH); analogWrite(PIN_MOTOR_PWM, QUARTER_SPEED); }
        else { digitalWrite(PIN_MOTOR_IN1, LOW); digitalWrite(PIN_MOTOR_IN2, LOW); analogWrite(PIN_MOTOR_PWM, 0); isSequence3Active = false; }
    }
    
    if (isSequence2Active && incomingData.id == 2) {
        if (currentTime - sequence2Time < 500) {
            digitalWrite(PIN_MOTOR_IN1, LOW); digitalWrite(PIN_MOTOR_IN2, HIGH);
            analogWrite(PIN_MOTOR_PWM, QUARTER_SPEED);
        } else {
            digitalWrite(PIN_MOTOR_IN1, LOW); digitalWrite(PIN_MOTOR_IN2, LOW);
            analogWrite(PIN_MOTOR_PWM, 0);
            isSequence2Active = false;
        }
    }
}

void OnDataRecv(uint8_t * mac, uint8_t *data, uint8_t len) {
    for (int i = 0; i < 6; i++) {
        if (mac[i] != allowedMacAddress[i]) return; 
    }

    memcpy(&incomingData, data, sizeof(incomingData));

    if (incomingData.id != lastReceivedID) {
        previousID = lastReceivedID;
        myDFPlayer.stop();
        
        isShakingActive = false; waitForLoop = false; 
        isSequence7Active = false; isSequence3Active = false; isSequence2Active = false;

        if (incomingData.id == 1) {
            if (previousID == 5 || previousID == 6) {
                myDFPlayer.playMp3Folder(21);
            } else {
                myDFPlayer.playMp3Folder(1); 
            }
        }
        else if (incomingData.id == 2) {
            myDFPlayer.playMp3Folder(8); isSequence2Active = true; sequence2Time = millis();
        }
        else if (incomingData.id == 3) {
            myDFPlayer.playMp3Folder(57); isSequence3Active = true; sequence3Time = millis();
        }
        else if (incomingData.id == 4) {
            myDFPlayer.playMp3Folder(27);
        }
        else if (incomingData.id == 5) {
            myDFPlayer.playMp3Folder(4); isShakingActive = true; shakeCount = 0; shakeDirection = 1; lastShakeTime = millis();
        }
        else if (incomingData.id == 6) {
            myDFPlayer.playMp3Folder(10); waitForLoop = true; lastLoopTime = millis();
        }
        else if (incomingData.id == 7) {
            sequence7Step = 0; isSequence7Active = true; myDFPlayer.playMp3Folder(sequence7Tracks[0]); sequenceChangeTime = millis();
        }

        lastReceivedID = incomingData.id;
    }
}

void setup() {
    Serial.begin(9600); 
    headServo.write(90 + HEAD_OFFSET);
    steeringServo.write(90 + STEERING_OFFSET);
    headServo.attach(PIN_HEAD_SERVO);
    steeringServo.attach(PIN_STEERING_SERVO);
    
    pinMode(PIN_MOTOR_IN1, OUTPUT); pinMode(PIN_MOTOR_IN2, OUTPUT); pinMode(PIN_MOTOR_PWM, OUTPUT);

    if (myDFPlayer.begin(Serial)) {
        int initVol = map(analogRead(PIN_POTENTIOMETER), 0, 1023, 0, 30);
        myDFPlayer.volume(initVol);
        lastVolumeVal = initVol;
    }

    WiFi.mode(WIFI_STA); WiFi.disconnect();
    if (esp_now_init() == 0) {
        esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
        esp_now_register_recv_cb(OnDataRecv);
    }

    delay(2000);
    myDFPlayer.playMp3Folder(1); 
    incomingData.id = 0; 
}

void loop() {
    updateVolume(); 
    processMovement();         
    updateEffects();
    yield();
}