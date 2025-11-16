#include <M5Stack.h>
#include <WiFi.h>
#include <HTTPClient.h>

const char* ssid = "telefono";
const char* password = "datosdatos";
const char* serverName = "http://10.96.43.222/";  // Cambia por la IP del ESP32

void setup() {
  M5.begin();
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Conectando WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    M5.Lcd.print(".");
  }
  M5.Lcd.println("\nConectado!");
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    int code = http.GET();
    if (code == 200) {
      String payload = http.getString();
      M5.Lcd.clear();
      M5.Lcd.setCursor(0, 0);
      M5.Lcd.println("Distancia:");
      M5.Lcd.setTextSize(3);
      M5.Lcd.println(payload + " cm");
    } else {
      M5.Lcd.println("Error HTTP");
    }
    http.end();
  } else {
    M5.Lcd.println("WiFi desconectado");
  }
  delay(1000);
}