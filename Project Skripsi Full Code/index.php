<?php
include "koneksi.php";

// --- PENGAMBILAN DATA YANG LEBIH EFISIEN (HANYA 1 KALI QUERY) ---
date_default_timezone_set('Asia/Makassar');
setlocale(LC_TIME, 'id_ID.UTF-8');
$tanggalSekarang = strftime('%A, %d %B %Y | %H:%M:%S');

// Ambil 10 data terakhir, data paling baru akan menjadi data utama
$query_history = mysqli_query($conn, "SELECT * FROM data_sensor ORDER BY id DESC LIMIT 10");

// Inisialisasi variabel
$data_terbaru = null;
$data_riwayat = [];
if (mysqli_num_rows($query_history) > 0) {
    $data_riwayat = mysqli_fetch_all($query_history, MYSQLI_ASSOC);
    $data_terbaru = $data_riwayat[0]; // Data paling baru adalah baris pertama
}

// Inisialisasi variabel default untuk tampilan
$status_ispu_tampil = "Tidak Ada Data";
$rekomendasi_olahraga_tampil = "-";
$catatan_kondisi_tampil = "Menunggu data pertama dari sensor.";
$statusClass = "alert-secondary";

if ($data_terbaru) {
    // Mengambil semua data dari kolom baru
    $status_ispu_tampil = str_replace('_', ' ', $data_terbaru['status_ispu']);
    $rekomendasi_olahraga_tampil = $data_terbaru['rekomendasi_olahraga'];
    $catatan_kondisi_tampil = $data_terbaru['catatan_kondisi'];
    
    // Tentukan warna kotak status berdasarkan ISPU
    if ($data_terbaru['status_ispu'] == "BAIK") { $statusClass = "alert-success"; }
    elseif ($data_terbaru['status_ispu'] == "SEDANG") { $statusClass = "alert-warning"; }
    elseif ($data_terbaru['status_ispu'] == "TIDAK SEHAT") { $statusClass = "alert-danger"; }
    elseif ($data_terbaru['status_ispu'] == "SANGAT TDK SEHAT") { $statusClass = "alert-primary"; }
    elseif ($data_terbaru['status_ispu'] == "BERBAHAYA") { $statusClass = "alert-dark"; }
}
?>
<!DOCTYPE html>
<html lang="id">
<head>
  <meta charset="UTF-8">
  <title>Dashboard Kualitas Udara</title>
  <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
  <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;500;600&display=swap" rel="stylesheet">
  <meta http-equiv="refresh" content="60">
  
  <style>
    body { font-family: 'Poppins', sans-serif; background: #f4f6f8; }
    .card-custom { border-radius: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); border: none; }
    .judul-section { font-weight: 600; margin-bottom: 20px; }
    .alert-success { background-color: #d1e7dd; color: #0f5132; }
    .alert-warning { background-color: #fff3cd; color: #664d03; }
    .alert-danger { background-color: #f8d7da; color: #842029; }
    .alert-primary { background-color: #e2d9f3; color: #583f7d; }
    .alert-dark { background-color: #d3d3d4; color: #141619; }
  </style>
</head>
<body>
<div class="container py-5">
<h2 class="text-center mb-2">🌿 Dashboard Pemantauan Kualitas Udara</h2>

<p class="text-center text-muted mb-4"><?= $tanggalSekarang ?></p>

<div class="row text-center">
  <div class="col-md-3 mb-3"><div class="card card-custom h-100"><div class="card-body d-flex flex-column justify-content-center"><h6>🌡️ Suhu</h6><h3><?= $data_terbaru ? number_format($data_terbaru['temperature'], 1) . '°C' : '-' ?></h3></div></div></div>
  <div class="col-md-3 mb-3"><div class="card card-custom h-100"><div class="card-body d-flex flex-column justify-content-center"><h6>💧 Kelembaban</h6><h3><?= $data_terbaru ? number_format($data_terbaru['humidity'], 0) . '%' : '-' ?></h3></div></div></div>
  <div class="col-md-3 mb-3"><div class="card card-custom h-100"><div class="card-body d-flex flex-column justify-content-center"><h6>🧪 Gas</h6><h3><?= $data_terbaru ? $data_terbaru['gas'] . 'PPM' : '-' ?></h3></div></div></div>
  <div class="col-md-3 mb-3"><div class="card card-custom h-100"><div class="card-body d-flex flex-column justify-content-center"><h6>🌫️ PM2.5</h6><h3><?= $data_terbaru ? number_format($data_terbaru['pm25'], 2) . 'µg/m³' : '-' ?></h3></div></div></div>
</div>

<div class="alert <?= $statusClass ?> text-center mt-4">
  <h5 class="alert-heading">Status ISPU: <?= htmlspecialchars($status_ispu_tampil) ?></h5>
  <p class="mb-1"><strong><?= htmlspecialchars($rekomendasi_olahraga_tampil) ?></strong></p>
  <hr>
  <p class="mb-0"><small><?= htmlspecialchars($catatan_kondisi_tampil) ?></small></p>
</div>

<div class="card mt-5 card-custom">
  <div class="card-body">
    <h5 class="card-title judul-section">📈 Riwayat Sensor 10 Data Terakhir</h5>
    <div class="table-responsive">
      <table class="table table-bordered table-hover align-middle">
        <thead>
          <tr class="table-dark text-center">
            <th style="width: 15%;">Waktu</th>
            <th style="width: 15%;">Status ISPU</th>
            <th style="width: 25%;">Detail Sensor</th>
            <th style="width: 45%;">Rekomendasi & Catatan</th>
          </tr>
        </thead>
        <tbody>
        <?php
          if(!empty($data_riwayat)) {
            foreach ($data_riwayat as $row) {
              
              $waktu = date('H:i:s', strtotime($row['waktu']));
              $status_ispu = "<strong>" . str_replace('_', ' ', $row['status_ispu']) . "</strong>";
              
              $detail_sensor = "
                <span class='badge text-bg-secondary me-2'>🌡️ " . htmlspecialchars($row['temperature']) . "°C</span>
                <span class='badge text-bg-secondary me-2'>💧 " . htmlspecialchars($row['humidity']) . "%</span>
                <span class='badge text-bg-secondary me-2'>🧪 " . htmlspecialchars($row['gas']) . "PPM</span>
                <span class='badge text-bg-secondary'>🌫️ " . number_format($row['pm25'], 2) . "µg/m³</span>
              ";
              
              $rekomendasi_dan_catatan = "
                <p class='mb-1'><strong>" . htmlspecialchars($row['rekomendasi_olahraga']) . "</strong></p>
                <small class='text-muted'><em>" . htmlspecialchars($row['catatan_kondisi']) . "</em></small>
              ";

              echo "<tr class='text-center'>
                      <td>{$waktu}</td>
                      <td>{$status_ispu}</td>
                      <td class='text-start'>{$detail_sensor}</td>
                      <td class='text-start'>{$rekomendasi_dan_catatan}</td>
                    </tr>";
            }
          } else {
            echo "<tr><td colspan='4' class='text-center'>Belum ada data tercatat.</td></tr>";
          }
        ?>
        </tbody>
      </table>
    </div>
  </div>
</div>

<div class="text-center mt-3"><a href="riwayat.php" class="btn btn-outline-secondary btn-sm">📜 Lihat Riwayat Lengkap</a></div>
<div class="text-center mt-4 text-muted"><p class="mb-0">&copy; <?= date('Y') ?> Dibuat oleh 2111102441144 | MOHAMAD FEBRIANSYAH AL AKBAR</p></div>

</div>
</body>
</html>