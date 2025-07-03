<?php
// file: riwayat.php (Final yang Diperbaiki)
include "koneksi.php";
date_default_timezone_set('Asia/Makassar');

// --- Logika Filter Tanggal ---
$tanggalFilter = isset($_GET['tanggal']) && !empty($_GET['tanggal']) ? $_GET['tanggal'] : date('Y-m-d');

// Query untuk mendapatkan semua data pada tanggal yang difilter
// Logika paginasi dihilangkan karena semua data akan ditampilkan
$stmt = mysqli_prepare($conn, "SELECT * FROM data_sensor WHERE DATE(waktu) = ? ORDER BY waktu DESC");
mysqli_stmt_bind_param($stmt, "s", $tanggalFilter);
mysqli_stmt_execute($stmt);
$result = mysqli_stmt_get_result($stmt);

// Ambil semua data ke dalam array untuk kemudian ditampilkan atau dicetak
$dataRiwayat = [];
if (mysqli_num_rows($result) > 0) {
    while ($row = mysqli_fetch_assoc($result)) {
        $dataRiwayat[] = $row;
    }
}
mysqli_stmt_close($stmt);
?>
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <title>Riwayat Kualitas Udara</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/css/bootstrap.min.css" rel="stylesheet">
    <link href="https://fonts.googleapis.com/css2?family=Poppins:wght@300;500;600&display=swap" rel="stylesheet">
    <style>
        body { font-family: 'Poppins', sans-serif; background: #f4f6f8; }
        .card-custom { border-radius: 20px; box-shadow: 0 4px 10px rgba(0,0,0,0.1); border: none; }
        .judul-section { font-weight: 600; margin-bottom: 20px; }
        .table-success-custom { background-color: #d1e7dd !important; }
        .table-warning-custom { background-color: #fff3cd !important; }
        .table-danger-custom { background-color: #f8d7da !important; }
        /* Paginasi disembunyikan karena tidak lagi digunakan untuk tampilan web */
        .pagination { display: none !important; }
        @media print {
            #tombol-pdf, #form-filter, .pagination, .kembali-btn { display: none !important; }
            /* Memastikan tabel terlihat baik saat dicetak */
            .table { width: 100%; margin-bottom: 1rem; color: #212529; }
            .table th, .table td { padding: 0.75rem; vertical-align: top; border-top: 1px solid #dee2e6; }
            .table-bordered th, .table-bordered td { border: 1px solid #dee2e6; }
            .table-responsive { overflow-x: visible !important; }
        }
    </style>
</head>
<body>
<div class="container py-5">
    <h2 class="text-center mb-4">📜 Riwayat Pemantauan Kualitas Udara</h2>

    <form method="GET" class="mb-4 text-center" id="form-filter">
        <label for="tanggal" class="me-2">📅 Tampilkan Data Tanggal:</label>
        <input type="date" name="tanggal" id="tanggal" value="<?= htmlspecialchars($tanggalFilter) ?>" class="form-control-sm">
        <button type="submit" class="btn btn-primary btn-sm ms-2">Filter</button>
    </form>

    <div id="area-cetak">
        <div class="card card-custom">
            <div class="card-body">
                <div class="d-flex justify-content-between align-items-center mb-3">
                    <h5 class="card-title judul-section mb-0">📊 Hasil untuk Tanggal: <?= date('d F Y', strtotime($tanggalFilter)) ?></h5>
                    <button id="tombol-pdf" class="btn btn-danger btn-sm">🖨️ Cetak PDF</button>
                </div>
                <div class="table-responsive">
                    <table class="table table-bordered table-hover align-middle">
                        <thead>
                            <tr class="table-dark text-center">
                                <th style="width: 5%;">No</th>
                                <th style="width: 15%;">Waktu</th>
                                <th style="width: 15%;">Status ISPU</th>
                                <th style="width: 25%;">Detail Sensor</th>
                                <th style="width: 40%;">Rekomendasi & Catatan</th>
                            </tr>
                        </thead>
                        <tbody>
                        <?php
                        $no = 1;
                        if (!empty($dataRiwayat)) {
                            foreach ($dataRiwayat as $row) {
                                $rowClass = '';
                                if ($row['status_ispu'] == "BAIK") { $rowClass = 'table-success-custom'; }
                                elseif ($row['status_ispu'] == "SEDANG") { $rowClass = 'table-warning-custom'; }
                                else { $rowClass = 'table-danger-custom'; }

                                $waktu = date('Y-m-d H:i:s', strtotime($row['waktu']));
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

                                echo "<tr class='text-center {$rowClass}'>
                                        <td>{$no}</td>
                                        <td>{$waktu}</td>
                                        <td>{$status_ispu}</td>
                                        <td class='text-start'>{$detail_sensor}</td>
                                        <td class='text-start'>{$rekomendasi_dan_catatan}</td>
                                    </tr>";
                                $no++;
                            }
                        } else {
                            echo "<tr><td colspan='5' class='text-center text-muted'>Tidak ada data untuk tanggal yang dipilih.</td></tr>";
                        }
                        ?>
                        </tbody>
                    </table>
                </div>
                </div>
        </div>
    </div>

    <div class="text-center mt-4"><a href="index.php" class="btn btn-outline-secondary btn-sm kembali-btn">⬅️ Kembali ke Dashboard</a></div>
</div>

<script src="https://cdnjs.cloudflare.com/ajax/libs/html2pdf.js/0.10.1/html2pdf.bundle.min.js"></script>
<script>
    document.getElementById('tombol-pdf').addEventListener('click', function() {
        const elementToPrint = document.getElementById('area-cetak');
        const tanggal = "<?= htmlspecialchars($tanggalFilter) ?>";
        const namaFile = 'Riwayat-Kualitas-Udara-' + tanggal + '.pdf';
        const opt = {
            margin: 1, filename: namaFile, image: { type: 'jpeg', quality: 0.98 },
            html2canvas: { scale: 2 }, jsPDF: { unit: 'cm', format: 'a4', orientation: 'landscape' }
        };
        html2pdf().from(elementToPrint).set(opt).save();
    });
</script>
</body>
</html>