# MQTT to/from 433MHz to/from BLYNK Gateway

This project is a MQTT gateway between a 433MHz Zap sockets/controller and the Blynk App on my iPhone.

See the following context diagram to see the various interfaces.



## Features

- Receives 433MHz messages from the Zap controller and publishes them to MQTT broker and as Blynk Virtual messages.
- Receives Blynk Virtual messages and publishes them to MQTT broker and transmits 433MHz codes. 
- Subscribes to MQTT messages and sends Blynk Virtual messages and transmits 433MHz codes.
- Reads DHT11 temperature/humidity and publishes MQTT messages.

