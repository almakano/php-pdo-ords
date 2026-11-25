<?php
$assert = static function (bool $condition, string $message): void {
    if (!$condition) {
        throw new RuntimeException($message);
    }
};
$pdo = new PDO('ords:http://127.0.0.1:18080/ords/demo', 'demo', 'secret', [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]);
$stmt = $pdo->prepare('SELECT :id AS ID, :name AS NAME, :rate AS RATE, :active AS ACTIVE, :note AS NOTE FROM dual');
$stmt->bindValue(':id', 42, PDO::PARAM_INT);
$stmt->bindValue(':name', "O'Reilly", PDO::PARAM_STR);
$stmt->bindValue(':rate', '12.5', PDO::PARAM_STR);
$stmt->bindValue(':active', true, PDO::PARAM_BOOL);
$stmt->bindValue(':note', null, PDO::PARAM_NULL);
$stmt->execute();
$row = $stmt->fetch(PDO::FETCH_ASSOC);
$assert((int) $row['ID'] === 42 && $row['NAME'] === "O'Reilly" && (float) $row['RATE'] === 12.5 && $row['ACTIVE'] === true && $row['NOTE'] === null, 'Named binds or FETCH_ASSOC failed');
$positional = $pdo->prepare('SELECT ? AS ID FROM dual');
$positional->bindValue(1, 42, PDO::PARAM_INT);
$positional->execute();
$assert((int) $positional->fetchColumn() === 42, 'Positional bind or FETCH_COLUMN failed');
$pdo->setAttribute(PDO_ORDS_ATTR_OFFSET, 5);
$pdo->setAttribute(PDO_ORDS_ATTR_LIMIT, 2);
$pagination = $pdo->query('SELECT PAGINATION_TEST FROM dual');
$assert($pagination->rowCount() === 2 && count($pagination->fetchAll(PDO::FETCH_ASSOC)) === 2, 'Pagination failed');
$out = null;
$output = $pdo->prepare('BEGIN :out_id := 9876; /* OUT_BIND_TEST */ END;');
$output->bindParam(':out_id', $out, PDO::PARAM_INT | PDO::PARAM_INPUT_OUTPUT, 40);
$output->execute();
$assert((int) $out === 9876 && $pdo->lastInsertId('out_id') === '9876', 'OUT bind or lastInsertId failed');
try {
    $pdo->query('SELECT BROKEN FROM dual');
    throw new RuntimeException('Expected ORDS error was not raised');
} catch (PDOException $exception) {
    $assert(str_contains($exception->getMessage(), 'ORA-00942'), 'Unexpected ORDS error: ' . $exception->getMessage());
}
echo "PDO ORDS integration tests passed\n";
