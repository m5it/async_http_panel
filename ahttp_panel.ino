#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
//#include "SPIFFS.h"
#include <ArduinoJson.h> // https://arduinojson.org/?utm_source=meta&utm_medium=library.properties
#include <GParser.h>     // https://github.com/GyverLibs/GParser
//#include <AceCRC.h>      // https://github.com/bxparks/AceCRC
//#include <Regexp.h>      // https://github.com/nickgammon/Regexp

//--
// wifi configs.
const char* ssid = "esp32byk0s";
const char* pwd  = "esp32bypwd";
//
struct Options {
    int LoopDefault      = 500; // If delay is too low looks server dont accept any connection.
    //int LoopIdle         = 1000;
    int disp_stats_every = 3;   // display stats to Serial every 3s
    //
    boolean upc_done = false;
    boolean up_done  = false;
};
//
Options options;
int loopDelay = options.LoopDefault;
//
IPAddress IP;
//
struct Statistics {
    int num_loops       = 0;
    int num_clients     = 0;
    int num_restarts    = 0;
    double last_loop_ts = 0; // last loop timestamp in ms
    double last_disp_ts = 0; // (timestamp in ms) to know when statistics was displayed last time (Used for debug and Serial output)
};
Statistics stats;
//
AsyncWebServer server(80);
AsyncWebSocket ws("/ws"); // access at ws://[esp ip]/ws
AsyncEventSource events("/events"); // event source (Server-Sent events)
//
bool shouldReboot = false;
// html_start_panel = upload_file_form.html converted with ghexc.php
char html_start_panel[] =
"\x3c\x68\x74\x6d\x6c\x3e\xa\x3c\x68\x65\x61\x64\x3e\xa\x9\x3c\x6d\x65\x74\x61\x20\x6e\x61\x6d\x65\x3d\x22\x76\x69\x65\x77"
"\x70\x6f\x72\x74\x22\x20\x63\x6f\x6e\x74\x65\x6e\x74\x3d\x22\x77\x69\x64\x74\x68\x3d\x31\x30\x30\x25\x2c\x20\x69\x6e\x69\x74"
"\x69\x61\x6c\x2d\x73\x63\x61\x6c\x65\x3d\x31\x2c\x20\x6d\x61\x78\x69\x6d\x75\x6d\x2d\x73\x63\x61\x6c\x65\x3d\x31\x2c\x20\x6d"
"\x69\x6e\x69\x6d\x75\x6d\x2d\x73\x63\x61\x6c\x65\x3d\x31\x2c\x20\x75\x73\x65\x72\x2d\x73\x63\x61\x6c\x61\x62\x6c\x65\x3d\x6e"
"\x6f\x22\x3e\xa\x9\x3c\x74\x69\x74\x6c\x65\x3e\x55\x70\x6c\x6f\x61\x64\x20\x61\x72\x64\x75\x69\x6e\x6f\x20\x63\x6f\x6e\x74"
"\x72\x6f\x6c\x20\x70\x61\x6e\x65\x6c\x2e\x3c\x2f\x74\x69\x74\x6c\x65\x3e\xa\x3c\x2f\x68\x65\x61\x64\x3e\xa\x3c\x62\x6f\x64"
"\x79\x3e\xa\x3c\x73\x63\x72\x69\x70\x74\x3e\xa\x66\x75\x6e\x63\x74\x69\x6f\x6e\x20\x6c\x6f\x61\x64\x46\x69\x6c\x65\x28\x66"
"\x70\x29\x20\x7b\xa\x9\x76\x61\x72\x20\x74\x79\x70\x65\x3d\x66\x70\x2e\x61\x74\x74\x72\x69\x62\x75\x74\x65\x73\x2e\x61\x74"
"\x74\x72\x74\x79\x70\x65\x2e\x6e\x6f\x64\x65\x56\x61\x6c\x75\x65\x3b\xa\x9\x69\x66\x28\x66\x70\x2e\x66\x69\x6c\x65\x73\x20"
"\x26\x26\x20\x66\x70\x2e\x66\x69\x6c\x65\x73\x5b\x30\x5d\x29\x20\x7b\xa\x9\x9\x76\x61\x72\x20\x66\x20\x20\x3d\x20\x66\x70"
"\x2e\x66\x69\x6c\x65\x73\x5b\x30\x5d\x3b\x20\x76\x61\x72\x20\x66\x64\x20\x3d\x20\x6e\x65\x77\x20\x46\x6f\x72\x6d\x44\x61\x74"
"\x61\x28\x29\x3b\xa\x9\x9\x66\x64\x2e\x61\x70\x70\x65\x6e\x64\x28\x22\x66\x69\x6c\x65\x22\x2c\x66\x29\x3b\xa\x9\x9\x66"
"\x65\x74\x63\x68\x28\x22\x2f\x22\x2b\x74\x79\x70\x65\x2c\x7b\x6d\x65\x74\x68\x6f\x64\x3a\x22\x50\x4f\x53\x54\x22\x2c\x62\x6f"
"\x64\x79\x3a\x66\x64\x7d\x29\x2e\x74\x68\x65\x6e\x28\x66\x75\x6e\x63\x74\x69\x6f\x6e\x28\x72\x65\x73\x70\x6f\x6e\x73\x65\x29"
"\x20\x7b\xa\x9\x9\x9\x63\x6f\x6e\x73\x6f\x6c\x65\x2e\x69\x6e\x66\x6f\x28\x74\x79\x70\x65\x2b\x22\x20\x72\x65\x73\x70\x6f"
"\x6e\x73\x65\x22\x2c\x72\x65\x73\x70\x6f\x6e\x73\x65\x29\x3b\xa\x9\x9\x9\x64\x6f\x63\x75\x6d\x65\x6e\x74\x2e\x6c\x6f\x63"
"\x61\x74\x69\x6f\x6e\x2e\x68\x72\x65\x66\x3d\x64\x6f\x63\x75\x6d\x65\x6e\x74\x2e\x6c\x6f\x63\x61\x74\x69\x6f\x6e\x2e\x68\x72"
"\x65\x66\x3b\xa\x9\x9\x7d\x29\x2e\x63\x61\x74\x63\x68\x28\x66\x75\x6e\x63\x74\x69\x6f\x6e\x28\x45\x29\x20\x7b\xa\x9\x9"
"\x9\x63\x6f\x6e\x73\x6f\x6c\x65\x2e\x77\x61\x72\x6e\x28\x22\x45\x52\x52\x4f\x52\x20\x22\x2b\x74\x79\x70\x65\x2c\x45\x29\x3b"
"\xa\x9\x9\x7d\x29\x3b\xa\x9\x7d\xa\x9\x65\x6c\x73\x65\x20\x7b\xa\x9\x9\x63\x6f\x6e\x73\x6f\x6c\x65\x2e\x77\x61\x72"
"\x6e\x28\x22\x53\x6f\x6d\x65\x74\x68\x69\x6e\x67\x20\x77\x65\x6e\x74\x20\x77\x72\x6f\x6e\x67\x2e\x22\x29\x3b\xa\x9\x7d\xa"
"\x7d\xa\x66\x75\x6e\x63\x74\x69\x6f\x6e\x20\x6f\x6e\x4c\x6f\x61\x64\x28\x65\x29\x20\x7b\xa\x9\x2f\x2f\xa\x9\x69\x66\x28"
"\x20\x64\x6f\x63\x75\x6d\x65\x6e\x74\x2e\x6c\x6f\x63\x61\x74\x69\x6f\x6e\x2e\x70\x61\x74\x68\x6e\x61\x6d\x65\x21\x3d\x22\x2f"
"\x22\x20\x7c\x7c\x20\x64\x6f\x63\x75\x6d\x65\x6e\x74\x2e\x6c\x6f\x63\x61\x74\x69\x6f\x6e\x2e\x73\x65\x61\x72\x63\x68\x21\x3d"
"\x22\x22\x20\x29\x20\x7b\xa\x9\x9\x77\x69\x6e\x64\x6f\x77\x2e\x68\x69\x73\x74\x6f\x72\x79\x2e\x72\x65\x70\x6c\x61\x63\x65"
"\x53\x74\x61\x74\x65\x28\x7b\x7d\x2c\x22\x22\x2c\x22\x2f\x22\x29\x3b\xa\x9\x7d\xa\x9\x2f\x2f\xa\x9\x66\x65\x74\x63\x68"
"\x28\x22\x2f\x73\x74\x61\x74\x75\x73\x22\x2c\x7b\x6d\x65\x74\x68\x6f\x64\x3a\x22\x47\x45\x54\x22\x7d\x29\x2e\x74\x68\x65\x6e"
"\x28\x66\x75\x6e\x63\x74\x69\x6f\x6e\x28\x72\x65\x73\x70\x6f\x6e\x73\x65\x29\x20\x7b\xa\x9\x9\x63\x6f\x6e\x73\x6f\x6c\x65"
"\x2e\x69\x6e\x66\x6f\x28\x22\x73\x74\x61\x74\x75\x73\x20\x72\x65\x73\x70\x6f\x6e\x73\x65\x22\x2c\x72\x65\x73\x70\x6f\x6e\x73"
"\x65\x29\x3b\xa\x9\x9\x72\x65\x74\x75\x72\x6e\x20\x72\x65\x73\x70\x6f\x6e\x73\x65\x2e\x6a\x73\x6f\x6e\x28\x29\x3b\xa\x9"
"\x7d\x29\x2e\x74\x68\x65\x6e\x28\x66\x75\x6e\x63\x74\x69\x6f\x6e\x28\x6a\x73\x6f\x6e\x29\x20\x7b\xa\x9\x9\x63\x6f\x6e\x73"
"\x6f\x6c\x65\x2e\x69\x6e\x66\x6f\x28\x22\x73\x74\x61\x74\x75\x73\x20\x6a\x73\x6f\x6e\x22\x2c\x6a\x73\x6f\x6e\x29\x3b\xa\x9"
"\x9\x69\x66\x28\x20\x6a\x73\x6f\x6e\x2e\x75\x70\x63\x5f\x64\x6f\x6e\x65\x20\x29\x20\x7b\xa\x9\x9\x9\x64\x6f\x63\x75\x6d"
"\x65\x6e\x74\x2e\x71\x75\x65\x72\x79\x53\x65\x6c\x65\x63\x74\x6f\x72\x28\x22\x2e\x75\x70\x63\x20\x69\x6e\x70\x75\x74\x22\x29"
"\x2e\x73\x65\x74\x41\x74\x74\x72\x69\x62\x75\x74\x65\x28\x22\x64\x69\x73\x61\x62\x6c\x65\x64\x22\x2c\x22\x64\x69\x73\x61\x62"
"\x6c\x65\x64\x22\x29\x3b\xa\x9\x9\x7d\xa\x9\x9\x69\x66\x28\x20\x6a\x73\x6f\x6e\x2e\x75\x70\x5f\x64\x6f\x6e\x65\x20\x29"
"\x20\x7b\xa\x9\x9\x9\x64\x6f\x63\x75\x6d\x65\x6e\x74\x2e\x71\x75\x65\x72\x79\x53\x65\x6c\x65\x63\x74\x6f\x72\x28\x22\x2e"
"\x75\x70\x20\x69\x6e\x70\x75\x74\x22\x29\x2e\x73\x65\x74\x41\x74\x74\x72\x69\x62\x75\x74\x65\x28\x22\x64\x69\x73\x61\x62\x6c"
"\x65\x64\x22\x2c\x22\x64\x69\x73\x61\x62\x6c\x65\x64\x22\x29\x3b\xa\x9\x9\x7d\xa\x9\x7d\x29\x2e\x63\x61\x74\x63\x68\x28"
"\x66\x75\x6e\x63\x74\x69\x6f\x6e\x28\x45\x29\x20\x7b\xa\x9\x9\x63\x6f\x6e\x73\x6f\x6c\x65\x2e\x77\x61\x72\x6e\x28\x22\x45"
"\x52\x52\x4f\x52\x20\x73\x74\x61\x74\x75\x73\x22\x2c\x45\x29\x3b\xa\x9\x7d\x29\x3b\xa\x7d\xa\x77\x69\x6e\x64\x6f\x77\x2e"
"\x6f\x6e\x6c\x6f\x61\x64\x20\x3d\x20\x66\x75\x6e\x63\x74\x69\x6f\x6e\x28\x65\x29\x20\x7b\x20\x6f\x6e\x4c\x6f\x61\x64\x28\x65"
"\x29\x3b\x20\x7d\xa\x3c\x2f\x73\x63\x72\x69\x70\x74\x3e\xa\xa\x3c\x68\x33\x3e\x3c\x61\x20\x68\x72\x65\x66\x3d\x22\x2f\x22"
"\x3e\x48\x6f\x6d\x65\x3c\x2f\x61\x3e\x3c\x2f\x68\x33\x3e\xa\x3c\x62\x72\x3e\xa\x3c\x62\x72\x3e\xa\xa\x3c\x64\x69\x76\x3e"
"\xa\x9\x3c\x64\x69\x76\x20\x73\x74\x79\x6c\x65\x3d\x22\x66\x6c\x6f\x61\x74\x3a\x6c\x65\x66\x74\x3b\x77\x69\x64\x74\x68\x3a"
"\x35\x30\x25\x3b\x62\x6f\x78\x2d\x73\x69\x7a\x69\x6e\x67\x3a\x62\x6f\x72\x64\x65\x72\x2d\x62\x6f\x78\x3b\x22\x3e\xa\x9\x9"
"\x43\x68\x6f\x6f\x73\x65\x20\x63\x6f\x6e\x74\x72\x6f\x6c\x20\x70\x61\x6e\x65\x6c\x20\x28\x48\x54\x4d\x4c\x29\x3a\x20\x3c\x62"
"\x72\x3e\xa\x9\x9\x3c\x66\x6f\x72\x6d\x20\x6d\x65\x74\x68\x6f\x64\x3d\x22\x50\x4f\x53\x54\x22\x20\x63\x6c\x61\x73\x73\x3d"
"\x22\x75\x70\x22\x3e\xa\x9\x9\x9\x3c\x69\x6e\x70\x75\x74\x20\x74\x79\x70\x65\x3d\x22\x66\x69\x6c\x65\x22\x20\x6f\x6e\x63"
"\x68\x61\x6e\x67\x65\x3d\x22\x6c\x6f\x61\x64\x46\x69\x6c\x65\x28\x74\x68\x69\x73\x29\x3b\x22\x20\x61\x74\x74\x72\x54\x79\x70"
"\x65\x3d\x22\x75\x70\x22\x3e\xa\x9\x9\x3c\x2f\x66\x6f\x72\x6d\x3e\xa\x9\x3c\x2f\x64\x69\x76\x3e\xa\x9\x3c\x64\x69\x76"
"\x20\x73\x74\x79\x6c\x65\x3d\x22\x66\x6c\x6f\x61\x74\x3a\x6c\x65\x66\x74\x3b\x77\x69\x64\x74\x68\x3a\x35\x30\x25\x3b\x62\x6f"
"\x78\x2d\x73\x69\x7a\x69\x6e\x67\x3a\x62\x6f\x72\x64\x65\x72\x2d\x62\x6f\x78\x3b\x22\x3e\xa\x9\x9\x43\x68\x6f\x6f\x73\x65"
"\x20\x63\x6f\x6e\x66\x69\x67\x75\x72\x61\x74\x69\x6f\x6e\x20\x66\x6f\x72\x20\x70\x61\x6e\x65\x6c\x20\x28\x4a\x53\x4f\x4e\x29"
"\x3a\x20\x3c\x62\x72\x3e\xa\x9\x9\x3c\x66\x6f\x72\x6d\x20\x6d\x65\x74\x68\x6f\x64\x3d\x22\x50\x4f\x53\x54\x22\x20\x63\x6c"
"\x61\x73\x73\x3d\x22\x75\x70\x63\x22\x3e\xa\x9\x9\x9\x3c\x69\x6e\x70\x75\x74\x20\x74\x79\x70\x65\x3d\x22\x66\x69\x6c\x65"
"\x22\x20\x6f\x6e\x63\x68\x61\x6e\x67\x65\x3d\x22\x6c\x6f\x61\x64\x46\x69\x6c\x65\x28\x74\x68\x69\x73\x29\x3b\x22\x20\x61\x74"
"\x74\x72\x54\x79\x70\x65\x3d\x22\x75\x70\x63\x22\x3e\xa\x9\x9\x3c\x2f\x66\x6f\x72\x6d\x3e\xa\x9\x3c\x2f\x64\x69\x76\x3e"
"\xa\x9\x3c\x64\x69\x76\x20\x73\x74\x79\x6c\x65\x3d\x22\x63\x6c\x65\x61\x72\x3a\x62\x6f\x74\x68\x3b\x22\x3e\x3c\x2f\x64\x69"
"\x76\x3e\xa\x3c\x2f\x64\x69\x76\x3e\xa\xa\x3c\x2f\x62\x6f\x64\x79\x3e\xa\x3c\x2f\x68\x74\x6d\x6c\x3e\xa";

//
DynamicJsonDocument json_user_panel(4048);
DynamicJsonDocument tasks(4048);
String sson_user_panel = "";
String html_user_panel = "";

//--
//
void onRequest(AsyncWebServerRequest *request){
    //Handle Unknown Request
    static char out[256]={0};
    sprintf(out,"onRequest() started, params.length: %i",request->params());
    Serial.println(out);
    request->send(404);
}
//
void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    //Handle body
    Serial.println("onBody() started.");
}

//
void onEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    //Handle WebSocket event
    Serial.println("onEvent() started.");
}

//
int handleActions(AsyncWebServerRequest *request, DynamicJsonDocument actions) {
	int returnVal = NULL;
	// Loop trough request actions and exec them..
	for(int j=0; j<actions.size(); j++) {
		Serial.println("Entering action...");
		// Ex. action: {"gpio":27,"value":0,"type":"DW"}
		// value 0 = LOW, 1=HIGH
		// todo DW = digitalWrite, DR = digitalRead, AW = analogWrite, AR = analogRead
		int gpio         = actions[j]["gpio"];
		int value        = actions[j]["value"];
		String paramName = actions[j]["paramName"]; // If set then value is retrived from urls parameters
		String type      = actions[j]["type"];
		// (deprecated) 4 special settings..xx
		String jsonName  = actions[j]["jsonName"];  // Same as paramName just value is json string
		String jsonValue = "";
		
		// Retrive value from cmd2 (url) if paramName not empty
		if( !paramName.isEmpty() && paramName!="null" && paramName!=NULL ) {
			//char *cparamName = c2c(paramName.c_str());
			value = strtol(getURLPV(request,paramName).c_str(),NULL,0);
			//free(cparamName);
		}
		// (deprecated)
		else if( !jsonName.isEmpty() && jsonName!="null" && jsonName!=NULL ) {
			//char *cjsonName = c2c(jsonName.c_str());
			jsonValue = GP_urldecode(getURLPV(request,jsonName)); // Ex.: %7B"channel"%3A1%2C"frequency"%3A5000%2C"resolution"%3A8%7D
			//free(cjsonName);
		}
		
		Serial.println(value);
		Serial.println(jsonValue);
		
		//--
		// Digital Write
		if( type=="DW" ) {
			Serial.println("DEBUG FIRING DW!");
			digitalWrite( gpio, value );
		}
		// Digital Read
		else if( type=="DR" ) {
			Serial.println("DEBUG FIRING DR!");
			returnVal = digitalRead( gpio );
			Serial.println( returnVal );
		}
		// Analog Write
		else if( type=="AW" ) {
			Serial.println("DEBUG FIRING AW!");
			analogWrite( gpio, value );
		}
		// Analog Read
		else if( type=="AR" ) {
			Serial.println("DEBUG FIRING AR!");
			returnVal = analogRead( gpio );
			Serial.println( returnVal );
		}
		// (deprecated) Just to test changing frequency and resolution after but cant notice any change with dc motor..
		else if( type=="LCWS" ) {
			Serial.println("DEBUG FIRING LCWS!");
			DynamicJsonDocument tmpjson(256);
			deserializeJson(tmpjson,jsonValue);
			ledcSetup( tmpjson["channel"], tmpjson["frequency"], tmpjson["resolution"] );
			tmpjson.clear();
		}
		//
		else if( type=="LCW" ) {
			Serial.println("DEBUG FIRING LCW!");
			// gpio  = pwm channel
			// value = dutyCycle
			ledcWrite( gpio, value );
		}
		// DHT Temperature
		/*else if( type=="DHTT" ) {
			Serial.println("DEBUG FIRING DHTT!");
			returnVal = dht.readTemperature();
			Serial.println( returnVal );
		}
		// DHT Humidity
		else if( type=="DHTH" ) {
			Serial.println("DEBUG FIRING DHTH!");
			returnVal = dht.readHumidity();
			Serial.println( returnVal );
		}*/
		// (deprecated) delay(.)
		else if( type=="delay" ) {
			Serial.println("DEBUG FIRING delay(...)");
			delay( value );
		}
		// Unknown command
		else {
			Serial.println("DEBUG NOT CORRECT TYPE...");
		}
	}
	//
	actions.clear();
	return returnVal;
}

//--
//
void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.println("Setup is starting...");

    //
    WiFi.mode(WIFI_AP);
    delay(100);
    WiFi.softAP(ssid, pwd);
    delay(200);
    IP = WiFi.softAPIP();
    //
    delay(100);
    Serial.print("WebServer IP: ");
    Serial.println(IP);
    
    //
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        static char out[256]={0};
        Serial.println( "on() / starting..." );
        //request->send(200,"text/html",(html_user_panel_ready?html_user_panel:html_start_panel));
        request->send(200,"text/html",(options.upc_done && options.up_done?html_user_panel:html_start_panel));
    });
    //
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println( "on() /status starting..." );
        Serial.println(sson_user_panel);
        String tmp;
        StaticJsonDocument<200> doc;
        doc["success"] = true;
        doc["upc_done"] = (options.upc_done?true:false);
        doc["up_done"] = (options.up_done?true:false);
        doc["taskslength"] = tasks.size();
        doc["uplength"] = html_user_panel.length();
        serializeJson(doc, tmp);
        request->send(200,"application/json",tmp);
        doc.clear();
    });
    //
    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
        R(request,true,"");
        shouldReboot = true;
    });
    //
    server.on("/upc", HTTP_POST,[](AsyncWebServerRequest *request ){
        //
        if( options.upc_done ) return;
        Serial.println("on /upc Done!");
        
        deserializeJson(json_user_panel, sson_user_panel);
        tasks = json_user_panel["tasks"];
        
        // Loop trough json configured setups, functions that should be executed only once. Ex.: pinMode(),...
        DynamicJsonDocument setups(1024);
        setups = json_user_panel["setups"];
        for(int i=0; i<setups.size(); i++) {
            String setupAction = setups[i]["action"]; // mode,DHT...
            int gpio = setups[i]["gpio"];
            //
            if( setupAction=="mode" ) {
                Serial.println("parseHandler()->Configuring mode... (d1)");
                pinMode(setups[i]["gpio"],(setups[i]["value"]=="OUTPUT"?OUTPUT:INPUT));
            }
            // setupAction "pwm" require value to be another JSON object with keys:
            //   {channel, frequency, resolution}
            else if( setupAction=="pwm" ) {
                Serial.println("parseHandler()->Configuring pwm... (d2)");
                DynamicJsonDocument values(256);
                values = setups[i]["value"];
                if( !values.containsKey("channel") ||
                    !values.containsKey("frequency") ||
                    !values.containsKey("resolution") ) {
                    Serial.println("parseHandler()->Configuring pwm Failed!");
                    continue;
                }
                ledcAttachPin( gpio, values["channel"] );
                ledcSetup( values["channel"], values["frequency"], values["resolution"] );
                values.clear();
            }
            //
            //else if( setupAction=="DHT" ) {
            //  Serial.println("Configuring dht... (d3)");
            //  dht.setPinNType( setups[i]["gpio"], setups[i]["value"] );
            //  dht.begin();
            //}
        }
        setups.clear();
        
        // Loop trough json configured tasks and check if request match. Functions that can be executed here ex.: analogWrite, analogRead, 
        //   digitalWrite, digitalRead etc..
        // Execute tasks
        for(int i=0; i<tasks.size(); i++) {
            //String taskTitle                 = tasks[i]["title"];
            String taskRequest               = tasks[i]["request"];
            //
            char tmpRequest[taskRequest.length()+1] = {0};
            taskRequest.toCharArray(tmpRequest, taskRequest.length()+1);
            
            //
            server.on(tmpRequest, HTTP_GET, [](AsyncWebServerRequest *request1) {
                int returnVal = NULL;
                boolean returnSuccess = false;
                int params = request1->params();
                
                String paramKey = "";
                DynamicJsonDocument actions(1024);
                for(int j=0; j<params; j++) {
                    AsyncWebParameter* p = request1->getParam(j);
                    if( p->name()=="paramKey" ) {
                        paramKey = p->value(); // paramKey = /request
                        break;
                    }
                }
                //
                if( paramKey=="" || paramKey.length()==0 ) {
                    return;
                }
                // Loop trough tasks and find correct key for task & exec all actions
                for(int j=0; j<tasks.size(); j++) {
                    String tmpr = tasks[j]["request"];
                    
                    if( paramKey.startsWith(tmpr)) {
                        Serial.println("Got correct paramKey!!! Executing...");
                        actions = tasks[j]["actions"];
                        //
                        returnVal     = handleActions( request1, actions );
                        returnSuccess = true;
                        break;
                    }
                }
                //
                char outVal[256];
                sprintf(outVal,"%i",returnVal);
                String tmpVal = outVal;
                R(request1, returnSuccess, tmpVal);
            });
            
        }
        
        options.upc_done = true;
        R(request, true, "");
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        Serial.println("on /upc started.");
        if( options.upc_done ) return;
        char tmp[len+1]={0};
        for(int i=0; i<len; i++) {
            char chr = data[i];
            tmp[i]=chr;
        }
        sson_user_panel += tmp;
    },onBody);
    //
    server.on("/up", HTTP_POST,[](AsyncWebServerRequest *request ) {
        if( options.up_done ) return;
        Serial.println("on /up Done!");
        options.up_done = true;
        R(request, true, "");
    },[](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        Serial.println("on /up started.");
        if( options.up_done ) return;
        char tmp[len+1]={0};
        for(int i=0; i<len; i++) {
            char chr = data[i];
            tmp[i]=chr;
        }
        html_user_panel += tmp;
    },onBody);

    server.onNotFound(onRequest);
    //server.onFileUpload(onUpload);
    server.onRequestBody(onBody);

    server.begin();
}

//
void loop() {
    //
    if( shouldReboot ) {
        Serial.println("Restarting device! :)*");
        ESP.restart();
        return;
    }
    //
    if( stats.last_disp_ts==0 || ((stats.last_disp_ts+(options.disp_stats_every*1000))<=(time(0)*1000)) ) {
        static char out[256];
        sprintf(out,"Debug END Loop, num_loops: %i, num_clients: %i, num_restarts: %i, freeheap: %lu, wifi.sleep: %ld, coreId: %i",
            stats.num_loops, stats.num_clients, stats.num_restarts, ESP.getFreeHeap(), WiFi.getSleep(), xPortGetCoreID());
        Serial.println(out);
        stats.last_disp_ts = time(0)*1000;
    }

    //
    stats.num_loops++;
    stats.last_loop_ts = time(0)*1000;
    delay(loopDelay);
}

// try find value by parameter name and return value
String getURLPV(AsyncWebServerRequest *request, String name) {
    String ret="";
    int params = request->params();      
    for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if( p->name()==name ) return p->value();
    }
    return ret;
}
//
void R(AsyncWebServerRequest *request, boolean Success, String data) {
    String tmp;
    //const int jsonsize = data.length()+200;
    StaticJsonDocument<1024> doc;
    doc["success"] = Success;
    doc["data"] = data;
    serializeJson(doc, tmp);
    request->send(200,"application/json",tmp);
    doc.clear();
}
