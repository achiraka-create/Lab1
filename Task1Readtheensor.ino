#include <DHT.h>
#include <ArduinoJson.h>

// --- กำหนดพินและชนิดเซนเซอร์ ---
#define DHTPIN 4      // ขาที่ต่อสาย DATA ของ DHT22 เข้ากับ ESP32-S3
#define DHTTYPE DHT22 // กำหนดชนิดเป็น DHT22 (AM2302)

// --- ตั้งค่าหมายเลขโหนด (เปลี่ยน p01 เป็นเลขคู่กลุ่มของตัวเอง เช่น p02, p04) ---
const char* NODE_ID = "p01"; 
const unsigned long READ_INTERVAL = 10000; // อ่านค่าทุก 10 วินาที

DHT dht(DHTPIN, DHTTYPE);
unsigned long lastReadTime = 0;
unsigned long seqNumber = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  // เริ่มต้นทำงานเซนเซอร์ DHT
  dht.begin();
  Serial.println("DHT22 Sensor Initialized.");
}

void loop() {
  unsigned long currentMillis = millis();

  // วนลูปอ่านค่าทุกๆ 10 วินาทีโดยไม่ใช้ delay() (Non-blocking)
  if (currentMillis - lastReadTime >= READ_INTERVAL) {
    lastReadTime = currentMillis;

    // อ่านค่าความชื้นและอุณหภูมิ (หน่วยเป็นเซลเซียส)
    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    // ตรวจสอบว่าอ่านค่าผิดพลาดหรือไม่ (NaN = Not a Number)
    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("{\"error\":\"Failed to read from DHT22 sensor!\"}");
      return;
    }

    // สร้างโครงสร้างข้อมูล JSON ตามรูปแบบที่โจทย์กำหนด
    JsonDocument doc;
    doc["node"] = NODE_ID;
    doc["seq"] = seqNumber++;
    doc["temp_c"] = round(temperature * 10.0) / 10.0; // ปัดเศษทศนิยม 1 ตำแหน่ง
    doc["rh"] = round(humidity * 10.0) / 10.0;

    // พิมพ์ผลลัพธ์ออกทาง Serial Monitor
    serializeJson(doc, Serial);
    Serial.println();
  }
}