                                                                                                                                                                                                                                                                                 // TODO
//  1)  Store user credentials a nd data etc in SPIFFS with JSON
//  3)  Detect external antenna for GPS
//  4)  Store Backlight and LED brightness in the filesystem
//  5)  Make local website with Websockets?
//  6)  Make local website to configure userdata?
//  8)  Do OTA stuff
//  9)  Disable bluetooth?



char HWVersion[10] = "Proto";
char SWVersion[10] = "V0.10";

#include <Arduino.h>      // Defines the ESP32 functions for the Arduino IDE
#include <math.h>         // Used for calculating floating point data

// Include the libraries needed for Wifi and stuff needeed for the autoconnect
// We are using the WIFIMANAGER from https://github.com/zhouhan0126/WIFIMANAGER-ESP32
// This library uses a config access point to connect to Wifi if we havnt done so before
#include <WiFi.h>         // To use the wifi built into the ESP32
#include <DNSServer.h>    // For wifimannager
#include <WebServer.h>    // For wifimanager
#include <WiFiManager.h>  // Used to connecto to local Wifi SSSID
#include <HTTPClient.h>   // Used for connecting to webserver
#include <PCD8544.h>      // From https://github.com/Anirudhvl/Esp32-Nokia-5110-Interfacing-and-NTP-Sync-IST-Digital-Clock
#include <TinyGPS++.h>    // Used for decoding the GPS data

// Define pins used which connect to ESP32
#define BLBRIGHT         200    // Brightness of the Backlight pin
#define BLCHANNEL          1    // Blacklight brightness is PWM controlled via this channel
#define SCK_PIN           33    // Serial clock of display
#define BL_PIN            32    // Backlight pin of display
#define DIN_PIN           25    // Data In pin (MOSI) of display
#define DC_PIN            26    // Data Enable of display
#define CE_PIN            27    // Clock Enable Pin of display
#define RST_PIN           14    // Reset Pin of display
#define PUSHBTN           12    // Pushbutton is connect to this pin
#define LEDPIN            21    // Signal LED is connected to this pin
#define LEDBRIGHT         12    // Brightnes of the LED pin
#define LEDCHANNEL         0    // The LED brightness is PWM controlled via this channel
#define PPSPIN            23    // The PPS signal is connected to this pin
#define FREQPIN           22    // The 50Hz signal is connected to this pin
#define RXD2              16    // GPS unit is connected to Serial 2 RX Pin
#define TXD2              17    // GPS unit is connected to Serial 2 TX Pin
#define ANT_DETECT         0    // GPS unit external antenna signal is connected to this pin  

// Define stuff for the display hardware
#define QUEUELENGTH       10    // The maximum number of sampoles to queue
#define MAXMENUS          7     // he number of display pages we cycle through when pushing the button
#define DISPMAXLINES      6     // The maximu number of lines in the Nokia 5110 display
#define DISPMAXCHARACTERS 14    // The maxiimum characters on a line of a Nokia 5110 display
#define DISPLAYWIDTH      84    // Number of pixel in width of Nokia 5110 display
#define DISPLAYHEIGHT     48    // Number of pixel in height of Nokia 5110 display
#define DISPLAYCONTRAST   50    // Default contrast of display;

//Define the screens to display
#define DISPMEASUREMENT   0     // Shows Date, time and frequency
#define DISPGPS           1     // Shows GPS LAT, LON en ALT 
#define DISPGPSDATE       2     // Shows GPS Date and Time
#define DISPWIFI          3     // Shows Wifi data 
#define DISPVERSION       4     // Shows Version info
#define DISPUSER          5     // Shows User info
#define DISPUPTIME        6     // Shows openring screen
#define DISPOPENING       7     // Shows openring screen
// Define the error displays
#define ERRORGPSLOST      10    // Errormessage that GPS fix is lost
#define ERRORSERVERLOST   11    // Errormessage that Server connection is lost
#define ERRORMAINSLOST    12    // Errormessage that Mains cant be detected
#define ERRORWIFILOST     13    // Errormessage that Wifi is lost
// Define hint to display
#define HINTWIFISTART     50    // Hint screen that we are starting wifi
#define HINTWIFIRESET     51    // Hint screen that wifi is reset
#define HINTTESTSERVER    52    // Hint screen that we are connecting to server
#define HINTSERVERSUCCES  53    // Hint screen that server is connected
#define HINTGPSSTART      54    // Hint screen that GPS is started
#define HINTGPSFOUND      55    // Hint screen that GPS has been found
#define HINTGPSFIXED      56    // Hint screen that GPS has a fix
#define HINTSTARTUNIT     57    // Hint screen that Unit straats measuments
#define HINTCLOCKCAL      58    // Hint screen that clock is being calibrfated
#define HINTWIFICONNECT   59    // HINT screen that we entered the WiFi Config

// Define the pushbutton behaviour
#define BACKLIGHTTIMEOUT    5000  // The time in millis that the BL will be on
#define PUSHDELAYMENU       1000   // The time that we need to push the butoon to go to the next menu
#define PUSHBUTTONTIMEOUT   20000  // The if no button pushed we revert to normal display
// structure with data for the LED signaling
struct LED_Struct {
  const uint8_t PIN;
  const uint8_t brightness; 
  const uint8_t channel; 
  boolean ledState;
};

// structure with data for the Balcklight
struct BL_Struct {
  const uint8_t PIN;
  const uint8_t brightness; 
  const uint8_t channel; 
};

// Structure for the PPS signal
struct PPS_Struct {
  const uint8_t PIN;
  uint64_t LastISRTime;
};

// Stucture fot the frequency data
struct Freq_Struct {
  const uint8_t PIN;
  uint32_t FreqPulses;
  uint64_t StartTime;
  uint64_t EndTime;
};

// Structure for the measured data
struct Meas_Struct {
  int32_t FreqPulses;
  int32_t Duration;
  int64_t Correction;
};

// Structure for the measured data
struct Data_Struct {
  char    utc[25];
  int     freq;
  int     volt = 11;
};

// Define variable in structures
volatile  PPS_Struct    PPSPin = {PPSPIN, 0};
volatile  Freq_Struct   FreqPin = {FREQPIN, 0, 0, 0}; 
volatile  Meas_Struct   MeasData = {0, 0, 0}; 
volatile  LED_Struct    LedData = {LEDPIN, LEDBRIGHT, LEDCHANNEL, false}; 
volatile  BL_Struct     blData = {BL_PIN, BLBRIGHT, BLCHANNEL}; 

// Define GPS connector
TinyGPSPlus   gps;

// Define HTTP connector
HTTPClient    http;   

// Define the Display
static PCD8544 lcd(SCK_PIN, DIN_PIN, DC_PIN, RST_PIN, CE_PIN);

// Semaphore to signal that we dont want the interrupt service routine interrupted
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//Define all other data
char      chipID[16]=               "000000000000";                           // Holds the chipID (hex) of the ESP32 which is the MAC adress
uint64_t  start =                   millis();                                 // Used to time durations
uint64_t  MeasurementStart =        0;                                        // Used to calculaate uptime
uint64_t  FixStart =                0;                                        // Used to calculate fix time
double    StationLat =              0.0;                                      // The station Latitude
double    StationLon =              0.0;                                      // The station Longitude
double    StationAlt =              0.0;                                      // The statoin Altitude
uint16_t  httpTimeOut =             5000;
int       httpResponseCode =        0;                                        // The http respconse code send back by the webserver
int       httpElapsedTime =         0;                                        // The duration of the last webserver call        
int       menuServed =              0;                                        // The current diaplay page on the display
boolean   GPSStableFix =            false;                                    // true if there is a stable GPS fix
boolean   ResetPushed =             false;                                    // Signals if Reset Wifi credential button was pushed
boolean   disableFreqUpdates =      false;                                    
float     Frequency =               0.0;  
float     CorrectionFactor =        0.0;    
char      str_temp[12];
char      FrequencyString[8] =      "00.0000";
char      DateString[11] =          "0000/00/00";
char      TimeString[9] =           "00:00:00";
char      WifiSSID[25] =            "000000000000000000000000";
uint32_t  UserID =                  100001;
char      UserNickName[30] =        "BaDu";
char      UserName[30] =            "Bas van Duijnhoven";
char      UserTown[30] =            "Rheden";
char      UserCountry[4] =          "NL";
char      UserStationName[30] =     "Rheden ESP32";
char      UserHBSubstation[30] =    "NL-Kattenberg";
char      configPortalName[10] =    "BaDu_MFM";
char      configPortalPass[15] =    "380kiloVolt";
String    hostName =                "https://www.netfrequentie.nl/";
String    pingURL =                 "ping.php";
String    dataURL =                 "fmeting.php";
String    ConnectedSSID =           "";
boolean   ErrorDisplayed =          false;

//Setup the structure to store the measurements
Data_Struct     DataToSend[QUEUELENGTH];            // The data aueue which we send to the server
uint8_t         queuedDataLength = 0;               // The mumber of samples in the queue to be send out    

// functio to show a single line on the display
void DisplayLine(unsigned char line, String val) {
  if (line < DISPMAXLINES) {                                                        // check that we are not over the lines limit
    lcd.setCursor(0, line);                                                         // Set cursot at the specified line
    if (val.length()>DISPMAXCHARACTERS) {val=val.substring(0,DISPMAXCHARACTERS);}   // Check if string isnt to long, shorten if needed
    lcd.print(val);                                                                 // Print line
  } 
}

void displayMenu(int menu) {
  int CurrentPage = 99999;
  
  lcd.setContrast(DISPLAYCONTRAST);
  if (menu == CurrentPage) { 
    return; // Skip the display stuff if we dont change;
  }  
  if (menu >= ERRORGPSLOST) {
    ErrorDisplayed=true;
  } else {   // We dont have an error or hint to display
    if (menu > MAXMENUS) {menu=0;} // check if we have the menu to high and adjust
  } 
  CurrentPage = menu;
  lcd.clear();
  DisplayLine(0,"-= BaDu MFM =-");
  DisplayLine(1,"");
  switch (menu) {
    case DISPMEASUREMENT: {       // Maindisplay with Date, Time and Frequency
      DisplayLine(2,(String)"DT "+DateString);
      DisplayLine(3,(String)"TM "+TimeString);
      DisplayLine(4,"");
      DisplayLine(5,(String)"MF "+FrequencyString+" Hz");
      break;
    }
    case DISPGPS: {       // GPS display with Date, Time, Lat, Lon, MSL
      DisplayLine(2,"GPS Position:");
      dtostrf(StationLat, 3, 6, str_temp);
      DisplayLine(3,(String)"LAT "+str_temp);
      dtostrf(StationLon, 3, 6, str_temp);
      DisplayLine(4,(String)"LON "+str_temp);
      dtostrf(StationAlt, 2, 1, str_temp);
      DisplayLine(5,(String)"ALT "+str_temp + "m MSL");
      break;
    }
    case DISPGPSDATE: {       // GPS display with Date, Time, Lat, Lon, MSL
      DisplayLine(2,"GPS Time (UTC):");
      DisplayLine(4,(String)"DT "+DateString);
      DisplayLine(5,(String)"TM "+TimeString);
      break;
    }
    case DISPWIFI: {       // WiFi display with SSID, Signalstrength, Channel, IP
      DisplayLine(2,"WiFi Data:");
      DisplayLine(3,"ST "+ConnectedSSID);
      DisplayLine(4,"SS "+(String)WiFi.RSSI()+" dB");
      DisplayLine(5,WiFi.localIP().toString());
      break;
    }
    case DISPVERSION: {       // Version display with HW and SW version
      DisplayLine(2,"Version info:");
      DisplayLine(3,"HW "+(String)HWVersion);
      DisplayLine(4,"SW "+(String)SWVersion);
      DisplayLine(5,"ID "+(String)chipID);
      break;
    }
    case DISPUSER: {       // User display with Username and Station ID
      DisplayLine(2,(String)"User data:");
      DisplayLine(3,(String)"SN "+UserStationName);
      DisplayLine(4,(String)"UN "+UserNickName);
      DisplayLine(5,(String)"WP "+UserCountry+"-"+UserTown);
      break;
    }
    case DISPUPTIME: {     // Display uptime and fix times
      DisplayLine(2,"Unit uptimes:"); 
      DisplayLine(3,"UT "+(String)(int)(millis()/1000)+"s"); 
      DisplayLine(4,"MT "+(String)(int)((millis()-MeasurementStart)/1000)+"s"); 
      if (GPSStableFix) {
        DisplayLine(5,"FT "+(String)(int)((millis()-FixStart)/1000)+"s");
      } else {
        DisplayLine(5,"FT NO FIX");
      }
      break;
    }
    case DISPOPENING: {     // Opening display
      DisplayLine(2,"     MAINS");
      DisplayLine(3,"   FREQUENCY");
      DisplayLine(4,"     METER");
      DisplayLine(5," (c)2019 BaDu");
      break;
    }
    case ERRORGPSLOST: {       // ERROR display for GPS fix
      DisplayLine(2,"ERROR:");
      DisplayLine(3,"GPS-Fix Lost");
      DisplayLine(4,"Waiting...");
      break;
    }
    case ERRORSERVERLOST: {       // ERROR display for webserver lost
      DisplayLine(2,"ERROR:");
      DisplayLine(3,"Queue full");
      DisplayLine(4,"No Server");
      DisplayLine(5,"Check Route");
      break;
    }
    case ERRORMAINSLOST: {       // ERROR display mains lost
      DisplayLine(2,"ERROR:");
      DisplayLine(3,"Mains lost");
      DisplayLine(4,"Waiting...");
      break;
    }
    case ERRORWIFILOST: {       // ERROR display mains lost
      DisplayLine(2,"ERROR:");
      DisplayLine(3,"WiFi lost");
      DisplayLine(4,"Waiting...");
      break;
    }
    case HINTWIFISTART: {       // ERROR display mains lost
      DisplayLine(2,"Starting WiFi ...");
      DisplayLine(4,"Push button");
      DisplayLine(5,"to reconnect");
      break;
    }
    case HINTWIFIRESET: {       // HINT that we are resetting wifi credentials
      DisplayLine(3, "Button pressed");
      DisplayLine(4, "Reseting WiFi");
      DisplayLine(5, "Reseting unit");
      break;
    }
    case HINTTESTSERVER: {    // HINT that we are contacting the server
      DisplayLine(2,"Contact server");
      break;
    }
    case HINTSERVERSUCCES: {    // HINT that we have a server connection
      DisplayLine(2,"Server contacted");
      DisplayLine(3,"Code "+(String)httpResponseCode);
      DisplayLine(4,"Delay "+(String)httpElapsedTime+" ms");
      break;
    }
    case HINTGPSSTART: {        // HINT that we are trying to connect to the GPS unit
      DisplayLine(2,"Starting GPS");
      break;
    }
    case HINTGPSFOUND: {        // Hint that we have found the GPS but are waiting to fix
      DisplayLine(2,"Starting GPS");
      DisplayLine(3,"GPS Found!");
      DisplayLine(5,"Need fix...");
      break;
    }
    case HINTGPSFIXED: {       // HINT that we have a fix
      DisplayLine(2,"GPS Fix stable");
      dtostrf(StationLat, 3, 6, str_temp);
      DisplayLine(4,(String)"LAT "+str_temp);
      dtostrf(StationLon, 3, 6, str_temp);
      DisplayLine(5,(String)"LON "+str_temp);
      break;      
    }
    case HINTSTARTUNIT: {     // HINT that are starting measurements
      DisplayLine(2,"Unit OK");
      DisplayLine(4,"Starting");
      DisplayLine(5,"Measurements");  
      break;
    }
    case HINTCLOCKCAL: {       // HINT we lost clock calibration and we are resetting
      DisplayLine(2,"Calibrating");
      DisplayLine(3,"Internal Clock");  
      break;
    }
    case HINTWIFICONNECT :{    // HINT that we entered the WiFi Config
      DisplayLine(2,"WiFi config:");
      DisplayLine(3,"Connect to");
      DisplayLine(5,configPortalName);
    }
    
  }
}


// function to check if GPS is stable (again)
static void waitTillStable(unsigned long sec) {
  int counter = 0;
  while (counter < sec*2) {
    if (gps.time.age() < 1500) { 
      // dats is less that 1.5 seconds old so not stale
      Serial.printf(".");
      counter += 1;
    } else { 
      // data is stale we reset the counter
      Serial.printf("!");
      counter = 0;
    }
    delay(500);
  }
  Serial.printf("OK\n");
}

// Interrupt service handler for the PPS pin, gets called exactly every second on the second
void IRAM_ATTR PPSIsrOn() {
  MeasData.Correction = (micros() - PPSPin.LastISRTime);
  PPSPin.LastISRTime = micros();
  //enter cricitcal section we dont want these values changed while we use them
  portENTER_CRITICAL(&timerMux);  
    MeasData.FreqPulses = FreqPin.FreqPulses;
    MeasData.Duration = FreqPin.EndTime - FreqPin.StartTime;
    FreqPin.FreqPulses = 0;
  portEXIT_CRITICAL(&timerMux);
}

// Interrupt service handelre for the frequency pulse pin
void IRAM_ATTR FreqIsr() {
  //enter cricitcal section we dont want these values changed while we use them
  portENTER_CRITICAL(&timerMux);
    if (FreqPin.FreqPulses == 0) {
      FreqPin.StartTime = micros();
    }
    FreqPin.FreqPulses += 1;
    FreqPin.EndTime = micros();
  portEXIT_CRITICAL(&timerMux);
}

// Use a callback to print that we are connecting via the config manager
static void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.printf("\nEntering Configmode via ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  displayMenu(HINTWIFICONNECT);
}

void taskButtonAndDisplay( void * parameter ) {
  uint64_t  StartBackLight = millis();       // Stores the time the backlight was switched on
  uint64_t  StartButtonPush = millis();      // Stores the time the button was last pushed
  uint64_t  StartMenuChange = millis();      // Stores the time the button was last pushed
  boolean   ButtonPressed = false;           // Stores if button was pressed
  for(;;) {
    if (digitalRead(PUSHBTN)==HIGH) {
      StartBackLight = millis();
     if (!ButtonPressed) {
        StartButtonPush = millis();
        StartMenuChange = millis(); 
        ButtonPressed = true;
        ledcWrite(blData.channel, blData.brightness);
      }
    } else {
      ButtonPressed = false;
    }
    if (((millis() - StartMenuChange) > PUSHDELAYMENU) and ButtonPressed) {
      menuServed ++; 
      StartMenuChange = millis();
    } 
    if ((millis() - StartBackLight) > BACKLIGHTTIMEOUT) { 
      ledcWrite(blData.channel, 0); 
      StartBackLight = millis();
    }
    if ((millis() - StartMenuChange) > PUSHBUTTONTIMEOUT) { 
      menuServed = DISPMEASUREMENT;
      StartMenuChange = millis();
    }
    displayMenu(menuServed);
    delay(500);
  }  
}

void taskPrintEverySecond( void * parameter ) {
   for (;;) {
    if (gps.time.age() < 1500) {
      //if we have an update we calculate the frequency
      if ((MeasData.Correction < 999900) or (MeasData.Correction > 1000100)) {
        Serial.printf("Calibrating internal clock\n");
        displayMenu(HINTCLOCKCAL);
      } else {
        if (MeasData.FreqPulses > 0) {
          if (!GPSStableFix) {
            GPSStableFix = true;
            FixStart = millis();
          }
          ledcWrite(LedData.channel, LedData.brightness);
          CorrectionFactor = 1000000.0 / (float)MeasData.Correction;
          Frequency = 0.5 * CorrectionFactor * (1000000 / ((float)MeasData.Duration / (float)(MeasData.FreqPulses-1))); // *0.5 for using the 100Hz pin
          dtostrf(Frequency, 6, 4, FrequencyString);    // Get Frequency and format it
          StationLat  = gps.location.lat();      //The Latitude of this station
          StationLon  = gps.location.lng();      //The Longitude of this station
          StationAlt  = gps.altitude.meters();   //The Altitude of this station
          sprintf(DateString, "%d/%02d/%02d", gps.date.year(), gps.date.month(), gps.date.day());   // get Date from GPS and format is
          sprintf(TimeString, "%02d:%02d:%02d", gps.time.hour(), gps.time.minute(), gps.time.second());    // get Time from GPS and format is
//          Serial.printf("Queue length: %d of %d\n",queuedDataLength,QUEUELENGTH);
          if (queuedDataLength<QUEUELENGTH) {
            strcpy (DataToSend[queuedDataLength].utc,DateString);
            strcat (DataToSend[queuedDataLength].utc," ");
            strcat (DataToSend[queuedDataLength].utc,TimeString);
            DataToSend[queuedDataLength].freq = (int)((Frequency-50)*10000);
            queuedDataLength++;
            ErrorDisplayed = false;
            Serial.printf("%s %s %s Hz\n",DateString, TimeString, FrequencyString);
          } else {
            displayMenu(ERRORSERVERLOST);
            Serial.println("ERROR: The connection to the server seems to be lost, check connection, not queueing more data");
          }
          delay(200);
          ledcWrite(LedData.channel, 0);
        } 
      }
    } else {
    // we somehow lost GPS reception nog we wait til we have a fix again for atleast 10 seconde
      GPSStableFix = false;
      Serial.printf("Lost GPS fix waiting till it is stable again ");
      displayMenu(ERRORGPSLOST);
      waitTillStable(10);
    } 
    delay(800);
  }  
}

void taskReadGPSSerial( void * parameter ) {
  for (;;) {
    while (Serial2.available()) {
      gps.encode(Serial2.read());
    }
  }
}

void taskSendViaWifi( void * parameter ) {
  unsigned long start = millis();
  unsigned long startTime; 
  String        json;
  for (;;) {
    if (GPSStableFix and (queuedDataLength>0) and ((millis() - start) > 3000)) {
      start = millis(); // reset timer
      if (WiFi.status() == WL_CONNECTED) {
//        Serial.printf("Sending %d packets\n", queuedDataLength);
        json = "{\"clID\":"+(String)UserID+",\"meas\":[" ;
        for(int i = 0; i < queuedDataLength; i++) {
          if (i>0) {json.concat(","); };
          json.concat("{\"utc\":\"" + (String)DataToSend[i].utc + "\",\"freq\":" + (String)DataToSend[i].freq + ",\"volt\":" + (String)DataToSend[i].volt + "}"); 
        }
        json.concat("]}");
        queuedDataLength = 0;
        startTime = millis();
        http.begin(hostName + dataURL);  //Specify destination for HTTP request
        http.setTimeout(httpTimeOut);
        http.addHeader("Content-Type", "application/x-www-form-urlencoded");
        httpResponseCode = http.POST(json);   //Send the actual POST request
        if (httpResponseCode > 0) {
          if (httpResponseCode == 200) {
            httpElapsedTime = millis()-startTime;
            Serial.println(http.getString() + " Data sent in " + (String)httpElapsedTime + "ms, HTTP-code: " + (String)httpResponseCode);
            httpTimeOut = 2*httpElapsedTime;
          } else {
            Serial.printf("ERROR : %d\n", httpResponseCode);
            displayMenu(ERRORSERVERLOST);
          }
        } else {
          Serial.printf("ERROR : %d\n", httpResponseCode);
        }
        http.end();
      } else {
        Serial.println("ERROR : WIFI signal lost");
        displayMenu(ERRORWIFILOST);
        WiFi.reconnect();
      }
    }
  }
}


// setup the frequency counter
void setup() {
  // Start serial monitoring
  Serial.begin(115200);
 //Starting the frequency station and check for GPS signal
  Serial.printf("\n+---------------------------------------------+\n");
  Serial.printf("| BaDu Frequency measurement starting         |\n");
  Serial.printf("|                                             |\n");
  Serial.printf("| Hardware version : %s\n",HWVersion);
  Serial.printf("| Software version : %s\n",SWVersion);
  Serial.printf("| ESP SDK version  : %s\n",ESP.getSdkVersion());
  Serial.printf("+---------------------------------------------+\n");
  // Get the unique ID for this statoin, basucally it is the MAC adres and convert it to HEX
  sprintf(chipID, "%04X%08X",(uint16_t)(ESP.getEfuseMac()>>32), (uint32_t)ESP.getEfuseMac());  
  //Set the pin for the pushbutton
  pinMode(PUSHBTN,INPUT);
  // Setup LED with PWM to control brightness
  ledcSetup(LedData.channel, 12000, 8);
  ledcAttachPin(LedData.PIN, LedData.channel);
  //Setup PWM for the backlight of the display
  ledcSetup(blData.channel, 12000, 8);
  ledcAttachPin(blData.PIN, blData.channel);
  ledcWrite(blData.channel, blData.brightness);
  // PCD8544-compatible displays may have a different resolution...
  lcd.begin(DISPLAYWIDTH, DISPLAYHEIGHT);
  lcd.setContrast(DISPLAYCONTRAST);
  
  displayMenu(DISPOPENING);
  delay(3000);
  displayMenu(DISPVERSION);
  delay(3000);
  
  // Flashing LED three times to indicate to user that we are onlin
  ledcWrite(0, LedData.brightness);
  delay(50);
  ledcWrite(0, 0);
  delay(50);
  ledcWrite(0, LedData.brightness);
  delay(50);
  ledcWrite(0, 0);
  delay(50);
  ledcWrite(0, LedData.brightness);
  delay(50);
  ledcWrite(0, 0);

  // Start connecting to a WiFi network
  Serial.printf("| Starting WiFi ...                           |\n");
  Serial.printf("+---------------------------------------------+\n");
  // WiFiManager Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Disable debug output from WiFiManager
  wifiManager.setDebugOutput(true);
  // Set the callback when we enter config mode to print this
  wifiManager.setAPCallback(configModeCallback);
  Serial.println("Push WiFi-Reset button now to reset WiFi credentials...");
  displayMenu(HINTWIFISTART);
  start = millis();
  while ((millis() - start) < 3000) {
    if (digitalRead(PUSHBTN)==HIGH) {
      ResetPushed = true;
      Serial.println(ResetPushed);
    }
    delay(1);  
  }
  //When there is no stored WiFi-network in sight or if the wifi rest was pushed, setup a Accesspoint
  wifiManager.autoConnect(configPortalName, configPortalPass); 
  // If we make it to here we are connected to a Wifi Network
  Serial.printf("\nConnected to network ");
  ConnectedSSID = wifiManager.getSSID();
  Serial.println(ConnectedSSID);
  displayMenu(DISPWIFI);
  
  if (ResetPushed == true) {
    // For testing the configportal, comment out if not testing
    Serial.print("WiFi Reset was pushed, resetting wifi credentials...");
    displayMenu(HINTWIFIRESET);
    wifiManager.resetSettings(); // Reset all setting to force going into config portal
    WiFi.disconnect(true); // Disconnect Wifi
    WiFi.begin("0","0");  
    Serial.println(" Done! Restarting ....");
    Serial.println("Connect to HSN_Frequentiemeter to set Wifi credentials for your network");
    delay(3000);
    ESP.restart();
  }
  delay(3000);
  
  Serial.printf("\n+---------------------------------------------+\n");
  Serial.printf("| Checking connectivity to webserver ...      |\n");
  Serial.printf("+---------------------------------------------+\n");
  displayMenu(HINTTESTSERVER);
  http.begin(hostName + pingURL);  //Specify destination for HTTP request
  http.setTimeout(httpTimeOut);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  unsigned long startTime = millis();
  httpResponseCode = http.POST("msg=Registering&SSID=" + ConnectedSSID);   //Send the actual POST request
  if (httpResponseCode== 200) {
    httpElapsedTime = millis()-startTime;
    Serial.println("\nSUCCESS in " + (String)httpElapsedTime + "ms, HTTP-code: " + httpResponseCode);
    httpTimeOut = 2*httpElapsedTime;
    displayMenu(HINTSERVERSUCCES);
  } else {
    Serial.println("\nError : " + httpResponseCode);
    displayMenu(ERRORSERVERLOST);
  }
  http.end();
  delay(3000);  
  // Start setting up the GPS
  // Note the format for setting a serial port is as follows: Serial2.begin(baud-rate, protocol, RX pin, TX pin);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial.printf("\n+---------------------------------------------+\n");
  Serial.printf("| Starting GPS ...                            |\n");
  Serial.printf("+---------------------------------------------+\n");
  xTaskCreate(      taskReadGPSSerial,          /* Task function. */
                    "taskReadGPSSerial",        /* String with name of task. */
                    10000,            /* Stack size in bytes. */
                    NULL,             /* Parameter passed as input of the task */
                    1,                /* Priority of the task. */
                    NULL);            /* Task handle. */
                    
  Serial.printf("\nWaiting for GPS signal");
  displayMenu(HINTGPSSTART);
  while (gps.location.isValid() != true) {
    Serial.printf(".");
    delay(1000);
  }
  //There is a valid GPS fix, now check if it is stable
  Serial.printf(" Found! \nSignal stable (10 seconds)");
  displayMenu(HINTGPSFOUND);
  waitTillStable(10);
  // GPS signal is not stale for at least 10 seconde we assume it is stable
  GPSStableFix = true;
  // Print some data on this frequency station
  Serial.printf("\n+---------------------------------------------+\n");
  StationLat  = gps.location.lat();      //The Latitude of this station
  StationLon  = gps.location.lng();      //The Longitude of this station
  StationAlt  = gps.altitude.meters();   //The Altitude of this stationt.
  //print the detected values
  Serial.printf("| Station Chip ID = %s              |\n",chipID);
  displayMenu(HINTGPSFIXED);
  
  dtostrf(StationLat, 3, 6, str_temp);
  Serial.printf("| Station is at LAT %s",str_temp); 
  dtostrf(StationLon, 3, 6, str_temp);
  Serial.printf(" LON %s    |\n",str_temp); 
  dtostrf(StationAlt, 3, 1, str_temp);
  Serial.printf("| Station is %s meters above MSL            |\n| There are %d satellites visible              |\n",str_temp, gps.satellites.value()); 
  //Print the current UJTC time reported
  Serial.printf("| UTC Time is %d/%02d/%02d %02d:%02d:%02d.%03d         |\n", gps.date.year(), gps.date.month(), gps.date.day(), gps.time.hour(), gps.time.minute(), gps.time.second(), gps.time.centisecond()); 
  FixStart = millis();
  delay(3000);  

  //GPS fix is there now setup the PPS interrupts
  pinMode(PPSPin.PIN, INPUT_PULLUP);
  attachInterrupt(PPSPin.PIN, PPSIsrOn, RISING);
  
  //Setup interrupt for frequency measuring pin
  pinMode(FreqPin.PIN, INPUT_PULLUP);
  attachInterrupt(FreqPin.PIN, FreqIsr, RISING);
  Serial.printf("+---------------------------------------------+\n");
  Serial.printf("| Everything is OK, starting measurements ... |\n");
  Serial.printf("+---------------------------------------------+\n\n");
  displayMenu(HINTSTARTUNIT);
  delay(3000);
// Create a task to calculate and print the frequency value 
  xTaskCreatePinnedToCore(  taskPrintEverySecond,          /* Task function. */
                            "taskPrintEverySecond",        /* String with name of task. */
                            10000,            /* Stack size in bytes. */
                            NULL,             /* Parameter passed as input of the task */
                            1,                /* Priority of the task. */
                            NULL,             /* Task handle. */
                            0);               /* Core to run this task */
                               
  // Create a task to send the the data via Wifi                
  xTaskCreatePinnedToCore(  taskSendViaWifi,          /* Task function. */
                            "taskSendViaWifi",        /* String with name of task. */
                            10000,            /* Stack size in bytes. */
                            NULL,             /* Parameter passed as input of the task */
                            1,                /* Priority of the task. */
                            NULL,             /* Task handle. */
                            0);               /* Core to run this task */
  // Create a task to listen to button and do display               
  xTaskCreatePinnedToCore(  taskButtonAndDisplay,          /* Task function. */
                            "taskButtonAndDisplay",        /* String with name of task. */
                            10000,            /* Stack size in bytes. */
                            NULL,             /* Parameter passed as input of the task */
                            1,                /* Priority of the task. */
                            NULL,             /* Task handle. */
                            0);               /* Core to run this task */
  MeasurementStart = millis();
  ledcWrite(blData.channel, 0);
}
             
void loop() {
  delay(1);
}
