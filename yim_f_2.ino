#include <NewPing.h>
#include <Adafruit_PWMServoDriver.h>
#include <DHT.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SoftwareSerial.h>

// ======== Configuration ========
// PWM Servo Driver
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVO_MIN 150
#define SERVO_MAX 600

// OLED Display
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// DHT11 Configuration
#define DHTPIN 12
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BUZZER_PIN 8


// Motor Control
const int in1 = 13, in2 = 9, in3 = 10, in4 = 11;
const int ena = 5, enb = 6;
int normalSpeed = 190;
int normalSpeedM = 160;
const int turnSpeed = 240;
const int turnSpeedM = 255;

// Ultrasonic Sensors
NewPing sonarScan(2, 3, 200);
NewPing sonarLeft(A2, A3, 200);
NewPing sonarRight(A0, A1, 200);

// Hall Sensors
const int hallSensorLeft = 7;
const int hallSensorRight = 4;

// Servo Channels
#define BASE_SERVO 1
#define SHOULDER_SERVO 0
#define ELBOW_SERVO 2
#define WRIST_SERVO 3
#define WRIST_ROT_SERVO 4
#define GRIP_SERVO 5

// Servo Positions
int base_angle = 145;
int shoulder_angle = 90;
int elbow_angle = 178;
int wrist_angle = 90;
int wrist_rot_angle = 0;
int grip_angle = 20;

// System Variables
enum Mode { IDLE, HUMAN_FOLLOW, TAPE_GUIDE, DANCE, THREE_SIXTY, WINK, DISPLAY_TEMP_HUM };
Mode currentMode = IDLE;
const int followMin = 20;
const int followMax = 40;
unsigned long actionTimeout = 0;

// ======== Function Prototypes ========
void moveServo(uint8_t servo, int angle);
void handleVoiceCommand(uint16_t command);
void triggerBuzzer(int times, int duration);

// ======== Setup & Main Loop ========
void setup() {
  Serial.begin(9600);
  pwm.begin();
  pwm.setPWMFreq(60);
  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  displayDefaultFace();
  
  dht.begin();
  pinMode(hallSensorLeft, INPUT);
  pinMode(hallSensorRight, INPUT);
  
  pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
  pinMode(ena, OUTPUT); pinMode(enb, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);

}

void loop() {
  // Voice command handling
  if (Serial.available() >= 2) {
    byte highByte = Serial.read();
    byte lowByte = Serial.read();
    uint16_t receivedValue = (highByte << 8) | lowByte;
    Serial.print("Received: 0x"); Serial.println(receivedValue, HEX);
    handleVoiceCommand(receivedValue);
  }

    // Active mode handling
  switch(currentMode) {
    case HUMAN_FOLLOW: humanFollow(); break;
    case TAPE_GUIDE: tapeGuide(); break;
    case WINK: wink(); break;
    case DISPLAY_TEMP_HUM: displayTempHum(); break;
    default: displayDefaultFace(); break;
  }


  // Timed action handling
  if (millis() > actionTimeout && actionTimeout != 0) {
    stopMoving();
    actionTimeout = 0;
  }

}

// ======== Motor Control Functions ========
void moveForward() {
  analogWrite(ena, normalSpeed);
  analogWrite(enb, normalSpeed);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void moveForwardF() {
  moveForward();
  delay(1000);
  stopMoving();
}

void moveBackwardF() {
  moveBackward();
  delay(1000);
  stopMoving();
}

void turnLeftF() {
  turnLeft();
  delay(800);
  stopMoving();
}

void turnRightF() {
  turnRight();
  delay(800);
  stopMoving();
}

void moveForwardM() {
  analogWrite(ena, normalSpeedM);
  analogWrite(enb, normalSpeedM);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void moveBackward() {
  analogWrite(ena, normalSpeed);
  analogWrite(enb, normalSpeed);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void turnLeft() {
  analogWrite(ena, turnSpeed);
  analogWrite(enb, turnSpeed);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void turnLeftM() {
  analogWrite(ena, turnSpeedM);
  analogWrite(enb, turnSpeedM);
  digitalWrite(in1, LOW);
  digitalWrite(in2, HIGH);
  digitalWrite(in3, HIGH);
  digitalWrite(in4, LOW);
}

void turnRight() {
  analogWrite(ena, turnSpeed);
  analogWrite(enb, turnSpeed);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void turnRightM() {
  analogWrite(ena, turnSpeedM);
  analogWrite(enb, turnSpeedM);
  digitalWrite(in1, HIGH);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, HIGH);
}

void stopMoving() {
  digitalWrite(in1, LOW);
  digitalWrite(in2, LOW);
  digitalWrite(in3, LOW);
  digitalWrite(in4, LOW);
}

// ======== Complex Movement Functions ========
void targetRight() {
  turnRight();
  delay(900); // Adjust based on 90° turn time
  stopMoving();
  moveForward();
  delay(2000);
  stopMoving();
  triggerBuzzer(2, 1000);
}

void targetLeft() {
  turnLeft();
  delay(900);
  stopMoving();
  moveForward();
  delay(2000);
  stopMoving();
  triggerBuzzer(2, 1000);
}

void reverseTargetRight() {
  turnRight();
  delay(3600); // Adjust for 360° turn
  while(true) {
    moveForward();
    if(digitalRead(hallSensorLeft) && digitalRead(hallSensorRight)) {
      stopMoving();
      turnLeft();
      delay(900);
      break;
    }
  }
}

// ======== Sensor-Based Functions ========
void humanFollow() {
  int scanDist = sonarScan.ping_cm();
  int leftDist = sonarLeft.ping_cm();
  int rightDist = sonarRight.ping_cm();

  if (rightDist > 15 && leftDist < 30) turnLeft();
  else if (leftDist > 15 && rightDist < 30) turnRight();
  else if (scanDist > followMax && scanDist < 80) moveForward();
  else if (scanDist < followMin && scanDist > 10) moveBackward();
  else if (scanDist >= followMin && scanDist <= followMax) stopMoving();
}

void tapeGuide() {
  int hallLeft = digitalRead(hallSensorLeft);
  int hallRight = digitalRead(hallSensorRight);


  // Display hall sensor values in serial monitor
  Serial.print("Hall Sensors - Left: ");
  Serial.print(hallLeft);
  Serial.print(" | Right: ");
  Serial.println(hallRight);



  if (!hallLeft && !hallRight) {
    moveForwardM();
  }
  else if (!hallRight && hallLeft) {
    Serial.println("Adjusting left");
    turnLeftM();
    delay(200);
    stopMoving();
    delay(300);  // Short adjustment period
    
  }
  else if (hallRight && !hallLeft) {
    Serial.println("Adjusting right");
    turnRightM();
    delay(200);  // Short adjustment period
   stopMoving();
   delay(300);  
  }
  else if (hallLeft && hallRight) {
    Serial.println("On track - stopping");
    stopMoving();
  }
}

// ======== Servo Control Functions ========
void moveServo(uint8_t servo, int angle) {
  int pulse = map(angle, 0, 180, SERVO_MIN, SERVO_MAX);
  pwm.setPWM(servo, 0, pulse);
}

// ======== Voice Command Handler ========
void handleVoiceCommand(uint16_t command) {
  switch(command) {
    case 0xAA00: // Stop command
      currentMode = IDLE;
      stopMoving();
      break;
      
    case 0xAA01: currentMode = HUMAN_FOLLOW; break;
    case 0xAA48: currentMode = TAPE_GUIDE; break;
    case 0xAA02: danceMode(); break;
    case 0xAA03: turnThreesixty(); break;
    case 0xAA04: currentMode = WINK; break;
    case 0xAA22: currentMode = DISPLAY_TEMP_HUM; break;     
      
    // Add remaining command cases following the same pattern
    // ...

    case 0xAA20:
      openGrip();
      break;

      case 0xAA21:
        closeGrip();
        break;

      case 0xAA18:
        unfold();
        break;    

      case 0xAA19:
        fold();
        break;    

      case 0xAA31:
        moveForward();
        break;    

      case 0xAA32:
        moveBackward();
        break;    

      case 0xAA33:
        turnRight();
        break;    

      case 0xAA34:
        turnLeft();
        break;    

      case 0xAA35:
        stopMoving();
        break;    

      case 0xAA36:
        moveForwardF();
        break;    

      case 0xAA37:
        moveBackwardF();
        break;    

      case 0xAA38:
        turnRightF();
        break;    

      case 0xAA39:
        turnLeftF();
        delay(1000);
        break;    

      case 0xAA42:
        elbowUp();
        break;    

      case 0xAA43:
        elbowDown();
        break;    

      case 0xAA44:
        shoulderFront();
        break;    

      case 0xAA45:
        shoulderBack();
        break;     

      case 0xAA46:
        wrestRight();
        break;  

      case 0xAA47:
        wrestLeft();
        break;  

      case 0xAA49:
        wrestRotationRight();
        break;  

      case 0xAA50:
        wrestRotationLeft();
        break;  

      case 0xAA51:
        baseRotationRight();
        break;  

      case 0xAA52:
        baseRotationLeft();
        break;  

      case 0xAA40:
        handShake();
        break;     
    default: break;
  }
}

// ======== Utility Functions ========
void triggerBuzzer(int times, int duration) {
  for(int i=0; i<times; i++) {
    digitalWrite(BUZZER_PIN, LOW);
    delay(duration);
    digitalWrite(BUZZER_PIN, HIGH);
    if(i != times-1) delay(500);
  }
}

// ======== Remaining Functions ========
// Implement all other functions (handShake, servoDance, etc) 
void resetServoPositions() {
  moveServo(SHOULDER_SERVO, 145);
  moveServo(BASE_SERVO, 90);
  moveServo(ELBOW_SERVO, 178);
  moveServo(WRIST_SERVO, 90);
  moveServo(WRIST_ROT_SERVO, 0);
}

void unfold() {
  moveServo(BASE_SERVO, 90);
  moveServo(ELBOW_SERVO, 90);
  moveServo(SHOULDER_SERVO, 90);
  moveServo(WRIST_ROT_SERVO, 90);
  moveServo(WRIST_SERVO, 50);
}

void fold() {
  moveServo(BASE_SERVO, 90);
  moveServo(ELBOW_SERVO, 178);
  moveServo(SHOULDER_SERVO, 145);
  moveServo(WRIST_ROT_SERVO, 0);
  moveServo(WRIST_SERVO, 90);
}

void handShake() {
  moveServo(BASE_SERVO, 90);
  moveServo(ELBOW_SERVO, 90);
  moveServo(SHOULDER_SERVO, 90);
  moveServo(WRIST_ROT_SERVO, 00);
  moveServo(WRIST_SERVO, 90);
  delay(1000);
  moveServo(ELBOW_SERVO, 120);
    delay(1000);
  moveServo(ELBOW_SERVO, 90);
    delay(1000);
  moveServo(ELBOW_SERVO, 120);
    delay(1000);
  moveServo(ELBOW_SERVO, 90);
}
void openGrip() { moveServo(GRIP_SERVO, 180); }
void closeGrip() { moveServo(GRIP_SERVO, 100); }

void elbowUp() { 
   elbow_angle = constrain(elbow_angle - 10, 0, 180); 
  moveServo(ELBOW_SERVO, elbow_angle);
}

void elbowDown() { 
    elbow_angle = constrain(elbow_angle + 10, 0, 180); 
  moveServo(ELBOW_SERVO, elbow_angle); 
}

void shoulderFront() { 
  shoulder_angle = constrain(shoulder_angle - 10, 0, 180); 
  moveServo(SHOULDER_SERVO, shoulder_angle);
}

void shoulderBack() {  
    shoulder_angle = constrain(shoulder_angle + 10, 0, 180); 
  moveServo(SHOULDER_SERVO, shoulder_angle);
}

void wrestRight() { 
  wrist_angle = constrain(wrist_angle + 10, 0, 180); 
  moveServo(WRIST_SERVO, wrist_angle); 
}

void wrestLeft() { 
  wrist_angle = constrain(wrist_angle - 10, 0, 180); 
  moveServo(WRIST_SERVO, wrist_angle); 
}

void wrestRotationRight() { 
  wrist_rot_angle = constrain(wrist_rot_angle + 10, 0, 180); 
  moveServo(WRIST_ROT_SERVO, wrist_rot_angle); 
}

void wrestRotationLeft() { 
  wrist_rot_angle = constrain(wrist_rot_angle - 10, 0, 180); 
  moveServo(WRIST_ROT_SERVO, wrist_rot_angle); 
}

void baseRotationRight() { 
  base_angle = constrain(base_angle + 10, 0, 180); 
  moveServo(BASE_SERVO, base_angle); 
}

void baseRotationLeft() { 
  base_angle = constrain(base_angle - 10, 0, 180); 
  moveServo(BASE_SERVO, base_angle); 
}
void wink() {
  static unsigned long previousMillis = 0;
  const long interval = 3000;
  static bool winkState = false;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    winkState = !winkState;

    display.clearDisplay();
    
    // Draw face outline
    display.drawCircle(64, 32, 30, WHITE);
    
    // Draw eyes
    if (winkState) {
      display.fillCircle(52, 24, 3, WHITE);  // Left eye open
      display.drawLine(74, 24, 78, 24, WHITE); // Right eye wink
    } else {
      display.fillCircle(52, 24, 3, WHITE);   // Both eyes open
      display.fillCircle(76, 24, 3, WHITE);
    }
    
    // Draw smile arc
    for (int angle = 20; angle <= 160; angle++) {
      float rad = angle * 3.14159265 / 180.0;
      int x = 64 + 10 * cos(rad);
      int y = 40 + 10 * sin(rad);
      display.drawPixel(x, y, WHITE);
    }
    
    display.display();
  }
}


void displayDefaultFace() {
    static unsigned long previousMillis = 0;
    static int eyeOffset = 0;
    static int eyeDirection = 1;
    const long interval = 100; // Animation speed (ms)

    unsigned long currentMillis = millis();

    // Update animation timing
    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        
        // Update eye position
        eyeOffset += eyeDirection;
        if (eyeOffset >= 3 || eyeOffset <= -3) {
            eyeDirection *= -1;
        }
    }

    // Clear display
    display.clearDisplay();

    // Draw static face elements
    // Head outline
    display.drawCircle(SCREEN_WIDTH/2, SCREEN_HEIGHT/2, 30, SSD1306_WHITE);
    
    // Antennas
    display.drawLine(SCREEN_WIDTH/2-15, 15, SCREEN_WIDTH/2-25, 5, SSD1306_WHITE);
    display.drawLine(SCREEN_WIDTH/2+15, 15, SCREEN_WIDTH/2+25, 5, SSD1306_WHITE);
    
    // Animated eyes (moving pupils)
    display.fillCircle(SCREEN_WIDTH/2 - 15 + eyeOffset, SCREEN_HEIGHT/2 - 10, 5, SSD1306_WHITE);
    display.fillCircle(SCREEN_WIDTH/2 + 15 + eyeOffset, SCREEN_HEIGHT/2 - 10, 5, SSD1306_WHITE);
    
    // Smiling mouth (arc with curved bottom)
    for(int y = 0; y < 5; y++) {
        display.drawFastHLine(
            SCREEN_WIDTH/2 - 20 + y*4,  // X-start
            SCREEN_HEIGHT/2 + 20 + y,    // Y-position
            40 - y*8,                   // Width
            SSD1306_WHITE
        );
    }

    // Add "YIM" text above the smile
    display.setTextSize(1);              // Set text size
    display.setTextColor(SSD1306_WHITE); // Set text color
    display.setCursor(SCREEN_WIDTH/2 - 15, SCREEN_HEIGHT/2 + 10); // Position text
    display.print("YIM");               // Display text

    // Display all elements
    display.display();
}

void displayTempHum() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0,0);
  display.print("Temp: "); display.print(t); display.println("C");
  display.print("Hum:  "); display.print(h); display.println("%");
  display.display();
  
}



// ======== Additional Functions ========


void turnThreesixty() {
  turnRight();
  delay(4000); 
  stopMoving();
}

void autGrip() {
  float distance = sonarScan.ping_cm();
  if (distance > 0 && distance < 10) {
    openGrip();
    delay(200);
    closeGrip();
  }
}



void servoDance() {
      moveServo(ELBOW_SERVO, 60);      // E -> ELBOW_SERVO
    delay(200);
   moveServo(SHOULDER_SERVO, 90);  // S -> SHOULDER_SERVO
    delay(200);
    moveServo(BASE_SERVO, 120);      // A -> BASE_SERVO
    delay(200);
    moveServo(WRIST_SERVO, 150);     // W -> WRIST_SERVO
    delay(200);
    moveServo(GRIP_SERVO, 120);      // C -> GRIP_SERVO
    delay(200);
}

void danceMode() {
  
  turnRight(); delay(600); stopMoving();
  servoDance(); resetServoPositions();

  turnLeft(); delay(600); stopMoving();
  turnLeft(); delay(600); stopMoving();
  servoDance(); resetServoPositions();

  turnRight(); delay(600); stopMoving();
  turnLeft(); delay(2000); stopMoving();
  servoDance(); resetServoPositions();

  turnRight(); delay(2000); stopMoving();
}
// following the same pattern with non-blocking where needed