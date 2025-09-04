#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

Adafruit_MPU6050 mpu;
Preferences prefs;

#define LED_PIN   19
#define BOOT_BTN  0   // BOOT button del ESP32

// ===== Declaración de tareas =====
void TaskMPU(void *pvParameters);   // núcleo 1
void TaskWiFi(void *pvParameters);  // núcleo 0

// ===== WebServer para configuración =====
WebServer server(80);

// ===== Función de AP temporal =====
void startAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "12345678");
  Serial.println("AP iniciado -> SSID: ESP32_Config, PASS: 12345678");
  Serial.println("Conéctate y entra a http://192.168.4.1/");

  // Formulario de configuración
  server.on("/", HTTP_GET, []() {
    String html =
      "<!DOCTYPE html><html><head><meta charset='utf-8'>"
      "<title>Configurar WiFi</title></head><body>"
      "<h2>Configurar WiFi</h2>"
      "<form action='/save' method='POST'>"
      "SSID: <input type='text' name='ssid'><br><br>"
      "PASS: <input type='password' name='pass'><br><br>"
      "<input type='submit' value='Guardar'>"
      "</form>"
      "</body></html>";
    server.send(200, "text/html", html);
  });

  // Guardar credenciales
  server.on("/save", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String pass = server.arg("pass");
    if (ssid.length() > 0 && pass.length() > 0) {
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      server.send(200, "text/html", "Credenciales guardadas. Reiniciando...");
      delay(1000);
      ESP.restart();
    } else {
      server.send(200, "text/html", "Datos inválidos, intenta de nuevo.");
    }
  });

  server.begin();
}

// ===== Función de conexión a WiFi =====
void connectWiFi(const char *ssid, const char *pass) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);

  Serial.printf("Conectando a %s", ssid);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Conectado! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Fallo conexión, volviendo a AP");
    startAP();
  }
}

// ===== SETUP =====
void setup() {
  Wire.begin(21, 22);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  Serial.begin(115200);
  prefs.begin("wifi", false);

  // Reset de credenciales si BOOT está presionado
  if (digitalRead(BOOT_BTN) == LOW) {
    prefs.clear();
    Serial.println("Credenciales borradas!");
  }

  // Ver si hay credenciales guardadas
  String ssid = prefs.getString("ssid", "");
  String pass = prefs.getString("pass", "");

  if (ssid == "" || pass == "") {
    Serial.println("No hay credenciales guardadas, iniciando AP...");
    startAP();
  } else {
    connectWiFi(ssid.c_str(), pass.c_str());
  }

  // Inicialización del MPU
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) { delay(10); }
  }
  Serial.println("MPU6050 Found!");
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // ===== Crear tareas en distintos cores =====
  xTaskCreatePinnedToCore(TaskMPU, "TaskMPU", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(TaskWiFi, "TaskWiFi", 4096, NULL, 1, NULL, 0);
}

void loop() {
  // vacío, todo se maneja con FreeRTOS
}

// ===== Tarea para MPU (core 1) =====
void TaskMPU(void *pvParameters) {
  for (;;) {
    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    Serial.printf(
      "Acceleration -> X: %.2f, Y: %.2f, Z: %.2f m/s^2\n"
      "Rotation    -> X: %.2f, Y: %.2f, Z: %.2f rad/s\n"
      "Temperature -> %.2f °C\n\n",
      a.acceleration.x, a.acceleration.y, a.acceleration.z,
      g.gyro.x, g.gyro.y, g.gyro.z,
      temp.temperature
    );

    if ( abs(a.acceleration.x) > 10 || abs(a.acceleration.y) > 10 || abs(a.acceleration.z) > 10 ||
         abs(g.gyro.x) > 1 || abs(g.gyro.y) > 1 || abs(g.gyro.z) < -10 ||
         abs(a.acceleration.x) < -10 || abs(a.acceleration.y) < -10 || abs(a.acceleration.z) < -10 ||
         abs(g.gyro.x) < -1 || abs(g.gyro.y) < -1 || abs(g.gyro.z) < -10 ) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

// ===== Tarea para WiFi + servidor (core 0) =====
void TaskWiFi(void *pvParameters) {
  for (;;) {
    server.handleClient();   // atiende las peticiones web
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}
