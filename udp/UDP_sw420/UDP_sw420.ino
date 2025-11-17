#include <WiFi.h>
#include <AsyncUDP.h>

// WiFi & UDP
const char* ssid = "";
const char* password = "";
const IPAddress serverIP(192, 168, 2, 136);
const int udpPort = 12345;
AsyncUDP udp;

// Sensor
#define pinSensor 2

// FreeRTOS
QueueHandle_t messageQueue;

// Prototipos
void WiFi_begin();
void UDP_begin();
void SensorTask(void* pvParameters);
void UDPTask(void* pvParameters);

void setup() {
  Serial.begin(115200);
  WiFi_begin();
  UDP_begin();
  pinMode(pinSensor, INPUT);

  // Crear cola para pasar mensajes
  messageQueue = xQueueCreate(5, sizeof(char[64]));
  if (messageQueue == NULL) {
    Serial.println("Error creando la cola");
    while (1);
  }

  // Crear tareas
  xTaskCreate(SensorTask, "SensorTask", 2048, NULL, 2, NULL);
  xTaskCreate(UDPTask, "UDPTask", 2048, NULL, 1, NULL);
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
  char msg[64];

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
        snprintf(msg, sizeof(msg), "⚠️ VANDALISMO DETECTADO ⚠️");
        xQueueSend(messageQueue, msg, portMAX_DELAY);
      }
    } else if (alertaActiva && millis() - ultimaActividad > tiempoAlerta) {
      alertaActiva = false;
      snprintf(msg, sizeof(msg), "Actividad normal retomada.");
      xQueueSend(messageQueue, msg, portMAX_DELAY);
    }

    vTaskDelay(pdMS_TO_TICKS(tiempoEntreLecturas));
  }
}

// Parám: pvParameters (no usado)
// Devuelve: 
// Tarea que envía mensajes UDP
void UDPTask(void* pvParameters) {
  char msg[64];
  for (;;) {
    if (xQueueReceive(messageQueue, msg, portMAX_DELAY) == pdTRUE) {
      if (udp.connected()) {
        udp.print(msg);
      }
      Serial.println(msg);
    }
  }
}

// Parám: ssid, password (globales)
// Devuelve: string
// Conecta a WiFi
void WiFi_begin() {
  delay(10);
  Serial.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi conectado!");
}

// Parám: serverIP, udpPort (globales)
// Devuelve: string
// Conecta a UDP
void UDP_begin() {
  if (udp.connect(serverIP, udpPort)) {
    Serial.println("UDP listo");
  } else {
    Serial.println("ERROR: UDP no conectado");
  }
}

void loop() {
  // FreeRTOS se encarga del control de tareas
}
