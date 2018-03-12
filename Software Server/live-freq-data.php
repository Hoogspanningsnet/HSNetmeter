<?php
//    error_reporting(E_ALL);
//    ini_set('display_errors', 1);
//    ini_set('memory_limit', '256M'); 
    
    require_once ('/home/deb115497/domains/netfrequentie.nl/dblogin/netmeter_login.php');

    function getUserIP() {
        $client  = @$_SERVER['HTTP_CLIENT_IP'];
        $forward = @$_SERVER['HTTP_X_FORWARDED_FOR'];
        $remote  = $_SERVER['REMOTE_ADDR'];
        
        if(filter_var($client, FILTER_VALIDATE_IP)) {
            $ip = $client;
        }
        elseif(filter_var($forward, FILTER_VALIDATE_IP)) {
            $ip = $forward;
        }
        else {
            $ip = $remote;
        }
        return $ip;
    }
    
    
    $hoursback = 48;
    // Opens a connection to a MySQL server.
    $connection = mysqli_connect ($server, $username, $password, $database);
    if (!$connection) {	die('Not connected to MySQL'); }
    mysqli_query($connection, "SET character_set_results = 'utf8', character_set_client = 'utf8', character_set_connection = 'utf8', character_set_database = 'utf8', character_set_server = 'utf8'");
    
    if (isset($_REQUEST['point'])) {$meting=$_REQUEST['point'];} else {$time="";};
    if (isset($_REQUEST['time'])) {$time=$_REQUEST['time'];} else {$time="";};
    if (isset($_REQUEST['hoursback'])) {$hoursback=$_REQUEST['hoursback'];} else {$hoursback=48;};
    
    // Set the JSON header
    header("Content-type: application/json");
    
    $data = mysqli_query($connection,"SELECT StationName FROM user WHERE `ID`=".intval($meting)." LIMIT 1");
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