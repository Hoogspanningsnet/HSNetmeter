#!/usr/bin/python3
from os import system

# Parse arguments
import argparse

parser = argparse.ArgumentParser(description = 'Measurement of netfrequency for https://www.netfrequentie.nl')
parser.add_argument('--client', type = int, required = True, help = 'specify your client id', metavar = '1')
parser.add_argument('--mode', choices = ['50Hz', '100Hz'], default = '50Hz', help = 'specify the mode to run in (default: 50Hz)')
parser.add_argument('--silent', action = 'store_true', help = 'run in silent mode (default: false)')
parser.add_argument('--verbose', action = 'store_true', help = 'run in verbose mode (default: false)')
parser.add_argument('--disable-led', action = 'store_true', help = 'disable flashing led (default: false)')
parser.add_argument('--led-brightness', type = int, choices = range(0,101), default = 100, metavar = '{0-101}', help = 'specify the led brightness (default: 100)')
parser.add_argument('--pin-led', type = int, choices = range(0,31), default = 18, metavar = '{0-31}', help = 'specify the led-pin (default: 18)')
parser.add_argument('--pin-50Hz', type = int, choices = range(0,31), default = 24, metavar = '{0-31}', help = 'specify the 50Hz-pin (default: 24)')
parser.add_argument('--pin-100Hz', type = int, choices = range(0,31), default = 23, metavar = '{0-31}', help = 'specify the 100Hz-pin (default: 23)')
args = parser.parse_args()

# Connect I/O
import os, pigpio

if os.name != 'nt':
    system("sudo pigpiod")

import pigpio, time, datetime, sys, select, os, json
import requests, copy, argparse

#define pins to use
LED_PIN = args.pin_led
VIJFTIGHZ_PIN = args.pin_50Hz
HONDERDHZ_PIN = args.pin_100Hz

#reset all global variables
sinecount = 0
firstuptick = 0
def_volt = 230.0
volt = 231
def_freq = 50.0
freq = 0
ledbright = args.led_brightness

url = 'https://www.netfrequentie.nl/fmeting.php?t='
emptypayload = {'clID': args.client, 'meas':[]}
maxpayloadlength = 10
payload = emptypayload
expert = args.verbose
silent = args.silent
flashled = not args.disable_led

def getserial():
    # Extract serial from cpuinfo file
    cpuserial = "0000000000000000"
    try:
        f = open('/proc/cpuinfo','r')
        for line in f:
            if line[0:6] == 'Serial':
                cpuserial = line[10:26]
        f.close()
    except:
        cpuserial = "0000000000000000"

    return cpuserial

def senddata(measuredFreq, measuredVolt, recordedTick):
    global payload

    if not silent and expert:
        print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
            + " - Frequentie: {0:.4f} Hz".format(def_freq+0.0001*measuredFreq)
            + ", Volt: {0:.4f} V".format(measuredVolt)
            + ", Tick: {0}".format(recordedTick))

    payload['meas'].append({ 'freq': measuredFreq, 'volt': measuredVolt, 'utc': round(time.time(), 3) })
    top10 = copy.deepcopy(payload)
    if len(top10['meas']) >= maxpayloadlength:
        top10['meas'] = top10['meas'][:maxpayloadlength]
    try:
        r = requests.post(url + str(recordedTick), data = json.dumps(top10), timeout = 0.15)
        if r.status_code != 200:
            r.raise_for_status()
    except requests.exceptions.HTTPError as errh:
        if not silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: HTTP error - Resultbuffer: ", len(payload['meas'])," ", str(errh).split(' ', 1)[0])
    except requests.exceptions.ConnectionError:
        if not silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: URL not found - Resultbuffer: ", len(payload['meas']))
    except requests.exceptions.Timeout:
        if not silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: Timeout - Resultbuffer: ", len(payload['meas']))
    except requests.exceptions.RequestException as err:
        if not silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: General error - Resultbuffer: ", len(payload['meas'])," ", err)
    else:
        if r.text.find('SUCCES') > -1:
            payload['meas'] = payload['meas'][maxpayloadlength:]
        if not silent and expert:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " -", r.text, "- Resultbuffer: ", len(payload['meas']))

#define callbackfunction for counting the sines
def countingcallback(gpio, level, tick):
    global firstuptick
    global sinecount

    # Print timeout errors
    if level == 2 and not silent and expert:
        print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
            + " - ERROR: Watchdog timeout")

    # Determine desired frequency
    if gpio == VIJFTIGHZ_PIN:
        desiredFreq = 50
    elif gpio == HONDERDHZ_PIN:
        desiredFreq = 100
    else:
        desiredFreq = 0

    if sinecount == 0:
        firstuptick = tick

    sinecount += 1

    if flashled and sinecount == 8:
        pi.write(LED_PIN, 0)    

    if sinecount > desiredFreq:
        freq = (desiredFreq * 1000000) / (tick - firstuptick)

        sinecount = 1
        firstuptick = tick
        if flashled:
            pi.set_PWM_dutycycle(LED_PIN, ledbright) 
        
        senddata(round((freq * 10000) - (def_freq * 10000)), round((volt * 10) - (def_volt * 10)), tick)

# Start the measurements
cb = None
pi = None
try:
    print ("Starting frequency measurements, press ctrl-c to quit.\n")

    # MAIN LOOP
    while True:
        if pi is None or not pi.connected:
            pi = pigpio.pi() 
            if pi.connected:
                print("Connected to pigpio-deamon")
                #we are connected configure pins and other stuff
                pi.set_mode(LED_PIN, pigpio.OUTPUT)
                pi.set_PWM_range(LED_PIN, 100)  # now  25 1/4,   50 1/2,   75 3/4 on
                # Start correct callback
                if cb is None:
                    if args.mode == '50Hz':
                        cb = pi.callback(VIJFTIGHZ_PIN, pigpio.RISING_EDGE, countingcallback)
                        print("Using 50Hz pin to measure frequency.")
                    elif args.mode == '100Hz':
                        cb = pi.callback(HONDERDHZ_PIN, pigpio.RISING_EDGE, countingcallback)
                        print("Using 100Hz pin to measure frequency.")
                while pi.connected:
                    time.sleep(0.1)
            else:    
                print ("Not connected to pigpio deamon, retrying...")
                time.sleep(5)
                continue
            
except KeyboardInterrupt:
    print ("\nFrequency measurement stopped through ctrl-c.")

    if not cb is None: 
        cb.cancel()

    pi.write(LED_PIN, 0)
    pi.stop()
