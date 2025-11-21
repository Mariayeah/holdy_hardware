#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <AsyncUDP.h>


#define SS_PIN 17
#define RST_PIN 22

const char* ssid = "Pixel_8214";
const char* password = "9aedsxm5zkite3u";
const IPAddress serverIP(10, 197, 27, 208);  //IP del m5stack
const int udpPort = 12345;

MFRC522 mfrc522(SS_PIN, RST_PIN);
byte authorizedUID[4] = { 0x6D, 0x05, 0x62, 0xD3 };
AsyncUDP udp;

// La cola pasa el mensaje de la tarea del sensor a la tarea UDP
QueueHandle_t messageQueue;

void sendToM5(String msg) {
  udp.write((uint8_t*)msg.c_str(), msg.length());
  Serial.println("Enviado al M5: " + msg);
}


void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  WiFi_begin();
  UDP_begin();
  delay(2000); //Espera para que inicie todo

  messageQueue = xQueueCreate(5, sizeof(char[64]));

  // Crear las tareas con pilas y prioridades
  xTaskCreate(WiFiTask, "WiFiTask", 2048, NULL, 1, NULL);
  xTaskCreate(RFIDTask, "RFIDTask", 4096, NULL, 2, NULL);
  xTaskCreate(UDPTask, "UDPTask", 2048, NULL, 3, NULL);
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

  // Conexión establecida
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

// Parám: parameter (no lo usa)
// Devuelve: no devuelve valor
// Mantiene la conexión WiFi
void WiFiTask(void *parameter) {
  //Bucle infinito para comprobar WiFi todo el tiempo
  for (;;) {
    // Si el WiFi no está conectado, intentar reconectar
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Reinicio WiFi");
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
// Lee sensor
void RFIDTask(void *parameter) {
  for (;;) {
    byte res = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);

    if (!(res == 0x92 || res == 0x91)) {
      Serial.println("Error en lector RFID");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      continue;
    }

    //Esperar tarjeta
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    // Verificar UID
    bool authorized = true;
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      if (mfrc522.uid.uidByte[i] != authorizedUID[i]) {
        authorized = false;
        break;
      }
    }

    // Mensaje final
    String message;

    if (authorized) {
      message = "RFID: Tarjeta autorizada";
      sendToM5(message);
    } else {
      message = "RFID: Tarjeta NO autorizada";
      sendToM5(message);
    }

    // Mandar mensaje a la cola
    xQueueSend(messageQueue, message.c_str(), portMAX_DELAY);

    mfrc522.PICC_HaltA();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
  }
}



// Parám: parameter (no lo usa)
// Devuelve: no devuelve valor
// Mantiene la conexión WiFi
void UDPTask(void *parameter) {
  char rxMsg[64];

  for (;;) {
    if (xQueueReceive(messageQueue, &rxMsg, portMAX_DELAY)) {
      Serial.print("Intentando enviar por UDP el mensaje: ");
      Serial.println(rxMsg);

      // Enviar mensaje por UDP
      uint8_t msg[strlen(rxMsg) + 1];
      memcpy(msg, rxMsg, strlen(rxMsg) + 1);

      if (!udp.write(msg, strlen(rxMsg))) {
        Serial.println("ERROR: Falla al enviar UDP");
      } else {
        Serial.println("Mensaje UDP enviado correctamente");
      }
    }
  }

  vTaskDelay(50 / portTICK_PERIOD_MS);
}

void loop() {
  // No se necesita código en loop; FreeRTOS gestiona las tareas
}
