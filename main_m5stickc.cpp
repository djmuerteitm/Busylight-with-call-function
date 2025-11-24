Here is a complete, modular, and commented solution tailored for **PlatformIO** using the **M5Stick-C** (Original).

### Project Structure / Estructura del Proyecto

1.  **platformio.ini**: Configuration file.
2.  **src/main.cpp**: Source code.
3.  Rename file to "main.cpp" first.

---

### 1. platformio.ini

```ini
; COMENTARIO: Configuración de PlatformIO para M5Stick-C
[env:m5stick-c]
platform = espressif32
board = m5stick-c
framework = arduino
monitor_speed = 115200
lib_deps =
    m5stack/M5StickC @ ^0.2.5
    knolleary/PubSubClient @ ^2.8
```

---

### 2. src/main.cpp

```cpp
/**
 * Project: Door Device (Remote Doorbell) for M5Stick-C
 * Author: Embedded Systems Expert
 * Date: 2023-10-27
 */

#include <M5StickC.h>
#include <WiFi.h>
#include <PubSubClient.h>

// ==========================================
// HARDWARE CONFIGURATION / CONFIGURACIÓN DE HARDWARE
// ==========================================
#define BUZZER_PIN 26
#define LED_PIN    10 // M5StickC internal LED (Inverted logic: LOW=ON)

// Screen Colors / Colores de Pantalla
#define COLOR_FREE    GREEN
#define COLOR_BUSY    RED
#define COLOR_WAIT    ORANGE
#define COLOR_GRANTED GREEN

// ==========================================
// NETWORK CONFIGURATION / CONFIGURACIÓN DE RED
// ==========================================
const char* ssid = "[YOUR-SSID]";
const char* password = "[YOUR-WIFI-PASSWORD]";
const char* mqtt_server = "[YOUR-MQQT-BROKER-NOUSER-NOPASSWORD]";
const int   mqtt_port = 1883;
const char* topic_sub = "busylight"; //DO NOT CHANGE TOPIC
const char* topic_pub = "busylight/status"; // Optional: to publish logic

WiFiClient espClient;
PubSubClient client(espClient);

// ==========================================
// STATE MACHINE / MÁQUINA DE ESTADOS
// ==========================================
enum DoorState {
    ST_FREE,    // Libre
    ST_BUSY,    // Ocupado
    ST_RINGING, // Esperando (Ringing)
    ST_GRANTED  // Pasa (Granted)
};

DoorState currentState = ST_BUSY; // Default state / Estado por defecto
DoorState previousState = ST_BUSY;

// Timers / Temporizadores
unsigned long stateTimer = 0;
const unsigned long TIMEOUT_RINGING = 15000; // 15 seconds
const unsigned long TIMEOUT_GRANTED = 5000;  // 5 seconds
unsigned long lastBlink = 0;
bool ledState = false;

// ==========================================
// FUNCTIONS / FUNCIONES
// ==========================================

// COMENTARIO: Actualiza la pantalla LCD según el estado actual
void updateDisplay() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextDatum(MC_DATUM); // Middle Center

    switch (currentState) {
        case ST_FREE:
            M5.Lcd.fillScreen(COLOR_FREE);
            M5.Lcd.setTextColor(BLACK);
            M5.Lcd.drawString("LIBRE", 80, 40);
            break;
        case ST_BUSY:
            M5.Lcd.fillScreen(COLOR_BUSY);
            M5.Lcd.setTextColor(WHITE);
            M5.Lcd.drawString("OCUPADO", 80, 40);
            break;
        case ST_RINGING:
            M5.Lcd.fillScreen(COLOR_WAIT);
            M5.Lcd.setTextColor(WHITE);
            M5.Lcd.drawString("ESPERA", 80, 40);
            break;
        case ST_GRANTED:
            M5.Lcd.fillScreen(COLOR_GRANTED);
            M5.Lcd.setTextColor(BLACK);
            M5.Lcd.drawString("PASA", 80, 40);
            break;
    }
}

// COMENTARIO: Control del Buzzer (PWM)
void toneOut(int frequency, int duration) {
    ledcSetup(0, 2000, 8); // Channel 0, 2kHz, 8bit
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWriteTone(0, frequency);
    delay(duration);
    ledcWriteTone(0, 0);
}

// COMENTARIO: Callback cuando llega un mensaje MQTT
void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // Debug serial
    Serial.print("MQTT Received: ");
    Serial.println(message);

    // COMENTARIO: Lógica de recepción MQTT
    if (message == "PASA") {
        // Go GRANTED (Green, Sound, Fast Blink)
        currentState = ST_GRANTED;
        stateTimer = millis();
        
        // Sound Notification
        toneOut(1000, 200); 
        delay(100);
        toneOut(2000, 200);
        
        updateDisplay();
    }
    else if (message == "FREE") {
        currentState = ST_FREE;
        updateDisplay();
    }
    else if (message == "BUSY") {
        currentState = ST_BUSY;
        updateDisplay();
    }
}

// COMENTARIO: Conexión WiFi
void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(ssid);
    M5.Lcd.print("WiFi...");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("Connected");
    M5.Lcd.println("OK");
}

// COMENTARIO: Conexión MQTT con reintentos
void reconnect() {
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Client ID (Random)
        String clientId = "M5StickC-Door-" + String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("connected");
            client.subscribe(topic_sub);
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
            delay(5000);
        }
    }
}

// ==========================================
// SETUP
// ==========================================
void setup() {
    // COMENTARIO: Inicialización hardware M5Stick
    M5.begin();
    M5.Lcd.setRotation(1); // Landscape 160x80
    
    // COMENTARIO: Configuración GPIO
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, HIGH); // OFF (High is OFF for built-in LED)

    // Setup WiFi & MQTT
    setupWiFi();
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    // Initial State
    currentState = ST_BUSY;
    updateDisplay();
}

// ==========================================
// LOOP
// ==========================================
void loop() {
    M5.update(); // COMENTARIO: Leer botones

    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    unsigned long currentMillis = millis();

    // ----------------------------------------
    // LOGIC: BUTTON A / LÓGICA: BOTÓN A
    // ----------------------------------------
    if (M5.BtnA.wasPressed()) {
        if (currentState == ST_BUSY) {
            // Logic: If BUSY -> Publish "RING", Go RINGING, 15s Timeout
            client.publish(topic_sub, "RING");
            currentState = ST_RINGING;
            stateTimer = currentMillis;
            updateDisplay();
            toneOut(800, 100); // Feedback beep
        } 
        else if (currentState == ST_FREE) {
            // Logic: If FREE -> Local Only "PASA", Blink 5s. NO Publish.
            currentState = ST_GRANTED;
            stateTimer = currentMillis;
            updateDisplay();
        }
    }

    // ----------------------------------------
    // LOGIC: TIMEOUTS & LED BLINKING
    // ----------------------------------------
    
    // Handle GRANTED State (Blink & Timeout)
    if (currentState == ST_GRANTED) {
        // Blink LED fast
        if (currentMillis - lastBlink > 200) {
            lastBlink = currentMillis;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState ? LOW : HIGH);
        }

        // Timeout 5s -> Return to BUSY
        if (currentMillis - stateTimer > TIMEOUT_GRANTED) {
            currentState = ST_BUSY; 
            digitalWrite(LED_PIN, HIGH); // LED OFF
            updateDisplay();
        }
    }

    // Handle RINGING State (Timeout)
    else if (currentState == ST_RINGING) {
         // Timeout 15s -> Return to BUSY (or previous state)
         if (currentMillis - stateTimer > TIMEOUT_RINGING) {
             currentState = ST_BUSY;
             updateDisplay();
         }
    }
    else {
        // Ensure LED is off in other states
        digitalWrite(LED_PIN, HIGH);
    }
}
```

### Summary of Logic / Resumen de Lógica

1.  **Initialization**: Starts in `ST_BUSY`. Connects to WiFi/MQTT.
2.  **Button A (Input)**:
    *   If **BUSY**: Publishes "RING" to MQTT topic `busylight`. Screen turns Orange ("ESPERA"). Starts 15s timer.
    *   If **FREE**: Changes screen to Green ("PASA"). Blinks LED. **Does not publish**. Starts 5s timer.
3.  **MQTT (Input)**:
    *   Receives "PASA": Triggers Sound, Green Screen, and LED Blinking for 5 seconds. Then reverts to `ST_BUSY`.
    *   Receives "FREE" / "BUSY": Updates the base state.
4.  **Timers**: Non-blocking `millis()` checks handle the return to default states automatically.
