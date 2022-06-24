/*
* Public Chat Application in Captive Portal
*
* Licence: MIT
* 2022 - Hasan Enes Şimşek
*/

#include <WiFi.h>
#include <DNSServer.h> // captive portal based on dns 
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <ArduinoJson.h> // json parser
#include "index.h" // it has: const char index_html[] PROGMEM (aka index.html)
#include "fileOps.h" // readFile, writeFile, deleteFile for spiffs

// <-- advised configurable parameters --

const char* ssid = "Public Chat";
const char* password = "";

// when server get this message from any client, it will clear flash (delete saved messages)
// if you dont want this feature, comment out it.
#define SPECIAL_TEXT_TO_CLEAR_FLASH "erase_flash"

// -- configurable parameters -->

// if size of incoming msg from any client is greater than defined value, it will be truncuted
// it may be suppressed by limits of websocket and webserver
#define INCOMING_MSG_BUFFER_SIZE 1024

// if following preprocessor is defined, the html page will be served dynamically to make sure that 
// a user cannot type more than INCOMING_MSG_BUFFER_SIZE in textarea
#define RESTRICT_USER_INPUT 

// where to save incoming messages with SPIFFS, data format: {json_1}{json_2}{json_3}...
#define FILE_NAME_FOR_MSGS_IN_JSON "/stored_msgs_in_json.txt" 

DNSServer dnsServer;
AsyncWebServer server(80); // Create AsyncWebServer object on port 80
AsyncWebSocket ws("/ws"); // web socket

char bufferIncomingMessages[INCOMING_MSG_BUFFER_SIZE + 3]; // buffer for incoming messages, +3 is for '[', ']', '\0'

// modify html page variable OUTGOING_MSG_BUFFER_SIZE before serve
String processor(const String& var)
{
  if(var == "OUTGOING_MSG_BUFFER_SIZE")
  {
    return String(INCOMING_MSG_BUFFER_SIZE);
  }
  return String();
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    //Serial.println("handlling");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    /* Just to note: Accessing http headers not possible here because of inferior web server lib */
#ifdef RESTRICT_USER_INPUT
    request->send_P(200, "text/html", index_html, processor);
#else
    request->send_P(200, "text/html", index_html);
#endif
  }
};

bool isCurlyBracketExist(const char* text)
{
  int i = 0;
  while(text[i] != NULL)
  {
    if(text[i] == '}')
      return true;
    else  
      i++;
  }  
  return false;
}

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

    // part 1: check msg size and write to buffer
    Serial.println("incoming msg size: " + String(len) + ", max: " + String(INCOMING_MSG_BUFFER_SIZE));
    if(len > INCOMING_MSG_BUFFER_SIZE)
    {
      String msgToSend = "Incoming msg size is greater than INCOMING_MSG_BUFFER_SIZE, size: " + String(len); 
      //Serial.println(msgToSend);
      ws.text(client->id(), msgToSend); // notify client
      return;
    }
    else
    {
      bufferIncomingMessages[len] = 0; // added null terminator 
      memcpy(bufferIncomingMessages, data, len); // string:<incoming_msg> + \0 = {...} + \0  
    }
    
    // part 2: deserialize incoming msg
    DynamicJsonDocument doc(22 * (INCOMING_MSG_BUFFER_SIZE/10)); // read only inputs require more memory lets say %220
    DeserializationError error = deserializeJson(doc, (const char *)bufferIncomingMessages); // without const, parser will modify input buffer
    if (error)
    {
      Serial.println(F("Failed to deserialization"));
      Serial.println(error.f_str());
      ws.text(client->id(), "invalid json err: " + String(error.f_str())); // notify client
      return; 
    }

    // part 3: check required keys are exist
    // Note: Because ArduinoJson does not support json scheme, and im bit lazy, i will not check unnecessary json keys.
    // This is actually security breach.
    const char* dateJSON = doc["date"];
    const char* nameJSON = doc["name"];
    const char* textJSON = doc["text"];
    if(dateJSON == NULL || nameJSON == NULL || textJSON == NULL)
    {
      Serial.println(F("Required JSON keys are not exist"));
      ws.text(client->id(), "Reqired json keys are not exist");
      return;
    }

    // part 4: char '}' is reserved for parsing when stored msgs are sent to newly connected client
    bool isExist1 = isCurlyBracketExist(dateJSON);
    bool isExist2 = isCurlyBracketExist(nameJSON);
    bool isExist3 = isCurlyBracketExist(textJSON);
    if(isExist1 || isExist2 || isExist3)
    {
      Serial.println("You cannot use } in your msg");
      ws.text(client->id(), "You cannot use } in your msg"); // notify client
      return;
    }

    // part 5: erase stored msgs if requested
#ifdef SPECIAL_TEXT_TO_CLEAR_FLASH
    const char* text_to_delete = SPECIAL_TEXT_TO_CLEAR_FLASH;
    if(text_to_delete != NULL)
    {
      if(strcmp(textJSON, text_to_delete) == 0)
      {
        Serial.println(F("Flash will be erased due to incoming msg")); 
        bool isOK = deleteFile(SPIFFS, FILE_NAME_FOR_MSGS_IN_JSON); 
        if(isOK)
        {
          Serial.println(F("Flash erased successfully.")); 
          ws.text(client->id(), "flash erased");
        } 
        else
        {
          Serial.println(F("Flash cannot be erased.")); 
          ws.text(client->id(), "flash cannot be erased");
        }

        return; // do not save this meesage to flash and do not send this message to all clients
      }
    }
#endif
     
    // part 6: save msg to flash
    writeFile(SPIFFS, FILE_NAME_FOR_MSGS_IN_JSON, bufferIncomingMessages);
    //Serial.printf("Stored data in flash:%s\n", bufferIncomingMessages); // debug 

    // part 7: send msg to clients connected (TODO: if msg size > limit this code will not work well)
    // wrap incoming message with [ ] and add \0 because my outgoing json format should be [{obj1},{obj2},{obj3},...]
    bufferIncomingMessages[0] = '[';
    bufferIncomingMessages[len+1] = ']';
    bufferIncomingMessages[len+2] = 0; // added null terminator 
    memcpy(bufferIncomingMessages+1, data, len); // string:"[<incoming_msg>]" with \0
    
    ws.textAll((char*)bufferIncomingMessages); // send incoming msg to all clients connected
    //Serial.printf("Outgoing data to clients:%s\n", bufferIncomingMessages); // debug
  }
}


// send previously stored messages to client newly connected
void sendPreviousMessages(AsyncWebSocketClient *client)
{
  File file = SPIFFS.open(FILE_NAME_FOR_MSGS_IN_JSON, "r");
  if(!file || file.isDirectory()){
    Serial.println("could not send stored data because flash is empty");
    return;
  }
  char bufferOutgoingMessages[INCOMING_MSG_BUFFER_SIZE + 3];
  bufferOutgoingMessages[0] = '[';
  int i = 0;
  Serial.println("Stored msgs will be send to client connected");
  // parse stored data as {json1}{json1}{json1}
  while(file.available()){
    i = i + 1;
    char c = (char)file.read();
    //Serial.println(int(c));
    //Serial.println(c);
    bufferOutgoingMessages[i] = c;
    if(c == '}')
    {
      //Serial.println("-------------i: " + String(i));
      bufferOutgoingMessages[i+1] = ']';
      bufferOutgoingMessages[i+2] = '\0';
      //Serial.println("\n\n\n");
      //Serial.println((char*)bufferOutgoingMessages);
      
      bool isOk = ws.availableForWrite(client->id());
      if(isOk)
        ws.text(client->id(), (char*)bufferOutgoingMessages);
      else
        Serial.println("server is busy");
       
      
      i = 0;
    }
    
  }
  
  file.close();
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      sendPreviousMessages(client);
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(client, arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent); // register callback function
  server.addHandler(&ws);
}

void setup(){
  Serial.begin(115200);

  // mount SPIFFS
  if(!SPIFFS.begin(true)){
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  WiFi.softAP(ssid, password);
  delay(100); // softAPConfig not working sometimes without wait
  
  // VERY IMPORTANT:
  // In order to open captive page automatically in android
  // we need to use other than private ips like 192.168.x.x
  // default ip is 192.168.4.1 so wee need to configure 
  
  IPAddress Ip(11,45,0,1);
  IPAddress NMask(255, 255, 255, 0);
  bool isConfigured = WiFi.softAPConfig(Ip, Ip, NMask);
  Serial.println(isConfigured);
  
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", WiFi.softAPIP()); // forward all dns requests to esp's ip

  initWebSocket();
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // only when requested from AP

  // CaptiveRequestHandler will handle all request
  /*
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
#ifdef RESTRICT_USER_INPUT
    request->send_P(200, "text/html", index_html, processor);
#else
    request->send_P(200, "text/html", index_html);
#endif
  });
  */

  // Start server
  server.begin();
}

void loop() {
  dnsServer.processNextRequest();
  ws.cleanupClients();
  //String a = readFile(SPIFFS, FILE_NAME_FOR_MSGS_IN_JSON);
  //Serial.println(a);
  //delay(15000);
}
