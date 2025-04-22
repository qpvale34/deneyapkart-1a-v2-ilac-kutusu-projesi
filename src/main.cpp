// Medicine Box Project for Deneyap 1010
// Enhanced Web UI: modern, styled, colored medicine cards

#include <Wire.h>
#include <Deneyap_GercekZamanliSaat.h>
#include <Deneyap_OLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

// Wi-Fi credentials (set before upload)
const char* ssid = "TurkTelekom_T6C7F";
const char* password = "jFRh0DgC.!.?09";

WebServer server(80);
Preferences prefs;
RTC rtc;
OLED oled;

int LED_PIN = D14;  // Use pin 2 instead of D0 (SDA) to avoid I2C conflict
int BUZZER_PIN = D13;

struct MedSchedule {
  uint8_t hour;
  uint8_t minute;
  char name[16];
  uint8_t lastDayTriggered;
} meds[3];

void loadSettings() {
  prefs.begin("medPrefs", false);
  for (int i = 0; i < 3; i++) {
    String hKey = "hour" + String(i);
    String mKey = "min"  + String(i);
    String nKey = "name" + String(i);
    meds[i].hour   = prefs.getUInt(hKey.c_str(), 8 + i*2);
    meds[i].minute = prefs.getUInt(mKey.c_str(), 0);
    String nm = prefs.getString(nKey.c_str(), "Med" + String(i+1));
    nm.toCharArray(meds[i].name, sizeof(meds[i].name));
    meds[i].lastDayTriggered = 0;
  }
}

// Büyük punto ve ortalama için yardımcı fonksiyon
void showBigCenteredText(const char* text) {
  oled.clearDisplay();
  char bigLine[33] = {0};
  int len = strlen(text);
  int idx = 0;
  for (int i = 0; i < len && idx < 31; i++) {
    bigLine[idx++] = text[i];
    if (i < len - 1) bigLine[idx++] = ' ';
  }
  int totalWidth = idx;
  int pad = (16 - totalWidth) / 2;
  oled.setTextXY(2, 0);
  for (int i = 0; i < pad; i++) oled.putString(" ");
  oled.putString(bigLine);
}

// Basit bir melodi (örnek: do-re-mi)
const int melody[] = { 262, 294, 330, 262, 262, 294, 330, 262 }; // frekanslar (Hz)
const int noteDurations[] = { 300, 300, 300, 300, 300, 300, 300, 300 }; // ms
const int melodyLength = sizeof(melody) / sizeof(melody[0]);

void playBuzzerMelody() {
  for (int thisNote = 0; thisNote < melodyLength; thisNote++) {
    tone(BUZZER_PIN, melody[thisNote]);
    delay(noteDurations[thisNote]);
    noTone(BUZZER_PIN);
    delay(30); // kısa ara
  }
}

// Alarm sırasında büyük yazı ve 1 saniyede bir yanıp sönme, buzzer melodili
void playAlarmWithBigText(const char* medName) {
  unsigned long start = millis();
  bool visible = true;
  while (millis() - start < 30000) { // 30 saniye boyunca
    digitalWrite(LED_PIN, HIGH);

    if (visible) {
      showBigCenteredText(medName);
      playBuzzerMelody(); // Melodi çal
    } else {
      oled.clearDisplay();
      noTone(BUZZER_PIN);
      delay(200);
    }
    visible = !visible;
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  oled.clearDisplay(); // Alarm bitince ekranı temizle
}

void handleRoot() {
  // Modern styled HTML with inline CSS
  String html =
    "<html><head><meta charset='UTF-8'><title>Medicine Scheduler</title>"
    "<style>"
      "body{font-family:Arial,sans-serif;background:#f4f7f9;color:#333;margin:0;padding:0;}"
      ".container{max-width:600px;margin:40px auto;padding:20px;background:#fff;border-radius:8px;box-shadow:0 2px 8px rgba(0,0,0,0.1);}"
      "h2{margin-bottom:20px;color:#4a90e2;}"
      ".med-card{display:flex;justify-content:space-between;align-items:center;padding:12px;border-radius:6px;margin-bottom:12px;color:#fff;}"
      ".med1{background:#e74c3c;} .med2{background:#27ae60;} .med3{background:#f1c40f;}"
      "label{flex:1;margin-right:8px;}"
      "input[type=text], input[type=time]{flex:1;padding:6px;border:1px solid #ccc;border-radius:4px;}"
      ".btn-save{display:block;width:100%;padding:12px;background:#4a90e2;color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer;}"
      ".btn-save:hover{background:#357ab8;}"
      ".time-card{background:#4a90e2;margin-bottom:20px;padding:15px;border-radius:6px;color:white;}"
      ".time-inputs{display:flex;gap:10px;margin-top:10px;}"
      ".time-inputs input{padding:6px;border-radius:4px;border:1px solid #ddd;}"
    "</style></head><body>"
    "<div class='container'>"
    
    // Add Time Setting Card
    "<div class='time-card'>"
    "<h3>SAAT AYARI</h3>"
    "<form method='POST' action='/settime'>"
    "<div class='time-inputs'>"
    "<input type='date' name='date' required>"
    "<input type='time' name='time' required>"
    "<button type='submit' style='padding:6px 12px;background:#fff;color:#4a90e2;border:none;border-radius:4px;cursor:pointer;'>SAATİ KUR</button>"
    "</div></form></div>"
    
    "<h2>İLAÇ SAATLERİNİ AYARLA</h2>"
    "<form method='POST' action='/save'>";

  // Generate 3 colored cards
  const char* colors[3] = {"med1","med2","med3"};
  for (int i = 0; i < 3; i++) {
    html += "<div class='med-card " + String(colors[i]) + "'>";
    html += "<label for='name" + String(i) + "'>İLACIN ADI:</label>";
    html += "<input type='text' id='name" + String(i) + "' name='name" + String(i) + "' value='" + meds[i].name + "' required>";
    html += "<label for='time" + String(i) + "'>İLACIN SAATİ:</label>";
    char buf[6];
    sprintf(buf, "%02d:%02d", meds[i].hour, meds[i].minute);
    html += "<input type='time' id='time" + String(i) + "' name='time" + String(i) + "' value='" + String(buf) + "' required>";
    html += "</div>";
  }

  html += "<button type='submit' class='btn-save'>KAYDET</button>";
  html += "</form></div></body></html>";

  server.send(200, "text/html; charset=UTF-8", html);
}

void handleSave() {
  // Save form data
  for (int i = 0; i < 3; i++) {
    String hKey = "hour" + String(i);
    String mKey = "min"  + String(i);
    String nKey = "name" + String(i);
    String name = server.arg(nKey);
    String time = server.arg("time" + String(i));
    int h = time.substring(0,2).toInt();
    int m = time.substring(3,5).toInt();
    prefs.putUInt(hKey.c_str(), h);
    prefs.putUInt(mKey.c_str(), m);
    prefs.putString(nKey.c_str(), name);
  }
  loadSettings();  // refresh local vars
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

// Yeni fonksiyon: RTC saatini manuel ayarlamak için
void setRTCTime(int year, int month, int day, int hour, int minute, int second) {
  DateTime newTime(year, month, day, hour, minute, second);
  rtc.adjust(newTime);
}

void handleSetTime() {
  String dateStr = server.arg("date");  // Format: YYYY-MM-DD
  String timeStr = server.arg("time");  // Format: HH:MM
  
  // Parse date components
  int year = dateStr.substring(0,4).toInt();
  int month = dateStr.substring(5,7).toInt();
  int day = dateStr.substring(8,10).toInt();
  
  // Parse time components
  int hour = timeStr.substring(0,2).toInt();
  int minute = timeStr.substring(3,5).toInt();
  
  // Set RTC time
  setRTCTime(year, month, day, hour, minute, 0);
  
  server.sendHeader("Location", "/");
  server.send(302, "text/plain", "");
}

void displayTimeAndMeds() {
  static uint8_t lastSecond = 61;
  static uint8_t lastHour = 255;
  static uint8_t lastMinute = 255;
  static char lastNames[3][16] = {"","",""};

  DateTime now = rtc.now();
  uint8_t h = now.hour();
  uint8_t mi = now.minute();
  uint8_t s = now.second();
  uint8_t d = now.day();

  // Saat gösterimini her saniye güncelle
  if (s != lastSecond) {
    lastSecond = s;
    oled.setTextXY(0,0);
    char timeStr[20];
    sprintf(timeStr, "SAAT: %02d:%02d:%02d", h, mi, s);
    oled.putString(timeStr);
  }

  // İlaç saatleri veya isimleri değiştiyse anında güncelle
  bool medsChanged = false;
  for (int i = 0; i < 3; i++) {
    if (strcmp(lastNames[i], meds[i].name) != 0) {
      strcpy(lastNames[i], meds[i].name);
      medsChanged = true;
    }
  }
  if (h != lastHour || mi != lastMinute || medsChanged) {
    lastHour = h;
    lastMinute = mi;
    oled.setTextXY(1,0);
    oled.putString("----------------");
    for(int i = 0; i < 3; i++) {
      oled.setTextXY((i*2)+2,0);
      char buffer[20];
      sprintf(buffer, "%-10s %02d:%02d", meds[i].name, meds[i].hour, meds[i].minute);
      oled.putString(buffer);
      if(i < 2) {
        oled.setTextXY((i*2)+3,0);
        oled.putString("----------------");
      }
    }
  }

  // İlaç kontrolleri
  for(int i = 0; i < 3; i++) {
    if(h == meds[i].hour && mi == meds[i].minute && d != meds[i].lastDayTriggered) {
      meds[i].lastDayTriggered = d;
      playAlarmWithBigText(meds[i].name);
      // Alarm bitince ekranı tekrar güncelle
      lastHour = 255; lastMinute = 255; lastSecond = 61;
      for (int j = 0; j < 3; j++) lastNames[j][0] = '\0';
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200);
  Wire.begin();

  // RTC başlatma ve saat ayarı
  Wire.setClock(100000);
  if (!rtc.begin()) {
    Serial.println("RTC baslatilamadi!");
    while (1);
  }
  
  // Örnek: Saati manuel olarak ayarlamak için (ihtiyaca göre değiştirin)
  // setRTCTime(2024, 1, 15, 10, 30, 0); // yıl, ay, gün, saat, dakika, saniye
  
  // OLED başlatma
  oled.begin(0x7A);
  oled.init();
  oled.clearDisplay();
  
  // Load settings & connect
  loadSettings();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(100);
  Serial.print("Open Web UI at http://"); Serial.println(WiFi.localIP());

  server.on("/", HTTP_GET,  handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/settime", HTTP_POST, handleSetTime);  // Add new endpoint
  server.begin();
}

void loop() {
  server.handleClient();
  displayTimeAndMeds();
  delay(100); // Shorter delay for more responsive updates
}
