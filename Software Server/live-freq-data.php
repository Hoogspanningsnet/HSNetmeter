<?php
require_once ('/home/deb115497/domains/netfrequentie.nl/dblogin/netmeter_login.php');

$hoursback = 48;

// Opens a connection to a MySQL server.
$connection = new mysqli($server, $username, $password, $database);
if ($connection->connect_error) {
    $connection->close();
    die('ERROR: Not connected to MySQL');
}

if (!$connection->set_charset('utf8')) {
    $connection->close();
    die('ERROR: Setting chartset failed');
}

if (isset($_REQUEST['point'])) {
    $meting = $_REQUEST['point'];
} else {
    $meting = "";
}
if (isset($_REQUEST['time'])) {
    $time=$_REQUEST['time'];
} else {
    $time = "";
}
if (isset($_REQUEST['hoursback'])) {
    $hoursback=$_REQUEST['hoursback'];
}

// Set the JSON header
header("Content-type: application/json");

if ($userStmt = $connection->prepare("SELECT StationName FROM user WHERE  ID = ?")) {
    $userStmt->bind_param('i', $meting);
    $userStmt->execute();

    
}

while ($row = mysqli_fetch_assoc($data)) {
    echo '[{"station":"'.$row["StationName"].'","hoursback":"'.$hoursback.'","data":[';
}
mysqli_free_result($data);
$tzoffset= date('Z');
if ($time=="") {
    $data = mysqli_query($connection,"SELECT UNIX_TIMESTAMP(time) as x, freq as y FROM freq WHERE `time`>FROM_UNIXTIME(".intval(time()-$tzoffset-($hoursback*3600)).") AND `user`=".intval($meting)." AND `freq`>-10000 ORDER BY `time` ASC",MYSQLI_USE_RESULT);
} else {
    $data = mysqli_query($connection,"SELECT UNIX_TIMESTAMP(time) as x, freq as y FROM freq WHERE `time`>FROM_UNIXTIME(".intval(($time/1000)-$tzoffset).") AND `user`=".intval($meting)." AND `freq`>-10000 ORDER BY `time` ASC");
}
$teller = 0;
while($row = mysqli_fetch_assoc($data)) {
    if ($teller !== 0) {
        echo ',';
    };
    $teller = $teller+1;
    echo '['.intval(($row["x"]+$tzoffset)*1000).','.intval($row["y"]).']';
}
echo ']}';
mysqli_free_result($data);
echo ',{"station":"Setpoint","hoursback":"'.$hoursback.'","data":[';
if ($time=="") {
    $data = mysqli_query($connection,"SELECT UNIX_TIMESTAMP(time) as x, freq as y FROM freq WHERE `time`>FROM_UNIXTIME(".intval(time()-$tzoffset-($hoursback*3600)).") AND `user`=1 AND `freq`>-10000 ORDER BY `time` ASC",MYSQLI_USE_RESULT);
} else {
    $data = mysqli_query($connection,"SELECT UNIX_TIMESTAMP(time) as x, freq as y FROM freq WHERE `time`>FROM_UNIXTIME(".intval(($time/1000)-$tzoffset).") AND `user`=1 AND `freq`>-10000 ORDER BY `time` ASC");
}
$teller = 0;
while($row = mysqli_fetch_assoc($data)) {
    if ($teller !== 0) {
        echo ',';
    };
    $teller = $teller+1;
    echo '['.intval(($row["x"]+$tzoffset)*1000).','.intval($row["y"]).']';
}
echo ']}]';
mysqli_free_result($data);
mysqli_close($connection);
?>