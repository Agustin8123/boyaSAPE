#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>

Adafruit_MPU6050 mpu;

#define RELE_PIN 19
#define BOOT_BTN 0

// ======= CONFIG WiFi =======
const char* ssid = "Starlink";
const char* password = "Adrian2727";

// ======= CONFIG TELEGRAM =======
const char* botToken = "8246929061:AAHLgjUnfXi9KtNQLomTgcjQ3xlXAslGoes";
const char* chatID = "7423413933";

WebServer server(80);
bool alarmaActiva = false;
unsigned long alarmaInicio = 0;

// ======= CONTROL DE SINCRONIZACIÓN =======
SemaphoreHandle_t xMutex;

// ======= ENVÍO TELEGRAM =======
void sendTelegramMessage(const String& message) {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(botToken) +
               "/sendMessage?chat_id=" + String(chatID) + "&text=" + message;
  http.begin(url);
  http.GET();
  http.end();
}

// ======= CONEXIÓN WiFi =======
void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.printf("Conectando a %s", ssid);
  unsigned long startAttempt = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado al WiFi");
  } else {
    Serial.println("\n⚠️ No se pudo conectar al WiFi, se ejecutará sin conexión.");
  }
}

// ======= TAREA SENSOR Y RELÉ (CORE 1) =======
void TaskMPU(void *pvParameters) {
  for (;;) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Detección de movimiento
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

    // Control de relé (independiente del WiFi)
    if (alarmaActiva) {
      unsigned long tiempo = millis() - alarmaInicio;
      digitalWrite(RELE_PIN, (tiempo / 200) % 2); // parpadeo rápido

      if (tiempo >= 90000) { // 1.5 minutos
        xSemaphoreTake(xMutex, portMAX_DELAY);
        alarmaActiva = false;
        xSemaphoreGive(xMutex);
        digitalWrite(RELE_PIN, LOW);
        Serial.println("✅ Alarma finalizada");
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); // frecuencia de lectura alta
  }
}

// ======= TAREA RED Y TELEGRAM (CORE 0) =======
void TaskNet(void *pvParameters) {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }

    server.handleClient();

    // Solo enviar mensajes si la alarma está activa
    xSemaphoreTake(xMutex, portMAX_DELAY);
    bool enviar = alarmaActiva;
    xSemaphoreGive(xMutex);

    if (enviar) {
      sendTelegramMessage("🚨 Se ha detectado movimiento en el agua. Por favor ve a revisar.");
    }

    vTaskDelay(200 / portTICK_PERIOD_MS); // misma frecuencia que parpadeo
  }
}

// ======= SETUP =======
void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);
  pinMode(RELE_PIN, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  xMutex = xSemaphoreCreateMutex();

  if (!mpu.begin()) {
    Serial.println("❌ No se detectó el MPU6050");
    while (true) delay(1000);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  Serial.println("✅ MPU6050 listo");

  connectWiFi();

  server.on("/", []() {
    String html = "<html><body><h1>ESP32 con MPU6050</h1>";
    html += "<p>Estado del relé: " + String(digitalRead(RELE_PIN) ? "Activado" : "Desactivado") + "</p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  server.begin();
  Serial.println("✅ Servidor iniciado");

  // Crear las tareas en diferentes núcleos
  xTaskCreatePinnedToCore(TaskMPU, "TaskMPU", 4096, NULL, 2, NULL, 1); // CORE 1 (sensor)
  xTaskCreatePinnedToCore(TaskNet, "TaskNet", 8192, NULL, 1, NULL, 0); // CORE 0 (red)
}

void loop() {
  vTaskDelay(1000); // loop vacío
}
