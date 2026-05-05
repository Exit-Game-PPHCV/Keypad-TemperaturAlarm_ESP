#include <Keypad.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ===== KEYPAD SETUP =====
const byte ROWS = 4;
const byte COLS = 4;

// ===== WIFI Setup =====
const char* WIFI_SSID = "das muss geändert werden!!";
const char* WIFI_PASSWORD = "das muss geändert werden!!";
const char* SERVER_URL = "das muss geändert werden!!"; 

unsigned long lastServerSendTime = 0;
const unsigned long SERVER_SEND_INTERVAL_MS = 2000;

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


// ===== WIFI VERBINDEN =====
void connectWifi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Verbinde mit WLAN");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("WLAN verbunden. ESP IP: ");
  Serial.println(WiFi.localIP());
}


// ===== STATUS AN FLASK SENDEN =====
void sendStateToServer() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Kein WLAN. Sende nicht.");
    return;
  }

  HTTPClient http;
  http.begin(SERVER_URL);
  http.addHeader("Content-Type", "application/json");

  String json = "{";
  json += "\"keypadSolved\":";
  json += keypadSolved ? "true" : "false";
  json += ",";
  json += "\"temperatureAlarm\":";
  json += temperatureAlarm ? "true" : "false";
  json += ",";
  json += "\"temperatureC\":";
  json += String(lastTempC, 2);
  json += "}";

  int httpCode = http.POST(json);

  Serial.print("An Server gesendet: ");
  Serial.println(json);

  Serial.print("Server Antwortcode: ");
  Serial.println(httpCode);

  http.end();
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

  Serial.print("Boolean keypadSolved fuer Flask: ");
  Serial.println(keypadSolved ? "true" : "false");

  sendStateToServer();
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

  connectWifi();

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

  if (millis() - lastServerSendTime >= SERVER_SEND_INTERVAL_MS) {
    lastServerSendTime = millis();
    sendStateToServer();
  }

  delay(50);
}
