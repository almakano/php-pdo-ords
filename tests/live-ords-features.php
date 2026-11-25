<?php
$dsn = 'ords:' . rtrim((string) getenv('ORDS_BASE_URL'), '/') . '/' . trim((string) getenv('ORDS_SCHEMA_ALIAS'), '/') . '/';
$pdo = new PDO($dsn, (string) getenv('ORDS_USERNAME'), (string) getenv('ORDS_PASSWORD'), [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$stmt = $pdo->prepare('SELECT :id AS ID, :label AS LABEL FROM dual');
$stmt->bindValue(':id', 42, PDO::PARAM_INT);
$stmt->bindValue(':label', 'ORDS bind test', PDO::PARAM_STR);
$stmt->execute();
$row = $stmt->fetch(PDO::FETCH_ASSOC);
if ((int) $row['ID'] !== 42 || $row['LABEL'] !== 'ORDS bind test') {
    throw new RuntimeException('Named IN binds failed');
}
$pdo->setAttribute(PDO_ORDS_ATTR_OFFSET, 2);
$pdo->setAttribute(PDO_ORDS_ATTR_LIMIT, 2);
$rows = $pdo->query('SELECT level AS ID FROM dual CONNECT BY level <= 8')->fetchAll(PDO::FETCH_ASSOC);
if (count($rows) !== 2) {
    throw new RuntimeException('ORDS pagination failed');
}
$out = null;
$output = $pdo->prepare('BEGIN :out_id := 9876; END;');
$output->bindParam(':out_id', $out, PDO::PARAM_INT | PDO::PARAM_INPUT_OUTPUT, 40);
$output->execute();
if ((int) $out !== 9876 || $pdo->lastInsertId('out_id') !== '9876') {
    throw new RuntimeException('PL/SQL OUT bind or lastInsertId failed');
}
$counter = 41;
$inout = $pdo->prepare('BEGIN :counter := :counter + 1; END;');
$inout->bindParam(':counter', $counter, PDO::PARAM_INT | PDO::PARAM_INPUT_OUTPUT, 40);
$inout->execute();
if ((int) $counter !== 42) {
    throw new RuntimeException('PL/SQL INOUT bind failed');
}
echo "Live ORDS advanced features passed\n";
