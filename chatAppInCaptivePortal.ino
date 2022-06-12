/*
* Public Chat Application in Captive Portal
*
* Licence: MIT
* 2022 - Hasan Enes Şimşek
*/

#include <WiFi.h>
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

// where to save incoming messages with SPIFFS, data format: "{json_1}{json_2}{json_3}..."
#define FILE_NAME_FOR_MSGS_IN_JSON "/stored_msgs_in_json.txt" 

AsyncWebServer server(80); // Create AsyncWebServer object on port 80
AsyncWebSocket ws("/ws"); // web socket

char bufferIncomingMessages[INCOMING_MSG_BUFFER_SIZE]; // buffer for incoming messages

// modify html page variable OUTGOING_MSG_BUFFER_SIZE before serve
String processor(const String& var)
{
  if(var == "OUTGOING_MSG_BUFFER_SIZE")
  {
    return String(INCOMING_MSG_BUFFER_SIZE);
  }
  return String();
}

void handleWebSocketMessage(AsyncWebSocketClient *client, void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {

    // part 1: check msg size and write to buffer
    Serial.println("incoming msg size: " + String(len) + ", max: " + String(INCOMING_MSG_BUFFER_SIZE));
    if(len > INCOMING_MSG_BUFFER_SIZE)
    {
      String msgToSend = "Incoming msg size is greater than INCOMING_MSG_BUFFER_SIZE, size: " + String(len); 
      Serial.println(msgToSend);
      ws.text(client->id(), msgToSend); // notify client

      // truncate incoming msg (this may result incorrectly formatted buffer but will be handled by json parser)
      bufferIncomingMessages[INCOMING_MSG_BUFFER_SIZE-3] = '"'; // manually added "} at the end of msg
      bufferIncomingMessages[INCOMING_MSG_BUFFER_SIZE-2] = '}'; // manually added "} at the end of msg
      bufferIncomingMessages[INCOMING_MSG_BUFFER_SIZE-1] = 0; // added null terminator 
      memcpy(bufferIncomingMessages, data, INCOMING_MSG_BUFFER_SIZE-3); // string:<incoming_msg> + \0 = {...} + \0 
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

    // part 3: erase stored msgs if requested
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
          
      }
    }
#endif
     
    // part 4: save msg to flash
    writeFile(SPIFFS, FILE_NAME_FOR_MSGS_IN_JSON, bufferIncomingMessages);
    Serial.printf("Stored data in flash:%s\n", bufferIncomingMessages); // debug 

    // part 5: send msg to clients connected (TODO: if msg size > limit this code will not work well)
    // wrap incoming message with [ ] and add \0 because my outgoing json format should be [{obj1},{obj2},{obj3},...]
    bufferIncomingMessages[0] = '[';
    bufferIncomingMessages[len+1] = ']';
    bufferIncomingMessages[len+2] = 0; // added null terminator 
    memcpy(bufferIncomingMessages+1, data, len); // string:"[<incoming_msg>]" with \0
    
    ws.textAll((char*)bufferIncomingMessages); // send incoming msg to all clients connected
    Serial.printf("Outgoing data to clients:%s\n", bufferIncomingMessages); // debug
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
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
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
#ifdef RESTRICT_USER_INPUT
    request->send_P(200, "text/html", index_html, processor);
#else
    request->send_P(200, "text/html", index_html);
#endif
    
  });

  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();
  //String a = readFile(SPIFFS, FILE_NAME_FOR_MSGS_IN_JSON);
  //delay(15000);
}
