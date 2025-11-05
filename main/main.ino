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
const char* password = "adrian2727";

// ======= CONFIG TELEGRAM =======
const char* botToken = "8246929061:AAHLgjUnfXi9KtNQLomTgcjQ3xlXAslGoes";
const char* chatID = "7423413933";

WebServer server(80);
bool alarmaActiva = false;
unsigned long alarmaInicio = 0;

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
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\n✅ Conectado al WiFi");
}

// ======= TAREA MPU (CORE 1) =======
void TaskMPU(void *pvParameters) {
  for (;;) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    if (abs(a.acceleration.x) > 10 || abs(a.acceleration.y) > 10 || abs(a.acceleration.z) > 10 ||
        abs(g.gyro.x) > 1 || abs(g.gyro.y) > 1 || abs(g.gyro.z) > 10) {
      if (!alarmaActiva) {
        alarmaActiva = true;
        alarmaInicio = millis();
        Serial.println("🚨 Movimiento detectado, alarma activada");
      }
    }

    vTaskDelay(50 / portTICK_PERIOD_MS); // alta frecuencia de lectura
  }
}

// ======= SETUP =======
void setup() {
  Wire.begin(21, 22);
  Serial.begin(115200);
  pinMode(RELE_PIN, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  connectWiFi();

  if (!mpu.begin()) {
    Serial.println("❌ No se detectó el MPU6050");
    while (true) delay(1000);
  }

  Serial.println("✅ MPU6050 listo");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  xTaskCreatePinnedToCore(TaskMPU, "TaskMPU", 4096, NULL, 1, NULL, 1);

  server.on("/", []() {
    String html = "<html><body><h1>ESP32 con MPU6050</h1>";
    html += "<p>Estado del relé: " + String(digitalRead(RELE_PIN) ? "Activado" : "Desactivado") + "</p>";
    html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
    html += "</body></html>";
    server.send(200, "text/html", html);
  });
  server.begin();
  Serial.println("✅ Servidor iniciado");
}

// ======= LOOP PRINCIPAL (CORE 0) =======
void loop() {
  server.handleClient();

  // Si la alarma está activa, hacer parpadear el relé y mandar spam
  if (alarmaActiva) {
    unsigned long tiempo = millis() - alarmaInicio;

    // alternar el estado del relé sin delay
    digitalWrite(RELE_PIN, (tiempo / 200) % 2);  // cambia cada 200 ms

    // mandar mensaje cada 200 ms, misma frecuencia que el parpadeo
    sendTelegramMessage("🚨 Se ha detectado movimiento en el agua. Por favor ve a revisar.");

    if (tiempo >= 90000) { // 1 minuto y medio
      alarmaActiva = false;
      digitalWrite(RELE_PIN, LOW);
      Serial.println("✅ Alarma finalizada");
    }
  }

  delay(1); // mínimo para estabilidad
}
