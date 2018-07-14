<?php
$starttijd = microtime(true);
require_once ('/home/deb115497/domains/netfrequentie.nl/dblogin/netmeter_login.php');

// Open a connection to a MySQL server, do the right settings
$connection = new mysqli($server, $username, $password, $database);
if ($connection->connect_error) {
    $connection->close();
    die('ERROR: Not connected to MySQL');
}

if (!$connection->set_charset('utf8')) {
    $connection->close();
    die('ERROR: Setting chartset failed');
}

if (!$connection->query("SET @@session.time_zone = '+00:00'")) {
    $connection->close();
    die('ERROR: Setting timezone failed');
}

// Get the input JSON data which was sent as a POST request put it in an Array
$data = json_decode(file_get_contents('php://input'), true);

// Get user data from this station and check if client is approved and has a valid hash or something goes below this
if ($userStmt = $connection->prepare("SELECT Stat FROM user WHERE id = ?")) {
    $userStmt->bind_param('i', $data['clID']);
    if (!$userStmt->execute()) {
        $userStmt->close();
        $connection->close();
        die('ERROR: Unable to query for user');
    }

    if ($userStmt->num_rows == 0) {
        $userStmt->close();
        $connection->close();
        die('ERROR: User does not eixst');
    }

    $userStmt->close();
} 


// Put the received data in the freq table
if ($insertStmt = $connection->prepare("INSERT IGNORE INTO freq (user, time, volt, freq) VALUES (?, FROM_UNIXTIME(?), ?, ?)")) {
    $insertStmt->bind_param('iiii', $user, $time, $volt, $freq);
    $user = $data['clID'];
    $teller = 0;
    foreach ($data['meas'] as $meting) {
        $teller = $teller + 1;
        $time = $meting['utc'];
        $volt = $meting['volt'];
        $freq = $meting['freq'];
        if (!$insertStmt->execute()) {
            $insertStmt->close();
            $connection->close();
            die('ERROR: Invalid MySQL query');
        }
    }

    $insertStmt->close();
}

$connection->close();
echo "SUCCES: ".$teller." records, verwerkt in ".floor((microtime(true) - $starttijd) * 1000)."ms";
?>