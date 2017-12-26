/*
*  This sketch initializes a wifi connection for OTA or HTTP-server 
*  with credentials either from SPIFFS File System or preconfigured in sketch 
*  
*  This sketch set up a async HTTP-REST-like server. and a parallel MQTT Client
*        REST topic must be lower case  [.../iot/...] or [/status]
*    
*  curl http://IoT-Relay-1/iot/relay:on    
*  curl http://IoT-Relay-1/iot/relay:off 
*  curl http://IoT-Relay-1/status

*  board control (via http only)
*  curl http://IoT-Relay-1/status		REM  status request
*  curl http://IoT-Relay-1/otarequest		REM enter Over The Air Programming Mode (exit Mode by power or reset)
*  curl http://IoT-Relay-1/storeasdefault
*  curl http://IoT-Relay-1/setmqtt:mqtttopic   REM setMQTT [mqtttopic]
*  curl http://IoT-Relay-1/setwifi:ssid:key:IoT-Relay-1  REM  switch to a different network by setwifi [ssid][password][host] ==> storeasdefault ==> reset
*
*  Second interface path via MQTT client interface (no Over the Air Programming commands)
*  on channel  see config file
*		status
*		relay:on   or relay:off
*		execute/file:[filename.txt]/mode:loop:1 REM execute filename in mode=once/loop with timefactor=1   any /iot command stops execution
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

/*	OPEN POINTS from testing
1.) 
*/


#include <stdio.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Hash.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>


#define SERIAL_DEBUG true  // if defined debug with serial output

// Load WLAN credentials from file system
// SPIFFS File Names
#define WIFICONFIGFILE "/wificonfig.txt"
#define CODEFILE "/codefile.txt"

boolean wemos_d1_mini=false;
boolean wemos_d1_wifi=false;
boolean nodemcu=false;

boolean relayshield = false;


// preconfigured WLAN, Download with Boot Loader
const char* ssid_init = "ssid*********";
const char* password_init = "key***********";
const char* host_init = "esp8266";
const char* ota_password_init = "ota8266";
const char* boardinfo_init = "nodemcu";
const char* shieldinfo_init = "rgbwled";
const char* powerup_init = "relay:on";
const char* mqttTopic_init = "esp8266default";

const char* http_username = "admin";
const char* http_password = "admin";



#define WIFITRAILS 200 // will establish access point after unsuccessful wifi-client connection-trails
// Default WiFi connection information. access point
const char* ap_hostname = "ESP8266"; // Hostename. The setup function adds the Chip ID at the end.
const char* ap_default_ssid = "esp8266"; // Default SSID.
const char* ap_default_psk = "esp8266ap"; // Default key

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
Ticker multiTimer;

// Wifi server structur
AsyncWebServer server(80);
// MQTT client
AsyncMqttClient mqttClient;
#define MQTT_BROKER IPAddress(192, 168, 178, 75) // Raspi mosquitto mqtt broker ethernet
// #define MQTT_BROKER IPAddress(192, 168, 178, 71) // Arduino Yun1 OPenWRT mqtt broker ethernet
Ticker mqttReconnectTimer;


// ms loop time for LED hardbeat
#define HARDBEAT 20000L

/* // Pin layout for Arduino yun und mega2560
#define D0 0 // GPIOn maps to Ardiuno D0
#define D1 1 // GPIOn maps to Arduino D1
#define D2 2 // GPIO16 maps to Ardiuno D2
#define D3 3 // GPIO5 maps to Ardiuno D3
#define D4 4 // GPIO4 maps to Ardiuno D4
#define D5 5 // GPIO0 maps to Ardiuno D5
#define D6 6 // GPIO2 maps to Ardiuno D6
#define D7 7// GPIO14 maps to Ardiuno D7
#define D8 8 // GPIO12 maps to Ardiuno D8
#define D9 9 // GPIO13 maps to Ardiuno D9
#define D10 10 // GPIO15 maps to Ardiuno D10
#define D11 11 // GPIO13 maps to Ardiuno D11
#define D12 12 // GPIO12 maps to Ardiuno D12
#define D13 13 // GPIO14 maps to Ardiuno D13
#define LED_BUILTIN 0
#define PIN_WIRE_SDA (20)
#define PIN_WIRE_SCL (21)
#define SDA = PIN_WIRE_SDA;
#define SCL = PIN_WIRE_SCL;
*/
/*
// Pin layout for WeMos D1 WiFi Arduino
#define D0 3 // GPIO3 maps to Ardiuno D0
#define D1 5 // GPIO1 maps to WeMos D1 D1
#define D2 16 // GPIO16 maps to Ardiuno D2
#define D3 5 // GPIO5 maps to Ardiuno D3
#define D4 4 // GPIO4 maps to Ardiuno D4
#define D5 14 // GPIO14 maps to Ardiuno D5
#define D6 12 // GPIO12 maps to Ardiuno D6
#define D7 13 // GPIO13 maps to Ardiuno D7
#define D8 0 // GPIO0 maps to Ardiuno D8
#define D9 2 // GPIO2 maps to Ardiuno D9
#define D10 15 // GPIO15 maps to Ardiuno D10
#define D11 13 // GPIO13 maps to Ardiuno D11
#define D12 12 // GPIO12 maps to Ardiuno D12
#define D13 14 // GPIO14 maps to Ardiuno D13
#define LED_BUILTIN 2
#define PIN_WIRE_SDA (4)
#define PIN_WIRE_SCL (5)
#define SDA = PIN_WIRE_SDA;
#define SCL = PIN_WIRE_SCL;
*/

// Pin Layout and GPIO-Mapping for WeMos D1 Mini, NodeMCU
#define D0 16 // GPIO16 maps to WeMos D1 Mini D0
#define D1 5 // GPIO5 maps to WeMos D1 Mini D1
#define D2 4 // GPIO04 maps to WeMos D1 Mini D2
#define D3 0 // GPIO0 maps to WeMos D1 Mini D3
#define D4 2 // GPIO2 maps to WeMos D1 Mini D4
#define D5 14 // GPIO14 maps to WeMos D1 Mini D5
#define D6 12 // GPIO12 maps to WeMos D1 Mini D6
#define D7 13 // GPIO13 maps to WeMos D1 Mini D7
#define D8 15 // GPIO15 maps to WeMos D1 Mini D8
#define RX 3 // GPIOx maps to WeMos D1 Mini xx
#define TX 1 // GPIOx maps to WeMos D1 Mini xx
#define A0 0 // Analog In 0 maps to WeMos D1 Mini A0
#define LED_BUILTIN D4 // board LED
#define PIN_WIRE_SDA (4)
#define PIN_WIRE_SCL (5)
#define SDA = PIN_WIRE_SDA;
#define SCL = PIN_WIRE_SCL;

#define RELAY_SHIELD D1 // WeMos D1 Mini Relay Shield

long timeout = 0;

int ota_flash = 0;
int i,j;
int relaystage = 0;


String ssid;
String password;
String host;
String otapassword;
String boardinfo;
String shieldinfo;
String powerup;
String mqtttopic;
String filereceived;
File uploadFile;
String executefilename;
File executefile;
String content;
long executefactor=1;
bool executeloop=false,executeactive=false;
Ticker executetimer;

char answer1[256];
String answer="";

String req="", req2="";
char *rest;
String seqvalue="";
int seqlength;
char seqbuffer[5];
int outvalue;


boolean ledindicator=true;
int pingtime,wifitrail;
unsigned long millis_offset=0;
unsigned long actualtime, timestamp=0L;

int Min(int a, int b) {
	int c;
	if (a < b) c = a; else b = a;
	return c;
}
int Max(int a, int b) {
	int c;
	if (a > b) c = a; else b = a;
	return c;
}


// WIFI connection handling
void connectToWifi() {
	Serial.print("Connecting to Wi-Fi...");
	WiFi.persistent(false);
	WiFi.mode(WIFI_OFF);   // this is a temporary line, to be removed after SDK update to 1.5.4
	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid.c_str(), password.c_str());
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
	Serial.println("Connected to Wi-Fi.");

	server.begin(); // server started
#ifdef SERIAL_DEBUG
	Serial.println("TCP-server port 80 started");
#endif
	MDNS.begin(host.c_str());
#ifdef SERIAL_DEBUG
	Serial.println("MDNS responder started");
	Serial.println("MQTT connection requested");
#endif
	connectToMqtt();
	digitalWrite(LED_BUILTIN, HIGH);   // turn the LED off 
	ledindicator = true;
	wifitrail = 0;

}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
	Serial.println("Disconnected from Wi-Fi.");
	mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
	wifiReconnectTimer.once(2, connectToWifi);
	wifitrail++;
}




// MQTT async communication
void connectToMqtt() {
	Serial.println("Connecting client to MQTT...");
	mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
	Serial.println("** Connected to the broker **");
	Serial.print("Session present: ");
	Serial.println(sessionPresent);
	uint16_t packetIdSub = mqttClient.subscribe(mqtttopic.c_str(), 2);
	Serial.print("Subscribing at QoS 2, packetId: ");
	Serial.println(packetIdSub);
	String mqttlog = host+":online/board:" + boardinfo + "/shield:" + shieldinfo;
	uint16_t packetIdPub1 = mqttClient.publish("mqttlog", 0, true, mqttlog.c_str());
	Serial.print("Publishing at QoS 0, packetId: ");
	Serial.println(packetIdPub1);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
	Serial.println("** Disconnected from the broker **");
	Serial.print("Reason : ");
	Serial.println((int)reason);
	Serial.println("Reconnecting to MQTT...");
	if (WiFi.isConnected()) {
		mqttReconnectTimer.once(2, connectToMqtt);
	}
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
	Serial.println("** Subscribe acknowledged **");
	Serial.print("  packetId: ");
	Serial.println(packetId);
	Serial.print("  qos: ");
	Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
	Serial.println("** Unsubscribe acknowledged **");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
	Serial.print("** Publish received **");
	Serial.print("  topic: ");
	Serial.println(topic);
	Serial.print("  qos: ");
	Serial.print(properties.qos);
	Serial.print("  dup: ");
	Serial.print(properties.dup);
	Serial.print("  retain: ");
	Serial.print(properties.retain);
	Serial.print("  len: ");
	Serial.print(len);
	Serial.print("  index: ");
	Serial.print(index);
	Serial.print("  total: ");
	Serial.println(total);
// ****************************************************************************************************
// mqtt command execution
// ****************************************************************************************************
	if (executeactive) {  // stop execution if active
		executeactive = false;
		executetimer.detach();
		executefile.close();
	}

	req = payload;
	req = req.substring(0,(int)len); // truncate to valid area of payload char*
	req = "/" + req;  // comply with REST format
	if (req.indexOf("/execute") != -1) {
		answer = implementexecute(req);
		sprintf(answer1, "start executing   %s", req.c_str());
		Serial.println(answer1);
	}
	else if (req.indexOf("/mqttping") != -1) {
		sprintf(answer1, "host:%s/hardbeat:%d", host.c_str(), (long)((float)(millis() - millis_offset) / 1000.0));
		uint16_t packetIdPub1 = mqttClient.publish("mqtthartbeat", 0, true, answer1);		// answer with hartbeat
		Serial.println(answer1);
	}
	else {
		Serial.println(req);
		answer = implementOrder(req);
	}
	// disregard, no answer on MQTT
}

void onMqttPublish(uint16_t packetId) {
	Serial.println("** Publish acknowledged **");
	Serial.print("  packetId: ");
	Serial.println(packetId);
}


void delayedrestart() {
	ESP.restart();
}

String implementOrder(String req) {
	/* ******************************************************************************************
	*
	*  APPLICATION CODE   Determine Action
	*
	* *******************************************************************************************
	*/
	long executenext=10;
	String reqcase = req; // case sensitive needed for crgb
	req.toLowerCase();  // execute lower case strings

	Serial.println("implement: " + req);

	if (executeactive == true) {
		if (req.indexOf("/delay:") != -1) {
			req2 = req.substring(req.indexOf("/delay:") + 7, req.length());
			executenext = strtol(req2.c_str(), &rest, 0);
			if ((executenext < 10)) executenext = 10;
		}
		else executenext = 1000; //default 1000ms
		Serial.println(executenext*executefactor);
		executetimer.once_ms((uint32_t)(executenext*executefactor), executetillend);


	}
//	Serial.println(req);

	if (wemos_d1_mini && relayshield) {  // Set RELAY_SHIELD according to the request
		relaystage = -1;
		if (req.indexOf("/relay:on") != -1)
			relaystage = 1;
		else if (req.indexOf("/relay:off") != -1)
			relaystage = 0;
		if (relaystage != -1) {
			digitalWrite(RELAY_SHIELD, relaystage);
			powerup = (relaystage) ? "on" : "off"; // update powerup string
			sprintf(answer1,"relay:%s",powerup.c_str());
			// Prepare the response
			powerup = answer1;
		}
		else return("Wrong Request");
	}


	else return("Wrong Request");

	return(answer1);
}

String implementexecute(String req) {
	Serial.println(req.c_str());
	req2 = req.substring(req.indexOf("/file") + 6, req.length());
	executefilename = req2.substring(0, req2.indexOf("/"));
	Serial.println(executefilename.c_str());
	Serial.println(req2.c_str());

	if (executefilename == "default") executefilename = CODEFILE;
	req2 = req.substring(req.indexOf("/mode") + 6, req.length());
	executeloop = false;
	Serial.println(req2.c_str());
	if (req2.indexOf("loop") != -1) executeloop = true;
	req2 = req2.substring(req2.indexOf(":") + 1, req2.length());
	executefactor = strtol(req2.c_str(), &rest, 0);
	if (executefactor < 1) executefactor = 1;  // only positive factor >=1
	executefile = SPIFFS.open(executefilename, "r");
	if (!executefile) {
		answer = "Failed to open: " + executefilename;
		return answer;
	}
	content = SPIFFSreadline(executefile);
	if (content.indexOf("shield:") != -1) req2 = content.substring(content.indexOf("shield:") + 7, content.length());
	if (req2 == shieldinfo) {
		executeactive = true;
		req = SPIFFSreadline(executefile);
		while (req.indexOf("rem ") != -1) req = SPIFFSreadline(executefile);

		req = "/" + req;  // comply with REST format
		answer = implementOrder(req); // execute request
		answer = "execute first command:  " + answer;

	}
	else answer = "Executefile for wrong shield";

	return answer;
}

void executetillend() {
	String content;

	content = SPIFFSreadline(executefile);
	while (content.indexOf("rem ") != -1) content = SPIFFSreadline(executefile);
	if (content == "end")
		if (executeloop) {
			executefile.close();
			executefile = SPIFFS.open(executefilename, "r");
			content = SPIFFSreadline(executefile); // shield already checked, go to first command
			content = SPIFFSreadline(executefile);
			while (content.indexOf("rem ") != -1) content = SPIFFSreadline(executefile);
		}
		else {
			executefile.close();
			executeactive = false;
			answer = "execute file ended  ";
		}
		if (executeactive == true) {
			content = "/" + content;
			answer = implementOrder(content); // execute request
			answer = "execute command:  " + answer;

		}
		Serial.println(answer.c_str());
}

// read one line of a SPIFFS textfile 
String SPIFFSreadline(File textfile)
{
	String content = textfile.readStringUntil('\n');
	if (content[content.length()] == '\r')
		content.substring(0, content.length() - 1); // if windows textfile \r\n cut last char
	content.trim(); // remove spaces
	return content;
}

// Save and Load WLAN credentials from file system
bool loadConfig(String *ssid, String *pass, String *host, String *otapass, String *mqtttopic, String *boardtype, String *shieldtype, String *powertype)
{
	String content;
	// open file for reading.
	File configFile = SPIFFS.open(WIFICONFIGFILE, "r");
	if (!configFile) {
#ifdef SERIAL_DEBUG
		Serial.print("Failed to open: ");
		Serial.println(WIFICONFIGFILE);
		Serial.println("SPIFFS Root content:");
		Dir dir = SPIFFS.openDir("/");
		while (dir.next()) {
			String fileName = dir.fileName();
			int fileSize = (int)dir.fileSize();
			Serial.print("FS File: ");
			Serial.print(fileName);
			Serial.print("  size: ");
			Serial.println(fileSize);
		}
#endif
		return false;
	}
	// Read content from config file.
	do {
		content = SPIFFSreadline(configFile);
		if (content.indexOf("ssid:") != -1) *ssid = content.substring(content.indexOf("ssid:") + 5, content.length());
		if (content.indexOf("key:") != -1) *pass = content.substring(content.indexOf("key:") + 4, content.length());
		if (content.indexOf("host:") != -1) *host = content.substring(content.indexOf("host:") + 5, content.length());;
		if (content.indexOf("ota:") != -1) *otapass = content.substring(content.indexOf("ota:") + 4, content.length());
		if (content.indexOf("mqtt:") != -1) *mqtttopic = content.substring(content.indexOf("mqtt:") + 5, content.length());
		if (content.indexOf("board:") != -1) *boardtype = content.substring(content.indexOf("board:") + 6, content.length());
		if (content.indexOf("shield:") != -1) *shieldtype = content.substring(content.indexOf("shield:") + 7, content.length());
		if (content.indexOf("init:") != -1) *powertype = content.substring(content.indexOf("init:") + 5, content.length());
	} while ((content != "") && (content != "end"));

	wemos_d1_mini = false;
	wemos_d1_wifi = false;
	nodemcu = false;

	if (*boardtype == "wemosd1wifi")
		wemos_d1_wifi = true;
	else if (*boardtype == "wemosd1mini")
		wemos_d1_mini = true;
	else if (*boardtype == "nodemcu")
		nodemcu = true;
	//	else return all false;

	if (*shieldtype == "relay")
		relayshield = true;
	
	if (content == "end")
		return true;
	else
		return false;
} // loadConfig

  // Save WiFi credentials to configuration file.
bool saveConfig(String *ssid, String *pass, String *host, String *otapass, String *mqtttopic, String *boardtype, String *shieldtype, String *powertype)
{
	// Open config file for writing.
	File configFile = SPIFFS.open(WIFICONFIGFILE, "w");
	if (!configFile)
	{
		//    Serial.println("Failed to open wificonfig.txt for writing");
		return false;
	}

	// Save SSID and PSK.
	configFile.print("ssid:");
	configFile.println(*ssid);
	configFile.print("key:");
	configFile.println(*pass);
	configFile.print("host:");
	configFile.println(*host);
	configFile.print("ota:");
	configFile.println(*otapass);
	configFile.print("mqtt:");
	configFile.println(*mqtttopic);
	configFile.print("board:");
	configFile.println(*boardtype);
	configFile.print("shield:");
	configFile.println(*shieldtype);
	configFile.print("init:");
	configFile.println(*powertype);

	configFile.print("end");

	configFile.close();

	return true;
}

unsigned long timedelay(unsigned long delta)   // delay by delta ms  ?????????????????????  use Ticker
{
	timestamp += delta;

	while ((actualtime != millis()) && (delta > 1L)) {
		delta--;
		actualtime++;
	}
	yield();
	if (delta > 0L) {
		delay(delta);
		actualtime = millis();
	}
	return actualtime;
}


void setup() {
	boolean wificonnected;
	wifitrail = 0;
	millis_offset = millis();
	timestamp = 0L;
	actualtime = millis();

#ifdef SERIAL_DEBUG
	Serial.begin(115200);
	Serial.println(" ");
	Serial.print("start Booting: ");
	Serial.println(millis());
#endif

	SPIFFS.begin(); // access file system
 
	bool config_flag=loadConfig(&ssid, &password, &host, &otapassword, &mqtttopic, &boardinfo, &shieldinfo, &powerup); // load wificonfig
  
	if (!config_flag)
	{
		Serial.println("wificonfig missing, booting with default network");
		ssid = ssid_init;
		password = password_init;
		host = host_init;
		otapassword = ota_password_init;
		mqtttopic = mqttTopic_init;
		boardinfo = boardinfo_init;
		shieldinfo = shieldinfo_init;
		powerup = powerup_init;
	}

	// initialize builtin led pin as an output.
	if (!wemos_d1_wifi) { // stay alive indication
		pinMode(LED_BUILTIN, OUTPUT);
		digitalWrite(LED_BUILTIN, HIGH);  // Board LED off
	}
	analogWriteFreq(255); // tries to reduce load for ESP8266 
	analogWriteRange(255);

  // ****************************************************************************************************
  // Hardware specific initialization
  // ****************************************************************************************************
	if (powerup.indexOf("execute") != -1) {
		content = powerup;  // store command initialize IO Black&off
		if (relayshield) powerup = "relay:off";
	}
	else content = "";

	if (wemos_d1_mini && relayshield) { // initialize and Set RELAY_SHIELD
		pinMode(RELAY_SHIELD, OUTPUT);
		digitalWrite(RELAY_SHIELD, LOW); 
		relaystage = -1; // Set RELAY_SHIELD according to powerup request
		if (powerup.indexOf("on") != -1)
			relaystage = 1;
		else if (powerup.indexOf("off") != -1)
			relaystage = 0;
		if (relaystage != -1) 
			digitalWrite(RELAY_SHIELD, relaystage);
		}
	
	/* ******************************************************************************************
	*
	*  APPLICATION CODE   setup Wifi, MQTT and Http_TCP
	*
	* *******************************************************************************************
	*/
#ifdef SERIAL_DEBUG
 	 Serial.print("Initialize WiFi to: ");
	 Serial.println(ssid.c_str());
	 Serial.print("Name:  ");
	 Serial.println(host.c_str());
	 Serial.print("Key:  ");  
//	 Serial.print(password.c_str()); // debug only
	 Serial.println("xx");
	 Serial.print("MQTT Subscribtion:  ");
	 Serial.println(mqtttopic);
#endif
	 // Initialize Wifi as Station
	wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
	wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
	WiFi.mode(WIFI_STA);
	WiFi.hostname(host.c_str());

	// Initialize TCP
	MDNS.addService("http", "tcp", 80);


	// Evaluate the requests all boards  new application "iot" request
	server.on("/iot", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		req = request->url();
		req = req.substring(4,req.length());  // cut off  /iot
		answer = implementOrder(req); // execute request
		request->send(200, "text/plain", answer);
		Serial.println(answer.c_str());
	});

	server.on("/execute", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		req = request->url();
		answer = implementexecute(req);

		sprintf(answer1,"start executing   %s", req.c_str());
		request->send(200, "text/plain", answer1);
		Serial.println(answer1);
	});
	
	server.on("/otarequest", HTTP_GET, [](AsyncWebServerRequest *request) {
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		ota_flash = 1; // OTA Flash request by http
		sprintf(answer1,"%s  ready for OTA - release code only - no debug over wlan",host.c_str());
		request->send(200, "text/plain", answer1);
		Serial.println(answer1);
	});

	server.on("/storeasdefault", HTTP_GET, [](AsyncWebServerRequest *request) { // save default in File wificonfig.txt
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		saveConfig(&ssid, &password, &host, &otapassword, &mqtttopic, &boardinfo, &shieldinfo, &powerup);
		answer = host + "  default stored";
		request->send(200, "text/plain",answer);
		Serial.println(answer.c_str());
	});

	server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {  // report status
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		answer = host + "/board:" + boardinfo + "/shield:" + shieldinfo + "/mqtt:" + mqtttopic + "/Vers:" + __DATE__ + "-" + __TIME__ + "/status:" + powerup;
		request->send(200, "text/plain", answer);
		Serial.println(answer.c_str());
	});
	// switch to a different network by setwifi ==> storeasdefault ==> reset
	server.on("/setwifi", HTTP_GET, [](AsyncWebServerRequest *request) {    // switch to a different network by setwifi ==> storeasdefault ==> reset
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		req = request->url();
		req2 = req.substring(req.indexOf("/setwifi") + 9, req.length());
		ssid = req2.substring(0, req2.indexOf(":"));
		req2 = req2.substring(req2.indexOf(":") + 1, req2.length());
		password = req2.substring(0, req2.indexOf(":"));
		req2 = req2.substring(req2.indexOf(":") + 1, req2.length());
		host = req2.substring(0, req2.indexOf(" HTTP"));

		saveConfig(&ssid, &password, &host, &otapassword, &mqtttopic, &boardinfo, &shieldinfo, &powerup);
		answer = "Network changed to : ";
		answer += ssid;
		answer += "\r\n    Password : ";
		answer += password;
		answer += "\r\n    Hostname : ";
		answer += host;
		answer += "    restart in 3sec</html>\n";
		request->send(200, "text/plain", answer);
		Serial.printf(answer.c_str());

		multiTimer.once(3, delayedrestart);
	});
	// set different  MQTT Subsribtion
	server.on("/setmqtt", HTTP_GET, [](AsyncWebServerRequest *request) {    // switch to a different network by setwifi ==> storeasdefault ==> reset
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		req = request->url();
		req2 = req.substring(req.indexOf("/setmqtt:") + 9, req.length());
		mqtttopic = req2.substring(0, req2.indexOf(" HTTP"));

		saveConfig(&ssid, &password, &host, &otapassword, &mqtttopic, &boardinfo, &shieldinfo, &powerup);
		answer = "MQTT Subscribtion changed to : ";
		answer += mqtttopic;
		answer += "    restart in 3sec</html>\n";
		request->send(200, "text/plain", answer);
		Serial.printf(answer.c_str());

		multiTimer.once(3, delayedrestart);

	});


	// HTML web server with limited access
	server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {    // switch to a different network by setwifi ==> storeasdefault ==> reset
		request->send(200, "text/plain", host+"  ESP8266 iot web server");
	});

	server.on("/send", HTTP_GET, [](AsyncWebServerRequest *request) {    // send file to esp
		if (executeactive) {  // stop execution if active
			executeactive = false;
			executetimer.detach();
			executefile.close();
		}
		req = request->url();
		if (req.indexOf("/file:") != -1) {
			req2 = req.substring(req.indexOf("/file:") + 6, req.length());
		}
		else req2 = CODEFILE;
		if (req2=="default") 
			filereceived = CODEFILE;
		else filereceived = req2;

		request->send(SPIFFS, "/send.html");
		Serial.printf("%d send.html downloaded to client: %s\n", millis(),filereceived.c_str());
	});

	server.onFileUpload([](AsyncWebServerRequest *request, const String& filepath, size_t index, uint8_t *data, size_t len, bool final) {
		Serial.printf("%d file Upload:\n", millis());
		if (!index) {
			uploadFile = SPIFFS.open(filereceived, "w"); // hard file name
			Serial.printf("UploadStart: %s\n", filereceived.c_str());
			if (!uploadFile) {
#ifdef SERIAL_DEBUG
				Serial.print("Failed to open: ");
				Serial.println(filereceived);
				Serial.println("SPIFFS Root content:");
				Dir dir = SPIFFS.openDir("/");
				while (dir.next()) {
					String fileName = dir.fileName();
					int fileSize = (int)dir.fileSize();
					Serial.print("FS File: ");
					Serial.print(fileName);
					Serial.print("  size: ");
					Serial.println(fileSize);
				}
#endif
			}
		}
		String payload = (const char*)data;
		payload = payload.substring(0, len);
		uploadFile.print(payload.c_str());
		uploadFile.println();
		Serial.printf("%s", payload.c_str());

		if (final) {
			Serial.printf("\nUploadEnd: %u    File: %s\n", index + len, filereceived.c_str());
			filereceived = "";
			uploadFile.close();
		}

	});

	//	server.on("/send.html", HTTP_POST, [](AsyncWebServerRequest *request) {    //  upload complete need to be executed in notfound otherwise no upload

	server.onNotFound([](AsyncWebServerRequest *request) {
		bool fileuploaded = false;
		Serial.printf("%d NOT_FOUND: ",millis());
		if (request->method() == HTTP_GET)
			Serial.printf("GET");
		else if (request->method() == HTTP_POST)
			Serial.printf("POST");
		else if (request->method() == HTTP_DELETE)
			Serial.printf("DELETE");
		else if (request->method() == HTTP_PUT)
			Serial.printf("PUT");
		else if (request->method() == HTTP_PATCH)
			Serial.printf("PATCH");
		else if (request->method() == HTTP_HEAD)
			Serial.printf("HEAD");
		else if (request->method() == HTTP_OPTIONS)
			Serial.printf("OPTIONS");
		else
			Serial.printf("UNKNOWN");
		Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

		if (request->contentLength()) {
			Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
			Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
		}

		int headers = request->headers();
		int i;
		for (i = 0; i<headers; i++) {
			AsyncWebHeader* h = request->getHeader(i);
			Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
		}

		int params = request->params();
		for (i = 0; i<params; i++) {
			AsyncWebParameter* p = request->getParam(i);
			if (p->isFile()) {
				Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
				fileuploaded = true;
			}
			else if (p->isPost()) {
				Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
			}
			else {
				Serial.printf("_GET[%s]: %s\n", p->name().c_str(), p->value().c_str());
			}
		}
		if (fileuploaded) {
			Serial.printf("not found POST related to regular upload, upload complete\n");
			request->send(200, "text/plain", host + "  file upload complete");
		}
		else request->send(404);
	});


	// initialize ArduinoOTA.setPort(8266); // Port defaults to 8266
	ArduinoOTA.setHostname(host.c_str()); // Hostname defaults to esp8266-[ChipID]
	ArduinoOTA.setPassword(otapassword.c_str()); // No authentication by default

	ArduinoOTA.onStart([]() {
		Serial.println("Start");
	});
	ArduinoOTA.onEnd([]() {
		Serial.println("\nEnd");
	});
	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		if (int(progress / (total / 100)) % 20 == 0)
			Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
	});
	ArduinoOTA.onError([](ota_error_t error) {
		Serial.printf("Error[%u]: ", error);
		if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
		else if (error == OTA_END_ERROR) Serial.println("End Failed");
	});

	// initialize MQTT client
	mqttClient.onConnect(onMqttConnect);
	mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onSubscribe(onMqttSubscribe);
	mqttClient.onUnsubscribe(onMqttUnsubscribe);
	mqttClient.onMessage(onMqttMessage);
	mqttClient.onPublish(onMqttPublish);
	mqttClient.setServer(MQTT_BROKER, 1883);
	mqttClient.setKeepAlive(300).setCleanSession(false).setWill("topic/online", 2, true, "no").setClientId(host.c_str()); // ??????????????????????????

	// Start async sequenze  WiFi, TCP, MQTT and subscribe MQTT messages
	wifitrail = 0;
	digitalWrite(LED_BUILTIN, LOW);  // Board LED on
	connectToWifi();

	if (WiFi.waitForConnectResult() == WL_CONNECTED) {
		digitalWrite(LED_BUILTIN, HIGH);   // turn the LED off 
		ledindicator = true;
	}
	ota_flash = 0;
	millis_offset = millis(); // reset time base

	if (content.indexOf("execute") != -1) {
		req = "/" + content;  // comply with REST format
		answer = implementexecute(req);

		sprintf(answer1, "start executing   %s", req.c_str());
		Serial.println(answer1);

	}

}

void loop() {
	if ((WiFi.status() != WL_CONNECTED)) {
		if (wifitrail >= WIFITRAILS) {
			WiFi.mode(WIFI_AP);
			delay(500);
			WiFi.hostname(ap_hostname);
			Serial.println("Connection Failed! network missing");
			Serial.print("access point :");
			Serial.println(ap_default_ssid);
			Serial.print("key :");
			Serial.println(ap_default_psk);
			WiFi.softAP(ap_default_ssid, ap_default_psk);
			delay(500);
			server.begin();
			Serial.println(WiFi.softAPIP());
			yield();
			wifitrail = 0;
			ota_flash = 3; //  html to enter configuration
		}
	}
	else {
		switch (ota_flash) {
  			case 1: {
				WiFi.persistent(false);
				WiFi.mode(WIFI_OFF);   // reestablish WiFi
				WiFi.mode(WIFI_STA);
				WiFi.begin(ssid.c_str(), password.c_str());

				if (wemos_d1_mini && relayshield)  // Set RELAY_SHIELD according to the request
					digitalWrite(RELAY_SHIELD, 0);
																				  // Initialize Serial reporting for OTA
  				Serial.begin(115200);
  				delay(1000);
        
  				  
  				Serial.println("OTA-Server starting");
  				ArduinoOTA.begin();
  				Serial.println("Ready");
				Serial.print("IP address: ");
				Serial.println(WiFi.localIP());
				Serial.print("OTA Password: ");
				Serial.println(otapassword.c_str());

  				digitalWrite(LED_BUILTIN, LOW);  // set io to safe state, wait for OTA-flash
  
  				Serial.println("Flash enabled");
				ota_flash=2;
  				break;      
  			}
  			case 2: {
  				ArduinoOTA.handle();
  				break;
  			}
  			case 3: {
  				// html input not implemented use normal rest input //host-ip/setwifi/ssid/password/host  REM  switch to a different network by setwifi ==> storeasdefault ==> reset
  			}
			case 0: {

				/**************************************************************************************************************************
				*
				*    IO Section
				*
				***************************************************************************************************************************/
				// one time IO done in Application Section

if ((wemos_d1_wifi || nodemcu) && (shieldinfo == "new")) {  // Set new_SHIELD according to the request
																				 // new shield
					timedelay(20);  // no action delay

				}

				if ((!wemos_d1_wifi) && (WiFi.status() == WL_CONNECTED)) { // stay alive indication if no wemos_d1_wifi
					if ((timestamp < 100L) && (ledindicator)) {
						digitalWrite(LED_BUILTIN, LOW);   // turn the LED on
						ledindicator = false;
					}
					if ((timestamp >= 100L) && (!ledindicator)) {
						digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage HIGH
						ledindicator = true;
					}
				}

				if (timestamp > HARDBEAT) {
					timestamp = 0L;
					sprintf(answer1, "host:%s/hardbeat:%d", host.c_str(), (long)((float)(millis() - millis_offset) / 1000.0));
					uint16_t packetIdPub1 = mqttClient.publish("mqtthartbeat", 0, true, answer1);		// ensures tcp/wifi connectivity for mqtt

#ifdef SERIAL_DEBUG
					Serial.println(answer1);
#endif
				}

				break;
  			}
  			default: {
	#ifdef SERIAL_DEBUG
  				Serial.println("error: wrong OTA flash flag, Rebooting...");
	#endif
  				digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  				timedelay(3000);
  				ESP.restart();
  			}
  		}
	} 
}

