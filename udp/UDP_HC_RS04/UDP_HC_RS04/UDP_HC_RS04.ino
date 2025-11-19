#include <WiFi.h>
#include <AsyncUDP.h>

#define TRIG 12
#define ECHO 15

const char* ssid = "Pixel_8214";
const char* password = "9aedsxm5zkite3u";

IPAddress destinoIP(10, 197, 27, 208);
const int destinoPort = 12345;

AsyncUDP udp;

float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long dur = pulseIn(ECHO, HIGH, 30000);
  if (dur == 0) return NAN;

  return dur * 0.034 / 2.0;
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  WiFi.begin(ssid, password);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Conectado. IP local:");
  Serial.println(WiFi.localIP());

  udp.connect(destinoIP, destinoPort);
  Serial.println("UDP conectado al servidor destino.");
}

void loop() {
  float dist = medirDistancia();
  String mensaje = isnan(dist) ? "sin_eco" : String(dist, 1);

  // Enviar la distancia a la IP destino
  udp.print(mensaje);

  Serial.print("Enviado a ");
  Serial.print(destinoIP);
  Serial.print(": ");
  Serial.println(mensaje);

  delay(500);  // envía dos veces por segundo
}

