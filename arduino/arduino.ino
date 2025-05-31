//# Arduino sketch: monitor with customizable welcome
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// —— LCD & Servo ——
LiquidCrystal_I2C lcd(0x27, 16, 2);
Servo doorServo;

// —— Ultrasonic sensor pins ——
const int trigPin = 9;
const int echoPin = 10;

// —— Touch reset and LEDs ——
const int touchPin   = 8;
const int ledPin     = 13; // motion LED
const int redLedPin  = 12; // “room full” LED

// —— Servo pin ——
const int servoPin = 6;

// —— Serial parsing ——
String    serialBuffer    = "";
bool      commandComplete = false;

// —— System state ——
int       personCount     = 0;
int       maxCount        = 5;      // capacity set by app
bool      isLocked        = false;  // door locked state
String    welcomeMsg      = "Welcome";  // default welcome

void setup() {
  // Serial for app connectivity
  Serial.begin(9600);

  // LCD
  lcd.init();
  lcd.backlight();

  // Pins
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledPin, OUTPUT);
  pinMode(redLedPin, OUTPUT);
  pinMode(touchPin, INPUT);

  // Servo
  doorServo.attach(servoPin);
  doorServo.write(0);  // unlocked

  // Initial display
  lcd.setCursor(0, 0);
  lcd.print("Person Count:");
  lcd.setCursor(0, 1);
  lcd.print(welcomeMsg);
}

void loop() {
  // ——— SERIAL READ ———
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      commandComplete = true;
    } else if (c != '\r') {
      serialBuffer += c;
    }
  }
  if (commandComplete) {
    commandComplete = false;
    String cmd = serialBuffer;
    serialBuffer = "";

    if (cmd.startsWith("MAX:")) {
      int newMax = cmd.substring(4).toInt();
      if (newMax > 0) {
        bool wasLocked = isLocked;
        maxCount = newMax;
        Serial.println("ACK_MAX:" + String(maxCount));
        // update display line 2
        lcd.setCursor(0, 1);
        lcd.print("Max: "); lcd.print(maxCount); lcd.print("        ");
        // unlock if capacity now above current count
        if (wasLocked && personCount < maxCount) {
          isLocked = false;
          doorServo.write(0);
        }
      }
    }
    else if (cmd.startsWith("WELCOME:")) {
      // customize welcome message
      welcomeMsg = cmd.substring(8);
      // truncate/pad to 16 chars
      if (welcomeMsg.length() > 16) welcomeMsg = welcomeMsg.substring(0,16);
      lcd.setCursor(0,1);
      lcd.print(welcomeMsg);
      for (int i=welcomeMsg.length(); i<16; i++) lcd.print(' ');
      Serial.println("ACK_WELCOME");
    }
    else if (cmd == "RESET") {
      // reset routine
      personCount = 0;
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Room Reset!");
      digitalWrite(redLedPin, LOW);
      isLocked = false;
      doorServo.write(0);
      delay(1000);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Person Count: ");
      lcd.setCursor(0, 1);
      lcd.print(welcomeMsg);
    }
  }

  // —— 1) TOUCH-TO-RESET ——
  if (digitalRead(touchPin) == HIGH) {
    personCount = 0;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Room Reset!");
    digitalWrite(redLedPin, LOW);
    isLocked = false;
    doorServo.write(0);
    delay(1000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Person Count: ");
    lcd.setCursor(0,1);
    lcd.print(welcomeMsg);
    return;
  }

  // —— 2) MEASURE DISTANCE ——
  int distance = getDistance();

  // —— 3) PERSON ENTERS (under max) ——
  if (distance > 0 && distance < 50) {
    if (personCount < maxCount) {
      personCount++;
      lcd.setCursor(0, 0);
      lcd.print("Person Count: "); lcd.setCursor(14, 0); lcd.print(personCount);
      lcd.setCursor(0, 1);
      lcd.print("Motion: Detected   ");
      digitalWrite(ledPin, HIGH);
      digitalWrite(redLedPin, LOW);
      Serial.println("Person Count:" + String(personCount));
    }
    // —— ROOM FULL ——
    else if (!isLocked) {
      lcd.setCursor(0, 1);
      lcd.print("Room Full     ");
      digitalWrite(ledPin, LOW);
      doorServo.write(90);
      isLocked = true;
      for (int i = 0; i < 6; i++) {
        if (digitalRead(touchPin) == HIGH) {
          personCount = 0;
          lcd.clear(); lcd.setCursor(0, 0); lcd.print("Room Reset!");
          digitalWrite(redLedPin, LOW); isLocked = false; doorServo.write(0);
          delay(1000);
          lcd.clear(); lcd.setCursor(0,0); lcd.print("Person Count: ");
          lcd.setCursor(0,1); lcd.print(welcomeMsg);
          break;
        }
        digitalWrite(redLedPin, HIGH); delay(500);
        digitalWrite(redLedPin, LOW);  delay(500);
      }
      Serial.println("Room Full");
    }
    else {
      for (int i = 0; i < 6; i++) {
        digitalWrite(redLedPin, HIGH); delay(500);
        digitalWrite(redLedPin, LOW);  delay(500);
      }
    }
    delay(1000);
  }
  // —— NO MOTION ——
  else {
    lcd.setCursor(0, 1);
    lcd.print(welcomeMsg);
    for (int i = welcomeMsg.length(); i<16; i++) lcd.print(' ');
    digitalWrite(ledPin, LOW);
  }

  delay(500);
}

int getDistance() {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}