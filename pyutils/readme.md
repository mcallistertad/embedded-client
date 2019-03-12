# To run embedded-lib server without an API server

* Create a "response" file with the following content:
```
HTTP/1.1 200 OK

<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<LocationRS version="2.0" xmlns="http://skyhookwireless.com/wps/2005">
    <location><latitude>42.297082</latitude><longitude>-71.233282</longitude><hpe>70</hpe></location>
</LocationRS>
```
* Create a simple netcat server which emulates an API server:
```
$ while true; do nc -l 8081 < response; done
```
$ Start the embedded-lib server:
```
$ ./server.py
```
Whale, that's about it.
