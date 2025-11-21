#include <M5Stack.h>
#include <WiFi.h>
#include <AsyncUDP.h>
#include <PubSubClient.h> 

const char* ssid = "Pixel_8214";
const char* password = "9aedsxm5zkite3u";

const int udpPort = 12345;

String msgRFID = "Esperando...";
String msgVIBRA = "Esperando...";
String msgINFRA = "Esperando...";

AsyncUDP udp;

// MQTT
WiFiClient espClient;
PubSubClient mqttClient(espClient);
const char* mqtt_server = "10.197.27.208"; // Raspberry Pi

// --> Cola para enviar mensajes del sensor a la pantalla/serial
QueueHandle_t messageQueue;

// --> Pin del sensor
const int pinSensor = 36;

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Conectando MQTT...");
    if (mqttClient.connect("M5Stack_Holdy")) {
      Serial.println("OK");
    } else {
      Serial.print("Fallo MQTT, rc=");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

void setup() {
  // Inicialización del M5Stack, monitor serie y WiFi
  M5.begin();
  Serial.begin(115200);
  pinMode(pinSensor, INPUT); // <-- necesario para el sensor
  WiFiBegin();
  delay(2000); //Espera para que inicie todo

  // MQTT
  mqttClient.setServer(mqtt_server, 1883);
  reconnectMQTT();

  // Crear cola
  messageQueue = xQueueCreate(5, sizeof(char[64]));

  // Crear las tareas con pilas y prioridades
  xTaskCreate(WiFiTask, "WiFiTask", 2048, NULL, 1, NULL);
  xTaskCreate(UDPTask, "UDPTask", 2048, NULL, 2, NULL);
  xTaskCreate(SensorTask, "SensorTask", 4096, NULL, 3, NULL);
}

// Parám: ssid, password (globales)
// Devuelve:
// Conecta a WiFi
void WiFiBegin() {
    delay(10);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    M5.Lcd.print(".");
    delay(500);
  }

  // Conexión establecida
  Serial.println("\nWiFi conectado!");
  M5.Lcd.clear();
  M5.Lcd.println("WiFi conectado!");
  IPAddress localIP = WiFi.localIP();
  Serial.println("IP local: ");
  Serial.print(localIP); //La uso para la conexion y enviar paquetes.
}

// Parám: parameter (no lo usa)
// Devuelve: no devuelve valor
// Mantiene la conexión WiFi
void WiFiTask(void *parameter) {
  //Bucle infinito para comprobar WiFi todo el tiempo
  for (;;) {
    // Si la WiFi no está conectada, intentar reconectar
    if (WiFi.status() != WL_CONNECTED) {
      M5.Lcd.clear();
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.println("Reinicio WiFi");
      WiFi.disconnect();
      WiFi.reconnect();
      vTaskDelay(2000 / portTICK_PERIOD_MS);  // Esperar 2 segundos antes de reintentar
    }
    // Espera de 1 segundo para actualizar el estado si es necesario
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Parám: parameter (no lo usa)
// Devuelve: no devuelve valor
// Inicia el servidor UDP y muestra los mensajes recibidos
void UDPTask(void *parameter) {
  // Bucle infinito: la tarea está a la espera de paquetes UDP
  for (;;) {

    // Inicializar el servidor UDP en el puerto definido
    if (!udp.listen(udpPort)) {
      Serial.println("No se pudo iniciar el servidor UDP.");
      vTaskDelete(NULL);  // Terminar la tarea si no se puede iniciar UDP
    }

    // Callback cuando llega un paquete UDP
    udp.onPacket([](AsyncUDPPacket packet) {

      // Convertir el dato recibido a un String
      String message = String((char *)packet.data(), packet.length());
      Serial.println("[UDP] " + message);

      // Clasificar mensaje por tipo
      if (message.startsWith("RFID:")) {
        msgRFID = message.substring(6);
      } 
      else if (message.startsWith("VIBRA:")) {
        msgVIBRA = message.substring(7);
      } 
      else if (message.startsWith("INFRA:")) {
        msgINFRA = message.substring(7);
      }

      // Actualizar la pantalla del M5
      actualizarPantalla();
    });

    // Espera breve para no bloquear la tarea si no hay paquetes
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// Parám: pvParameters (no usado)
// Devuelve:
// Tarea que lee el sensor y genera mensajes
void SensorTask(void* pvParameters) {
  const int ventana = 10;
  const int umbralActivaciones = 3;
  const unsigned long tiempoAlerta = 2000;
  const unsigned long tiempoEntreLecturas = 50;

  static int lecturas[ventana] = {0};
  int indice = 0;
  int suma = 0;
  bool alertaActiva = false;
  unsigned long ultimaActividad = 0;

  for (;;) {
    int lectura = digitalRead(pinSensor);

    // Promedio móvil
    suma -= lecturas[indice];
    lecturas[indice] = lectura;
    suma += lectura;
    indice = (indice + 1) % ventana;

    // Detección
    if (suma >= umbralActivaciones) {
      ultimaActividad = millis();
      if (!alertaActiva) {
        alertaActiva = true;
        msgVIBRA = "VANDALISMO DETECTADO";
        actualizarPantalla();
        Serial.println("⚠️ VANDALISMO DETECTADO ⚠️");
      }

    } else if (alertaActiva && millis() - ultimaActividad > tiempoAlerta) {
      alertaActiva = false;
      msgVIBRA = "Actividad normal";
      actualizarPantalla();
      Serial.println("Actividad normal retomada.");
    }

    vTaskDelay(pdMS_TO_TICKS(tiempoEntreLecturas));
  }
}

void actualizarPantalla() {
  M5.Lcd.clear();
  M5.Lcd.setTextSize(2);

  M5.Lcd.setCursor(0, 0);
  M5.Lcd.println("──── SENSORES HOLDY ────");

  M5.Lcd.setCursor(0, 40);
  M5.Lcd.println("[RFID]  " + msgRFID);

  M5.Lcd.setCursor(0, 80);
  M5.Lcd.println("[VIBRA] " + msgVIBRA);

  M5.Lcd.setCursor(0, 120);
  M5.Lcd.println("[INFRA] " + msgINFRA);

  // MQTT publish (AÑADIDO, SIN TOCAR TUS COMENTARIOS)
  if (!mqttClient.connected()) reconnectMQTT();
  mqttClient.publish("holdy/rfid", msgRFID.c_str());
  mqttClient.publish("holdy/vibra", msgVIBRA.c_str());
  mqttClient.publish("holdy/infra", msgINFRA.c_str());
}


void loop() {
  // No se necesita código en loop; FreeRTOS gestiona las tareas
  mqttClient.loop();  // MQTT necesita ejecutarse aquí
}

