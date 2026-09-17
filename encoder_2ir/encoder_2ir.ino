#include <Arduino.h>

// --- أطراف المحركات (STM32 GenF4) ---
#define LEFT_IN1    PA_0
#define LEFT_IN2    PA_3
#define RIGHT_IN1   PA_6
#define RIGHT_IN2   PB_1

// --- أطراف الإنكودر ---
#define LEFT_ENC_A  PA_2
#define LEFT_ENC_B  PA_1
#define RIGHT_ENC_A PB_0
#define RIGHT_ENC_B PA_7

// --- أطراف حساسات الـ IR الديجيتال ---
#define IR_LEFT_PIN   PA_4
#define IR_RIGHT_PIN  PA_5

#define WALL_DETECTED LOW // الحساس يرسل LOW عند اكتشاف جدار

#define BASE_SPEED    110

// متغيرات الإنكودر
volatile long leftEncoderCount = 0;
volatile long rightEncoderCount = 0;
volatile uint8_t leftPreviousState = 0;
volatile uint8_t rightPreviousState = 0;

const int8_t encoderTable[16] = {
   0, -1,  1,  0,
   1,  0,  0, -1,
  -1,  0,  0,  1,
   0,  1, -1,  0
};

// --- مقاطعات الإنكودر ---
void leftEncoderISR() {
  uint8_t A = digitalRead(LEFT_ENC_A);
  uint8_t B = digitalRead(LEFT_ENC_B);
  uint8_t currentState = (A << 1) | B;
  uint8_t index = (leftPreviousState << 2) | currentState;
  leftEncoderCount += encoderTable[index];
  leftPreviousState = currentState;
}

void rightEncoderISR() {
  uint8_t A = digitalRead(RIGHT_ENC_A);
  uint8_t B = digitalRead(RIGHT_ENC_B);
  uint8_t currentState = (A << 1) | B;
  uint8_t index = (rightPreviousState << 2) | currentState;
  rightEncoderCount += encoderTable[index];
  rightPreviousState = currentState;
}

void setMotors(int leftPwm, int rightPwm) {
  leftPwm  = constrain(leftPwm,  -255, 255);
  rightPwm = constrain(rightPwm, -255, 255);

  if (leftPwm >= 0) {
    digitalWrite(LEFT_IN1, LOW);
    analogWrite(LEFT_IN2, leftPwm);
  } else {
    analogWrite(LEFT_IN1, -leftPwm);
    digitalWrite(LEFT_IN2, LOW);
  }

  if (rightPwm >= 0) {
    analogWrite(RIGHT_IN1, rightPwm);
    digitalWrite(RIGHT_IN2, LOW);
  } else {
    digitalWrite(RIGHT_IN1, LOW);
    analogWrite(RIGHT_IN2, -rightPwm);
  }
}

void stopMotors() {
  setMotors(0, 0);
}

long targetOffset = 0;
unsigned long lastPrintTime = 0;

void setup() {
  // تهيئة السيريال لمتحكم STM32
  Serial.setRx(PA_10);
  Serial.setTx(PA_9);
  Serial.begin(115200);
  delay(1000);


  Serial.println(F("  MicroMouse Centering System Started   "));


  // تهيئة المحركات
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);
  stopMotors();
  Serial.println(F("[SETUP] Motor pins initialized."));

  // تهيئة الإنكودر
  pinMode(LEFT_ENC_A, INPUT_PULLUP);
  pinMode(LEFT_ENC_B, INPUT_PULLUP);
  pinMode(RIGHT_ENC_A, INPUT_PULLUP);
  pinMode(RIGHT_ENC_B, INPUT_PULLUP);

  leftPreviousState  = (digitalRead(LEFT_ENC_A) << 1) | digitalRead(LEFT_ENC_B);
  rightPreviousState = (digitalRead(RIGHT_ENC_A) << 1) | digitalRead(RIGHT_ENC_B);

  attachInterrupt(digitalPinToInterrupt(LEFT_ENC_A), leftEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(LEFT_ENC_B), leftEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_A), rightEncoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(RIGHT_ENC_B), rightEncoderISR, CHANGE);
  Serial.println(F("[SETUP] Encoders attached with interrupts."));

  // تهيئة حساسات الأشعة تحت الحمراء
  pinMode(IR_LEFT_PIN, INPUT);
  pinMode(IR_RIGHT_PIN, INPUT);
  Serial.println(F("[SETUP] IR sensors initialized."));

  Serial.println(F("[SETUP] Robot will start moving in 3 seconds..."));
  delay(3000);
}

void loop() {
  // 1. قراءة الحساسات الرقمية
  bool leftClose  = (digitalRead(IR_LEFT_PIN) == WALL_DETECTED);
  bool rightClose = (digitalRead(IR_RIGHT_PIN) == WALL_DETECTED);

  // 2. تعديل إزاحة المسار بناءً على الجدران
  if (leftClose && !rightClose) {
    targetOffset -= 1; // تعديل لليمين
  } else if (rightClose && !leftClose) {
    targetOffset += 1; // تعديل لليسار
  }
  targetOffset = constrain(targetOffset, -25, 25);

  // 3. قراءة العدادات وحساب تصحيح السرعة
  long lCount = leftEncoderCount;
  long rCount = rightEncoderCount;
  long rawDiff = lCount - rCount;

  long error = rawDiff - targetOffset;
  int correction = error * 3;
  correction = constrain(correction, -30, 30);

  int leftMotorSpeed  = BASE_SPEED - correction;
  int rightMotorSpeed = BASE_SPEED + correction;

  // 4. إرسال السرعات للمحركات
  setMotors(leftMotorSpeed, rightMotorSpeed);

  // 5. طباعة كل البيانات عبر السيريال كل 80 مللي ثانية لمنع تهنيج البورد
  if (millis() - lastPrintTime >= 80) {
    lastPrintTime = millis();

    Serial.print(F("IR: [L: "));
    Serial.print(leftClose ? F("WALL ") : F("OPEN "));
    Serial.print(F("| R: "));
    Serial.print(rightClose ? F("WALL ") : F("OPEN "));
    Serial.print(F("] | ENC: [L: "));
    Serial.print(lCount);
    Serial.print(F(" | R: "));
    Serial.print(rCount);
    Serial.print(F(" | Diff: "));
    Serial.print(rawDiff);
    Serial.print(F("] | Offset: "));
    Serial.print(targetOffset);
    Serial.print(F(" | Error: "));
    Serial.print(error);
    Serial.print(F(" | PWM: [L: "));
    Serial.print(leftMotorSpeed);
    Serial.print(F(" | R: "));
    Serial.print(rightMotorSpeed);
    Serial.println(F("]"));
  }

  delay(10);
}