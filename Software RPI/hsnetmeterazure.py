#!/usr/bin/python3
from os import system

# Parse arguments
import argparse

# Setup arguments
parser = argparse.ArgumentParser(description = 'Measurement of netfrequency for https://www.netfrequentie.nl')
parser.add_argument('--client', type = int, required = True, help = 'specify your client id', metavar = '1')
parser.add_argument('--connectionstring', required = True, help = 'specify your azure IoT hub connection string')
parser.add_argument('--mode', choices = ['50Hz', '100Hz'], default = '50Hz', help = 'specify the mode to run in (default: 50Hz)')
parser.add_argument('--silent', action = 'store_true', help = 'run in silent mode (default: false)')
parser.add_argument('--verbose', action = 'store_true', help = 'run in verbose mode (default: false)')
parser.add_argument('--disable-azure', action = 'store_true', help = 'disable sending messages to azure (default: false)')
parser.add_argument('--disable-web', action = 'store_true', help = 'disable sending web messages (default: false)')
parser.add_argument('--disable-led', action = 'store_true', help = 'disable flashing led (default: false)')
parser.add_argument('--queue-size', type = int, choices = range(1, 100), default = 10, metavar = '{1-100}', help = 'specify the queue size (default: 10)')
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

from azure.iot.device import IoTHubDeviceClient, Message

# Reset all global variables
sinecount = 0
firstuptick = 0
desiredVolt = 230.0
volt = 231

# Setup variables
url = 'https://www.netfrequentie.nl/fmeting.php'
payload = { 'clID': args.client, 'meas': [] }

# Setup flags
flashled = not args.disable_led
azure = not args.disable_azure
web = not args.disable_web

def create_azure_client():
    azure_client = IoTHubDeviceClient.create_from_connection_string(args.connectionstring)
    return azure_client

def queuedata(desiredFreq, measuredFreq, measuredVolt, recordedTick):
    global payload

    if not args.silent and args.verbose:
        print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
            + " - Frequentie: {0:.4f} Hz".format(desiredFreq + 0.0001 * measuredFreq)
            + ", Volt: {0:.4f} V".format(measuredVolt)
            + ", Tick: {0}".format(recordedTick))

    # Add data to queue
    payload['meas'].append({ 'freq': measuredFreq, 'volt': measuredVolt, 'utc': round(time.time(), 3) })

    if len(payload['meas']) == args.queue_size:
        # Send if 10 in queue
        json_data = json.dumps(payload)
        senddata_azure(json_data)
        senddata_web(json_data)

        # Empty queue
        payload['meas'].clear()


def senddata_azure(json_data):
    global azure_client
    if not azure:
        return
    azure_client.send_message(json_data)

def senddata_web(json_data):
    if not web:
        return
    try:
        r = requests.post(url, data = json_data, timeout = 0.15)
        if r.status_code != 200:
            r.raise_for_status()
    except requests.exceptions.HTTPError as errh:
        if not args.silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: HTTP error - Resultbuffer: ", len(json_data['meas'])," ", str(errh).split(' ', 1)[0])
    except requests.exceptions.ConnectionError:
        if not args.silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: URL not found - Resultbuffer: ", len(json_data['meas']))
    except requests.exceptions.Timeout:
        if not args.silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: Timeout - Resultbuffer: ", len(json_data['meas']))
    except requests.exceptions.RequestException as err:
        if not args.silent:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " - ERROR: General error - Resultbuffer: ", len(json_data['meas'])," ", err)
    else:
        if not args.silent and args.verbose:
            print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
                + " -", r.text, "- Resultbuffer: ", len(payload['meas']))

# Define callbackfunction for counting the sines
def countingcallback(gpio, level, tick):
    global firstuptick
    global sinecount

    # Print timeout errors
    if level == 2 and not args.silent and args.verbose:
        print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
            + " - ERROR: Watchdog timeout")

    # Determine desired frequency
    if gpio == args.pin_50Hz:
        desiredFreq = 50
    elif gpio == args.pin_100Hz:
        desiredFreq = 100
    else:
        desiredFreq = 0

    # Store starting tick
    if sinecount == 0:
        firstuptick = tick

    sinecount += 1

    # Turn off led
    if flashled and sinecount == 8:
        pi.write(args.pin_led, 0)

    # Determine actual frequency
    if sinecount > desiredFreq:
        freq = (desiredFreq * 1000000) / (tick - firstuptick)
        queuedata(
            desiredFreq,
            round((freq * 10000) - (desiredFreq * 10000)),
            round((volt * 10) - (desiredVolt * 10)),
            tick)

        # Reset for next check
        sinecount = 1
        firstuptick = tick

        # Turn on led
        if flashled:
            pi.set_PWM_dutycycle(args.pin_led, args.led_brightness)

# Start the measurements
cb = None
pi = None
try:
    print("Starting frequency measurements, press ctrl-c to quit.\n")

    azure_client = create_azure_client()
    azure_client.connect()
    print("Azure connected!\n")

    # Main loop
    while True:
        if pi is None or not pi.connected:
            pi = pigpio.pi() 
            if pi.connected:
                print("Connected to pigpio-deamon")

                # We are connected configure pins and other stuff
                pi.set_mode(args.pin_led, pigpio.OUTPUT)
                pi.set_PWM_range(args.pin_led, 100)  # now  25 1/4,   50 1/2,   75 3/4 on

                # Start correct callback
                if cb is None:
                    if args.mode == '50Hz':
                        cb = pi.callback(args.pin_50Hz, pigpio.RISING_EDGE, countingcallback)
                        print("Using 50Hz pin to measure frequency.")
                    elif args.mode == '100Hz':
                        cb = pi.callback(args.pin_100Hz, pigpio.RISING_EDGE, countingcallback)
                        print("Using 100Hz pin to measure frequency.")

                # Wait loop
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

    pi.write(args.pin_led, 0)
    pi.stop()

    azure_client.disconnect()
    print("\nAzure disconnected!")
