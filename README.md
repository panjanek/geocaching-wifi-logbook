# Geocaching gadget: WiFi logbook
Geocaching gadget: WiFi logbook
If you are new to geocaching go to https://www.geocaching.com/ .
Each geocaching hide contains small paper logbook where people log their visit using pencil. 
Additionally, geocache can contain some fun objects.
This project is a geocaching gadget: Digital logbook operated with a smartphone browser using temporary WiFi network.
After pressing a button WiFi logbook enables temporary WiFi network. 
Geocachers can connect to the network and leave a short message using browser UI. The can browse previous messages too.
This project uses ESP8266 microcontroller.

The WiFi network is:
```
network:  GeocacheLogbook
password: geocaching
```

The web page is at:
```
http://192.168.4.1
```


## Features

* Allows user to input logbook entry: nick (max, max 20 letters) and message (max 100 letters) using simple Web UI.
* Current date is also saved (taken from the smarthphone device)
* Entries saved in permanent memory
* Browsing list of stored entries in web UI.
* Enters sleep mode after 120 seconds of inactivity
* Low current in sleep mode - 20uA
* Around 80mA when active
* Low battery indicator in Web UI
* Can be powered from two AA or AAA batteries or CR123 battery (2.8V - 5V) for year at least (if used once a day)
* Admin mode under special URL:
   * Browse system log
   * Clear logbook
   * Clear system log
   * Wiec statistics
   * OTA software update
   * HTML file update

### parts

1. ESP8266 module with 4MB Flash. For example ESP-07S. Important: without builtin USB! For example: https://www.ebay.com.au/itm/384751580612 or raw module
2. Push button
3. Optionally two LEDs with 220ohm resistors
4. Serial USB converter for programming (like FT232)

### wiring

Wiring is trivial. Pushbutton connected to RST PIN.
Optional LEDs on GPIO12 and 15 with resistors

### customize to your geocache
1. Copy data_pl and data_en to data folder. This folder contains HTMLs. Edit HTMLs to your need.
2. Set administrator password in `ADMIN_PASSWORD`
   
### steps to upload code to esp8266
In Arduino IDE:

1. Install esp8266 board plugin: https://github.com/esp8266/Arduino (may require adding Board Manager URLs in File->Preferences. Add Additional Board Managers URL https://arduino.esp8266.com/stable/package_esp8266com_index.json )
2. Set: Tools -> Board -> esp8266 -> Generic ESP8266 Module
3. Set: Tools -> Flash Size -> 4MB (FS:2MB, OTA:~1019KB)
4. Compile the code
5. Connect USB Serial converter: cross TX and RX. VCC, GND. Connect GPIO0 to GND to enter software upload mode
6. Set Tooles -> Port -> To the COM port where your converter showed up
7. Upload
8. To upload HTML files from data folder install this plugin: https://github.com/earlephilhower/arduino-littlefs-upload
9. Hit `[Ctrl]` + `[Shift]` + `[P]` then "Upload LittleFS to Pico/ESP8266/ESP32".

 ## prototyping
