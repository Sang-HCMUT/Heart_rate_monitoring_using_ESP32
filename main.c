#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pulse Sensor 
#define PULSE_PIN 34
#define ADC_SAMPLES 20
#define BUFFER_SIZE 10

float signalFiltered = 0;
float baseline = 2000;
const float filterAlpha = 0.9;

int BPM = 0;
unsigned long lastBeat = 0;
int bpmBuffer[BUFFER_SIZE];
int bpmIndex = 0;

// LED tích hợp
#define LED_PIN LED_BUILTIN
bool ledState = false;

// WiFi (AP mode)
const char* ap_ssid = "HeartRateMonitor"; //SSID NAME
const char* ap_password = "12345678";     //PASS

// Web server
WebServer server(80); //PORT 80

// Time value for web
unsigned long startTime = 0;

// Đọc tín hiệu ADC
int readPulseSensor() {
  int sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PULSE_PIN);
    delayMicroseconds(50);
  }
  return sum / ADC_SAMPLES;
}

// Xử lý yêu cầu web
void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Heart Rate Monitor</title>
    <style>
      body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
      h1 { color: #333; }
      .bpm-display { font-size: 48px; color: #e74c3c; margin: 20px 0; }
      table { margin: 20px auto; border-collapse: collapse; }
      th, td { border: 1px solid #ddd; padding: 8px; }
      button { padding: 10px 20px; margin: 10px; cursor: pointer; }
    </style>
    <script>
      let startTime = Date.now();

      function updateData() {
        fetch('/data')
          .then(response => response.json())
          .then(data => {
            document.getElementById('bpm').innerText = data.bpm > 0 ? data.bpm : '--';
            const relativeTime = Math.floor((Date.now() - startTime) / 1000);
            const history = JSON.parse(localStorage.getItem('bpm_history') || '[]');
            history.push({ time: relativeTime, bpm: data.bpm });
            if (history.length > 1000) history.shift();
            localStorage.setItem('bpm_history', JSON.stringify(history));
            updateHistoryTable();
          });
        setTimeout(updateData, 1000);
      }

      function updateHistoryTable() {
        const history = JSON.parse(localStorage.getItem('bpm_history') || '[]');
        const table = document.getElementById('history');
        while (table.rows.length > 1) table.deleteRow(1);
        history.forEach(entry => {
          const row = table.insertRow();
          row.insertCell().innerText = entry.time;
          row.insertCell().innerText = entry.bpm;
        });
      }

      function downloadCSV() {
        const history = JSON.parse(localStorage.getItem('bpm_history') || '[]');
        let csv = 'Time (s),BPM\n';
        history.forEach(entry => {
          csv += `${entry.time},${entry.bpm}\n`;
        });
        const blob = new Blob([csv], { type: 'text/csv' });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = 'bpm_data.csv';
        a.click();
        URL.revokeObjectURL(url);
      }

      function clearData() {
        localStorage.setItem('bpm_history', '[]');
        startTime = Date.now();
        updateHistoryTable();
      }

      window.onload = () => {
        updateData();
        updateHistoryTable();
      };
    </script>
  </head>
  <body>
    <h1>Heart Rate Monitor</h1>
    <p>Current BPM: <span id="bpm" class="bpm-display">--</span></p>
    <h2>History</h2>
    <table id="history">
      <tr><th>Time (s)</th><th>BPM</th></tr>
    </table>
    <button onclick="downloadCSV()">Download CSV</button>
    <button onclick="clearData()">Clear Data</button>
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

void handleData() {
  StaticJsonDocument<200> jsonDoc;
  jsonDoc["bpm"] = BPM;
  String output;
  serializeJson(jsonDoc, output);
  server.send(200, "application/json", output);
}

// Kết nối Wi-Fi AP
void connectWiFi() {
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0)); 
  WiFi.softAP(ap_ssid, ap_password, 1, 0, 2);
//check status WIFI
  Serial.println("WiFi AP khởi động: ");
  Serial.print("SSID: "); Serial.println(ap_ssid);
  Serial.print("IP: "); Serial.println(WiFi.softAPIP());
}
//hàm chính
void setup() {
  pinMode(LED_PIN, OUTPUT); 
  digitalWrite(LED_PIN, LOW);
  Serial.begin(115200);
  Wire.begin(21, 22);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Màn hình OLED thất bại");
    digitalWrite(LED_PIN, HIGH);
    for (;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Heart Rate Monitor");
  display.display();

  // Kết nối Wi-Fi AP
  connectWiFi();

  // Thiết lập web server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Web server khởi động trên cổng 80");

  // Khởi tạo thời gian bắt đầu
  startTime = millis();

  // Xử lý cảm biến
  int samples = 50;
  for (int i = 0; i < samples; i++) {
    int rawSignal = readPulseSensor();
    signalFiltered = filterAlpha * signalFiltered + (1 - filterAlpha) * rawSignal;
    baseline = 0.999 * baseline + 0.001 * signalFiltered;
    float relativeSignal = signalFiltered - baseline;

    static float lastSignal = 0;
    static bool rising = false;
    static float peak = 0;

    if (relativeSignal > lastSignal && relativeSignal > 80) {
      rising = true;
      if (relativeSignal > peak) peak = relativeSignal;
    } else if (relativeSignal < lastSignal && rising && peak > 150) {
      unsigned long now = millis();
      if (now - lastBeat > 600) {
        int interval = now - lastBeat;
        lastBeat = now;
        int currentBPM = 60000 / interval;

        if (currentBPM >= 60 && currentBPM <= 89) {
          bpmBuffer[bpmIndex] = currentBPM;
          bpmIndex = (bpmIndex + 1) % BUFFER_SIZE;

          int sum = 0;
          for (int j = 0; j < BUFFER_SIZE; j++) sum += bpmBuffer[j];
          BPM = sum / BUFFER_SIZE;

          Serial.printf("BPM: %d, Signal: %.2f, Baseline: %.2f\n", BPM, signalFiltered, baseline);
        }
      }
      rising = false;
      peak = 0;
    }
    lastSignal = relativeSignal;
    delay(20);
  }

  //OLED display
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("IP: "); display.println(WiFi.softAPIP());
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.print("BPM: ");
  if (BPM > 0) display.println(BPM);
  else display.println("---");
  display.display();
}

void loop() {
  server.handleClient();

  // Cập nhật cảm biến
  int rawSignal = readPulseSensor();
  signalFiltered = filterAlpha * signalFiltered + (1 - filterAlpha) * rawSignal;
  baseline = 0.999 * baseline + 0.001 * signalFiltered;
  float relativeSignal = signalFiltered - baseline;

  static float lastSignal = 0;
  static bool rising = false;
  static float peak = 0;
  static unsigned long lastDisplay = 0;

  if (relativeSignal > lastSignal && relativeSignal > 80) {
    rising = true;
    if (relativeSignal > peak) peak = relativeSignal;
  } else if (relativeSignal < lastSignal && rising && peak > 150) {
    unsigned long now = millis();
    if (now - lastBeat > 600) {
      int interval = now - lastBeat;
      lastBeat = now;
      int currentBPM = 60000 / interval;

      if (currentBPM >= 60 && currentBPM <= 89) {
        bpmBuffer[bpmIndex] = currentBPM;
        bpmIndex = (bpmIndex + 1) % BUFFER_SIZE;

        int sum = 0;
        for (int j = 0; j < BUFFER_SIZE; j++) sum += bpmBuffer[j];
        BPM = sum / BUFFER_SIZE;

        Serial.printf("BPM: %d, Signal: %.2f, Baseline: %.2f\n", BPM, signalFiltered, baseline);
      }
    }
    rising = false;
    peak = 0;
  }
  lastSignal = relativeSignal;

  // Cập nhật OLED
  if (millis() - lastDisplay > 500) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("IP: "); display.println(WiFi.softAPIP());
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print("BPM: ");
    if (BPM > 0) display.println(BPM);
    else display.println("---");
    display.display();
    lastDisplay = millis();
  }
}