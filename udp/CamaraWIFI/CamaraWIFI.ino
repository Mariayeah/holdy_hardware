#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>

// --------- WIFI ----------
const char* WIFI_SSID = "TU_WIFI";
const char* WIFI_PASS = "TU_PASSWORD";

// IP de la Raspberry + endpoint del servidor Flask
// Cambia la IP por la de tu Raspberry en la red
const char* RPI_BUTTON_URL = "http://192.168.1.50:5000/button";

// Id opcional del dispositivo (por si tienes varios M5)
const char* DEVICE_ID = "m5-buzon-entrada";

void setup() {
  M5.begin();
  Serial.begin(115200);

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Conectando WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado");
  Serial.print("IP M5: ");
  Serial.println(WiFi.localIP());

  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.println("Listo");
  M5.Lcd.println("Pulsa boton A");
  M5.Lcd.println("para avisar");
}

// Enviar POST a la Raspberry para que haga UNA foto
void enviarAvisoBoton() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi no conectado, no envio aviso");
    return;
  }

  HTTPClient http;
  http.begin(RPI_BUTTON_URL);
  http.addHeader("Content-Type", "application/json");

  // JSON simple con el id del dispositivo
  String body = "{ \"deviceId\": \"" + String(DEVICE_ID) + "\" }";

  int code = http.POST(body);
  Serial.printf("POST /button -> HTTP %d\n", code);

  http.end();

  // Feedback en pantalla
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.println("Aviso enviado");
  M5.Lcd.println("Esperando foto...");
  delay(1000);

  // Volvemos al mensaje normal
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(10, 20);
  M5.Lcd.println("Listo");
  M5.Lcd.println("Pulsa boton A");
}

void loop() {
  M5.update();

  // Detectar pulso (transicion) del boton A
  if (M5.BtnA.wasPressed()) {
    Serial.println("Boton A pulsado -> aviso a Raspberry");
    enviarAvisoBoton();
  }

  delay(20);
}
