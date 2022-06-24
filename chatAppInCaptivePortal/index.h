/*
* Public Chat Application in Captive Portal
*
* Licence: MIT
* 2022 - Hasan Enes Şimşek
*/

// this header will store html page in a char array to be served by server
// codes are copied from index.html
// Note: html page should not include % char else than processor variable because of inferior design of ESPAsyncWebServer

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>

  <title>Public Chat</title>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">

<style>
  html {
    font-family: Arial, Helvetica, sans-serif;
    text-align: center;
	background-color: #e2e9ec;
  }
  h2 {
    font-size: 1.8rem;
    color: white;
  }
  .topnav {
    overflow: hidden;
    background-color: #143642;
  }
  body {
    margin: 0;
  }
  table {
	margin-left: auto;
	margin-right: auto;
  }
  textarea, input {
  background-color: #e2e9ec;
  }
  a.button{
  	display:inline-block;
  	padding:0.1em 1em;
  	border:0.1em solid #143642;
  	margin:0 0.3em 0.3em 0;
  	border-radius:0.12em;
  	box-sizing: border-box;
  	text-decoration:none;
  	font-family:'Roboto',sans-serif;
  	font-weight:100;
  	color:#143642;
  	text-align:center;
  	transition: all 0.2s;
  }
  a.button:hover{
  	color:#ffffff;
  	background-color:#143642;
  }
  @media all and (max-width:30em){
  	a.button{
  		display:block;
  		margin:0.4em auto;
  	}
  }
</style>
  
</head>
<body>

  <div class="topnav">
	<h2>Public Chat</h2>
  </div>
  <br>
  <textarea id="globalChat" style="width: -webkit-fill-available; margin: 10px" rows="20"></textarea>
  <textarea id="sendToChat" style="width: -webkit-fill-available; margin: 10px" rows="2"></textarea>
  <br>
  <br>
  <table>
    <tr>
		<td><input type="text" name="name" id="name" placeholder="nick name" maxlength="35"></td>
		<td><a class="button" onclick="sendMsg()">Send</a></td>
    </tr>
  </table>
  
<script>
  // max size of outgoing msg buffer may be defined dynamically from server
  // if OUTGOING_MSG_BUFFER_SIZE is defined, it will be used
  // otherwise, user will not be restricted
  let restrictUser = false;
  let max_size_of_msg_dynamic = "%OUTGOING_MSG_BUFFER_SIZE%";
  let max_size_of_msg = parseInt(max_size_of_msg_dynamic);
  if(!isNaN(max_size_of_msg))
    restrictUser = true;
  
  
  // restrict size of outgoing msg textarea 
  function restrictSizeOfTextarea()
  {
    if(restrictUser)
    {
      let sizeOfDate = getDate().length;
      let sizeOfName = document.getElementById("name").maxLength;
      // 35 is for json keys and punctations
      let sizeOfText = max_size_of_msg - sizeOfName - sizeOfDate - 35;
      document.getElementById("sendToChat").maxLength = sizeOfText;
    }
  }


  window.addEventListener('load', function() {
    restrictSizeOfTextarea();
  })

  // Create WebSocket connection.
  let host_name = window.location.hostname;
  if(host_name == "")
    host_name = "127.0.0.1" // for debug purpose (websocket test python files were configured to work in localhost)
  
  const socket = new WebSocket("ws://"+ host_name +"/ws");
  
  // Connection opened
  /*
  socket.addEventListener('open', function (event) {
  	socket.send("server-client connected");
  });
  */
  
  // Listen for messages
  socket.addEventListener('message', function (event) {
  	console.log('Message from server:', event.data);
    try {
      let jsonObj = JSON.parse(event.data);
      for(let i = 0; i < jsonObj.length; i++) {
        let onemsg = jsonObj[i];
        appendTextToTextArea(onemsg.date, onemsg.name, onemsg.text);
      }
    }
      catch (e){
      console.log("incoming message could not be parsed.");
      appendTextToTextArea("!!", "Server", event.data);
    }
  	
  });
	
  function appendTextToTextArea(datetime, name, text) {
    let form = document.getElementById("globalChat"); 
    form.value = form.value + datetime + " (" + name + "):" + text +"\n"
    form.scrollTop = form.scrollHeight; // scroll down to bottom
  }
  
  function getDate() {
    var currentdate = new Date(); 
    var datetime =  currentdate.getDate() + "/"
            + (currentdate.getMonth()+1)  + "/" 
            + currentdate.getFullYear() + " "  
            + currentdate.getHours() + ":"  
            + currentdate.getMinutes() + ":" 
            + currentdate.getSeconds();
    return datetime
  }
  
  function sendMsg() {
    let username = document.getElementById("name").value;
    let textToSend = document.getElementById("sendToChat").value; 
    document.getElementById("sendToChat").value = ""; // clear textarea 
    
    // erase new line
    username = username.replace(/(\r\n|\n|\r)/gm, "");
    textToSend = textToSend.replace(/(\r\n|\n|\r)/gm, "");
    
    let date = getDate();
    let jsonToSend = {
      "date": date,
      "name": username,
      "text": textToSend
    }
    let jsonToSendString = JSON.stringify(jsonToSend);
    socket.send(jsonToSendString);
  }
  
</script>
</body>
</html>
)rawliteral";