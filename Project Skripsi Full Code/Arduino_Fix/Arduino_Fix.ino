// Deklarasi Library
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

//Konfigurasi Perangkat Keras
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define MQ135_PIN 36
#define PM_SENSOR_LED 32
#define PM_SENSOR_ANALOG 35
LiquidCrystal_I2C lcd(0x27, 16, 2);

// NILAI INI HASIL KALIBRASI YANG STABIL
const float RZERO = 20.35;
const float RLOAD = 10.0;
const float PARA = 116.6020682;
const float PARB = 2.769034857;
// --------------------------------------------------------

// --- Konfigurasi Jaringan & Server ---
const char* ssid = "LANTAI 2";
const char* password = "bentardiliatdulu";
const char* serverName = "http://192.168.20.123/project_skripsi/simpan_data.php";

// --- Variabel untuk Timer Pengiriman Data ---
const long intervalKirim = 180000; // 3 menit (180.000 ms)
unsigned long waktuKirimTerakhir = 0;

// --- Variabel untuk Timer Tampilan LCD ---
const long intervalTampilLCD = 20000; // Ganti tampilan setiap 20 detik
unsigned long waktuTampilTerakhir = 0;

// --- Fungsi-fungsi Bantu (Helper) ---

// Fungsi untuk menghitung PPM dari sensor MQ-135
float getMq135PPM() {
  int adc_val = analogRead(MQ135_PIN);
  float v_out = (float)adc_val * (3.3 / 4095.0);
  if (v_out < 0.01) return 0; // Pengaman untuk menghindari pembagian dengan nol
  float Rs = ((3.3 * RLOAD) / v_out) - RLOAD;
  if (Rs < 0) Rs = 0; // Pastikan Rs tidak negatif
  float ratio = Rs / RZERO;
  if (ratio <= 0) return 0; // Pengaman
  float ppm = PARA * pow(ratio, -PARB); // Menggunakan fungsi pow() dari math.h
  return ppm;
}

// Fungsi konversi float ke String dengan jumlah desimal tertentu
String floatToString(float val, int dec) { char b[20]; dtostrf(val, 1, dec, b); return String(b); }

// --- Fungsi untuk membaca nilai PM2.5 dari sensor GP2Y1010AU0F (Dengan Smoothing Filter) ---
float bacaPM25() {
  const int NUM_SAMPLES = 50; // Ambil 100 sampel untuk dirata-ratakan
  float sum_volt = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    digitalWrite(PM_SENSOR_LED, LOW); // Nyalakan LED IR sensor
    delayMicroseconds(280);
    int val = analogRead(PM_SENSOR_ANALOG); // Baca nilai analog
    delayMicroseconds(40);
    digitalWrite(PM_SENSOR_LED, HIGH); // Matikan LED IR
    delayMicroseconds(9680); // Total siklus 10ms (280+40+9680)

    sum_volt += (float)val * (3.3 / 4095.0); // Konversi ke tegangan dan jumlahkan
  }
  float volt = sum_volt / NUM_SAMPLES; // Rata-ratakan tegangan dari N sampel

  const float V_NO_DUST = 0.25;     // Tegangan output di udara sangat bersih
  const float SENSITIVITY = 0.005; // Sensitivitas sensor dalam V/(µg/m³)

  float pm25_density = 0; // Inisialisasi

  if (volt > V_NO_DUST) {
    pm25_density = (volt - V_NO_DUST) / SENSITIVITY;
  }

  // Opsional: Tambahkan floor/batas bawah untuk PM2.5 agar tidak langsung 0 jika ada noise sangat kecil
  if (pm25_density < 0.1 && pm25_density > 0) { // Jika hasilnya sangat kecil tapi bukan 0, bulatkan ke 0.1
     pm25_density = 0.1;
  }
  if (pm25_density < 0) { // Pastikan tidak negatif
     pm25_density = 0;
  }

  return pm25_density; // Mengembalikan nilai PM2.5 dalam µg/m³
}

// Fungsi untuk mendapatkan Status ISPU berdasarkan nilai PM2.5 (PermenLHK No. 14 Tahun 2020)
String getStatusIspu(float pm25) {
  if (pm25 <= 15.4) return "BAIK";
  if (pm25 <= 55.4) return "SEDANG";
  if (pm25 <= 150.4) return "TIDAK SEHAT";
  if (pm25 <= 250.4) return "SANGAT_TDK_SEHAT";
  return "BERBAHAYA";
}

// Fungsi untuk mendapatkan rekomendasi berdasarkan suhu, kelembaban, dan konsentrasi gas
String getRekomendasi(float temperature, float humidity, float gas_ppm) {
  String suhu_text, lembab_text, gas_text;
  if (temperature >= 34) { suhu_text = "Suhu panas (" + String(temperature, 1) + "C). "; }
  else if (temperature < 23) { suhu_text = "Suhu sejuk (" + String(temperature, 1) + "C). "; }
  else { suhu_text = "Suhu nyaman (" + String(temperature, 1) + "C). "; }

  if (humidity >= 90) { lembab_text = "Kelembapan tinggi (" + String(humidity, 0) + "%)."; }
  else if (humidity <= 35) { lembab_text = "Kelembapan rendah (" + String(humidity, 0) + "%)."; }
  else { lembab_text = "Kelembapan ideal (" + String(humidity, 0) + "%)."; }

  if (gas_ppm >= 800) {
    gas_text = " Terdeteksi gas asing/sirkulasi buruk (" + String(gas_ppm, 0) + " PPM)."; // Menampilkan nilai gas
  } else {
    gas_text = " Kualitas udara normal (" + String(gas_ppm, 0) + " PPM)."; // Menampilkan nilai gas
  }
  
  return suhu_text + lembab_text + gas_text;
}

// --- Fungsi untuk Menampilkan Status, PM2.5, dan Rekomendasi di LCD (dengan Scrolling) ---
// Fungsi ini sekarang menerima parameter pm25_val untuk ditampilkan
void tampilkanLayarStatusDanScroll(String status, String rekomendasi, float pm25_val) { 
  const int SCROLL_DELAY = 350; // Kecepatan scroll (ms per karakter)
  const long MINIMUM_DISPLAY_TIME_STATUS_PM25 = 5000; // Tahan tampilan Status + PM2.5 selama 5 detik
  const long MINIMUM_DISPLAY_TIME_SCROLL = 10000; // Minimal waktu tampil scroll (10 detik)

  // --- Fase 1: Tampilan Awal Status dan Nilai PM2.5 ---
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status: " + status); // Contoh: Status: SEDANG
  lcd.setCursor(0, 1);
  lcd.print("PM2.5: " + String(pm25_val, 1) + " ug/m3"); // Tampilkan nilai PM2.5 di baris 1
  
  // Menahan tampilan ini selama beberapa detik agar terlihat jelas (non-blocking)
  unsigned long startTimeFixedDisplay = millis();
  while (millis() - startTimeFixedDisplay < MINIMUM_DISPLAY_TIME_STATUS_PM25) {
      delay(1); // Delay sangat kecil untuk menjaga responsifitas, bukan blocking total
  }

  // --- Fase 2: Lanjut ke Tampilan Scrolling Rekomendasi ---
  lcd.clear(); // Bersihkan LCD untuk tampilan scrolling
  lcd.setCursor(0,0);
  lcd.print("Status: " + status); // Tampilkan Status lagi di baris 0 (agar tetap terlihat)

  String textToScroll = " " + rekomendasi + " --- ";
  int textLen = textToScroll.length();
  
  long waktuScrollTotal = (long)textLen * SCROLL_DELAY;
  long durasiTampilanScroll = max(waktuScrollTotal, MINIMUM_DISPLAY_TIME_SCROLL);

  Serial.printf("Teks scroll: %d karakter. Butuh %ld ms. Tampil scroll selama %ld ms.\n", textLen, waktuScrollTotal, durasiTampilanScroll);

  int scrollIndex = 0;
  unsigned long startTimeScroll = millis(); // Waktu mulai untuk fase scrolling
  unsigned long lastScrollTime = 0;

  while (millis() - startTimeScroll < durasiTampilanScroll) {
    if (millis() - lastScrollTime > SCROLL_DELAY) {
      lcd.setCursor(0, 1); // Scrolling di baris 1
      String sub = textToScroll.substring(scrollIndex, scrollIndex + 16);
      lcd.print(sub);

      scrollIndex++;
      if (scrollIndex > (textLen - 16)) {
        scrollIndex = 0;
      }
      lastScrollTime = millis();
    }
    delay(1); // Delay kecil untuk stabilitas loop
  }
}


// --- Fungsi Setup (Inisialisasi) ---
void setup() {
  Serial.begin(115200);
  dht.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Menghub. WiFi...");
  pinMode(PM_SENSOR_LED, OUTPUT);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("WiFi Terhubung");
  delay(1000); // Delay singkat untuk melihat pesan
}

// --- Fungsi Loop Utama ---
// --- Variabel BARU untuk Timer Update Status & Rekomendasi ---
const long intervalUpdateStatusRekomendasi = 30000; // Hitung ulang status dan rekomendasi setiap 30 detik
unsigned long waktuUpdateStatusRekomendasiTerakhir = 0;
float pm25_terakhir_untuk_status = 0; // Simpan nilai pm25 yang terakhir digunakan untuk status
String statusIspu_terakhir = "BAIK"; // Simpan status ISPU yang terakhir
String rekomendasi_terakhir = ""; // Simpan rekomendasi yang terakhir

// ... (Fungsi-fungsi Bantu tetap sama) ...

void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Gagal DHT. Membaca ulang...");
    return;
  }

  float gas_ppm = getMq135PPM();
  float pm25 = bacaPM25(); // Ini adalah PM2.5 instan yang sudah di-smoothing 10 sampel

  unsigned long waktuSekarang = millis();

  // --- LOGIKA UPDATE STATUS ISPU & REKOMENDASI BERBASIS WAKTU ---
  // Status ISPU dan Rekomendasi hanya akan diperbarui setiap intervalUpdateStatusRekomendasi
  if (waktuSekarang - waktuUpdateStatusRekomendasiTerakhir >= intervalUpdateStatusRekomendasi) {
    waktuUpdateStatusRekomendasiTerakhir = waktuSekarang;

    // Gunakan nilai pm25 saat ini untuk menghitung status dan rekomendasi
    // Anda bisa juga mengambil rata-rata pm25 dari beberapa detik terakhir jika ingin lebih stabil lagi
    pm25_terakhir_untuk_status = pm25; // Simpan nilai pm25 yang digunakan
    statusIspu_terakhir = getStatusIspu(pm25_terakhir_untuk_status);
    statusIspu_terakhir.replace("_", " ");
    rekomendasi_terakhir = getRekomendasi(temperature, humidity, gas_ppm);

    Serial.println("--- STATUS & REKOMENDASI BARU DIHITUNG ---");
    Serial.println("PM2.5 untuk Status: " + String(pm25_terakhir_untuk_status, 1) + " | Status ISPU: " + statusIspu_terakhir + " | Rekomendasi: " + rekomendasi_terakhir);
  }

  // --- Logika Pengaturan Tampilan LCD (Non-blocking) ---
  // Tampilan LCD akan menggunakan status dan rekomendasi yang terakhir dihitung (yang lebih stabil)
  if (waktuSekarang - waktuTampilTerakhir >= intervalTampilLCD) {
    waktuTampilTerakhir = waktuSekarang;

    static bool tampilkanLayarStatusMode1 = true; 
    
    if (tampilkanLayarStatusMode1) {
      // Mode 1: Menampilkan Status ISPU dan Rekomendasi (dengan Scrolling)
      Serial.println("--- LCD Mode 1 (Status & Rekomendasi) Updated ---");
      Serial.println("PM2.5 (saat tampil): " + String(pm25_terakhir_untuk_status, 1) + " | Status ISPU: " + statusIspu_terakhir + " | Rekomendasi: " + rekomendasi_terakhir);
      tampilkanLayarStatusDanScroll(statusIspu_terakhir, rekomendasi_terakhir, pm25_terakhir_untuk_status); // Menggunakan variabel yang disimpan
    } else {
      // Mode 2: Menampilkan nilai sensor mentah (Suhu, Kelembaban, Gas, PM2.5)
      Serial.println("--- LCD Mode 2 (Raw Data) Updated ---");
      Serial.println("PM2.5 (saat tampil): " + String(pm25, 1) + " | Status ISPU: " + getStatusIspu(pm25) + " | Rekomendasi: " + getRekomendasi(temperature, humidity, gas_ppm)); // Menampilkan nilai PM2.5 instan
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("T:" + String(temperature, 1) + " H:" + String(humidity, 0) + "%");
      lcd.setCursor(0,1); 
      lcd.print("G:" + String(gas_ppm, 0) + " P:" + String(pm25, 1));
    }
    tampilkanLayarStatusMode1 = !tampilkanLayarStatusMode1; 
  }
  // --- Akhir Logika Tampilan LCD ---


  // --- Logika Pengiriman Data ke Server (Non-blocking, setiap intervalKirim) ---
  // Pengiriman data juga akan menggunakan status dan rekomendasi yang terakhir dihitung
  if (WiFi.status() == WL_CONNECTED && (waktuSekarang - waktuKirimTerakhir >= intervalKirim || waktuKirimTerakhir == 0)) {
    waktuKirimTerakhir = waktuSekarang;

    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String postData = "temperature=" + floatToString(temperature, 2)
                      + "&humidity=" + floatToString(humidity, 2)
                      + "&gas=" + String(gas_ppm, 2)
                      + "&pm25=" + floatToString(pm25_terakhir_untuk_status, 2) // Gunakan pm25 yang terakhir dihitung untuk status
                      + "&status_ispu=" + statusIspu_terakhir // Gunakan status yang terakhir dihitung
                      + "&rekomendasi=" + rekomendasi_terakhir; // Gunakan rekomendasi yang terakhir dihitung

    Serial.println("--- MENGIRIM DATA KE SERVER (Interval 3 Menit) ---");
    Serial.println("Data: " + postData);
    
    int httpResponseCode = http.POST(postData);
    if (httpResponseCode > 0) {
      Serial.printf("HTTP Response code: %d\n", httpResponseCode);
    } else {
      Serial.printf("HTTP Error code: %d\n", httpResponseCode);
    }
    http.end();
  }
}