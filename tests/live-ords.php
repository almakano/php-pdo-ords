<?php
$dsn = 'ords:' . rtrim((string) getenv('ORDS_BASE_URL'), '/') . '/' . trim((string) getenv('ORDS_SCHEMA_ALIAS'), '/') . '/';
$pdo = new PDO($dsn, (string) getenv('ORDS_USERNAME'), (string) getenv('ORDS_PASSWORD'), [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$value = $pdo->query('SELECT 1 AS VALUE FROM dual')->fetchColumn();
if ((int) $value !== 1) {
    throw new RuntimeException('ORDS live connectivity check failed');
}
echo "Live ORDS connectivity passed\n";
