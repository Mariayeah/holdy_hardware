#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <AsyncUDP.h>

#define SS_PIN 17
#define RST_PIN 22

const char *ssid = "";
const char *password = "";
const IPAddress serverIP(192, 168, 2, 136);  //IP del m5stack
const int udpPort = 12345;

MFRC522 mfrc522(SS_PIN, RST_PIN);
byte authorizedUID[4] = { 0x6D, 0x05, 0x62, 0xD3 };
AsyncUDP udp;

// La cola pasa el mensaje de la tarea del sensor a la tarea UDP
QueueHandle_t messageQueue;

// Parám: parameter (no lo usa)
// Devuelve: no devuelve valor
// Mantiene la conexión WiFi
void WiFiTask(void *parameter) {
  //Bucle infinito para comprobar WiFi todo el tiempo
  for (;;) {
    // Si el WiFi no está conectado, intentar reconectar
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
// Mantiene la conexión WiFi
void RFIDTask(void *parameter) {
  for (;;) {
    byte res = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);


    if (!(res == 0x92 || res == 0x91)) {
      Serial.println("Error en lector RFID")
        vTaskDelay(2000 / portTICK_PERIOD_MS);
      continue;
    }

    //Esperar tarjeta
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      continue;
    }

    String message;
    if (mfrc522.uid.size == 4 && memcmp(mfrc522.uid.uidByte, authorizedUID, 4) == 0) {
      message = "Repartidor autorizado. Proceda con la entrega.";
    } else {
      message = "UID no autorizado. Acceso denegado.";
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

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  delay(2000);

  //Conectar WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado!");

  //Conectar UDP
  if (udp.connect(serverIP, udpPort)) {
    Serial.println("UDP listo");
  } else {
    Serial.println("ERROR: UDP no conectado");
  }

  messageQueue = xQueueCreate(5, sizeof(char[64]));

  // Crear las tareas con pilas y prioridades
  xTaskCreate(WiFiTask, "WiFiTask", 2048, NULL, 1, NULL);
  xTaskCreate(RFIDTask, "RFIDTask", 4096, NULL, 2, NULL);
  xTaskCreate(UDPTask, "UDPTask", 2048, NULL, 3, NULL);
}

void loop() {
  // No se necesita código en loop; FreeRTOS gestiona las tareas
}