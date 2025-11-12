#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <FS.h>
#include <SPIFFS.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#define RELE_PIN 19

Adafruit_MPU6050 mpu;
WebServer server(80);

bool alarmaActiva = false;
unsigned long alarmaInicio = 0;
SemaphoreHandle_t xMutex;

String ssid = "";
String password = "";
String chatID = "";

bool modoConfig = false;

// ======= CONFIGURACIÓN FIJA =======
const char* botToken = "8246929061:AAHLgjUnfXi9KtNQLomTgcjQ3xlXAslGoes";

// ======= Guardar datos en SPIFFS =======
void guardarConfig() {
  File file = SPIFFS.open("/config.txt", FILE_WRITE);
  if (!file) return;
  file.printf("%s\n%s\n%s\n", ssid.c_str(), password.c_str(), chatID.c_str());
  file.close();
}

// ======= Cargar datos guardados =======
bool cargarConfig() {
  if (!SPIFFS.exists("/config.txt")) return false;
  File file = SPIFFS.open("/config.txt", FILE_READ);
  if (!file) return false;
  ssid = file.readStringUntil('\n'); ssid.trim();
  password = file.readStringUntil('\n'); password.trim();
  chatID = file.readStringUntil('\n'); chatID.trim();
  file.close();
  return true;
}

// ======= Borrar configuración =======
void borrarConfig() {
  SPIFFS.remove("/config.txt");
  Serial.println("❌ Configuración eliminada, reiniciando...");
  delay(1000);
  ESP.restart();
}

// ======= Página de configuración (modo AP) =======
void iniciarModoConfig() {
  modoConfig = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Wilson_Config", "12345678");
  Serial.println("📶 Modo configuración: Conéctate a 'Wilson_Config' (clave: 12345678)");

  server.on("/", HTTP_GET, []() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                  "<title>Configuración Wilson</title>"
                  "<style>"
                  "body { font-family: Arial; background: #121212; color: #eee; text-align: center; }"
                  "h2 { color: #00ffc8; }"
                  "form { background: #1e1e1e; padding: 20px; border-radius: 10px; display: inline-block; margin-top: 30px; }"
                  "input { margin: 8px; padding: 10px; border-radius: 5px; border: 1px solid #444; width: 250px; }"
                  "input[type='submit'] { background: #00ffc8; color: #000; font-weight: bold; cursor: pointer; width: 150px; }"
                  "input[type='submit']:hover { background: #00b38f; }"
                  "</style></head><body>"
                  "<h2>Configuración de Wi-Fi y Telegram</h2>"
                  "<form method='POST' action='/guardar'>"
                  "<input name='ssid' placeholder='Nombre de WiFi'><br>"
                  "<input name='password' type='password' placeholder='Contraseña WiFi'><br>"
                  "<input name='chat' placeholder='Chat ID de Telegram'><br><br>"
                  "<input type='submit' value='Guardar'>"
                  "</form></body></html>";
    server.send(200, "text/html", html);
  });

  server.on("/guardar", HTTP_POST, []() {
    ssid = server.arg("ssid");
    password = server.arg("password");
    chatID = server.arg("chat");
    guardarConfig();
    server.send(200, "text/html", "<meta http-equiv='refresh' content='2;url=/'><h3 style='font-family:Arial;text-align:center;'>✅ Configuración guardada.<br>Reiniciando...</h3>");
    delay(2000);
    ESP.restart();
  });

  server.begin();
}

// ======= Conexión WiFi normal =======
void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("Conectando a %s", ssid.c_str());
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado al WiFi");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n⚠️ No se pudo conectar, entrando en modo configuración.");
    iniciarModoConfig();
  }
}

// ======= Envío Telegram =======
void sendTelegramMessage(const String &msg) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(botToken) +
               "/sendMessage?chat_id=" + chatID + "&text=" + msg;
  http.begin(url);
  http.GET();
  http.end();
}

// ======= Tarea sensor (CORE 1) =======
void TaskMPU(void *pvParams) {
  for (;;) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);

    if (abs(a.acceleration.x) > 10 || abs(a.acceleration.y) > 10 || abs(a.acceleration.z) > 10 ||
        abs(g.gyro.x) > 1 || abs(g.gyro.y) > 1 || abs(g.gyro.z) > 10) {

      if (!alarmaActiva) {
        xSemaphoreTake(xMutex, portMAX_DELAY);
        alarmaActiva = true;
        alarmaInicio = millis();
        xSemaphoreGive(xMutex);
        Serial.println("🚨 Movimiento detectado, alarma activada");
      }
    }

    if (alarmaActiva) {
      unsigned long tiempo = millis() - alarmaInicio;
      digitalWrite(RELE_PIN, (tiempo / 200) % 2);
      if (tiempo >= 90000) {
        xSemaphoreTake(xMutex, portMAX_DELAY);
        alarmaActiva = false;
        xSemaphoreGive(xMutex);
        digitalWrite(RELE_PIN, LOW);
        Serial.println("✅ Alarma finalizada");
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

// ======= Tarea red y Telegram (CORE 0) =======
void TaskNet(void *pvParams) {
  for (;;) {
    if (modoConfig) {
      server.handleClient();
    } else {
      if (WiFi.status() != WL_CONNECTED) conectarWiFi();
      server.handleClient();
      xSemaphoreTake(xMutex, portMAX_DELAY);
      bool activa = alarmaActiva;
      xSemaphoreGive(xMutex);
      if (activa) sendTelegramMessage("🚨 Movimiento detectado en el agua");
    }
    vTaskDelay(200 / portTICK_PERIOD_MS);
  }
}

// ======= SETUP =======
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  pinMode(RELE_PIN, OUTPUT);
  SPIFFS.begin(true);
  xMutex = xSemaphoreCreateMutex();

  if (!cargarConfig()) {
    iniciarModoConfig();
  } else {
    conectarWiFi();
  }

  if (!mpu.begin()) {
    Serial.println("❌ No se detectó el MPU6050");
    while (true) delay(1000);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("✅ MPU listo");

  // Servidor info (si no estás en modo config)
  server.on("/", []() {
    String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Wilson</title>"
                  "<style>body{font-family:Arial;background:#121212;color:#eee;text-align:center;}"
                  "h1{color:#00ffc8;}p{font-size:18px;}</style></head><body>"
                  "<h1>🛰️ Wilson activo</h1>"
                  "<p>Relé: " + String(digitalRead(RELE_PIN) ? "ON" : "OFF") + "</p>"
                  "<p>IP: " + WiFi.localIP().toString() + "</p>"
                  "</body></html>";
    server.send(200, "text/html", html);
  });
  server.begin();

  // Tareas
  xTaskCreatePinnedToCore(TaskMPU, "TaskMPU", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskNet, "TaskNet", 8192, NULL, 1, NULL, 0);
}

// ======= LOOP =======
void loop() {
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    comando.trim();
    if (comando.equalsIgnoreCase("eliminar")) {
      borrarConfig();
    }
  }
  vTaskDelay(100);
}
