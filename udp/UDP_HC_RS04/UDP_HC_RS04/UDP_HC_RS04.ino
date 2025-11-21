#include <WiFi.h>
#include <AsyncUDP.h>

#define TRIG 5
#define ECHO 18

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
  Serial.println("\nWiFi conectado");
  Serial.println(WiFi.localIP());

  if (!udp.connect(destinoIP, destinoPort)) {
    Serial.println("ERROR al conectar UDP");
  } else {
    Serial.println("UDP listo!");
  }
}

void loop() {
  float dist = medirDistancia();
  static bool objetoCerca = false;

  if (!isnan(dist) && dist < 10.0) {
    if (!objetoCerca) {
      objetoCerca = true;
      String msg = "INFRA: ALGUIEN DELANTE";
      udp.write((uint8_t*)msg.c_str(), msg.length());
      Serial.println(msg);
    }
  } else {
    if (objetoCerca) {
      objetoCerca = false;
      String msg = "INFRA: Zona despejada";
      udp.write((uint8_t*)msg.c_str(), msg.length());
      Serial.println(msg);
    }
  }

  delay(200);
}



