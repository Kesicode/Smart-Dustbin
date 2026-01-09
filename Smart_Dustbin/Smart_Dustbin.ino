#include <Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pin definitions - Proximity Sensor (outside)
const int trigPin = 9;
const int echoPin = 10;

// Pin definitions - Level Sensor (inside)
const int levelTrigPin = 11;
const int levelEchoPin = 12;

// Other pins
const int servoPin = 6;
const int buzzerPin = 8;
const int ledPin = 13;

// Servo object
Servo lidServo;

// Proximity detection config
const int detectionDistance = 30; // cm
const int lidOpenAngle = 90;
const int lidCloseAngle = 0;
const int lidOpenTime = 5000; // ms

// Waste level detection config
const int binHeight = 40;       // cm
const int fullThreshold = 10;   // cm from top
const int warningThreshold = 15;

// Variables
int distance = 0;
int wasteLevel = 0;
int fillPercentage = 0;
bool lidOpen = false;
bool binFull = false;
unsigned long lidOpenedAt = 0;
unsigned long lastFullAlert = 0;
unsigned long lastLevelRead = 0;
unsigned long lastSerial = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("=== Smart Dustbin System ==="));

  // Initialize pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(levelTrigPin, OUTPUT);
  pinMode(levelEchoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Initialize servo
  lidServo.attach(servoPin);
  lidServo.write(lidCloseAngle);
  delay(500);

  // Initialize OLED
  Serial.println(F("Initializing OLED..."));
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed!"));
    while (1) {
      digitalWrite(ledPin, !digitalRead(ledPin));
      delay(200);
    }
  }
  Serial.println(F("OLED initialized successfully!"));

  // Startup screen
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println(F("SMART"));
  display.setCursor(10, 35);
  display.println(F("DUSTBIN"));
  display.display();
  delay(2000);

  // Startup beep
  tone(buzzerPin, 1000, 100);
  delay(150);
  tone(buzzerPin, 1500, 100);

  Serial.println(F("System Ready!"));
  Serial.print(F("Bin Height: ")); Serial.print(binHeight); Serial.println(F(" cm"));
  Serial.print(F("Detection Distance: ")); Serial.print(detectionDistance); Serial.println(F(" cm"));
}

void loop() {
  // Measure proximity
  distance = measureDistance(trigPin, echoPin);

  // Measure waste level (every 500ms)
  if (millis() - lastLevelRead >= 500) {
    wasteLevel = measureDistance(levelTrigPin, levelEchoPin);
    calculateFillLevel();
    lastLevelRead = millis();
  }

  // Update OLED
  updateDisplay();

  // Print status every 500ms
  if (millis() - lastSerial >= 500) {
    lastSerial = millis();
    Serial.print(F("Prox: ")); Serial.print(distance);
    Serial.print(F(" cm | Level: ")); Serial.print(wasteLevel);
    Serial.print(F(" cm | Fill: ")); Serial.print(fillPercentage);
    Serial.print(F("% | "));
    if (binFull) Serial.println(F("BIN FULL"));
    else if (lidOpen) Serial.println(F("LID OPEN"));
    else Serial.println(F("READY"));
  }

  // Check if bin is full
  checkBinFull();

  // Check for person approach
  if (distance > 0 && distance <= detectionDistance && !binFull) {
    if (!lidOpen) openLid();
    else lidOpenedAt = millis(); // Reset open timer
  } else {
    if (lidOpen && (millis() - lidOpenedAt >= lidOpenTime)) closeLid();
  }

  delay(100);
}

// Measure distance for ultrasonic sensor
int measureDistance(int trig, int echo) {
  long duration;
  int dist;

  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  duration = pulseIn(echo, HIGH, 30000); // 30ms timeout
  if (duration == 0) return 0;

  dist = (int)(duration * 0.034 / 2);
  if (dist > 400) return 0;

  return dist;
}

// Calculate fill percentage
void calculateFillLevel() {
  if (wasteLevel > 0 && wasteLevel <= binHeight) {
    int filledHeight = binHeight - wasteLevel;
    fillPercentage = (filledHeight * 100) / binHeight;
    fillPercentage = constrain(fillPercentage, 0, 100);
  } else fillPercentage = 0;
}

// Check if bin is full
void checkBinFull() {
  if (wasteLevel > 0 && wasteLevel <= fullThreshold) {
    if (!binFull) alertBinFull();
    binFull = true;

    // Repeat alert every 10s
    if (millis() - lastFullAlert >= 10000) alertBinFull();
  } else binFull = false;
}

// Update OLED display
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(15, 0);
  display.println(F("SMART DUSTBIN"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  // Distance
  display.setCursor(0, 14);
  display.print(F("Prox: "));
  display.print(distance > 0 ? distance : 0);
  display.println(F(" cm"));

  // Fill bar
  display.setCursor(0, 24);
  display.print(F("Fill: "));
  display.print(fillPercentage);
  display.println(F("%"));

  int barWidth = 80, barHeight = 8, barX = 24, barY = 34;
  display.drawRect(barX, barY, barWidth, barHeight, SSD1306_WHITE);
  int fillWidth = (barWidth - 2) * fillPercentage / 100;
  if (fillWidth > 0) display.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, SSD1306_WHITE);

  // Lid / bin status
  display.setCursor(0, 46);
  display.setTextSize(1);
  if (binFull) {
    display.setTextSize(2);
    display.println(F("BIN FULL!"));
  } else if (lidOpen) {
    display.print(F("Lid: OPEN"));
    if (fillPercentage >= 70) {
      display.setTextSize(1);
      display.setCursor(85, 46);
      display.print(F("[!]"));
    }
  } else display.print(F("Lid: CLOSED"));

  // Bottom status
  display.setTextSize(1);
  display.setCursor(0, 56);
  if (binFull) display.print(F("! EMPTY BIN !"));
  else if (distance > 0 && distance <= detectionDistance) display.print(F("> PERSON NEAR <"));
  else if (fillPercentage >= 70) display.print(F("Nearly Full"));
  else display.print(F("Ready"));

  display.display();
}

// Alert bin full
void alertBinFull() {
  Serial.println(F("\n!!! BIN FULL - PLEASE EMPTY !!!\n"));
  lastFullAlert = millis();

  for (int i = 0; i < 3; i++) {
    tone(buzzerPin, 1500, 200);
    digitalWrite(ledPin, HIGH);
    delay(200);
    digitalWrite(ledPin, LOW);
    delay(100);
  }
}

// Open lid
void openLid() {
  Serial.println(F(">>> OPENING LID <<<"));
  digitalWrite(ledPin, HIGH);

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println(F("OPENING"));
  display.setCursor(25, 40);
  display.println(F("LID..."));
  display.display();

  tone(buzzerPin, 1000, 200);
  delay(250);

  for (int angle = lidCloseAngle; angle <= lidOpenAngle; angle++) {
    lidServo.write(angle);
    delay(15);
  }

  lidOpen = true;
  lidOpenedAt = millis();
  Serial.println(F("Lid fully opened!"));
}

// Close lid
void closeLid() {
  Serial.println(F(">>> CLOSING LID <<<"));

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(10, 20);
  display.println(F("CLOSING"));
  display.setCursor(25, 40);
  display.println(F("LID..."));
  display.display();

  tone(buzzerPin, 800, 100);
  delay(150);
  tone(buzzerPin, 800, 100);
  delay(250);

  for (int angle = lidOpenAngle; angle >= lidCloseAngle; angle--) {
    lidServo.write(angle);
    delay(15);
  }

  digitalWrite(ledPin, LOW);
  lidOpen = false;
  Serial.println(F("Lid closed! Waiting for approach..."));
}
