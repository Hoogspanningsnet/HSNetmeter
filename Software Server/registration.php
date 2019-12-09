<?php
require_once ('/home/deb115497/domains/netfrequentie.nl/dblogin/netmeter_login.php');

$connection = new mysqli($server, $username, $password, $database);
if ($connection->connect_error) {
        $connection->close();
    die('ERROR: Not connected to MySQL');
}

if (!$connection->set_charset('utf8')) {
    $connection->close();
    die('ERROR: Setting chartset failed');
}

$user = 0;
if ($stmt = $connection->prepare("INSERT IGNORE INTO user (StationName, FirstName, LastName, Nick, Town, Country, Lat, Lon, HV-substation) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)")) {
    $stmt->bind_param('ssssssdds', 
        $_POST['stationname'],
        $_POST['firstname'],
        $_POST['lastname'],
        $_POST['nickname'],
        $_POST['town'],
        $_POST['country'],
        $_POST['latitude'],
        $_POST['longitude'],
        $_POST['hvsubstation']);
    if (!$stmt->execute()) {
        $stmt->close();
        $connection->close();
        die('ERROR: Registration failed');
    }

    $user = $stmt->insert_id;
    $stmt->close();
}

$connection->close();
echo "SUCCES: registration successful ".$user;
?>