// Calvin Lu
//CPE 301 Final Project
//12/15/2024
// Water cooler project

#include <LiquidCrystal.h>
#include <DHT.h>
#include <Stepper.h>
#include <Wire.h>
#include <RTClib.h>

// Pin assignments for the system
#define FAN_IC_IN1_PIN 24    // Pin for L293D Input 1 (Fan control)
#define FAN_IC_IN2_PIN 25    // Pin for L293D Input 2 (Fan control)
#define START_BUTTON_PIN 2   // Pin for the start button (interrupt)
#define STOP_BUTTON_PIN 3    // Pin for the stop button
#define RESET_BUTTON_PIN 4   // Pin for the reset button
#define DHT_PIN 11           // Pin for the DHT11 sensor data
#define WATER_SENSOR_PIN A0  // Pin for the water level sensor
#define YELLOW_LED_PIN 5     // Yellow LED for DISABLED state
#define GREEN_LED_PIN 6      // Green LED for IDLE state
#define RED_LED_PIN 7        // Red LED for ERROR state
#define BLUE_LED_PIN 8       // Blue LED for RUNNING state

// LCD Pins (fixed to 22-27)
#define LCD_RS 22            // Pin for LCD RS
#define LCD_EN 23            // Pin for LCD EN
#define LCD_D4 24            // Pin for LCD D4
#define LCD_D5 25            // Pin for LCD D5
#define LCD_D6 26            // Pin for LCD D6
#define LCD_D7 27            // Pin for LCD D7

// Stepper Motor Pins
#define STEP_PIN_1 29        // Stepper motor pin 1
#define STEP_PIN_2 30        // Stepper motor pin 2

// Create objects for LCD, DHT, RTC, and Stepper Motor
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DHT dht(DHT_PIN, DHT11);
RTC_DS3231 rtc;
Stepper stepper(2048, STEP_PIN_1, STEP_PIN_2); // 2048 steps for a full rotation

// State variables
bool systemEnabled = false;
bool motorRunning = false;
bool waterLow = false;
unsigned long lastTempUpdate = 0;
unsigned long lastWaterCheck = 0;
unsigned long currentMillis = 0;
unsigned long interval = 60000; // 1-minute interval for updates

void setup() {
  // Initialize Serial Monitor
  Serial.begin(9600);
  
  // Initialize pins
  pinMode(FAN_IC_IN1_PIN, OUTPUT);
  pinMode(FAN_IC_IN2_PIN, OUTPUT);
  pinMode(START_BUTTON_PIN, INPUT_PULLUP);
  pinMode(STOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize LEDs
  pinMode(YELLOW_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BLUE_LED_PIN, OUTPUT);
  
  // Initialize LCD
  lcd.begin(16, 2);
  lcd.print("Cooler System");
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize RTC
  if (!rtc.begin()) {
    lcd.print("RTC not found!");
    while (1);
  }
  
  // Attach interrupt for the start button (pin 2)
  attachInterrupt(digitalPinToInterrupt(START_BUTTON_PIN), startButtonPressed, FALLING);
  
  // Display welcome message on LCD
  lcd.setCursor(0, 1);
  lcd.print("System Disabled");
  
  // Set initial state
  digitalWrite(YELLOW_LED_PIN, HIGH);  // Yellow LED ON for DISABLED state
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BLUE_LED_PIN, LOW);
}

void loop() {
  currentMillis = millis();

  if (systemEnabled) {
    if (currentMillis - lastTempUpdate >= interval) {
      lastTempUpdate = currentMillis;
      updateTemperatureHumidity();
    }

    if (currentMillis - lastWaterCheck >= interval) {
      lastWaterCheck = currentMillis;
      checkWaterLevel();
    }
    
    if (waterLow) {
      digitalWrite(RED_LED_PIN, HIGH);
      digitalWrite(BLUE_LED_PIN, LOW);
      digitalWrite(FAN_IC_IN1_PIN, LOW);
      digitalWrite(FAN_IC_IN2_PIN, LOW); // Fan turned OFF if water is low
      lcd.clear();
      lcd.print("Water Level Low!");
      motorRunning = false;
    } else {
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BLUE_LED_PIN, HIGH);
      
      // If the temperature is out of range, turn on the fan
      int temp = dht.readTemperature();
      if (temp > 30 && !motorRunning) {
        digitalWrite(FAN_IC_IN1_PIN, HIGH); // Turn on the fan
        digitalWrite(FAN_IC_IN2_PIN, LOW);
        motorRunning = true;
        lcd.clear();
        lcd.print("Fan Running...");
        logEvent("Fan Started");
      } else if (temp < 20 && motorRunning) {
        digitalWrite(FAN_IC_IN1_PIN, LOW); // Turn off the fan
        digitalWrite(FAN_IC_IN2_PIN, LOW);
        motorRunning = false;
        lcd.clear();
        lcd.print("Fan Stopped");
        logEvent("Fan Stopped");
      }
    }
  }
}

void startButtonPressed() {
  if (!systemEnabled) {
    systemEnabled = true;
    digitalWrite(YELLOW_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);  // Green LED for IDLE state
    lcd.clear();
    lcd.print("System IDLE");
    logEvent("System Started");
  }
}

void stopButtonPressed() {
  if (systemEnabled) {
    systemEnabled = false;
    digitalWrite(YELLOW_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);  // Green LED OFF
    lcd.clear();
    lcd.print("System Disabled");
    logEvent("System Disabled");
  }
}

void resetButtonPressed() {
  if (waterLow) {
    waterLow = false;
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);  // Green LED for IDLE state
    lcd.clear();
    lcd.print("Water Level Normal");
    logEvent("System Reset");
  }
}

void updateTemperatureHumidity() {
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();
  
  if (isnan(temp) || isnan(humidity)) {
    lcd.clear();
    lcd.print("Error reading DHT");
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Temp: ");
  lcd.print(temp);
  lcd.print(" C");

  lcd.setCursor(0, 1);
  lcd.print("Humidity: ");
  lcd.print(humidity);
  lcd.print(" %");
}

void checkWaterLevel() {
  int waterLevel = analogRead(WATER_SENSOR_PIN);
  
  if (waterLevel < 200) {  // Threshold value for low water level
    waterLow = true;
  } else {
    waterLow = false;
  }
}

void logEvent(String event) {
  DateTime now = rtc.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.print(" - ");
  Serial.println(event);
}
