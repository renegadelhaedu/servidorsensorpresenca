/*
 * ESP8266 ESP-12E: sensor infravermelho em GPIO12 e buzzer em GPIO14.
 * O ESP também disponibiliza uma página simples na porta 80.
 *
 * Configure WIFI_SSID, WIFI_PASSWORD e FLASK_SERVER_IP antes de gravar.
 * O computador/servidor Flask precisa estar acessível na mesma rede Wi-Fi.
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

const char *WIFI_SSID = "NOME_DA_REDE";
const char *WIFI_PASSWORD = "SENHA_DA_REDE";
const char *FLASK_SERVER_IP = "192.168.1.100"; // IP do computador que executa o Flask
const uint16_t FLASK_SERVER_PORT = 5000;
const char *SENSOR_ID = "sensor-entrada";

constexpr uint8_t SENSOR_PIN = 12; // GPIO12 (D6 em placas NodeMCU)
constexpr uint8_t BUZZER_PIN = 14; // GPIO14 (D5 em placas NodeMCU)
constexpr uint8_t SENSOR_ATIVO = HIGH; // Altere para LOW se o módulo detectar em nível baixo
constexpr unsigned long DEBOUNCE_MS = 50;
constexpr unsigned long RECONNECT_INTERVAL_MS = 10000;
constexpr unsigned long BEEP_INTERVAL_MS = 200;
constexpr uint8_t BEEPS_PER_DETECTION = 5;

ESP8266WebServer webServer(80);

int sensorState = LOW;
int lastSensorReading = LOW;
unsigned long lastSensorChangeMs = 0;
unsigned long lastReconnectAttemptMs = 0;
unsigned long nextBeepMs = 0;
uint8_t beepsRemaining = 0;
bool buzzerOn = false;

void handleRoot() {
  String html = F("<!doctype html><html lang='pt-BR'><meta charset='utf-8'>"
                  "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>Sensor de presença</title><body><h1>Sensor de presença</h1>");
  html += F("<p>Wi-Fi: ");
  if (WiFi.status() == WL_CONNECTED) html += F("conectado");
  else html += F("desconectado");
  html += F("</p><p>IP do ESP8266: ");
  if (WiFi.status() == WL_CONNECTED) html += WiFi.localIP().toString();
  else html += F("indisponível");
  html += F("</p><p>Sensor: ");
  if (sensorState == SENSOR_ATIVO) html += F("presença detectada");
  else html += F("sem presença");
  html += F("</p></body></html>");
  webServer.send(200, "text/html; charset=utf-8", html);
}

void handleStatus() {
  String json = F("{\"wifi\":\"");
  if (WiFi.status() == WL_CONNECTED) json += F("conectado");
  else json += F("desconectado");
  json += F("\",\"presenca\":");
  if (sensorState == SENSOR_ATIVO) json += F("true");
  else json += F("false");
  json += F("}");
  webServer.send(200, "application/json", json);
}

void connectToWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("Conectando ao Wi-Fi"));
  const unsigned long startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < 20000) {
    delay(250);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("IP do ESP8266: "));
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(F("Wi-Fi indisponível; novas tentativas serão feitas."));
  }
}

void registerPresence() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("Presença detectada, mas sem Wi-Fi; registro não enviado."));
    return;
  }

  WiFiClient client;
  HTTPClient http;
  String url = F("http://");
  url += FLASK_SERVER_IP;
  url += ':';
  url += FLASK_SERVER_PORT;
  url += F("/presenca?sensor_id=");
  url += SENSOR_ID;

  http.setTimeout(3000);
  if (!http.begin(client, url)) {
    Serial.println(F("Não foi possível iniciar a requisição HTTP."));
    return;
  }
  const int responseCode = http.GET();
  Serial.printf("GET %s -> HTTP %d\n", url.c_str(), responseCode);
  if (responseCode > 0) {
    Serial.println(http.getString());
  }
  http.end();
}

void startBuzzerSequence() {
  beepsRemaining = BEEPS_PER_DETECTION;
  buzzerOn = true;
  digitalWrite(BUZZER_PIN, HIGH);
  nextBeepMs = millis() + BEEP_INTERVAL_MS;
}

void updateBuzzer() {
  if (beepsRemaining == 0 || millis() < nextBeepMs) return;

  if (buzzerOn) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerOn = false;
    --beepsRemaining;
  } else {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerOn = true;
  }
  nextBeepMs = millis() + BEEP_INTERVAL_MS;
}

void updateSensor() {
  const int reading = digitalRead(SENSOR_PIN);
  if (reading != lastSensorReading) {
    lastSensorReading = reading;
    lastSensorChangeMs = millis();
  }

  if (reading != sensorState && millis() - lastSensorChangeMs >= DEBOUNCE_MS) {
    sensorState = reading;
    if (sensorState == SENSOR_ATIVO) {
      Serial.println(F("Presença detectada."));
      registerPresence();
      startBuzzerSequence();
    }
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  sensorState = digitalRead(SENSOR_PIN);
  lastSensorReading = sensorState;
  lastSensorChangeMs = millis();

  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/status", HTTP_GET, handleStatus);
  webServer.onNotFound([]() { webServer.send(404, "text/plain", "Nao encontrado"); });
  webServer.begin();
  Serial.println(F("Servidor web iniciado na porta 80."));

  connectToWiFi();
  lastReconnectAttemptMs = millis();
}

void loop() {
  webServer.handleClient();
  updateSensor();
  updateBuzzer();

  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastReconnectAttemptMs >= RECONNECT_INTERVAL_MS) {
    lastReconnectAttemptMs = millis();
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}
