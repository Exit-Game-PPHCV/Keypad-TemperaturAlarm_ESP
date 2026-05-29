# Code-Dokumentation – ESP32 #2: Keypad- und Temperaturmodul

## 1. Übersicht

Diese Datei beschreibt den Code für den zweiten ESP32 im Flight Simulator Exit Game. Der Mikrocontroller übernimmt zwei Aufgaben im Spiel:

* die Auswertung eines Codes über ein 4x4-Keypad
* die Überwachung der Temperatur mit einem DHT22-/AM2302-Sensor

Zusätzlich steuert der ESP32 mehrere LEDs und einen aktiven Buzzer. Dadurch erhalten die Spieler direkt am Aufbau eine Rückmeldung, ob der eingegebene Code richtig war oder ob ein Temperaturalarm aktiv ist.

Wie beim ersten ESP32 wird auch hier ein Teil der Spiellogik direkt lokal auf dem Mikrocontroller verarbeitet. Der Server bekommt nicht jeden Tastendruck oder jeden Temperaturmesswert als Rohdaten, sondern nur die daraus abgeleiteten Boolean-Zustände. Diese Zustände werden über Zigbee als virtuelle Steckdosen an das restliche System weitergegeben.

---

## 2. Aufgabe des ESP32 im Spiel

Der ESP32 #2 ist für zwei Spielmechaniken zuständig, die im Szenario mit der Stabilisierung des Flugzeugsystems verbunden sind.

| Aufgabe               | Bedeutung im Spiel                     | Technische Umsetzung                                                   |
| --------------------- | -------------------------------------- | ---------------------------------------------------------------------- |
| Keypad-Code           | Eingabe eines Freigabe- oder Funkcodes | Spieler geben einen Code über ein 4x4-Keypad ein                       |
| Temperaturüberwachung | Überhitzung eines Systems erkennen     | DHT22 misst die Temperatur und löst bei Überschreitung einen Alarm aus |

Der ESP32 übernimmt damit sowohl eine aktive Eingabeaufgabe als auch eine dauerhaft laufende Überwachungsaufgabe. Beide Zustände werden unabhängig voneinander verwaltet und über Zigbee übertragen.

---

## 3. Eingebundene Bibliotheken

Am Anfang des Codes werden drei Bibliotheken eingebunden:

```cpp
#include <Keypad.h>
#include <DHT.h>
#include <Zigbee.h>
```

Die `Keypad`-Bibliothek wird verwendet, um das 4x4-Tastenfeld einfach auszulesen. Die `DHT`-Bibliothek übernimmt die Kommunikation mit dem AM2302-/DHT22-Temperatursensor. Die `Zigbee`-Bibliothek wird genutzt, um den ESP32 als Zigbee-Gerät im Netzwerk anzumelden und die Boolean-Werte als virtuelle Steckdosen zu übertragen.

Zusätzlich enthält der Code eine Sicherheitsprüfung:

```cpp
#ifndef ZIGBEE_MODE_ZCZR
#error "Zigbee Router/Coordinator mode is not selected in Tools -> Zigbee mode"
#endif
```

Damit wird beim Kompilieren geprüft, ob in der Arduino-Umgebung der passende Zigbee-Modus ausgewählt wurde. Der Code ist für den Betrieb als Zigbee-Router vorgesehen.

---

## 4. Zigbee-Endpunkte für die Statuswerte

Der ESP32 stellt zwei Boolean-Zustände bereit:

```cpp
#define ZB_ENDPOINT_KEYPAD_SOLVED      1
#define ZB_ENDPOINT_TEMPERATURE_ALARM  2
```

Daraus werden zwei virtuelle Zigbee-Steckdosen erzeugt:

```cpp
ZigbeePowerOutlet zbKeypadSolved =
  ZigbeePowerOutlet(ZB_ENDPOINT_KEYPAD_SOLVED);

ZigbeePowerOutlet zbTemperatureAlarm =
  ZigbeePowerOutlet(ZB_ENDPOINT_TEMPERATURE_ALARM);
```

Die Zustände werden folgendermaßen abgebildet:

| Boolean            | Zigbee-Endpunkt | Bedeutung                                         |
| ------------------ | --------------- | ------------------------------------------------- |
| `keypadSolved`     | EP 1            | Der eingegebene Keypad-Code ist korrekt           |
| `temperatureAlarm` | EP 2            | Die gemessene Temperatur liegt über dem Grenzwert |

Dabei gilt:

```text
false = Steckdose AUS
true  = Steckdose AN
```

Diese Lösung passt zur restlichen Systemarchitektur, weil Zigbee2MQTT Steckdosen-Zustände direkt als `ON` oder `OFF` weitergeben kann. Der Flask-Server muss dadurch keine Tastenfolgen oder Temperaturwerte interpretieren, sondern bekommt nur den spielrelevanten Zustand.

---

## 5. Keypad-Aufbau und Pinbelegung

Das Keypad wird als 4x4-Matrix definiert:

```cpp
const byte ROWS = 4;
const byte COLS = 4;
```

Die Tastenbelegung entspricht einem typischen 4x4-Keypad:

```cpp
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
```

Die verwendeten Pins sind:

```cpp
byte rowPins[ROWS] = {3, 2, 11, 10};
byte colPins[COLS] = {8, 1, 0, 7};
```

Daraus wird anschließend das Keypad-Objekt erzeugt:

```cpp
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);
```

Die Pinbelegung des Keypads ist damit:

| Keypad-Bereich | Pins                             |
| -------------- | -------------------------------- |
| Reihen         | GPIO 3, GPIO 2, GPIO 11, GPIO 10 |
| Spalten        | GPIO 8, GPIO 1, GPIO 0, GPIO 7   |

---

## 6. LEDs für den Keypad-Status

Für das Keypad werden zwei LEDs verwendet:

```cpp
const int RED_LED_PIN = 21;
const int GREEN_LED_PIN = 20;
```

Die rote LED zeigt an, dass der Code noch nicht korrekt eingegeben wurde. Die grüne LED zeigt an, dass das Keypad-Rätsel gelöst ist.

Die Logik dafür befindet sich in der Funktion `setStatusLeds()`:

```cpp
void setStatusLeds() {
  if (keypadSolved) {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, HIGH);
  } else {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(GREEN_LED_PIN, LOW);
  }
}
```

Solange `keypadSolved` den Wert `false` hat, leuchtet die rote LED. Sobald der richtige Code erkannt wurde, geht die rote LED aus und die grüne LED an.

---

## 7. Code-Eingabe über das Keypad

Der eingegebene Code wird in einer Zeichenkette gespeichert:

```cpp
String inputCode = "";
const String correctCode = "10";
```

Im aktuellen Stand lautet der richtige Code also:

```text
10
```

Im `loop()` wird dauerhaft geprüft, ob eine Taste gedrückt wurde:

```cpp
char key = keypad.getKey();
```

Wenn eine Ziffer gedrückt wird, wird sie an `inputCode` angehängt:

```cpp
if (key >= '0' && key <= '9') {
  if (inputCode.length() < correctCode.length()) {
    inputCode += key;
  }
}
```

Dabei wird die Eingabe nur so lange erweitert, bis sie die Länge des richtigen Codes erreicht hat. Dadurch wird verhindert, dass beliebig lange Eingaben entstehen.

Die Taste `#` löscht die aktuelle Eingabe:

```cpp
if (key == '#') {
  inputCode = "";
  Serial.println("Eingabe geloescht");
  return;
}
```

Die Taste `*` startet die Prüfung des eingegebenen Codes:

```cpp
if (key == '*') {
  checkInputCode();
  return;
}
```

---

## 8. Prüfung des Keypad-Codes

Die Prüfung findet in der Funktion `checkInputCode()` statt. Zuerst wird die aktuelle Eingabe ausgegeben und eine kurze LED-Animation gestartet:

```cpp
Serial.print("Pruefe Eingabe: ");
Serial.println(inputCode);

checkAnimation();
```

Danach wird der eingegebene Code mit dem richtigen Code verglichen:

```cpp
if (inputCode == correctCode) {
  keypadSolved = true;
  Serial.println("RICHTIG -> keypadSolved = true");
} else {
  keypadSolved = false;
  Serial.println("FALSCH -> keypadSolved = false");
}
```

Nach der Prüfung wird die Eingabe gelöscht und die Status-LEDs werden aktualisiert:

```cpp
inputCode = "";
setStatusLeds();
```

Zusätzlich wird der neue Zustand direkt an Zigbee gesendet:

```cpp
sendStateToZigbee();
```

Damit wird der Server zeitnah darüber informiert, ob das Keypad-Rätsel gelöst wurde.

---

## 9. Prüfanimation

Während der Code geprüft wird, blinken rote und grüne LED gemeinsam. Diese Animation ist kurz gehalten und dient als sichtbares Feedback für die Spieler.

Die Dauer und Blinkgeschwindigkeit werden über Konstanten gesteuert:

```cpp
const unsigned long CHECK_TIME_MS = 1000;
const unsigned long BLINK_INTERVAL_MS = 100;
```

Die Animation läuft in der Funktion `checkAnimation()`:

```cpp
while (millis() - startTime < CHECK_TIME_MS) {
  ledState = !ledState;

  digitalWrite(RED_LED_PIN, ledState ? HIGH : LOW);
  digitalWrite(GREEN_LED_PIN, ledState ? HIGH : LOW);

  delay(BLINK_INTERVAL_MS);
}
```

Nach der Animation entscheidet der Code, ob anschließend die rote oder die grüne LED leuchten soll.

---

## 10. Temperaturmessung mit DHT22 / AM2302

Für die Temperaturmessung wird ein DHT22-/AM2302-Sensor verwendet:

```cpp
#define DHTPIN 6
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);
```

Der Sensor ist an GPIO 6 angeschlossen. Die Messung erfolgt nicht mit `analogRead()`, sondern über die DHT-Bibliothek, weil der DHT22 ein digitaler Sensor ist.

Der Grenzwert ist aktuell so festgelegt:

```cpp
const float TEMP_LIMIT_C = 19.0;
```

Wenn die gemessene Temperatur über diesem Wert liegt, wird `temperatureAlarm` auf `true` gesetzt.

---

## 11. Temperatur-Alarm, LEDs und Buzzer

Für den Temperaturzustand werden zwei LEDs und ein aktiver Buzzer verwendet:

```cpp
const int BUZZER_PIN = 4;
const int TEMP_ALARM_LED = 22;
const int TEMP_OK_LED = 23;
```

Die Zuordnung ist:

| Komponente           | Pin     |
| -------------------- | ------- |
| Active Buzzer        | GPIO 4  |
| Temperatur-Alarm-LED | GPIO 22 |
| Temperatur-OK-LED    | GPIO 23 |

Die Temperatur wird in der Funktion `updateTemperatureAlarm()` verarbeitet. Dort wird in regelmäßigen Abständen ein neuer Messwert gelesen:

```cpp
float tempC = dht.readTemperature();
```

Falls kein gültiger Wert gelesen werden kann, wird eine Fehlermeldung ausgegeben und die Funktion beendet:

```cpp
if (isnan(tempC)) {
  Serial.println("DHT22 Fehler: Keine Temperatur gelesen");
  return;
}
```

Bei einer gültigen Messung wird der Wert gespeichert:

```cpp
lastTempC = tempC;
```

Anschließend wird der Alarmzustand gesetzt:

```cpp
temperatureAlarm = lastTempC > TEMP_LIMIT_C;
```

Wenn ein Temperaturalarm aktiv ist, leuchtet die Alarm-LED und der Buzzer piept. Ist kein Alarm aktiv, leuchtet die OK-LED und der Buzzer bleibt aus.

---

## 12. Timing der Temperaturmessung

Die Temperatur wird nicht bei jedem Durchlauf des `loop()` neu gelesen. Stattdessen wird ein Zeitintervall verwendet:

```cpp
const unsigned long TEMP_READ_INTERVAL_MS = 1000;
```

Der Code prüft mit `millis()`, ob seit der letzten Messung genug Zeit vergangen ist:

```cpp
if (millis() - lastTempReadTime >= TEMP_READ_INTERVAL_MS) {
  lastTempReadTime = millis();
  float tempC = dht.readTemperature();
}
```

Dieses Vorgehen verhindert, dass der Sensor zu häufig ausgelesen wird. Gleichzeitig bleibt die Reaktion des Systems für das Spiel ausreichend schnell.

---

## 13. Buzzer-Logik

Der aktive Buzzer wird nicht dauerhaft eingeschaltet, sondern in einem festen Rhythmus an- und ausgeschaltet:

```cpp
const unsigned long BUZZER_INTERVAL_MS = 500;
```

Wenn der Temperaturalarm aktiv ist, wird der Buzzerzustand alle 500 Millisekunden gewechselt:

```cpp
if (millis() - lastBuzzerTime >= BUZZER_INTERVAL_MS) {
  lastBuzzerTime = millis();
  buzzerState = !buzzerState;
  digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
}
```

Dadurch entsteht ein wiederkehrendes Piepen statt eines durchgehenden Tons. Das passt besser zu einem Alarm im Spielkontext und ist für die Spieler deutlicher wahrnehmbar.

---

## 14. Zigbee-Initialisierung

Die Funktion `setupZigbee()` bereitet die Zigbee-Kommunikation vor.

Zunächst werden Namen für die beiden virtuellen Steckdosen gesetzt:

```cpp
zbKeypadSolved.setManufacturerAndModel(
  "EscapeGame",
  "KeypadSolvedOutlet"
);

zbTemperatureAlarm.setManufacturerAndModel(
  "EscapeGame",
  "TemperatureAlarmOutlet"
);
```

Danach werden beide Endpunkte registriert:

```cpp
Zigbee.addEndpoint(&zbKeypadSolved);
Zigbee.addEndpoint(&zbTemperatureAlarm);
```

Anschließend wird Zigbee gestartet:

```cpp
Zigbee.begin(ZIGBEE_ROUTER);
```

Wenn der Start fehlschlägt, wird der ESP32 neu gestartet:

```cpp
ESP.restart();
```

Nach dem Start wartet der Code auf eine erfolgreiche Verbindung zum Zigbee-Netzwerk:

```cpp
while (!Zigbee.connected()) {
  Serial.print(".");
  delay(100);
}
```

Erst danach geht das Programm weiter. Dadurch wird verhindert, dass Statuswerte gesendet werden, bevor der ESP32 mit dem Netzwerk verbunden ist.

---

## 15. Senden der Zustände an Zigbee

Der Code sendet die beiden Zustände `keypadSolved` und `temperatureAlarm` als virtuelle Steckdosen.

Das direkte Senden erfolgt in `sendStateToZigbeeForced()`:

```cpp
zbKeypadSolved.setState(keypadSolved);
zbTemperatureAlarm.setState(temperatureAlarm);
```

Zusätzlich merkt sich der Code, welche Werte zuletzt gesendet wurden:

```cpp
lastSentKeypadSolved = keypadSolved;
lastSentTemperatureAlarm = temperatureAlarm;
firstZigbeeSend = false;
```

Die Funktion `sendStateToZigbee()` prüft, ob sich ein Zustand geändert hat:

```cpp
bool changed =
  firstZigbeeSend ||
  keypadSolved != lastSentKeypadSolved ||
  temperatureAlarm != lastSentTemperatureAlarm;
```

Wenn keine Änderung vorliegt, wird nichts gesendet. Wenn sich ein Wert geändert hat, wird `sendStateToZigbeeForced()` aufgerufen.

---

## 16. Regelmäßiger Heartbeat

Zusätzlich zum Senden bei Änderungen wird alle fünf Sekunden der aktuelle Zustand erneut übertragen:

```cpp
const unsigned long ZIGBEE_ROUTINE_SEND_INTERVAL_MS = 5000;
```

Im `loop()` wird geprüft, ob das Intervall abgelaufen ist:

```cpp
if (millis() - lastZigbeeRoutineSendTime >= ZIGBEE_ROUTINE_SEND_INTERVAL_MS) {
  lastZigbeeRoutineSendTime = millis();
  sendStateToZigbeeForced();
}
```

Dieser Heartbeat macht die Kommunikation robuster. Auch wenn eine einzelne Nachricht verloren geht, wird der aktuelle Zustand regelmäßig erneut gesendet.

---

## 17. Ablauf im `setup()`

Im `setup()` wird das Modul vorbereitet:

1. Serielle Ausgabe starten
2. Zigbee starten und Verbindung abwarten
3. DHT22-Sensor initialisieren
4. LED- und Buzzer-Pins als Ausgänge setzen
5. Startzustände setzen
6. Status-LEDs aktualisieren
7. Anfangszustand an Zigbee senden
8. Timer für den regelmäßigen Heartbeat starten

Nach dem `setup()` ist der ESP32 bereit, Eingaben über das Keypad auszuwerten und den Temperaturzustand zu überwachen.

---

## 18. Ablauf im `loop()`

In der Hauptschleife laufen mehrere Aufgaben dauerhaft ab:

1. Temperatur prüfen und gegebenenfalls Alarm auslösen
2. Keypad-Eingaben auslesen
3. Code bei `*` prüfen
4. Eingabe bei `#` löschen
5. geänderte Boolean-Werte an Zigbee senden
6. alle fünf Sekunden einen Heartbeat senden

Dadurch kann das Keypad-Rätsel gelöst werden, während die Temperaturüberwachung parallel weiterläuft.

---

## 19. Datenfluss

Der Datenfluss des Moduls lässt sich so darstellen:

```text
Keypad / DHT22
        ↓
ESP32 liest Eingaben und Temperatur
        ↓
ESP32 wertet lokal aus
        ↓
keypadSolved / temperatureAlarm werden gesetzt
        ↓
virtuelle Zigbee-Steckdosen werden aktualisiert
        ↓
Zigbee2MQTT übersetzt die Zustände in MQTT-Nachrichten
        ↓
Flask-Server verarbeitet die Events im Spielzustand
```

Der Server muss also nicht jeden Tastendruck oder jeden Temperaturmesswert einzeln auswerten. Er erhält stattdessen nur die Zustände, die für den Spielfortschritt relevant sind.

---

## 20. Wichtige Statusvariablen

| Variable                    | Bedeutung                                                        |
| --------------------------- | ---------------------------------------------------------------- |
| `keypadSolved`              | Wird `true`, wenn der richtige Code eingegeben wurde             |
| `temperatureAlarm`          | Wird `true`, wenn die Temperatur über dem Grenzwert liegt        |
| `inputCode`                 | Speichert die aktuell eingegebenen Ziffern                       |
| `correctCode`               | Enthält den erwarteten Code                                      |
| `lastTempC`                 | Speichert die zuletzt gemessene Temperatur                       |
| `buzzerState`               | Speichert den aktuellen Zustand des Buzzers                      |
| `lastSentKeypadSolved`      | Merkt sich den zuletzt gesendeten Zustand von `keypadSolved`     |
| `lastSentTemperatureAlarm`  | Merkt sich den zuletzt gesendeten Zustand von `temperatureAlarm` |
| `firstZigbeeSend`           | Sorgt dafür, dass beim Start einmal sicher gesendet wird         |
| `lastZigbeeRoutineSendTime` | Zeitstempel für den regelmäßigen Zigbee-Heartbeat                |

---

## 21. Zusammenfassung

`ESP2_zigbee.ino` verbindet das Keypad-Rätsel und die Temperaturüberwachung in einem ESP32-Programm. Der Code prüft lokal, ob der richtige Code eingegeben wurde, misst regelmäßig die Temperatur und steuert LEDs sowie Buzzer als direktes Feedback für die Spieler.

Die Ergebnisse werden nicht als Rohdaten, sondern als Boolean-Zustände über Zigbee weitergegeben. Durch die Umsetzung als virtuelle Steckdosen können Zigbee2MQTT und der Flask-Server die Informationen einfach weiterverarbeiten. Der ESP32 #2 übernimmt damit eine klare Rolle als lokales Eingabe- und Überwachungsmodul im gesamten Exit-Game-System.
