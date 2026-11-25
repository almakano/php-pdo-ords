<?php
$drivers = PDO::getAvailableDrivers();
if (!in_array('ords', $drivers, true)) {
    throw new RuntimeException('PDO ORDS driver is not registered: ' . implode(', ', $drivers));
}
echo "PDO ORDS driver registered\n";
