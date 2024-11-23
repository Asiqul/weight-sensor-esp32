#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <HX711.h>
#include <EEPROM.h>
#include <Keypad.h>
#include <splash.h>

/* TASKS HANDLER DEFINITION */
TaskHandle_t DisplayController;
/* TASKS HANDLER DEFINITION END */

/* DEFINING HX711 PINS */
#define LOADCELL_DOUT_PIN 16
#define LOADCELL_SCK_PIN 4
HX711 scale;
/* DEFINING HX711 PINS END */

/* RELAY and LIMIT SWITCH PINS */
#define RELAY_PIN 23
#define BUZZER_PIN 5
/* RELAY and LIMIT SWITCH PINS END */

/* OLED DISPLAY PARAMETERS */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
/* OLED DISPLAY PARAMETERS END */

/* KEYPAD SETUP */
const byte ROWS = 4;
const byte COLS = 4;
char hexaKeys[ROWS][COLS] = {
    {'1', '4', '7', '*'},
    {'2', '5', '8', '0'},
    {'3', '6', '9', '#'},
    {'A', 'B', 'C', 'D'},
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};
Keypad customKeypad = Keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS);
/* KEYPAD SETUP END */

/* GLOBAL PARAMETERS */
float calibration_factor = 195.0;
long zero_offset = 0;
float weight;
bool isTare = false;
float limitWeight;
bool isCalibrate = false;
bool start = false;
bool isReady = false;
// String calStatus = "Not OK";
String tareStatus = "Not OK";
/* GLOBAL PARAMETERS END */

/* FUNCTION DEFINITION */
void keypadEvent(KeypadEvent key);
void DisplayControl(void *pvParameters);
void DisplayText(int x, int y, int size, String text);
/* FUNCTION DEFINITION END*/

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(2);

  if (EEPROM.read(1) <= 5)
  {
    EEPROM.write(1, 12);
    EEPROM.commit();
    limitWeight = EEPROM.read(1) * 100;
  }
  else
  {
    limitWeight = EEPROM.read(1) * 100;
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  // Splash Screen Display
  display.clearDisplay();
  display.drawBitmap(0, 0, splash, 128, 64, WHITE);
  display.display();

  // HX711 Setup
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor);
  scale.tare();

  // Relay and Limit Switch Setup
  pinMode(RELAY_PIN, OUTPUT_OPEN_DRAIN);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(BUZZER_PIN, LOW);
  delay(2000);

  // Task Creation
  xTaskCreatePinnedToCore(
      DisplayControl,
      "DisplayController",
      10000,
      NULL,
      1,
      &DisplayController,
      0);
  // Task Creation End

  display.clearDisplay();

  customKeypad.addEventListener(keypadEvent);
}

void loop()
{
  display.clearDisplay();
  while (!isReady)
  {
    char input = customKeypad.getKey();

    DisplayText(40, 0, 1, String("MAIN MENU"));
    DisplayText(30, 17, 1, String("Tare: " + tareStatus));
    DisplayText(25, 28, 1, String("Limit: " + String(limitWeight / 1000) + " Kg"));
    DisplayText(20, 46, 1, String("A:Tare, B:Start"));
    DisplayText(16, 56, 1, String("C: Set Limit Max"));
  }
}

void keypadEvent(KeypadEvent key)
{
  switch (customKeypad.getState())
  {
  case HOLD:
    if (key == 'A')
    {
      display.clearDisplay();
      isTare = true;
      tareStatus = "OK";
      zero_offset = scale.read_average(15);
      scale.set_offset(zero_offset);
      DisplayText(5, 25, 2, String("Tare Done!"));
      delay(2000);
      display.clearDisplay();
    }
    else if (key == 'B' && isTare)
    {
      display.clearDisplay();
      while (true)
      {
        char input = customKeypad.getKey();
        weight = scale.get_units(2);

        display.setCursor(13, 0);
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        display.print("CALCULATING WEIGHT");

        display.setCursor(20, 25);
        display.setTextSize(2);
        display.print(weight / 100, 2);
        display.print(" ons");

        display.setCursor(18, 55);
        display.setTextSize(1);
        display.print("Current: ");
        display.print(String(calibration_factor));
        display.display();

        if (input == 'A')
        {
          calibration_factor += 2;
          scale.set_scale(calibration_factor);
        }
        else if (input == 'B')
        {
          calibration_factor -= 2;
          scale.set_scale(calibration_factor);
        }
        else if (input == '#')
        {
          start = false;
          display.clearDisplay();
          break;
        }
        display.clearDisplay();
      }
    }
    else if (key == 'C')
    {
      isCalibrate = false;
      String newFactor = "";
      display.clearDisplay();
      int maxString;
      while (!isCalibrate)
      {
        maxString = newFactor.length();
        char input = customKeypad.getKey();
        DisplayText(9, 0, 1, String("Input Limit Weight"));
        DisplayText(80, 26, 1, String("ons"));
        DisplayText(20, 55, 1, String("Press * to Save"));

        if (input == '0' || input == '1' || input == '2' || input == '3' || input == '4' || input == '5' || input == '6' || input == '7' || input == '8' || input == '9' && maxString < 3)
        {
          newFactor += input;
          DisplayText(37, 23, 2, String(newFactor));
        }
        else if (input == 'D')
        {
          newFactor = "";
          display.clearDisplay();
          DisplayText(37, 23, 2, String(newFactor));
        }
        else if (input == '*')
        {
          isCalibrate = true;
          EEPROM.write(1, newFactor.toInt());
          EEPROM.commit();
          limitWeight = EEPROM.read(1) * 100;
          display.clearDisplay();
          DisplayText(30, 25, 2, String("SAVED!"));
          delay(1500);
          display.clearDisplay();
          break;
        }
      }
    }
    else if (key == 'B' && !isTare)
    {
      display.clearDisplay();
      DisplayText(4, 25, 2, String("Tare First"));
      delay(1500);
      display.clearDisplay();
    }
    delay(1000);
    break;
  }
}

void DisplayText(int x, int y, int size, String text)
{
  display.setCursor(x, y);
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.println((text));
  display.display();
}

void DisplayControl(void *pvParameters)
{
  while (true)
  {
    if (weight >= limitWeight)
    {
      digitalWrite(RELAY_PIN, LOW);
      while (true)
      {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(200);
        digitalWrite(BUZZER_PIN, LOW);
        delay(200);
        if (weight < limitWeight)
          break;
      }
    }
    else
    {
      digitalWrite(RELAY_PIN, HIGH);
    }
    delay(500);
  }
}