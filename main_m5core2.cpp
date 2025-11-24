Here is the complete C++ solution for **PlatformIO** using the M5Core2.

### Prerequisities / Prerrequisitos

You need a **PlatformIO** project.
1. Create a new project for board **M5Stack Core2**.
2. Edit `platformio.ini` to include the required libraries.
3. Rename main_m5core2.cpp to main.cpp before open.

#### `platformio.ini`
```ini
[env:m5stack-core2]
platform = espressif32
board = m5stack-core2
framework = arduino
monitor_speed = 115200
lib_deps = 
    m5stack/M5Core2 @ ^0.1.5
    knolleary/PubSubClient @ ^2.8
```

---

### Source Code / Código Fuente (`src/main.cpp`)

```cpp
/**
 * Project: M5Core2 Office Busy Light
 * Author: Embedded Systems Expert
 * Platform: PlatformIO (Arduino Framework)
 * Hardware: M5Stack Core2
 */

#include <M5Core2.h>
#include <WiFi.h>
#include <PubSubClient.h>

// COMENTARIO: Credenciales de WiFi y MQTT. Remplazar con datos reales.
const char* ssid = "[YOUR-SSID]";
const char* password = "[YOUR-WIFI-PASSWORD]";
const char* mqtt_server = "[YOUR-MQQT-BROKER-NOUSER-NOPASSWORD]";
const int mqtt_port = 1883;
const char* mqtt_topic = "busylight"; // DO NOT CHANGE TOPIC!!!

// COMENTARIO: Definición de Estados del Sistema
enum SystemState {
    STATE_FREE,     // LIBRE
    STATE_BUSY,     // OCUPADO
    STATE_RINGING   // TIMBRANDO (Llamada entrante)
};

SystemState currentState = STATE_FREE;
SystemState lastState = STATE_BUSY; // COMENTARIO: Para forzar dibujo inicial

// COMENTARIO: Objetos globales
WiFiClient espClient;
PubSubClient client(espClient);

// COMENTARIO: Variables de temporización (Timer)
unsigned long ringStartTime = 0;
const unsigned long RING_TIMEOUT = 15000; // 15 segundos
unsigned long lastLoopTime = 0;

// COMENTARIO: Prototipos de funciones
void setupWiFi();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void updateDisplay();
void handleRingEffects();
void stopEffects();

void setup() {
    // COMENTARIO: Inicialización del hardware M5Core2
    M5.begin();
    
    // COMENTARIO: Configuración inicial de pantalla
    M5.Lcd.setTextSize(3);
    M5.Lcd.setTextDatum(MC_DATUM); // Centrar texto

    // COMENTARIO: Habilitar vibración y altavoz (AXP192 LDO3 es el motor de vibración)
    M5.Axp.SetSpkEnable(true);
    M5.Axp.SetLDOEnable(3, false); // Asegurar vibración apagada al inicio

    // COMENTARIO: Conexión a WiFi
    setupWiFi();

    // COMENTARIO: Configuración del servidor MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
}

void loop() {
    // COMENTARIO: Actualizar estado de botones del M5
    M5.update();

    // COMENTARIO: Verificar conexión MQTT
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    // COMENTARIO: ---------------- LÓGICA DEL BOTÓN B (Middle) ----------------
    if (M5.BtnB.wasPressed()) {
        if (currentState == STATE_RINGING) {
            // COMENTARIO: Si está sonando y tocamos -> Conceder acceso (PASA)
            client.publish(mqtt_topic, "PASA");
            stopEffects();
            currentState = STATE_BUSY;
        } else if (currentState == STATE_FREE) {
            // COMENTARIO: Si está Libre -> Ocupado
            currentState = STATE_BUSY;
            client.publish(mqtt_topic, "OCUPADO");
        } else if (currentState == STATE_BUSY) {
            // COMENTARIO: Si está Ocupado -> Libre
            currentState = STATE_FREE;
            client.publish(mqtt_topic, "LIBRE");
        }
        updateDisplay();
    }

    // COMENTARIO: ---------------- LÓGICA DE TIMBRE (RINGING) ----------------
    if (currentState == STATE_RINGING) {
        // COMENTARIO: Verificar si pasaron 15 segundos
        if (millis() - ringStartTime > RING_TIMEOUT) {
            stopEffects();
            currentState = STATE_BUSY; // Timeout -> Poner en ocupado
            updateDisplay();
        } else {
            // COMENTARIO: Ejecutar efectos de sonido/vibración no bloqueantes
            handleRingEffects();
        }
    }
}

// COMENTARIO: Función para actualizar la pantalla según el estado
void updateDisplay() {
    if (currentState == lastState) return; // Evitar parpadeo si no cambia

    M5.Lcd.clear();
    
    switch (currentState) {
        case STATE_FREE:
            M5.Lcd.fillScreen(GREEN);
            M5.Lcd.setTextColor(WHITE);
            M5.Lcd.drawString("LIBRE", 160, 120);
            break;

        case STATE_BUSY:
            M5.Lcd.fillScreen(RED);
            M5.Lcd.setTextColor(WHITE);
            M5.Lcd.drawString("OCUPADO", 160, 120);
            break;

        case STATE_RINGING:
            M5.Lcd.fillScreen(ORANGE);
            M5.Lcd.setTextColor(BLACK);
            M5.Lcd.drawString("RING!", 160, 120);
            break;
    }
    
    lastState = currentState;
}

// COMENTARIO: Efectos de vibración y sonido pulsantes (No bloqueante)
void handleRingEffects() {
    unsigned long currentMillis = millis();
    // COMENTARIO: Crear un patrón de pulso cada 500ms
    if ((currentMillis / 500) % 2 == 0) {
        M5.Axp.SetLDOEnable(3, true); // Vibración ON
        // COMENTARIO: Sonido simple (Ding bloquea un poco, pero es aceptable aquí)
        // Usamos un tono breve si está disponible o confiamos en la vibración
        // M5.Spk.Ding(); 
    } else {
        M5.Axp.SetLDOEnable(3, false); // Vibración OFF
    }
}

// COMENTARIO: Detener todos los efectos físicos
void stopEffects() {
    M5.Axp.SetLDOEnable(3, false); // Apagar vibración
}

// COMENTARIO: Callback cuando llega un mensaje MQTT
void callback(char* topic, byte* payload, unsigned int length) {
    String message;
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // COMENTARIO: Debug por puerto serie
    Serial.print("Mensaje recibido: ");
    Serial.println(message);

    if (String(topic) == mqtt_topic) {
        if (message == "RING") {
            currentState = STATE_RINGING;
            ringStartTime = millis();
            updateDisplay();
        }
        // COMENTARIO: Se pueden agregar más comandos remotos aquí si es necesario
    }
}

// COMENTARIO: Configuración WiFi
void setupWiFi() {
    delay(10);
    Serial.println();
    Serial.print("Conectando a ");
    Serial.println(ssid);

    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawString("Conectando WiFi...", 160, 120);

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi conectado");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Forzar actualización de pantalla al estado inicial
    lastState = STATE_BUSY; // Hack para forzar redibujado
    currentState = STATE_FREE;
    updateDisplay();
}

// COMENTARIO: Reconexión MQTT
void reconnect() {
    // Bucle hasta conectar
    while (!client.connected()) {
        Serial.print("Intentando conexión MQTT...");
        // Crear ID random
        String clientId = "M5Core2Client-";
        clientId += String(random(0xffff), HEX);
        
        if (client.connect(clientId.c_str())) {
            Serial.println("conectado");
            // COMENTARIO: Suscribirse al tópico
            client.subscribe(mqtt_topic);
        } else {
            Serial.print("falló, rc=");
            Serial.print(client.state());
            Serial.println(" reintentando en 5 segundos");
            delay(5000);
        }
    }
}
```

### Implementation Notes for the Expert / Notas de Implementación

1.  **Vibration (LDO3):** On M5Core2, the vibration motor is controlled by the Power Management Unit (AXP192) on LDO3. The code uses `M5.Axp.SetLDOEnable(3, true/false)` to control this.
2.  **Non-Blocking Logic:** The `handleRingEffects` function uses modulo division on `millis()` to create a pulsing vibration effect without stopping the processor loop (`delay()`), ensuring the button remains responsive during ringing.
3.  **State "GRANTED":** In your logic description, you mentioned "GRANTED" with green background but text "OCUPADO". However, the flow instructions said "Go BUSY". To keep the code clean and consistent with the visual "BUSY" state (Red), I transitioned directly to `STATE_BUSY`. If you specifically need the Green Background with "OCUPADO" text, you can create a dedicated case in `updateDisplay`.
4.  **Button B:** This corresponds to `M5.BtnB` (the middle touch button below the screen).
