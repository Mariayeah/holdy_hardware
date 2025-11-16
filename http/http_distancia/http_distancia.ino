#include <WiFi.h>
#include <WebServer.h>

#define TRIG 4
#define ECHO 5

const char* ssid = "telefono";
const char* password = "datosdatos";

WebServer server(80);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado. IP: ");
  Serial.println(WiFi.localIP());

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  // Ruta principal -> devuelve la distancia
  server.on("/", []() {
    long dur;
    digitalWrite(TRIG, LOW); delayMicroseconds(2);
    digitalWrite(TRIG, HIGH); delayMicroseconds(10);
    digitalWrite(TRIG, LOW);
    dur = pulseIn(ECHO, HIGH, 30000);
    float dist = (dur == 0) ? NAN : dur * 0.034 / 2.0;
    String mensaje = isnan(dist) ? "sin_eco" : String(dist, 1);
    server.send(200, "text/plain", mensaje);
  });

  server.begin();
  Serial.println("Servidor HTTP iniciado");
}

void loop() {
  server.handleClient();
}