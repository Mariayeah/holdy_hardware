#include <M5Stack.h>
#include <WiFi.h>
#include <AsyncUDP.h>

const char *ssid = "";
const char *password = "";

const int udpPort = 12345;

AsyncUDP udp;

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
      Serial.print("Mensaje UDP recibido: ");
      Serial.println(message);

      M5.Lcd.clear();
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println(message);
    });
    // Espera breve para no bloquear la tarea si no hay paquetes
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

void setup() {
  // Inicialización del hardware M5Stack y el monitor serie
  M5.begin();
  Serial.begin(115200);

  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Conectando a WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    M5.Lcd.print(".");
    delay(500);
  }

  // Conexión establecida
  Serial.println("\nWiFi conectado!");
  M5.Lcd.println("WiFi conectado!");
  IPAddress localIP = WiFi.localIP();
  Serial.println("IP local: ");
  Serial.print(localIP); //La uso para la conexion y enviar paquetes.

  // Crear las tareas con pilas y prioridades
  xTaskCreate(WiFiTask, "WiFiTask", 2048, NULL, 1, NULL);
  xTaskCreate(UDPTask, "UDPTask", 2048, NULL, 2, NULL);
}


void loop() {
  // No se necesita código en loop; FreeRTOS gestiona las tareas
}