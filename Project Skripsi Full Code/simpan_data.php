<?php
// file: simpan_data.php (Final - Mengirim Notifikasi Setiap Saat)
include "koneksi.php";

// Konfigurasi Bot Telegram dari file yang Anda unggah
$botToken = "8100092886:AAHTQOv88ba6jQ59FfnwLdKABCKuJXVlfPs"; 
$chatId = "@infokualitasudaragor";

function kirimTelegram($pesan) {
    global $botToken, $chatId;
    $url = "https://api.telegram.org/bot" . $botToken . "/sendMessage";
    $data = ['chat_id' => $chatId, 'text' => $pesan, 'parse_mode' => 'HTML'];
    $options = ["http" => ["method" => "GET", "header" => "Accept-language: en\r\n"]];
    $context = stream_context_create($options);
    @file_get_contents($url . "?" . http_build_query($data), false, $context);
}

if (
    isset($_POST['temperature'], $_POST['humidity'], $_POST['gas'], 
          $_POST['pm25'], $_POST['status_ispu'], $_POST['rekomendasi'])
) {
    // Ambil data dari POST
    $temperature      = (float)$_POST['temperature'];
    $humidity         = (float)$_POST['humidity'];
    $gas              = (int)$_POST['gas'];
    $pm25             = (float)$_POST['pm25'];
    $status_ispu_baru = $_POST['status_ispu'];
    $catatan_kondisi  = $_POST['rekomendasi'];

    // Logika pembuatan rekomendasi olahraga 
$rekomendasi_olahraga = "-"; // Default atau fallback jika tidak ada kondisi yang cocok

// --- PRIORITAS 1: Kondisi ISPU Buruk (PALING PENTING) ---
if ($status_ispu_baru == 'BERBAHAYA' || $status_ispu_baru == 'SANGAT TDK SEHAT' || $status_ispu_baru == 'TIDAK SEHAT') {
    switch ($status_ispu_baru) {
        case 'TIDAK SEHAT':
            $rekomendasi_olahraga = "Udara tidak sehat. Hindari olahraga berat di luar. Pilih olahraga di dalam ruangan.";
            break;
        case 'SANGAT TDK SEHAT':
            $rekomendasi_olahraga = "Sangat tidak sehat! Hindari semua aktivitas di luar. Jaga kesehatan Anda, tetap di dalam.";
            break;
        case 'BERBAHAYA':
            $rekomendasi_olahraga = "Berbahaya! Aktivitas di luar ruangan sangat tidak dianjurkan untuk semua orang.";
            break;
    }
}
// --- PRIORITAS 2: Kondisi Lingkungan Ekstrem LAINNYA (HANYA JIKA ISPU TIDAK BERBAHAYA) ---
// Perhatikan urutannya: Heat Index > Suhu Panas > Gas Buruk > Kelembaban Ekstrem > Suhu Sejuk
else if ($temperature >= 34 && $humidity >= 90) { 
    $rekomendasi_olahraga = "Waspada Heat Index! Suhu panas & kelembaban sangat tinggi. Kurangi intensitas & durasi olahraga. Perbanyak minum."; 
}
else if ($temperature >= 34) { // Suhu panas saja, tanpa kelembaban ekstrem
    $rekomendasi_olahraga = "Udara cukup baik, namun suhu sangat panas. Hindari olahraga berat di siang hari & perbanyak minum air."; 
} 
else if ($gas >= 800) { // Sirkulasi udara buruk terdeteksi, meskipun ISPU mungkin baik
    $rekomendasi_olahraga = "Kualitas udara baik, namun terdeteksi sirkulasi udara buruk. Sebaiknya kurangi aktivitas berat."; 
}
else if ($humidity >= 90) { // Kelembaban sangat tinggi saja
    $rekomendasi_olahraga = "Udara baik, namun sangat lembab. Kenakan pakaian ringan & perhatikan tanda-tanda kelelahan akibat panas."; 
}
else if ($humidity <= 35) { // Kelembaban sangat kering saja
    $rekomendasi_olahraga = "Udara sangat kering. Jaga hidrasi dengan baik dan gunakan pelembab kulit jika perlu saat berolahraga."; 
}
else if ($temperature < 23) { // Suhu sejuk (jika tidak ada kondisi ekstrem lain yang terpenuhi)
    $rekomendasi_olahraga = "Udara sejuk, ideal untuk olahraga ketahanan seperti lari atau bersepeda. Pastikan melakukan pemanasan yang cukup."; 
}
// --- PRIORITAS 3: Kondisi ISPU Baik/Sedang (JIKA TIDAK ADA KONDISI DI ATAS YANG TERPENUHI) ---
else {
    switch ($status_ispu_baru) {
        case 'BAIK':
            $rekomendasi_olahraga = "Udara sangat baik & segar! Waktu yang sempurna untuk semua jenis olahraga di luar.";
            break;
        case 'SEDANG':
            $rekomendasi_olahraga = "Kualitas udara cukup baik. Olahraga ringan seperti jogging atau bersepeda dianjurkan.";
            break;
        default:
            // Ini adalah kondisi fallback jika status ISPU tidak 'BAIK' atau 'SEDANG' 
            // dan tidak ada kondisi ekstrem lainnya yang cocok.
            $rekomendasi_olahraga = "Kondisi lingkungan normal. Tetap aktif dan perhatikan kesehatan Anda.";
            break;
    }
}
    
    // --- PERUBAHAN DI SINI ---
    // Logika notifikasi sekarang tidak lagi memeriksa perubahan status.
    // Pesan akan dibuat dan dikirim setiap kali data diterima.
    
    $status_tampil = str_replace('_', ' ', $status_ispu_baru);
    $emoji = "✅"; // Default untuk BAIK
    if($status_ispu_baru == 'SEDANG') $emoji = "⚠️";
    if($status_ispu_baru == 'TIDAK SEHAT') $emoji = "❌";
    if($status_ispu_baru == 'SANGAT TDK SEHAT') $emoji = "⛔️";
    if($status_ispu_baru == 'BERBAHAYA') $emoji = "🚨";

    // Membuat string pesan notifikasi yang baru dan informatif
    $pesanNotifikasi = "<b>Laporan Kualitas Udara Terbaru</b>\n\n"
                     . "Status ISPU: <b>" . $emoji . " " . htmlspecialchars($status_tampil) . "</b>\n\n"
                     . "<b>DETAIL SENSOR:</b>\n"
                     . "🌡️ Suhu: " . number_format($temperature, 1) . "°C\n"
                     . "💧 Kelembaban: " . number_format($humidity, 0) . "%\n"
                     . "🧪 Gas: " . $gas . "PPM\n"
                     . "🌫️ PM2.5: " . number_format($pm25, 2) . " µg/m³\n\n"
                     . "<b>REKOMENDASI:</b>\n"
                     . "<i>" . htmlspecialchars($rekomendasi_olahraga) . "</i>\n\n"
                     . "<pre>Catatan Teknis: " . htmlspecialchars($catatan_kondisi) . "</pre>";
    
    kirimTelegram($pesanNotifikasi);
    // --- AKHIR DARI PERUBAHAN ---
    

    // Menyimpan data menggunakan Prepared Statements (Tidak Diubah)
    $stmt = mysqli_prepare($conn, "INSERT INTO data_sensor (temperature, humidity, gas, pm25, status_ispu, catatan_kondisi, rekomendasi_olahraga) VALUES (?, ?, ?, ?, ?, ?, ?)");
    mysqli_stmt_bind_param($stmt, "ddissss", $temperature, $humidity, $gas, $pm25, $status_ispu_baru, $catatan_kondisi, $rekomendasi_olahraga);
    if (mysqli_stmt_execute($stmt)) {
        echo "Data berhasil disimpan."; 
    } else {
        echo "Error: " . mysqli_stmt_error($stmt);
    }
    mysqli_stmt_close($stmt);

} else {
    echo "Parameter tidak lengkap!";
}
mysqli_close($conn);
?>