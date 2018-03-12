<?php 
//    error_reporting(E_ALL);
//    ini_set('display_errors', 1);
    $starttijd = microtime(true);
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
    
    $user_ip = getUserIP();
    
    // Open a connection to a MySQL server, do the right settings
    $connection = mysqli_connect ($server, $username, $password, $database);
    if (!$connection) {	die('ERROR: Not connected to MySQL'); }
    mysqli_query($connection, "SET character_set_results = 'utf8', character_set_client = 'utf8', character_set_connection = 'utf8', character_set_database = 'utf8', character_set_server = 'utf8', @@session.time_zone = '+00:00'");
    
    //Get the input JSON data which was sent as a POST request put it in an Array
    $data = json_decode(file_get_contents('php://input'), true);
    
    //Get user data from thius station
    $query = "SELECT * FROM user WHERE id=".$data['clID'];
    $result = mysqli_query($connection, $query);
    if (!$result) {
        mysqli_close($connection);
        die('ERROR: Invalid MySQL query');
    }
    // Check if client is approved and has a valid hash or something goes below this
    
    // Put the received data in the freq table
    $teller=0;
    foreach ($data['meas'] as $meting) {
        $teller = $teller+1;
        $query = "INSERT IGNORE INTO freq SET user=".$data['clID'].", time=FROM_UNIXTIME(".$meting['utc']."), volt=".$meting['volt'].", freq=".$meting['freq'];
        $result = mysqli_query($connection, $query);
        if (!$result) {	
            mysqli_close($connection);
            die('ERROR: Invalid MySQL query');
        }
    }
    
    mysqli_close($connection);
    echo "SUCCES : ".$teller." records,verwerkt in ".floor((microtime(true)-$starttijd)*1000)."ms";
?>