# Firmware-Dokumentation – ESP32 #1: Potentiometer- und LDR-Modul

## 1. Übersicht

Diese Firmware steuert den ersten ESP32 des Flight Simulator Exit Games. Der Mikrocontroller übernimmt zwei physische Rätselmodule:

* das Frequenz-Rätsel mit drei Potentiometern
* das Laser-/LDR-Rätsel mit einem Fotosensor

Beide Rätsel werden lokal auf dem ESP32 ausgewertet. Der zentrale Server erhält also nicht die einzelnen Rohwerte der Sensoren, sondern nur die fertigen Zustände, ob ein Rätsel gelöst wurde oder nicht. Dadurch bleibt die Spiellogik aufgeteilt: Der ESP32 verarbeitet die direkte Hardware-Interaktion, während der Server den übergeordneten Spielablauf steuert.

Die Kommunikation zum Raspberry Pi erfolgt über Zigbee. Damit die Boolean-Werte einfach in Zigbee2MQTT und anschließend im Flask-Server verarbeitet werden können, werden sie als virtuelle Zigbee-Steckdosen umgesetzt.

---

## 2. Rolle im Gesamtsystem

Der ESP32 #1 ist für den Abschnitt zuständig, in dem die Spieler technische Systeme des Flugzeugs wieder stabilisieren müssen. Im Spielkontext stehen die beiden Rätsel für:

| Rätsel            | Bedeutung im Spiel                      | Technische Umsetzung                                               |
| ----------------- | --------------------------------------- | ------------------------------------------------------------------ |
| Frequenz-Rätsel   | Funkgerät / Antenne korrekt kalibrieren | Drei Potentiometer müssen in zufällige Zielbereiche gedreht werden |
| Laser-/LDR-Rätsel | Optischen Datenstrom wiederherstellen   | Ein Laser muss auf einen LDR-Sensor ausgerichtet werden            |

Der ESP32 arbeitet dabei als Edge-Gerät. Er entscheidet selbst, ob die Sensoreingaben korrekt sind, und sendet nur die daraus abgeleiteten Boolean-Zustände weiter.

---

## 3. Zigbee-Endpunkte

Am Anfang des Codes werden zwei Zigbee-Endpunkte definiert:

```cpp
#define ZB_ENDPOINT_PUZZLE_SOLVED 1
#define ZB_ENDPOINT_LDR_SOLVED    2
```

Jeder Endpunkt wird als `ZigbeePowerOutlet` angelegt:

```cpp
ZigbeePowerOutlet zbPuzzleSolved = ZigbeePowerOutlet(ZB_ENDPOINT_PUZZLE_SOLVED);
ZigbeePowerOutlet zbLdrSolved    = ZigbeePowerOutlet(ZB_ENDPOINT_LDR_SOLVED);
```

Die Idee dahinter ist, einfache Boolean-Werte als virtuelle Steckdosen darzustellen:

| Boolean        | Zigbee-Endpunkt | Bedeutung                             |
| -------------- | --------------- | ------------------------------------- |
| `puzzleSolved` | EP 1            | Frequenz-/Potentiometer-Rätsel gelöst |
| `ldrSolved`    | EP 2            | Laser-/LDR-Rätsel gelöst              |

Dabei gilt:

```text
false = Steckdose AUS
true  = Steckdose AN
```

Dieser Ansatz ist bewusst einfach gehalten. Zigbee2MQTT kann Steckdosen-Zustände direkt als `ON` oder `OFF` interpretieren, wodurch die Flask-App später keine komplexen Sensordaten auswerten muss.

---

## 4. Hardware-Pinbelegung

Die Firmware nutzt drei analoge Eingänge für die Potentiometer und vier LEDs für das visuelle Feedback.

```cpp
const int SENSOR_PINS[3] = {4, 5, 0};
const int LED_PINS[3]    = {20, 21, 22};
```

Zusätzlich wird ein LDR-Sensor verwendet:

```cpp
const int LDR_PIN = 6;
const int LDR_LED_PIN = 23;
```

Die Zuordnung ist damit:

| Komponente          | Pin     |
| ------------------- | ------- |
| Potentiometer 1     | GPIO 4  |
| Potentiometer 2     | GPIO 5  |
| Potentiometer 3     | GPIO 0  |
| LED Potentiometer 1 | GPIO 20 |
| LED Potentiometer 2 | GPIO 21 |
| LED Potentiometer 3 | GPIO 22 |
| LDR Sensor          | GPIO 6  |
| LDR Status-LED      | GPIO 23 |

Die Potentiometer und der LDR werden über `analogRead()` ausgelesen. Der ESP32 verwendet dabei ADC-Werte im Bereich von 0 bis 4095.

---

## 5. Frequenz-Rätsel mit Potentiometern

Das Frequenz-Rätsel basiert auf drei zufällig erzeugten Zielbereichen. Für jeden der drei Sensoren wird ein eigener Bereich generiert:

```cpp
targetMin[i] = random(ADC_MIN, ADC_MAX - TARGET_WIDTH);
targetMax[i] = targetMin[i] + TARGET_WIDTH;
```

Die Breite eines Zielbereichs beträgt:

```cpp
const int TARGET_WIDTH = 200;
```

Damit hat jedes Potentiometer einen gültigen Bereich von 200 ADC-Werten. Die Zielbereiche werden zu Beginn einer Runde erzeugt und nach Ablauf der Rundenzeit neu gesetzt.

Die Prüfung erfolgt über die Funktion:

```cpp
bool inTarget(int i, int value) {
  return value >= targetMin[i] && value <= targetMax[i];
}
```

Im Spielbetrieb wird jeder Sensorwert ausgelesen und mit seinem Zielbereich verglichen. Liegt der Wert im Zielbereich, leuchtet die zugehörige LED. Liegt er außerhalb, bleibt die LED aus.

```cpp
if (inTarget(i, value)) {
  digitalWrite(LED_PINS[i], HIGH);
} else {
  digitalWrite(LED_PINS[i], LOW);
  allCorrect = false;
}
```

Erst wenn alle drei Potentiometer gleichzeitig im jeweiligen Zielbereich liegen, wird das Frequenz-Rätsel als gelöst betrachtet:

```cpp
puzzleSolved = allCorrect;
```

---

## 6. Erfolgsmeldung und Spielzustände

Für das Potentiometer-Rätsel wird eine kleine State Machine verwendet. Sie besteht aus drei Zuständen:

```cpp
enum GameState {
  PLAYING,
  SUCCESS_BLINK,
  SUCCESS_HOLD
};
```

### PLAYING

In diesem Zustand läuft das eigentliche Rätsel. Die Potentiometer werden regelmäßig ausgelesen, die LEDs zeigen den aktuellen Status an und nach 30 Sekunden werden neue Zielbereiche erzeugt.

```cpp
const unsigned long ROUND_TIME_MS = 30000;
```

### SUCCESS_BLINK

Sobald alle drei Potentiometer korrekt eingestellt sind, wechseln die LEDs in eine gemeinsame Blinkphase. Das erzeugt ein sichtbares Erfolgsfeedback für die Spieler.

```cpp
const unsigned long BLINK_TIME_MS = 5000;
```

Während dieser Phase blinken alle drei Potentiometer-LEDs gemeinsam.

### SUCCESS_HOLD

Nach der Blinkphase bleiben alle drei LEDs dauerhaft eingeschaltet. Dieser Zustand hält für eine längere Zeit an, bevor automatisch eine neue Runde gestartet wird.

```cpp
const unsigned long HOLD_TIME_MS = 1200000;
```

Dadurch bleibt das gelöste Rätsel für das Spielsystem stabil sichtbar und wird nicht sofort zurückgesetzt.

---

## 7. Laser-/LDR-Rätsel

Das zweite Rätsel nutzt einen LDR-Sensor. Dieser erkennt, ob ausreichend Licht auf den Fotosensor trifft. Im Spiel entspricht das dem erfolgreichen Ausrichten des Lasers über die Spiegel.

Der LDR wird in der Funktion `updateLdr()` ausgelesen:

```cpp
int ldrValue = analogRead(LDR_PIN);
```

Der gemessene Wert wird mit einem festen Schwellwert verglichen:

```cpp
const int LDR_THRESHOLD = 3750;
```

Sobald der LDR-Wert diesen Schwellwert erreicht oder überschreitet, wird das Rätsel als gelöst markiert:

```cpp
if (ldrValue >= LDR_THRESHOLD) {
  ldrSolved = true;
}
```

Die zugehörige LED zeigt den Status direkt an:

```cpp
if (ldrSolved) {
  digitalWrite(LDR_LED_PIN, HIGH);
} else {
  digitalWrite(LDR_LED_PIN, LOW);
}
```

Auffällig ist, dass `ldrSolved` nach dem Erreichen des Schwellwerts nicht wieder automatisch auf `false` gesetzt wird. Das ist für den Spielablauf sinnvoll, weil das Laser-Rätsel nach erfolgreicher Lösung dauerhaft als abgeschlossen gelten soll.

---

## 8. Zigbee-Initialisierung

Die Zigbee-Kommunikation wird in der Funktion `setupZigbee()` vorbereitet.

Zuerst erhalten die beiden virtuellen Geräte Hersteller- und Modellnamen:

```cpp
zbPuzzleSolved.setManufacturerAndModel("EscapeGame", "PuzzleSolved");
zbLdrSolved.setManufacturerAndModel("EscapeGame", "LdrSolved");
```

Danach werden die beiden PowerOutlet-Endpunkte registriert:

```cpp
Zigbee.addEndpoint(&zbPuzzleSolved);
Zigbee.addEndpoint(&zbLdrSolved);
```

Anschließend startet der ESP32 als Zigbee-Router:

```cpp
Zigbee.begin(ZIGBEE_ROUTER);
```

Der Code wartet so lange, bis eine Verbindung zum Zigbee-Netzwerk besteht:

```cpp
while (!Zigbee.connected()) {
  Serial.print(".");
  delay(100);
}
```

Erst danach beginnt die eigentliche Spiellogik. Dadurch wird verhindert, dass Zustände erzeugt werden, bevor der Mikrocontroller im Zigbee-Netzwerk erreichbar ist.

---

## 9. Senden der Boolean-Werte

Die Firmware unterscheidet zwischen zwei Arten des Sendens.

### 9.1 Eventbasiertes Senden

Die Funktion `sendBooleanOutletsToZigbee()` prüft zunächst, ob sich einer der beiden Boolean-Werte geändert hat:

```cpp
bool changed =
  firstZigbeeSend ||
  puzzleSolved != lastSentPuzzleSolved ||
  ldrSolved != lastSentLdrSolved;
```

Nur wenn sich etwas geändert hat, wird der neue Zustand gesendet. Dadurch wird unnötiger Zigbee-Verkehr vermieden.

### 9.2 Regelmäßiger Heartbeat

Zusätzlich wird alle fünf Sekunden der aktuelle Zustand erneut gesendet:

```cpp
const unsigned long ZIGBEE_ROUTINE_SEND_INTERVAL_MS = 5000;
```

Dieser Heartbeat sorgt dafür, dass der Server und Zigbee2MQTT den aktuellen Zustand regelmäßig erneut erhalten, selbst wenn keine Änderung stattgefunden hat. Das macht das System robuster, da verlorene Nachrichten oder spätere Reconnects besser abgefangen werden können.

Das eigentliche Setzen der Zigbee-Steckdosen erfolgt in:

```cpp
zbPuzzleSolved.setState(puzzleSolved);
zbLdrSolved.setState(ldrSolved);
```

---

## 10. Ablauf im `setup()`

Im `setup()` werden alle Komponenten vorbereitet:

1. Serielle Ausgabe starten
2. Zufallsgenerator initialisieren
3. LED-Pins als Ausgänge konfigurieren
4. LDR-LED vorbereiten
5. Zigbee starten
6. erste Zielbereiche erzeugen
7. Anfangszustand an Zigbee senden
8. Heartbeat-Timer starten

Der ESP32 ist danach vollständig initialisiert und kann im Spielbetrieb sowohl Sensordaten auswerten als auch seine Zustände an den Raspberry Pi melden.

---

## 11. Ablauf im `loop()`

Die Hauptschleife des Programms besteht aus vier Aufgaben:

1. LDR-Wert lesen und `ldrSolved` aktualisieren
2. Potentiometer-Rätsel abhängig vom aktuellen Spielzustand verarbeiten
3. bei Änderungen sofort Zigbee-Zustände senden
4. zusätzlich alle fünf Sekunden den aktuellen Zustand erneut senden

Die Struktur ist bewusst einfach gehalten. Die Hardware wird direkt im Loop ausgewertet, während alle größeren Entscheidungen über den Spielfortschritt später vom Server getroffen werden.

---

## 12. Datenfluss

Der Datenfluss des Moduls lässt sich folgendermaßen zusammenfassen:

```text
Potentiometer / LDR
        ↓
ESP32 liest Sensorwerte
        ↓
ESP32 wertet lokal aus
        ↓
puzzleSolved / ldrSolved werden gesetzt
        ↓
virtuelle Zigbee-Steckdosen werden aktualisiert
        ↓
Zigbee2MQTT übersetzt die Zustände in MQTT-Nachrichten
        ↓
Flask-Server verarbeitet die Events im Spielzustand
```

Damit sendet der ESP32 keine Rohdaten wie einzelne ADC-Werte an den Server. Stattdessen werden nur die spielrelevanten Ergebnisse übertragen. Das reduziert die Komplexität im Netzwerk und hält die Verantwortlichkeiten klar getrennt.

---

## 13. Technische Einordnung

Die Firmware folgt dem Grundprinzip der gesamten Systemarchitektur: Hardware-nahe Logik wird lokal auf dem ESP32 verarbeitet, während der zentrale Server den globalen Spielzustand verwaltet.

Der ESP32 #1 ist damit nicht nur ein einfacher Sensor, sondern ein kleines eigenständiges Rätselmodul. Er erkennt selbstständig, ob das Frequenz-Rätsel oder das Laser-Rätsel gelöst wurde, gibt den Spielern direktes LED-Feedback und meldet den finalen Zustand über Zigbee an das restliche System.

Besonders wichtig ist dabei die Abbildung der Boolean-Werte als Zigbee-Steckdosen. Diese Lösung nutzt bestehende Smart-Home-Profile, anstatt ein eigenes Kommunikationsprotokoll zu entwickeln. Dadurch können Zigbee2MQTT und der Flask-Server die Zustände einfach weiterverarbeiten.

---

## 14. Relevante Statusvariablen

| Variable                    | Bedeutung                                                              |
| --------------------------- | ---------------------------------------------------------------------- |
| `puzzleSolved`              | Wird `true`, wenn alle drei Potentiometer im Zielbereich liegen        |
| `ldrSolved`                 | Wird `true`, wenn der LDR-Sensor ausreichend Licht erkennt             |
| `blinkState`                | Speichert den aktuellen Blinkzustand während der Erfolgsanimation      |
| `currentState`              | Speichert den aktuellen Zustand der Potentiometer-State-Machine        |
| `lastSentPuzzleSolved`      | Merkt sich den zuletzt an Zigbee gesendeten Zustand von `puzzleSolved` |
| `lastSentLdrSolved`         | Merkt sich den zuletzt an Zigbee gesendeten Zustand von `ldrSolved`    |
| `firstZigbeeSend`           | Erzwingt das erste Senden nach dem Start                               |
| `lastZigbeeRoutineSendTime` | Zeitstempel für den regelmäßigen Zigbee-Heartbeat                      |

---

## 15. Zusammenfassung

`Sensor_zigbee.ino` verbindet zwei physische Rätsel des Exit Games in einer Firmware: das Potentiometer-basierte Frequenz-Rätsel und das Laser-/LDR-Rätsel. Beide Aufgaben werden lokal auf dem ESP32 ausgewertet. Die Spieler erhalten direktes Feedback über LEDs, während der Server nur die fertigen Boolean-Zustände erhält.

Durch die Umsetzung als virtuelle Zigbee-Steckdosen passt sich die Firmware sauber in die restliche Systemarchitektur ein. Der Raspberry Pi kann die Zustände über Zigbee2MQTT und MQTT empfangen, ohne die internen ADC-Werte oder die lokale Rätselberechnung kennen zu müssen.
