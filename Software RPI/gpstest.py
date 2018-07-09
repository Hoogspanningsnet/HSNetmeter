#!/usr/bin/python3
from os import system 
#system("sudo pigpiod")

import pigpio, time, datetime, sys, select, os, json, serial
import requests, copy

#define pins to use
LED_PIN = 18
PPS_PIN = 4
prev_tick = 0
timestr = ['','']
hours = 0
minutes = 0
seconds = 0

#define callbackfunction for receiving a pps pulse
def ppsoncallback(gpio, level, tick):
    global prev_tick
    global timestr
    global hours
    global minutes
    global seconds
    
    pi.write(LED_PIN, 1)
    tickDiff = tick - prev_tick
    print (datetime.datetime.utcnow().strftime('%H:%M:%S:%f') + " | " + str(hours) + ":"+ str(minutes) + ":" + str(seconds) + "." + str(timestr[1]) + " - PPS received, difference: " + str(tickDiff-1000000) + " us")
    prev_tick = tick;

def ppsoffcallback(gpio, level, tick):
    pi.write(LED_PIN, 0)
	
#start the measurements
try:
    print("Starting PPS receiver, press ctrl-c to quit.\n")
    pi = pigpio.pi()
    pi.set_mode(LED_PIN, pigpio.OUTPUT)
    pi.set_mode(PPS_PIN, pigpio.INPUT)
    cbon = pi.callback(PPS_PIN, pigpio.RISING_EDGE, ppsoncallback)
    cboff = pi.callback(PPS_PIN, pigpio.FALLING_EDGE, ppsoffcallback)
    ser = serial.Serial('/dev/ttyS0')  # open serial port
    print("Opened serial port " + ser.name)
    while True:
        line = str(ser.readline())
        if line.find('GPZDA') > 0:
            words=line.split(",")
            timestr=words[1].split(".")
            hours   = int(str(timestr[0][0:2]))
            minutes = int(str(timestr[0][2:4]))
            seconds = int(str(timestr[0][4:6]))+1
            days = int(str(words[2]))
            months = int(str(words[3]))
            years = int(str(words[4]))
            
            # we need to increment the seconds as the GPZDA string contains the time of the last PPS-pulse
            # This not a really safe way of doing things as we can lose a fix between GPZDA string and PPS-pulse
            # any improvements are greatly appreciated
            if seconds > 59:
                seconds = 0
                minutes = minutes +1
                if minutes > 59:
                    minutes = 0
                    hours = hours +1
                    if hours >23:  
                        hours = 0
                        #needs expanding to day/month/year rollover 
                        
except KeyboardInterrupt:
    print ("\nPPS detection stopped through ctrl-c.")
    cbon.cancel()
    cboff.cancel()
    pi.write(LED_PIN, 0)
    pi.stop()
    
