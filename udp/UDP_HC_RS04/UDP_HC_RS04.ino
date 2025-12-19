#include <Arduino.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#define TRIG 5
#define ECHO 16

const char* ssid = "Pixel_8214";
const char* password = "9aedsxm5zkite3u";

IPAddress destinoIP(10, 119, 185, 208);
const int destinoPort = 12345;

AsyncUDP udp;
bool objetoCerca = false;

// Prototipos
float medirDistancia();
void tareaSensor(void* pvParameters);
void tareaWiFi(void* pvParameters);

float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long dur = pulseIn(ECHO, HIGH, 30000);
  
  // Si no se recibe un eco, devolvemos NAN
  if (dur == 0) return NAN;

  // Calcular la distancia a partir de la duración del pulso
  return dur * 0.034 / 2.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Solo imprimimos un mensaje inicial breve
  Serial.println("Iniciando medición de distancia...");

  // Tarea WiFi (prioridad alta)
  xTaskCreatePinnedToCore(
    tareaWiFi,
    "Tarea WiFi",
    4096,
    NULL,
    2,
    NULL,
    0
  );

  // Tarea Sensor (prioridad media)
  xTaskCreatePinnedToCore(
    tareaSensor,
    "Tarea Sensor",
    4096,
    NULL,
    1,
    NULL,
    1
  );
}

void loop() {
  // El loop ya no se usa. FreeRTOS gestiona las tareas.
  vTaskDelay(portMAX_DELAY);
}

// Tarea que gestiona la conexión WiFi y prepara UDP
void tareaWiFi(void* pvParameters) {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  
  // Solo mostramos la IP cuando nos conectamos exitosamente
  Serial.println(WiFi.localIP());

  if (!udp.connect(destinoIP, destinoPort)) {
    Serial.println("ERROR al conectar UDP");
  }

  // Esta tarea podría quedarse dormida indefinidamente o supervisar la conexión
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
    }
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

// Tarea que mide la distancia y envía mensajes UDP
void tareaSensor(void* pvParameters) {
  for (;;) {
    float dist = medirDistancia();

    // Si la distancia es válida y el objeto está cerca (menos de 10 cm)
    if (!isnan(dist) && dist < 10.0) {
      if (!objetoCerca) {
        objetoCerca = true;
        String msg = "INFRA: ALGUIEN DELANTE";
        udp.write((uint8_t*)msg.c_str(), msg.length());
      }
    } else {
      if (objetoCerca) {
        objetoCerca = false;
        String msg = "INFRA: Zona despejada";
        udp.write((uint8_t*)msg.c_str(), msg.length());
      }
    }

    // Imprimir solo la distancia medida y un punto para indicar que está funcionando
    Serial.print("Distancia: ");
    Serial.print(dist);
    Serial.print(" cm. ");
    Serial.println(".");
    
    vTaskDelay(pdMS_TO_TICKS(200)); // Espera 200 ms
  }
}
