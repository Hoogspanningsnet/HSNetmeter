#!/usr/bin/python3
from os import system

# Parse arguments
import argparse

parser = argparse.ArgumentParser(description = 'Measurement of netfrequency for https://www.netfrequentie.nl')
parser.add_argument('--mode', choices = ['50Hz', '100Hz'], default = '50Hz', help = 'specify the mode to run in (default: 50Hz)')
parser.add_argument('--silent', action = 'store_true', help = 'run in silent mode (default: false)')
parser.add_argument('--verbose', action = 'store_true', help = 'run in verbose mode (default: false)')
parser.add_argument('--disable-led', action = 'store_true', help = 'disable flashing led (default: false)')
parser.add_argument('--pin-led', type = int, choices = range(0,31), default = 18, metavar = '{0-31}', help = 'specify the led-pin (default: 18)')
parser.add_argument('--pin-50Hz', type = int, choices = range(0,31), default = 24, metavar = '{0-31}', help = 'specify the 50Hz-pin (default: 24)')
parser.add_argument('--pin-100Hz', type = int, choices = range(0,31), default = 23, metavar = '{0-31}', help = 'specify the 100Hz-pin (default: 23)')
args = parser.parse_args()

import pigpio, time, datetime, json
import socket, asyncio, websockets, janus

#define pins to use
LED_PIN = args.pin_led
VIJFTIGHZ_PIN = args.pin_50Hz
HONDERDHZ_PIN = args.pin_100Hz

expert = args.verbose
silent = args.silent
flashled = not args.disable_led

freq_data = []
loop = asyncio.get_event_loop()
freq_queue = janus.Queue(loop=loop)

connected = set()
counter = 0

async def socketloop(websocket, path):
    print ("websocket connected: '{}:{}'".format(*websocket.remote_address))
    connected.add(websocket)
    try:
        while True:
            message = await freq_queue.async_q.get()
            await websocket.send(message)
            freq_queue.async_q.task_done()
    except websockets.exceptions.ConnectionClosed:
        print ("websocket closed: '{}:{}".format(*websocket.remote_address))
    finally:
        connected.remove(websocket)
        freq_queue.async_q.task_done()

#define callbackfunction for counting the sines
def countingcallback(gpio, level, tick):
    global freq_data
    global counter

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
        print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f')
            + " - ERROR: Invalid pin")

    freq_data.append(tick)

    if len(freq_data) > desiredFreq:
        firstuptick = freq_data.pop(0)
        freq = (desiredFreq * 1000000) / (tick - firstuptick)

        if len(connected) > 0: 
            counter += 1

            # if lowered it might crash the clientside
            if counter > 2:
                json = "{\"tick\":" + str(round(time.time(), 3)) + ",\"freq\":" + str(round(freq, 4)) + "}"
                freq_queue.sync_q.put(json)
                freq_queue.sync_q.join()
                counter = 0

# Start the measurements
cb = None
pi = None
try:
    print ("Starting frequency measurements, press ctrl-c to quit.\n")
    if pi is None or not pi.connected:
        pi = pigpio.pi()

    pi.set_mode(LED_PIN, pigpio.OUTPUT)

    # Start correct callback
    if cb is None:
        if args.mode == '50Hz':
            cb = pi.callback(VIJFTIGHZ_PIN, pigpio.RISING_EDGE, countingcallback)
        elif args.mode == '100Hz':
            cb = pi.callback(HONDERDHZ_PIN, pigpio.RISING_EDGE, countingcallback)

    # Loop indefinitly
    hostname = socket.getfqdn()
    print ("websocket opened at: " + hostname + ":6789")
    loop.run_until_complete(websockets.serve(socketloop, None, 6789))
    loop.run_forever() 

except KeyboardInterrupt:
    print ("\nFrequency measurement stopped through ctrl-c.")

    if not cb is None:
        cb.cancel()

    pi.write(LED_PIN, 0)
    pi.stop()
