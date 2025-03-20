#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

const int irPin = A0;
const int threshold = 500;
int irValue;
int previousValue = -1;
const int servoReturnDelay = 1000; // Delay in milliseconds for servo return

LiquidCrystal_I2C lcd(0x27, 20, 2);
Servo myServo;

const int buttonPin = 2; // Button connected to digital pin 2
int buttonState = 0;
int lastButtonState = 0;
bool scanMode = false;

unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 1000; // 1 second debounce

bool bottleDetected = false; // Flag to indicate bottle detection from Python
int bottleCount = 0; // Counter for detected bottles
String voucherCode = ""; // Store voucher code
bool bottleStatusDisplayed = false; // Flag to prevent repeated "no bottle" messages

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  myServo.attach(10);
  myServo.write(90);
  pinMode(buttonPin, INPUT_PULLUP);
  updateLCD(); // Initial LCD update
}

void loop() {

  // Button Debouncing and State Change
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) { // Button pressed (LOW due to pull-up)
        if (scanMode) {
          Serial.println("end scan");
          scanMode = false;
          Serial.println("Scan Ended.");
          showFinalResults(); //display final results
        } else {
          Serial.println("scan mode");
          scanMode = true;
          Serial.println("Scan Started.");
          bottleCount = 0; //reset count when scan mode starts.
          voucherCode = ""; //reset voucher code.
        }
        updateLCD(); // Update LCD after button press
      }
    }
  }

  lastButtonState = reading;

  if (scanMode) { // Only scan IR if in scan mode
    irValue = analogRead(irPin);

    if (previousValue == -1 || abs(irValue - previousValue) > 50) {
      Serial.print("IR Value: ");
      Serial.println(irValue);

      if (irValue < threshold) {
        Serial.println("Object detected!");
      }
      previousValue = irValue;
      delay(200);
    }
  }

  if (Serial.available() > 0) {
    String serialMessage = Serial.readStringUntil('\n');

    if (serialMessage == "bottle detected") {
      bottleDetected = true;
      bottleCount++; // Increment the bottle count
      bottleStatusDisplayed = false;
      updateLCD(); // Update LCD to show bottle detected and count
    } else if (serialMessage == "no bottle detected") {
      bottleDetected = false;
      if (!bottleStatusDisplayed) {
        bottleStatusDisplayed = true;
        updateLCD(); // Update LCD to show "bottle not detected"
      }
    } else { // Handle other serial messages
      voucherCode = serialMessage; //assign voucher code
    }
  }

  if (bottleDetected) {
    myServo.write(180);
    delay(servoReturnDelay);
    myServo.write(90);
    bottleDetected = false; // Reset the flag
  }

  delay(200);
}

void updateLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (scanMode) {
    if (bottleDetected) {
      lcd.print("Bottle Detected!");
      lcd.setCursor(0, 1);
      lcd.print("Count: ");
      lcd.print(bottleCount);
    } else if (bottleStatusDisplayed) {
      lcd.print("No Bottle Detected");
      lcd.setCursor(0, 1);
      lcd.print("Count: ");
      lcd.print(bottleCount);
    } else {
      //If neither bottleDetected nor bottleStatusDisplayed are true, then the system is waiting for a bottle.
      lcd.print("Ready to Scan");
      lcd.setCursor(0, 1);
      lcd.print("Count: ");
      lcd.print(bottleCount);
    }
  } else {
    lcd.print("Hold the button");
    lcd.setCursor(0, 1);
    lcd.print("To start");
  }
}

void showFinalResults() {
  lcd.clear();
  if (bottleCount > 0) {
    lcd.setCursor(0, 0);
    lcd.print("Total Bottles:");
    lcd.setCursor(0, 1);
    lcd.print(bottleCount);
    delay(3000);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Voucher Code:");

    // Wait for voucher code with a timeout
    unsigned long startTime = millis();
    unsigned long timeout = 5000; // 5 seconds timeout
    while (voucherCode == "" && (millis() - startTime) < timeout) {
      if (Serial.available() > 0) {
        voucherCode = Serial.readStringUntil('\n');
        voucherCode.trim(); //remove trailing whitespace.
      }
      delay(100); // Check every 100 milliseconds
    }

    lcd.setCursor(0, 1);
    if (voucherCode != "") {
      lcd.print(voucherCode);
    } else {
      lcd.print("Timeout");
    }
    delay(10000);
  } else {
    lcd.setCursor(0, 0);
    lcd.print("No Bottles Detected");
    delay(5000);
  }
  voucherCode = ""; //reset the voucher code.
  updateLCD();
}