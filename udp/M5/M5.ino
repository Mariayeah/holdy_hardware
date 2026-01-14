#include <M5Stack.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

// ---------------- CONFIG WIFI ----------------
const char* ssid = "Pixel_8214";
const char* password = "9aedsxm5zkite3u";

// ---------------- CONFIG MQTT ----------------
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqtt_server = "10.191.253.232";  // Raspberry / Broker Mosquitto
const int mqtt_port = 1883;

// ---------------- CONFIG UDP -----------------
AsyncUDP udp;
const int udpPort = 12345;

// ---------------- VARIABLES ------------------
String msgRFID = "Esperando...";
String msgINFRA = "Esperando...";

const int pinSensor = 36;  // sensor vibración (ojo: en ESP32 GPIO36 es input only)

// Flag global para evento vibración y evento capture
volatile bool eventoVibracion = false;
bool alertaActiva = false;
unsigned long ultimaActividad = 0;
volatile bool eventoCapture = false;

// Variables Globales para que mensaje de "Foto Hecha!" desaparezca con el tiempo
unsigned long fotoMsgUntil = 0;
const unsigned long FOTO_MSG_MS = 3000;


// ---------------- NTP / TIMEZONE -------------
// Zona horaria España (Madrid) con cambio horario
// CET-1 / CEST-2: regla EU (último domingo marzo +1h, último domingo octubre -1h)
const char* tzInfo = "CET-1CEST,M3.5.0/2,M10.5.0/3";
const char* ntp1 = "pool.ntp.org";
const char* ntp2 = "time.nist.gov";

bool timeReady = false;

// ------------------------------------------------------------
// ---------------------- PANTALLA -----------------------------
// ------------------------------------------------------------
void actualizarPantalla(const String& estadoVibra) {
  M5.Lcd.clear();
  M5.Lcd.setTextSize(2);

  /*M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("---- SENSORES HOLDY ----");

  M5.Lcd.setCursor(0, 40);
  M5.Lcd.println("[RFID]  " + msgRFID);

  M5.Lcd.setCursor(0, 80);
  M5.Lcd.println("[VIBRA] " + estadoVibra);

  M5.Lcd.setCursor(0, 120);
  M5.Lcd.println("[INFRA] " + msgINFRA);*/

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("--------- HOLA! ---------");

  M5.Lcd.setCursor(0, 40);
  M5.Lcd.println("Pulsa el boton de en medio para pedir abrir el buzon \n\n AVISO: \n\n Se te tomara una foto \n para saber tu identidad");
}

// ------------------------------------------------------------
// ---------------------- UDP CALLBACK -------------------------
// ------------------------------------------------------------
void onUDP(AsyncUDPPacket packet) {
  String message = String((char*)packet.data(), packet.length());

  if (message.startsWith("RFID:"))
    msgRFID = message.substring(5);
  else if (message.startsWith("INFRA:"))
    msgINFRA = message.substring(6);

  actualizarPantalla("Esperando...");
}

// ------------------------------------------------------------
// ---------------------- WIFI INIT ----------------------------
// ------------------------------------------------------------
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  M5.Lcd.clear();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Conectando a WiFi...");

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    if (millis() - start > 15000) {
      M5.Lcd.println("WiFi timeout, reintento...");
      start = millis();
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }

  M5.Lcd.println("WiFi OK: " + WiFi.localIP().toString());
  IPAddress localIP = WiFi.localIP();
  Serial.println("IP local: ");
  Serial.print(localIP);
}

// ------------------------------------------------------------
// ---------------------- TIME (NTP) ---------------------------
// ------------------------------------------------------------
void initTime() {
  // Configura zona horaria España + NTP
  setenv("TZ", tzInfo, 1);
  tzset();
  configTime(0, 0, ntp1, ntp2);

  // Espera a que la hora sea válida
  M5.Lcd.println("Sincronizando NTP...");
  for (int i = 0; i < 30; i++) {
    time_t now = time(nullptr);
    if (now > 1700000000) {  // umbral: ~2023-11
      timeReady = true;
      M5.Lcd.println("Hora OK (NTP)");
      return;
    }
    delay(300);
  }
  timeReady = false;
  M5.Lcd.println("NTP no listo (seguimos igual)");
}

// ------------------------------------------------------------
// --------------------- MQTT RECONNECT ------------------------
// ------------------------------------------------------------
String makeClientId() {
  // ID único basado en MAC (evita expulsiones por ID duplicado)
  uint64_t mac = ESP.getEfuseMac();
  char buf[40];
  snprintf(buf, sizeof(buf), "M5Stack_Holdy_%08X%08X",
           (uint32_t)(mac >> 32), (uint32_t)mac);
  return String(buf);
}

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    if (WiFi.status() != WL_CONNECTED) {
      conectarWiFi();
      initTime();
    }

    String clientId = makeClientId();
    Serial.print("Conectando MQTT con ID: ");
    Serial.println(clientId);

    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT OK");
      M5.Lcd.println("MQTT conectado!");
    } else {
      Serial.printf("Fallo MQTT (%d). Reintentando...\n", mqttClient.state());
      delay(2000);
    }
  }
}

// ------------------------------------------------------------
// -------------------- SENSOR TASK ----------------------------
// Detecta sacudidas
// ------------------------------------------------------------
void SensorTask(void* pvParameters) {
  const int ventana = 10;
  const int umbral = 3;
  const unsigned long tiempoLectura = 50;
  const unsigned long tiempoAlerta = 2000;  // ms de duración de estado de alerta

  int lecturas[ventana] = { 0 };
  int idx = 0;
  int suma = 0;
  unsigned long ultimaActividad = 0;

  for (;;) {
    int lectura = digitalRead(pinSensor);

    suma -= lecturas[idx];
    lecturas[idx] = lectura;
    suma += lectura;
    idx = (idx + 1) % ventana;

    // ---------- EVALUAR VANDALISMO ----------
    if (suma >= umbral) {
      ultimaActividad = millis();

      if (!alertaActiva) {
        alertaActiva = true;
        eventoVibracion = true;   // dispara MQTT UNA sola vez
        Serial.println("¡POSIBLE VANDALISMO DETECTADO!");
        actualizarPantalla("¡POSIBLE VANDALISMO DETECTADO!");
      }
    }
    else if (alertaActiva && millis() - ultimaActividad > tiempoAlerta) {
      alertaActiva = false;
      Serial.println("Actividad normal retomada.");
      actualizarPantalla("Actividad normal retomada.");
    }
    // ---------------------------------------

    vTaskDelay(pdMS_TO_TICKS(tiempoLectura));
  }
}

// ------------------------------------------------------------
// -------------------- BUTTON TASK ----------------------------
// Botón central (BtnB) -> eventoCapture
// ------------------------------------------------------------
void ButtonTask(void* pvParameters) {
  unsigned long lastPress = 0;
  const unsigned long debounceMs = 800;

  for (;;) {
    M5.update();

    if (M5.BtnB.wasPressed()) {
      unsigned long now = millis();
      if (now - lastPress > debounceMs) {
        lastPress = now;
        eventoCapture = true;  // lo publicará el task MQTT
        Serial.println("Boton B: captura solicitada");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

// ------------------------------------------------------------
// ----------------- MQTT PUBLISH TASK ------------------------
// Publica OBJETO JSON al detectar sacudida
// ------------------------------------------------------------
void MqttPublishTask(void* pvParameters) {
  for (;;) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();

    // ------------ Evento vibración --------------

    if (eventoVibracion) {
      eventoVibracion = false;

      StaticJsonDocument<256> doc;
      doc["buzonId"] = "buzon_Pepe";
      doc["mensaje"] = "Sacudidas detectadas";

      // timestamp ISO (solo si NTP está listo)
      if (timeReady) {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char buffer[100];  // Asegúrate de tener suficiente espacio para la cadena

        // Formato: 15 de diciembre de 2025 a las 6:42:11 p. m. UTC+1
        strftime(buffer, sizeof(buffer), "%d de %B de %Y a las %H:%M:%S %p UTC%z", &timeinfo);
        doc["timestamp"] = buffer;  // timestamp en el formato legible
      } else {
        doc["timestamp"] = "NTP_NOT_READY";
      }

      char payload[256];
      size_t n = serializeJson(doc, payload, sizeof(payload));
      if (n == 0) {
        Serial.println("Error serializando JSON (buffer pequeño)");
      } else {
        // Publica con longitud (evita problemas de strings no terminadas)
        bool ok = mqttClient.publish("holdy/vibra", (uint8_t*)payload, n, true);
        Serial.println(ok ? "Evento vibración enviado MQTT" : "ERROR publish MQTT");
      }
    }

    // ------------ Evento Captura Foto --------------

    if (eventoCapture) {
      eventoCapture = false;

      StaticJsonDocument<256> doc;
      doc["type"] = "capture";
      doc["cmd"] = "capture";
      doc["src"] = "m5stack";
      doc["deviceId"] = "m5-01";

      // timestamp para trazabilidad (no para nombre de archivo)
      if (timeReady) {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
        doc["timestamp"] = buffer;
      } else {
        doc["timestamp"] = "NTP_NOT_READY";
      }

      char payload[256];
      size_t n = serializeJson(doc, payload, sizeof(payload));

      if (n > 0) {
        // Retained = false (no quieres que un cliente “nuevo” dispare capturas antiguas)
        bool ok = mqttClient.publish("holdy/capture", (uint8_t*)payload, n, false);
        Serial.println(ok ? "Capture -> MQTT OK" : "Capture -> MQTT ERROR");

        M5.Lcd.setCursor(0, 200);
        M5.Lcd.println(ok ? "       Foto Hecha!" : "ERROR enviando captura");
        fotoMsgUntil = millis() + FOTO_MSG_MS;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    if (fotoMsgUntil != 0 && (long)(millis() - fotoMsgUntil) >= 0)
    {
      M5.Lcd.fillRect(0, 200, 320, 24, BLACK);
      fotoMsgUntil = 0;
    }
  }
}

// ------------------------------------------------------------
// ------------------------ SETUP ------------------------------
// ------------------------------------------------------------
void setup() {
  M5.begin();
  Serial.begin(115200);

  pinMode(pinSensor, INPUT);

  conectarWiFi();

  mqttClient.setServer(mqtt_server, mqtt_port);
  reconnectMQTT();

  initTime();

  if (udp.listen(udpPort)) {
    udp.onPacket(onUDP);
    Serial.println("UDP escuchando");
  }

  xTaskCreate(SensorTask, "SensorTask", 4096, NULL, 2, NULL);
  xTaskCreate(ButtonTask, "ButtonTask", 4096, NULL, 2, NULL);
  xTaskCreate(MqttPublishTask, "MqttPublishTask", 4096, NULL, 1, NULL);

  actualizarPantalla("Esperando...");
}

// ------------------------------------------------------------
// -------------------------- LOOP -----------------------------
// ------------------------------------------------------------
void loop() {
  // FreeRTOS tasks gestionan todo
  vTaskDelay(pdMS_TO_TICKS(1000));
}
