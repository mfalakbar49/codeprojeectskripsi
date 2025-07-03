// Deklarasi Library
// Bagian ini berisi 'include' untuk pustaka-pustaka eksternal yang diperlukan sistem.
// Pustaka ini memungkinkan fungsi seperti konektivitas WiFi, komunikasi HTTP,
// pembacaan sensor, kontrol LCD I2C, dan operasi matematika.
#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>

// Konfigurasi Perangkat Keras
// Blok ini mendefinisikan pin-pin GPIO ESP32 yang terhubung ke masing-masing sensor
// dan LCD, serta membuat objek untuk interaksi dengan komponen-komponen tersebut.
#define DHTPIN 4              // Pin untuk sensor DHT22 (suhu & kelembaban)
#define DHTTYPE DHT22         // Tipe sensor DHT yang digunakan
DHT dht(DHTPIN, DHTTYPE);     // Objek untuk sensor DHT

#define MQ135_PIN 36          // Pin analog untuk sensor gas MQ-135
#define PM_SENSOR_LED 32      // Pin digital untuk mengontrol LED IR pada sensor PM2.5
#define PM_SENSOR_ANALOG 35   // Pin analog untuk membaca output dari sensor PM2.5
LiquidCrystal_I2C lcd(0x27, 16, 2); // Objek untuk LCD I2C (alamat 0x27, 16 kolom, 2 baris)

// NILAI INI HASIL KALIBRASI YANG STABIL
// Konstanta kalibrasi untuk sensor MQ-135 dan PM2.5.
// Nilai-nilai ini digunakan untuk mengonversi pembacaan sensor mentah
// menjadi satuan yang bermakna (PPM untuk gas, µg/m³ untuk PM2.5).
const float RZERO = 20.35;        // Resistansi sensor MQ-135 di udara bersih
const float RLOAD = 10.0;         // Resistansi beban seri untuk MQ-135
const float PARA = 116.6020682;   // Konstanta A dalam rumus konversi PPM (MQ-135)
const float PARB = 2.769034857;   // Konstanta B dalam rumus konversi PPM (MQ-135)
// --------------------------------------------------------

// --- Konfigurasi Jaringan & Server ---
// Blok ini berisi kredensial jaringan Wi-Fi dan alamat server tujuan
// tempat data akan dikirimkan.
const char* ssid = "LANTAI 2";                      // Nama jaringan Wi-Fi
const char* password = "bentardiliatdulu";          // Kata sandi jaringan Wi-Fi
const char* serverName = "http://192.168.20.123/project_skripsi/simpan_data.php"; // URL server penerima data

// --- Variabel untuk Timer Pengiriman Data ---
// Mengatur interval waktu non-blocking untuk pengiriman data ke server.
const long intervalKirim = 180000; // Data dikirim setiap 3 menit (180.000 ms)
unsigned long waktuKirimTerakhir = 0; // Menyimpan waktu pengiriman terakhir

// --- Variabel untuk Timer Tampilan LCD ---
// Mengatur interval waktu non-blocking untuk pergantian mode tampilan pada LCD.
const long intervalTampilLCD = 20000; // Tampilan LCD berganti mode setiap 20 detik
unsigned long waktuTampilTerakhir = 0; // Menyimpan waktu tampilan LCD terakhir

// --- Variabel untuk Timer Update Status & Rekomendasi ---
// Mengatur interval waktu non-blocking untuk perhitungan dan stabilisasi
// nilai Status ISPU dan Rekomendasi yang akan ditampilkan/dikirim.
const long intervalUpdateStatusRekomendasi = 30000; // Status dihitung ulang setiap 30 detik
unsigned long waktuUpdateStatusRekomendasiTerakhir = 0; // Waktu update status terakhir
float pm25_terakhir_untuk_status = 0; // Nilai PM2.5 stabil untuk status
String statusIspu_terakhir = "BAIK"; // Status ISPU yang stabil
String rekomendasi_terakhir = ""; // Rekomendasi yang stabil

// --- Fungsi-fungsi Bantu (Helper) ---

// Fungsi untuk menghitung PPM (Parts Per Million) dari sensor MQ-135.
// Membaca nilai analog, mengonversi ke tegangan, menghitung resistansi sensor (Rs),
// rasio resistansi (Rs/R0), lalu mengonversi ke PPM menggunakan rumus pangkat.
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

// Fungsi konversi nilai float ke String dengan jumlah desimal tertentu.
String floatToString(float val, int dec) {
  char b[20];
  dtostrf(val, 1, dec, b);
  return String(b);
}

// --- Fungsi untuk membaca nilai PM2.5 dari sensor GP2Y1010AU0F (Dengan Smoothing Filter) ---
// Fungsi ini membaca nilai dari sensor PM2.5, melakukan averaging (smoothing)
// dari beberapa sampel untuk menstabilkan data, dan mengonversinya menjadi
// konsentrasi PM2.5 dalam µg/m³.
float bacaPM25() {
  const int NUM_SAMPLES = 10; // Jumlah sampel untuk dirata-ratakan
  float sum_volt = 0;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    digitalWrite(PM_SENSOR_LED, LOW); // Nyalakan LED IR sensor
    delayMicroseconds(280);
    int val = analogRead(PM_SENSOR_ANALOG); // Baca nilai analog
    delayMicroseconds(40);
    digitalWrite(PM_SENSOR_LED, HIGH); // Matikan LED IR
    delayMicroseconds(9680); // Total siklus pembacaan 10ms
    sum_volt += (float)val * (3.3 / 4095.0); // Konversi ke tegangan dan jumlahkan
  }
  float volt = sum_volt / NUM_SAMPLES; // Rata-ratakan tegangan dari N sampel

  // Debugging: Cetak nilai rata-rata Volt PM2.5
  Serial.println("DEBUG bacaPM25() - Avg Volt: " + String(volt, 3) + "V");

  const float V_NO_DUST = 0.25;     // DIKOREKSI: Tegangan ambang batas untuk PM2.5 dianggap 0 (kalibrasi)
  const float SENSITIVITY = 0.005; // Sensitivitas sensor dalam V/(µg/m³)

  float pm25_density = 0; // Inisialisasi density PM2.5

  if (volt > V_NO_DUST) {
    pm25_density = (volt - V_NO_DUST) / SENSITIVITY;
  }

  // Filter opsional untuk nilai PM2.5 yang sangat kecil
  if (pm25_density < 0.1 && pm25_density > 0) {
    pm25_density = 0.1;
  }
  if (pm25_density < 0) {
    pm25_density = 0;
  }

  return pm25_density; // Mengembalikan nilai PM2.5 dalam µg/m³
}

// Fungsi untuk mendapatkan Status ISPU (Indeks Standar Pencemar Udara)
// berdasarkan nilai PM2.5, sesuai PermenLHK No. 14 Tahun 2020.
String getStatusIspu(float pm25) {
  if (pm25 <= 15.4) return "BAIK";
  if (pm25 <= 55.4) return "SEDANG";
  if (pm25 <= 150.4) return "TIDAK SEHAT";
  if (pm25 <= 250.4) return "SANGAT_TDK_SEHAT";
  return "BERBAHAYA";
}

// Fungsi untuk mendapatkan pesan rekomendasi berdasarkan suhu, kelembaban, dan konsentrasi gas.
// Pesan ini melengkapi informasi Status ISPU dan memberikan saran kontekstual.
String getRekomendasi(float temperature, float humidity, float gas_ppm) {
  String suhu_text, lembab_text, gas_text;

  // Kondisi untuk Suhu
  if (temperature >= 34) { suhu_text = "Suhu panas (" + String(temperature, 1) + "C). "; }
  else if (temperature < 23) { suhu_text = "Suhu sejuk (" + String(temperature, 1) + "C). "; }
  else { suhu_text = "Suhu nyaman (" + String(temperature, 1) + "C). "; }

  // Kondisi untuk Kelembaban
  if (humidity >= 90) { lembab_text = "Kelembapan tinggi (" + String(humidity, 0) + "%)."; }
  else if (humidity <= 35) { lembab_text = "Kelembapan rendah (" + String(humidity, 0) + "%)."; }
  else { lembab_text = "Kelembapan ideal (" + String(humidity, 0) + "%)."; }

  // Kondisi untuk Gas (menampilkan nilainya juga)
  if (gas_ppm >= 800) {
    gas_text = " Terdeteksi gas asing/sirkulasi buruk (" + String(gas_ppm, 0) + " PPM).";
  } else {
    gas_text = " Kualitas udara normal (" + String(gas_ppm, 0) + " PPM).";
  }
  
  return suhu_text + lembab_text + gas_text;
}

// --- Fungsi untuk Menampilkan Status, PM2.5, dan Rekomendasi di LCD (dengan Scrolling) ---
// Fungsi ini mengelola tampilan informasi di layar LCD 16x2.
// Menampilkan status ISPU dan nilai PM2.5, kemudian menggulir pesan rekomendasi.
void tampilkanLayarStatusDanScroll(String status, String rekomendasi, float pm25_val) { 
  const int SCROLL_DELAY = 350; // Kecepatan scrolling per karakter (ms)
  const long MINIMUM_DISPLAY_TIME_STATUS_PM25 = 5000; // Durasi tampilan Status + PM2.5 awal (5 detik)
  const long MINIMUM_DISPLAY_TIME_SCROLL = 10000; // Durasi minimal tampilan scrolling (10 detik)

  // Fase 1: Tampilan Awal Status dan Nilai PM2.5 (ditahan)
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Status: " + status);
  lcd.setCursor(0, 1);
  lcd.print("PM2.5: " + String(pm25_val, 1) + " ug/m3");
  
  // Menahan tampilan ini selama beberapa detik agar terlihat jelas
  unsigned long startTimeFixedDisplay = millis();
  while (millis() - startTimeFixedDisplay < MINIMUM_DISPLAY_TIME_STATUS_PM25) {
      delay(1); // Delay sangat kecil agar tetap responsif
  }

  // Fase 2: Lanjut ke Tampilan Scrolling Rekomendasi
  lcd.clear(); // Bersihkan LCD untuk tampilan scrolling
  lcd.setCursor(0,0);
  lcd.print("Status: " + status); // Tampilkan Status lagi di baris 0

  String textToScroll = " " + rekomendasi + " --- ";
  int textLen = textToScroll.length();
  
  long waktuScrollTotal = (long)textLen * SCROLL_DELAY;
  long durasiTampilanScroll = max(waktuScrollTotal, MINIMUM_DISPLAY_TIME_SCROLL);

  Serial.printf("Teks scroll: %d karakter. Butuh %ld ms. Tampil scroll selama %ld ms.\n", textLen, waktuScrollTotal, durasiTampilanScroll);

  int scrollIndex = 0;
  unsigned long startTimeScroll = millis();
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
// Fungsi ini dieksekusi satu kali di awal program.
// Tugasnya adalah menginisialisasi komunikasi, sensor, LCD, dan Wi-Fi.
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
// Ini adalah fungsi utama yang berjalan terus-menerus setelah setup().
// Mengatur pembacaan sensor, update LCD, dan pengiriman data secara non-blocking.
void loop() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();
  // Tangani kegagalan pembacaan DHT tanpa memblokir
  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Gagal DHT. Membaca ulang...");
    return; // Keluar dari loop dan coba lagi di iterasi berikutnya
  }

  float gas_ppm = getMq135PPM();
  float pm25 = bacaPM25(); // Ambil nilai PM2.5 (sudah di-smoothing)
  
  // Hitung status dan rekomendasi (ini adalah nilai 'instan' untuk referensi)
  String statusIspu = getStatusIspu(pm25);
  statusIspu.replace("_", " "); // Ganti underscore dengan spasi untuk tampilan
  String rekomendasi = getRekomendasi(temperature, humidity, gas_ppm);

  unsigned long waktuSekarang = millis(); // Ambil waktu saat ini untuk semua timer

  // --- LOGIKA UPDATE STATUS ISPU & REKOMENDASI BERBASIS WAKTU ---
  // Status ISPU dan Rekomendasi hanya akan diperbarui setiap intervalUpdateStatusRekomendasi (30 detik).
  // Ini untuk menstabilkan tampilan status dan data yang dikirim/dicatat.
  if (waktuSekarang - waktuUpdateStatusRekomendasiTerakhir >= intervalUpdateStatusRekomendasi) {
    waktuUpdateStatusRekomendasiTerakhir = waktuSekarang; // Reset timer update status

    // Simpan nilai PM2.5, Status ISPU, dan Rekomendasi yang akan dianggap "stabil"
    pm25_terakhir_untuk_status = pm25; 
    statusIspu_terakhir = getStatusIspu(pm25_terakhir_untuk_status);
    statusIspu_terakhir.replace("_", " ");
    rekomendasi_terakhir = getRekomendasi(temperature, humidity, gas_ppm);

    // Debugging: Cetak status & rekomendasi yang baru dihitung (setiap 30 detik)
    Serial.println("--- STATUS & REKOMENDASI BARU DIHITUNG (Setiap 30 Detik) ---");
    Serial.println("PM2.5 (untuk Status): " + String(pm25_terakhir_untuk_status, 1) + 
                    " | Status ISPU: " + statusIspu_terakhir + " | Rekomendasi: " + rekomendasi_terakhir);
  }

  // --- Logika Pengaturan Tampilan LCD (Non-blocking) ---
  // Tampilan LCD akan berganti mode setiap intervalTampilLCD (20 detik).
  // Menggunakan status dan rekomendasi yang terakhir dihitung (yang lebih stabil).
  if (waktuSekarang - waktuTampilTerakhir >= intervalTampilLCD) {
    waktuTampilTerakhir = waktuSekarang; // Reset timer tampilan LCD

    // Variabel static untuk mengontrol pergantian mode tampilan LCD
    static bool tampilkanLayarStatusMode1 = true; 
    
    if (tampilkanLayarStatusMode1) {
      // Mode 1: Menampilkan Status ISPU dan Rekomendasi (dengan Scrolling)
      // Debugging: Cetak saat LCD Mode 1 diperbarui
      Serial.println("--- LCD Mode 1 (Status & Rekomendasi) Updated ---");
      Serial.println("PM2.5: " + String(pm25_terakhir_untuk_status, 1) + " | T:" + String(temperature, 1) + 
                      "C | H:" + String(humidity, 0) + "% | Gas: " + String(gas_ppm, 0) + " PPM | Status ISPU: " + statusIspu_terakhir + 
                        " | Rekomendasi: " + rekomendasi_terakhir);
      // Panggil fungsi tampil LCD dengan nilai stabil
      tampilkanLayarStatusDanScroll(statusIspu_terakhir, rekomendasi_terakhir, pm25_terakhir_untuk_status);
    } else {
      // Mode 2: Menampilkan nilai sensor mentah (Suhu, Kelembaban, Gas, PM2.5)
      // Debugging: Cetak saat LCD Mode 2 diperbarui
      Serial.println("--- LCD Mode 2 (Raw Data) Updated ---");
      Serial.println("PM2.5: " + String(pm25, 1) + " | T:" + String(temperature, 1) + "C | H:" +
                     String(humidity, 0) + "% | Gas: " + String(gas_ppm, 0) + " PPM | Status ISPU: " + 
                     getStatusIspu(pm25) + " | Rekomendasi: " + getRekomendasi(temperature, humidity, gas_ppm));
      // Tampilkan nilai mentah di LCD
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print("T:" + String(temperature, 1) + " H:" + String(humidity, 0) + "%");
      lcd.setCursor(0,1); 
      lcd.print("G:" + String(gas_ppm, 0) + " P:" + String(pm25, 1));
    }
    // Toggle status untuk mode tampilan berikutnya
    tampilkanLayarStatusMode1 = !tampilkanLayarStatusMode1; 
  }
  // --- Akhir Logika Tampilan LCD ---


  // --- Logika Pengiriman Data ke Server (Non-blocking, setiap intervalKirim) ---
  // Data yang dikirim ke server adalah nilai status dan rekomendasi yang stabil
  if (WiFi.status() == WL_CONNECTED && (waktuSekarang - waktuKirimTerakhir >= intervalKirim || waktuKirimTerakhir == 0)) {
    waktuKirimTerakhir = waktuSekarang; // Reset timer pengiriman

    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    // Membangun string data POST dengan semua parameter, menggunakan nilai stabil
    String postData = "temperature=" + floatToString(temperature, 2)
                      + "&humidity=" + floatToString(humidity, 2)
                      + "&gas=" + String(gas_ppm, 2)
                      + "&pm25=" + floatToString(pm25_terakhir_untuk_status, 2)
                      + "&status_ispu=" + statusIspu_terakhir
                      + "&rekomendasi=" + rekomendasi_terakhir;

    // Debugging: Cetak info pengiriman data ke Serial Monitor
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