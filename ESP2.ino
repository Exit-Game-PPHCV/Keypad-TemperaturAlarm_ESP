#include <Keypad.h>
#include <DHT.h>

// ===== KEYPAD SETUP =====
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Deine Keypad-Pins
byte rowPins[ROWS] = {3, 2, 11, 10};
byte colPins[COLS] = {8, 1, 0, 7};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ===== KEYPAD LED PINS =====
const int RED_LED_PIN = 21;
const int GREEN_LED_PIN = 20;

// ===== DHT22 / AM2302 =====
#define DHTPIN 6
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// ===== TEMPERATUR / ALARM =====
const int BUZZER_PIN = 4;
const int TEMP_ALARM_LED = 22;
const int TEMP_OK_LED = 23;

const float TEMP_LIMIT_C = 25.0;

// ===== CODE =====
String inputCode = "";
const String correctCode = "121950";

// Booleans für späteren Server
bool keypadSolved = false;
bool temperatureAlarm = false;

// ===== PRÜF-ANIMATION =====
const unsigned long CHECK_TIME_MS = 1000;
const unsigned long BLINK_INTERVAL_MS = 100;

// ===== BUZZER TIMING =====
unsigned long lastBuzzerTime = 0;
bool buzzerState = false;
const unsigned long BUZZER_INTERVAL_MS = 500;

// ===== DHT TIMING =====
unsigned long lastTempReadTime = 0;
const unsigned long TEMP_READ_INTERVAL_MS = 2000;
float lastTempC = 0.0;


// ===== KEYPAD STATUS-LEDS =====
void setStatusLeds() {
  if (keypadSolved) {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  } else {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  }
}


// ===== PRÜFANIMATION =====
void checkAnimation() {
  unsigned long startTime = millis();
  bool ledState = false;

  while (millis() - startTime < CHECK_TIME_MS) {
    ledState = !ledState;

    digitalWrite(RED_LED_PIN, ledState ? HIGH : LOW);
    digitalWrite(GREEN_LED_PIN, ledState ? HIGH : LOW);

    delay(BLINK_INTERVAL_MS);
  }
}


// ===== CODE PRÜFEN =====
void checkInputCode() {
  Serial.print("Pruefe Eingabe: ");
  Serial.println(inputCode);

  checkAnimation();

  if (inputCode == correctCode) {
    keypadSolved = true;
    Serial.println("RICHTIG -> keypadSolved = true");
  } else {
    keypadSolved = false;
    Serial.println("FALSCH -> keypadSolved = false");
  }

  inputCode = "";
  setStatusLeds();

  Serial.print("Boolean keypadSolved fuer Flask: ");
  Serial.println(keypadSolved ? "true" : "false");
}


// ===== TEMPERATUR / BUZZER UPDATE =====
void updateTemperatureAlarm() {
  if (millis() - lastTempReadTime >= TEMP_READ_INTERVAL_MS) {
    lastTempReadTime = millis();

    float tempC = dht.readTemperature();

    if (isnan(tempC)) {
      Serial.println("DHT22 Fehler: Keine Temperatur gelesen");
      return;
    }

    lastTempC = tempC;

    Serial.print("DHT22 Temperatur: ");
    Serial.print(lastTempC);
    Serial.println(" C");

    temperatureAlarm = lastTempC > TEMP_LIMIT_C;
  }

  if (temperatureAlarm) {
    digitalWrite(TEMP_ALARM_LED, HIGH);
    digitalWrite(TEMP_OK_LED, LOW);

    if (millis() - lastBuzzerTime >= BUZZER_INTERVAL_MS) {
      lastBuzzerTime = millis();
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    }

  } else {
    digitalWrite(TEMP_ALARM_LED, LOW);
    digitalWrite(TEMP_OK_LED, HIGH);

    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
  }
}


// ===== SETUP =====
void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TEMP_ALARM_LED, OUTPUT);
  pinMode(TEMP_OK_LED, OUTPUT);

  keypadSolved = false;
  temperatureAlarm = false;

  setStatusLeds();

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(TEMP_ALARM_LED, LOW);
  digitalWrite(TEMP_OK_LED, LOW);

  Serial.println("Keypad + DHT22 Temperatur + Buzzer bereit.");
  Serial.println("Code eingeben und mit * pruefen.");
}


// ===== LOOP =====
void loop() {
  updateTemperatureAlarm();

  char key = keypad.getKey();

  if (key) {
    Serial.print("Taste: ");
    Serial.println(key);

    if (key == '*') {
      checkInputCode();
      return;
    }

    if (key == '#') {
      inputCode = "";
      Serial.println("Eingabe geloescht");
      return;
    }

    if (key >= '0' && key <= '9') {
      if (inputCode.length() < correctCode.length()) {
        inputCode += key;
      }

      Serial.print("Aktuelle Eingabe: ");
      Serial.println(inputCode);
    }
  }

  delay(50);
}
