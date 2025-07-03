<?php
$host = "localhost";
$user = "root";
$pass = ""; // biarkan kosong jika tidak ada password
$db   = "project_skripsi";

// Membuat koneksi
$conn = mysqli_connect($host, $user, $pass, $db);

// Periksa koneksi
if (!$conn) {
    die("Koneksi database gagal: " . mysqli_connect_error());
}
?>
