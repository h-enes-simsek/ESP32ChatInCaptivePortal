import logging
from websocket_server import WebsocketServer
import json

"""
improved test server for websockets with some functionality in order to test html page without using esp32
* when a client send message to server, server will save it and send it to all
* when a client connected to server, server will send all saved messages to the client
"""

savedMessages = [] # to keep messages from clients as str

def new_client(client, server):
    json_data = json.dumps(savedMessages) 
    server.send_message(client, json_data) # send all saved msg to new client

def message_from_client(client, server, msg):
    try:
        incomingJSONObj = json.loads(msg) # convert into json obj to verify incoming string is json
        
        # if name is empty, change it to "unknown"
        if(incomingJSONObj["name"] == ""):
            incomingJSONObj["name"] = "unknown"
        
        
        savedMessages.append(incomingJSONObj) # save incoming message
        
        array_wrapper = [incomingJSONObj]
        
        json_data = json.dumps(array_wrapper) # incoming string is json so convert back to string again
        
        server.send_message_to_all(json_data) # send all clients
    
    except:
        #incoming message is not json, so drop it
        print("dropped msg:" + msg)
        pass
    
    
server = WebsocketServer(host='127.0.0.1', port=80, loglevel=logging.INFO)
server.set_fn_message_received(message_from_client)
server.set_fn_new_client(new_client)
server.run_forever()