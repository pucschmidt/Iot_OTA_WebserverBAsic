Iot_OTA_WebserverBasic
Webserver with OTA and file upload & execution

Trying Out GitHub

Thank you to the arduino und ESP8266 community I based this code on (see all the includes)

/*
*  This sketch initializes a wifi connection for OTA or HTTP-server 
*  with credentials either from SPIFFS File System or preconfigured in sketch 
*  
*  This sketch set up a async HTTP-REST-like server. and a parallel MQTT Client
*        REST topic must be lower case 
*    
*  curl http://IoT-Relay-1/iot/relay:on    
*  curl http://IoT-Relay-1/iot/relay:off 
*  curl http://IoT-Relay-1/status

*  board control (via http only)
*  curl http://IoT-Relay-1/status		REM  status request
*  curl http://IoT-Relay-1/otarequest		REM enter Over The Air Programming Mode (exit Mode by power or reset)
*  curl http://IoT-Relay-1/storeasdefault
*  curl http://IoT-Relay-1/setmqtt:mqtttopic   REM setMQTT 
*  curl http://IoT-Relay-1/setwifi:ssid:key:IoT-Relay-1  REM  switch to a different network 
*
*  Second interface path via MQTT client interface (no Over the Air Programming commands)
*  on channel  see config file
*		status
*		relay:on   or relay:off
*		execute/file:filename.txt/mode:loop:1 REM execute filename in mode=once/loop with timefactor=1   any /iot command stops execution
*
*  curl http://IoT-HausNr/send/file:executefile.txt   REM send (upload) file=*.* to SPIFFS File system on ESP. File can have a different PC name
*  curl http://IoT-HausNr/execute/file:executefile.txt/mode:loop:1 REM execute filename in mode=once/loop with timefactor=1   any /iot command stops execution
  executefile.txt example
	shield:relay
	relay:on/delay:2000   / delay=delay after command completion
	relay:off/delay:4000   / delay=delay after command completion
	relay:on/delay:4000   / delay=delay after command completion
	relay:off/delay:2000   / delay=delay after command completion
	rem rgb:0:0:128/time:1000/seq:0x0000ff0000000000/delay:1000  / no action    comment
	rem
	end


*
* Upload Config-File with SPIFFS_File_upload in Arduino SDK; example of config file content
* File format and content
	line1: ssid:SSID
	line2: key:wifi-key
	line3: host:host-name in DNS
	line4: ota:OTA-passcode
	line5: mqtt:mqtt-subscribtion 
	line5: board:board-type (wemosd1wifi, wemosd1mini, nodemcu)
	line6: shield:shield-type (relay)
	line7: init:initialization-string, e.g. relay:on   (relay:on,relay:off)
         init:execute/file:......  is valid, will be executed after async server is present
*/
