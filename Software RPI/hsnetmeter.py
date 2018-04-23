#!/usr/bin/python3
from os import system
system("sudo pigpiod")

import pigpio, time, datetime, sys, select, os, json
import requests, copy

#define pins to use
LED_PIN = 18
VIJFTIGHZ_PIN = 24
HONDERDHZ_PIN = 23

#reset all global variables
sinecount = 0
firstuptick = 0
def_volt = 230.0
volt = 231
def_freq = 50.0
ClientID = 10000
url = 'https://www.netfrequentie.nl/fmeting.php?t='
emptypayload = {'clID': ClientID, 'meas':[]}
maxpayloadlength = 10
payload = emptypayload
expert = 1
silent = 1

#define callbackfunction for counting the sines
def countingcallback(gpio, level, tick):
    global firstuptick
    global sinecount
    global payload
    if sinecount == 0:
       firstuptick = tick
    sinecount += 1
    if sinecount == 8:
       pi.write(LED_PIN, 0)
    if sinecount > 50:
 #if sinecount > 100: #change to 50 for 50Hz pin
       freq = 50000000/(tick-firstuptick)
       if silent != 1:
          if expert == 1:
             print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f') + " - Frequentie: {0:.4f} Hz".format(freq),end="")
       sinecount = 1
       firstuptick = tick
       pi.write(LED_PIN, 1)
       payload['meas'].append({'freq': round((freq*10000)-(def_freq*10000)),'volt': round((volt*10)-(def_volt*10)),'utc':round(time.time(),3)})
       top10 = copy.deepcopy(payload)
       if len(top10['meas']) >= maxpayloadlength:
          top10['meas'] = top10['meas'][:maxpayloadlength]
       try:
          r = requests.post(url+str(tick), data=json.dumps(top10), timeout=0.15)
          if r.status_code != 200:
             r.raise_for_status()
       except requests.exceptions.HTTPError as errh:
          if silent != 1:
             print (" - ERROR: HTTP error - Resultbuffer: ", len(payload['meas'])," ", str(errh).split(' ', 1)[0])
       except requests.exceptions.ConnectionError:
          if silent != 1:
             print (" - ERROR: URL not found - Resultbuffer: ", len(payload['meas']))
       except requests.exceptions.Timeout:
          if silent != 1:
             print (" - ERROR: Timeout - Resultbuffer: ", len(payload['meas']))
       except requests.exceptions.RequestException as err:
          if silent != 1:
             print (" - ERROR: General error - Resultbuffer: ", len(payload['meas'])," ", err)
       else:
          if r.text.find('SUCCES') > -1:
             payload['meas'] = payload['meas'][maxpayloadlength:]
          if silent != 1:
             if expert == 1:
                print(" -", r.text, "- Resultbuffer: ", len(payload['meas']))

#start the mesurements
try:
   print("Starting frequency measurements, press ctrl-c to quit.\n")
   pi = pigpio.pi()
   pi.set_mode(LED_PIN, pigpio.OUTPUT)
   cb = pi.callback(VIJFTIGHZ_PIN, pigpio.RISING_EDGE, countingcallback)
#cb = pi.callback(HONDERDHZ_PIN, pigpio.RISING_EDGE, countingcallback)
   while True:
      time.sleep(0.1)
except KeyboardInterrupt:
   print ("\nFrequency measurement stopped through ctrl-c.")
   cb.cancel()
   pi.write(LED_PIN, 0)
   pi.stop()
