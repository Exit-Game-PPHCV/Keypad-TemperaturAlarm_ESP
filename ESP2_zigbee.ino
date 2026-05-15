#include <Keypad.h>
#include <DHT.h>
#include <Zigbee.h>

// =======================================================
// ===== ZIGBEE SETUP ====================================
// =======================================================
//
// Jeder Boolean wird als eigene Zigbee-"Steckdose" registriert:
//
// Endpoint 1: keypadSolved
// Endpoint 2: temperatureAlarm
//
// false = Steckdose AUS
// true  = Steckdose AN

#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee Router/Coordinator mode is not selected in Tools -> Zigbee mode"
#endif

#define ZB_ENDPOINT_KEYPAD_SOLVED      1
#define ZB_ENDPOINT_TEMPERATURE_ALARM  2

ZigbeePowerOutlet zbKeypadSolved =
  ZigbeePowerOutlet(ZB_ENDPOINT_KEYPAD_SOLVED);

ZigbeePowerOutlet zbTemperatureAlarm =
  ZigbeePowerOutlet(ZB_ENDPOINT_TEMPERATURE_ALARM);


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

const float TEMP_LIMIT_C = 19.0;

// ===== CODE =====
String inputCode = "";
const String correctCode = "10";

// Booleans für Zigbee
bool keypadSolved = false;
bool temperatureAlarm = true;

// ===== ZIGBEE STATUS CACHE =====
// Damit nicht dauerhaft derselbe Status gesendet wird
bool lastSentKeypadSolved = false;
bool lastSentTemperatureAlarm = false;
bool firstZigbeeSend = true;

// ===== ZIGBEE REGELMÄSSIG SENDEN =====
// Zusätzlich zur Statusänderung alle 5 Sekunden senden
unsigned long lastZigbeeRoutineSendTime = 0;
const unsigned long ZIGBEE_ROUTINE_SEND_INTERVAL_MS = 5000;

// ===== PRÜF-ANIMATION =====
const unsigned long CHECK_TIME_MS = 1000;
const unsigned long BLINK_INTERVAL_MS = 100;

// ===== BUZZER TIMING =====
unsigned long lastBuzzerTime = 0;
bool buzzerState = false;
const unsigned long BUZZER_INTERVAL_MS = 500;

// ===== DHT TIMING =====
unsigned long lastTempReadTime = 0;
const unsigned long TEMP_READ_INTERVAL_MS = 1000;
float lastTempC = 0.0;


// =======================================================
// ===== ZIGBEE STARTEN ==================================
// =======================================================

void setupZigbee() {
  Serial.println("Starte Zigbee...");

  // Namen setzen, damit die "Steckdosen" auf dem Raspberry Pi
  // besser erkennbar sind.
  zbKeypadSolved.setManufacturerAndModel(
    "EscapeGame",
    "KeypadSolvedOutlet"
  );

  zbTemperatureAlarm.setManufacturerAndModel(
    "EscapeGame",
    "TemperatureAlarmOutlet"
  );

  // Jeden Boolean als eigene Zigbee-Steckdose registrieren
  Serial.println("Registriere Zigbee-Steckdose: keypadSolved");
  Zigbee.addEndpoint(&zbKeypadSolved);

  Serial.println("Registriere Zigbee-Steckdose: temperatureAlarm");
  Zigbee.addEndpoint(&zbTemperatureAlarm);

  // Zigbee starten
  if (!Zigbee.begin(ZIGBEE_ROUTER)) {
    Serial.println("Zigbee konnte nicht gestartet werden!");
    Serial.println("ESP wird neu gestartet...");
    ESP.restart();
  }

  Serial.println("Zigbee gestartet. Warte auf Verbindung zum Netzwerk...");

  while (!Zigbee.connected()) {
    Serial.print(".");
    delay(100);
  }

  Serial.println();
  Serial.println("Zigbee verbunden!");
}


// =======================================================
// ===== STATUS AN ZIGBEE SENDEN =========================
// =======================================================

void sendStateToZigbeeForced() {
  Serial.println("=== Sende Zigbee-Status ===");

  Serial.print("keypadSolved Steckdose: ");
  Serial.println(keypadSolved ? "AN" : "AUS");

  Serial.print("temperatureAlarm Steckdose: ");
  Serial.println(temperatureAlarm ? "AN" : "AUS");

  // Jeder Boolean wird als eigene Steckdose gesetzt.
  // false = aus, true = an
  zbKeypadSolved.setState(keypadSolved);
  zbTemperatureAlarm.setState(temperatureAlarm);

  lastSentKeypadSolved = keypadSolved;
  lastSentTemperatureAlarm = temperatureAlarm;
  firstZigbeeSend = false;
}

void sendStateToZigbee() {
  bool changed =
    firstZigbeeSend ||
    keypadSolved != lastSentKeypadSolved ||
    temperatureAlarm != lastSentTemperatureAlarm;

  if (!changed) {
    return;
  }

  sendStateToZigbeeForced();
} 


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

  Serial.print("Boolean keypadSolved fuer Zigbee: ");
  Serial.println(keypadSolved ? "true" : "false");

  sendStateToZigbee();
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

  setupZigbee();

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

  // Anfangszustand einmal an Zigbee senden
  sendStateToZigbee();
  lastZigbeeRoutineSendTime = millis();
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

// Eventbasiert senden: nur wenn sich ein Zustand geändert hat
sendStateToZigbee();

// Zusätzlich alle 5 Sekunden den aktuellen Status senden
if (millis() - lastZigbeeRoutineSendTime >= ZIGBEE_ROUTINE_SEND_INTERVAL_MS) {
  lastZigbeeRoutineSendTime = millis();
  sendStateToZigbeeForced();
}

delay(50);
}
