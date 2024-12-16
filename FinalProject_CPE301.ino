// Calvin Lu
//CPE 301 Final Project
//12/15/2024
// Water cooler project

#include <LiquidCrystal.h>
#include <DHT.h>
#include <Stepper.h>
#include <RTClib.h>

// Hardware pin definitions
#define FAN_PIN 5
#define MOTOR_SPEED_PIN 5
#define DIR1_PIN 4
#define DIR2_PIN 3
#define TEMP_THRESHOLD 21.5
#define POTENTIOMETER_PIN A0
#define WATER_SENSOR_PIN A5
#define START_BUTTON_PIN 2
#define STOP_BUTTON_PIN 32
#define RESET_BUTTON_PIN 34
#define YELLOW_LED_PIN 22
#define GREEN_LED_PIN 26
#define BLUE_LED_PIN 24
#define RED_LED_PIN 28

#define LCD_RS 27
#define LCD_EN 25
#define LCD_D4 37
#define LCD_D5 35
#define LCD_D6 33
#define LCD_D7 31

// Create objects
LiquidCrystal lcd(LCD_RS, LCD_EN, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DHT dht(7, DHT11); // DHT sensor
RTC_DS3231 rtc;
Stepper stepper(200, 8, 10, 9, 11); // 200 steps per revolution

// System state
enum State { DISABLED, IDLE, ERROR, RUNNING };
volatile State systemState = DISABLED;

// Variables
int waterLevel = 0;
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;
unsigned long motorStepStartTime = 0;
unsigned long motorStepDuration = 5000; // Motor step duration in ms
bool motorStepInProgress = false;

// Setup function
void setup() {
    initSerial(9600);
    initializePins();
    initializeLCD();
    initializeSensors();
    rtc.begin();
    adc_init();
}

// Main loop
void loop() {
    handleButtonPresses();
    if (systemState == IDLE || systemState == RUNNING) {
        monitorWaterLevel();
        updateTemperatureAndDisplay();
        controlMotor();
        checkWaterLevel();
    }
}

// Initialize Serial communication
void initSerial(unsigned long baudRate) {
    unsigned long F_CPU = 16000000;
    unsigned int baudValue = (F_CPU / 16 / baudRate - 1);
    *(volatile unsigned int *)0x00C4 = baudValue;
}

// Pin initialization
void initializePins() {
    *DDR_E |= 0x08;  // Set FAN_PIN (PE3) to output
    *DDR_G |= 0x20;  // Set DIR1_PIN (PG5) to output
    *DDR_E |= 0x20;  // Set DIR2_PIN (PE5) to output
    *DDR_E |= 0x08;  // Set FAN_PIN (PE3) to output

    *DDR_E &= 0xEF;  // Set START_BUTTON_PIN (PE4) to input
    *PORT_E |= 0x10;  // Enable pull-up resistor for START_BUTTON_PIN

    *DDR_C &= 0xDF;  // Set STOP_BUTTON_PIN (PC5) to input
    *PORT_C |= 0x20;  // Enable pull-up resistor for STOP_BUTTON_PIN

    // Configure LED pins
    *DDR_A |= 0x01;  // Set YELLOW_LED_PIN (PA0) to output
    *DDR_A |= 0x10;  // Set GREEN_LED_PIN (PA4) to output
    *DDR_A |= 0x04;  // Set BLUE_LED_PIN (PA2) to output
    *DDR_A |= 0x40;  // Set RED_LED_PIN (PA6) to output

    // Configure water sensor pin
    *DDR_H |= 0x08;  // Set WATER_SENSOR_PIN (PH3) to input
    *PORT_A |= 0x01;  // Set YELLOW_LED_PIN (PA0) high
}

// LCD initialization
void initializeLCD() {
    lcd.begin(16, 2);  // Initialize LCD with 16 columns and 2 rows
    lcd.print("System Ready");
}

// Sensor initialization
void initializeSensors() {
    dht.begin();
}

// Handle button presses
void handleButtonPresses() {
    handleStartPress();
    handleStopPress();
    handleResetPress();
}

// Start button press handling
void handleStartPress() {
    static unsigned long lastInterruptTime = 0;
    unsigned long currentTime = millis();

    if ((currentTime - lastInterruptTime > debounceDelay) && ((*PIN_E & 0x10) == LOW)) {
        systemState = IDLE;
        setLEDState(GREEN_LED_PIN, true);
        setLEDState(YELLOW_LED_PIN, false);
        setLEDState(RED_LED_PIN, false);
        setLEDState(BLUE_LED_PIN, false);

        lastInterruptTime = currentTime;
    }
}

// Stop button press handling
void handleStopPress() {
    static unsigned long lastInterruptTime = 0;
    unsigned long currentTime = millis();

    if ((currentTime - lastInterruptTime > debounceDelay) && ((*PIN_C & 0x20) == LOW)) {
        systemState = DISABLED;
        setLEDState(GREEN_LED_PIN, false);
        setLEDState(YELLOW_LED_PIN, true);
        setLEDState(RED_LED_PIN, false);
        setLEDState(BLUE_LED_PIN, false);

        lastInterruptTime = currentTime;
        lcd.clear();
        lcd.print("System Ready!");
    }
}

// Reset button press handling
void handleResetPress() {
    static unsigned long lastInterruptTime = 0;
    unsigned long currentTime = millis();

    if ((currentTime - lastInterruptTime > debounceDelay) && ((*PIN_C & 0x08) == LOW && systemState == ERROR)) {
        systemState = IDLE;
        setLEDState(GREEN_LED_PIN, true);
        setLEDState(YELLOW_LED_PIN, false);
        setLEDState(RED_LED_PIN, false);
        setLEDState(BLUE_LED_PIN, false);

        lastInterruptTime = currentTime;
    }
}

// Set LED state (on/off)
void setLEDState(int pin, bool state) {
    if (state) {
        *PORT_A |= (1 << pin);
    } else {
        *PORT_A &= ~(1 << pin);
    }
}

// Monitor water level sensor
void monitorWaterLevel() {
    *PORT_H |= 0x08;  // Set water sensor pin high
    waterLevel = adc_read(5);  // Read water level
}

// Update temperature and display on LCD
void updateTemperatureAndDisplay() {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Temp: ");
    lcd.print(temperature);
    lcd.print(" C");

    lcd.setCursor(0, 1);
    lcd.print("Humidity: ");
    lcd.print(humidity);
    lcd.print(" %");
}

// Control motor based on system state
void controlMotor() {
    if (systemState == IDLE && dht.readTemperature() > TEMP_THRESHOLD) {
        systemState = RUNNING;
        setLEDState(GREEN_LED_PIN, false);
        setLEDState(BLUE_LED_PIN, true);
        setLEDState(RED_LED_PIN, false);
        setLEDState(YELLOW_LED_PIN, false);

        startFan();
    } else if (systemState == RUNNING && dht.readTemperature() <= TEMP_THRESHOLD) {
        systemState = IDLE;
        setLEDState(BLUE_LED_PIN, false);
        setLEDState(GREEN_LED_PIN, true);
        setLEDState(RED_LED_PIN, false);
        setLEDState(YELLOW_LED_PIN, false);

        stopFan();
    }
}

// Start fan operation
void startFan() {
    *PORT_G &= 0xDF;  // Set DIR1_PIN low
    *PORT_E |= 0x20;  // Set DIR2_PIN high
}

// Stop fan operation
void stopFan() {
    *PORT_E &= 0xF7;  // Set FAN_PIN low
}

// ADC initialization
void adc_init() {
    ADMUX = (1 << REFS0);  // Set reference voltage to AVcc
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);  // Enable ADC and set prescaler
}

// ADC read function
uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);  // Start conversion
    while (ADCSRA & (1 << ADSC));  // Wait for conversion to finish
    return ADC;
}

// Check if water level is too low
void checkWaterLevel() {
    if (waterLevel < 20) {
        systemState = ERROR;
        setLEDState(RED_LED_PIN, true);
        setLEDState(YELLOW_LED_PIN, false);
        setLEDState(GREEN_LED_PIN, false);
        setLEDState(BLUE_LED_PIN, false);
        lcd.clear();
        lcd.print("ERROR: Low Water!");
    }

    *PORT_H &= 0xF7;  // Set water sensor pin low
}
