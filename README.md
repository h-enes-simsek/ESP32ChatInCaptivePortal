# ESP32ChatInCaptivePortal
Web-based public chat application in captive portal page via esp32 access point

## My Motivation
I thought it will be an interesting idea to create a publicly accessible chat via a free wifi access point with an intriguing ssid, especially for dormitories. In order to make the application easily accessible for the average internet user, I've used the captive portal by avoiding using an ip address.

## Requirements
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) (for webserver and websocket)

## Installation
- Change maximum limit of message number in queue in AsyncWebSocket.h, you cannot send number of stored messages to newly connected client more than this limit. Current limit is so low, 8 or 32. 

```
#define WS_MAX_QUEUED_MESSAGES 500 // send up to 500 previously stored message to newly connected clients
```

- (Optional) You can set configurable things as you desire in chatAppInCaptivePortal.ino

```
// <-- advised configurable parameters --

const char* ssid = "Public Chat";
const char* password = "";

// when server get this message from any client, it will clear flash (delete saved messages)
// if you dont want this feature, comment out it.
#define SPECIAL_TEXT_TO_CLEAR_FLASH "erase_flash"

// -- configurable parameters -->
```
For example choice a special message to erase flash (not program, only stored messages) 

- (Optional) If you wish to modify html page, only index.h will be considered by compiler. Just change index.html and look what happens with a browser, then copy paste codes to index.h

## Secreenshots
![ss](docs/SS.jpg)

## Documentation
### Sequence Diagram
![seq-diagram](docs/SeqDiagram.jpg)
