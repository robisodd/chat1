// ----------------------------------------------------------------------- //
//  PebbleKit JS Functions
// ----------------------------------------------------------------------- //
var OPEN_CONNECTION = 0;
var CLOSE_CONNECTION = 1;
var TAP = 3;
var tapID = parseInt(Math.random() * 200) + 10;

// Function to send a message to the Pebble using AppMessage API
// We are currently only sending a message using the "status" appKey defined in appinfo.json/Settings
function sendUserMessagetoPebble(usr, msg) {
  Pebble.sendAppMessage({"userkey":usr, "message": msg}, messageSuccessHandler, messageFailureHandler);
}
function sendMessagetoPebble(msg) {
	Pebble.sendAppMessage({"message": msg}, messageSuccessHandler, messageFailureHandler);
}
function sendStatustoPebble(pos) {
	Pebble.sendAppMessage({"status": pos}, messageSuccessHandler, messageFailureHandler);
}
function sendSystemMessagetoPebble(msg) {
	Pebble.sendAppMessage({"system_message": msg}, messageSuccessHandler, messageFailureHandler);
}
function sendErrorMessagetoPebble(msg) {
	Pebble.sendAppMessage({"err_message": msg}, messageSuccessHandler, messageFailureHandler);
}

function messageSuccessHandler() {
  //console.log("Message send succeeded.");
}
function messageFailureHandler() {
  console.log("Message send failed.");
}

Pebble.addEventListener("ready", function(e) {
  console.log("JS is ready!");
  sendSystemMessagetoPebble("PebbleKit JS Ready");
  //sendMessagetoPebble();
  //establish_connection();
});


Pebble.addEventListener("appmessage", function(e) {
  if (typeof e.payload.message !== 'undefined') {
    console.log("Received Message from Pebble C: " + e.payload.message);
    send_ws_message(e.payload.message);
  }

  if (typeof e.payload.status !== 'undefined') {
    console.log("Received Status from Pebble C: " + e.payload.status);
    if(e.payload.status === OPEN_CONNECTION) {
      console.log("Task: Create connection...");
      establish_connection();
    } else if(e.payload.status === CLOSE_CONNECTION) {
      console.log("Task: Close connection...");
      close_connection();
    } else if(e.payload.status === TAP) {
      send_ws_tap();
    }
  }
});

// ----------------------------------------------------------------------- //
//  WebSocket Functions
// ----------------------------------------------------------------------- //
var SOCKET_CONNECTING = 0;
var SOCKET_OPEN = 1;
var SOCKET_CLOSING = 2;
var SOCKET_CLOSED = 3;

var socket = {};
//var wsServer = "ws://192.168.33.103:8080";
var wsServer = 'wss://chat1-robisodd.rhcloud.com:8443/';
//var wsServer = 'ws://chat1-robisodd.rhcloud.com:8000/';


function close_connection() {
  switch(socket.readyState) {
    case SOCKET_OPEN: 
    case SOCKET_CONNECTING:
      console.log("Closing connection...");
      sendSystemMessagetoPebble("Closing connection...");
      send_ws_message("Goodbye everybody!");
      socket.close();
      return;
  }
  console.log("Connection has already been closed.");
  sendErrorMessagetoPebble("ALREADY CLOSED");
}


function establish_connection() {
  switch(socket.readyState) {
    case SOCKET_OPEN:
      console.log("Connection already established.");
      sendErrorMessagetoPebble("ALREADY OPEN");
      return true;

    case SOCKET_CONNECTING:
      console.log("Still trying to connect...");
      sendErrorMessagetoPebble("STILL CONNECTING");
      return false;

    case SOCKET_CLOSING: 
    case SOCKET_CLOSED:
      //default:
      console.log("Connection is closed.");
  }
  console.log("Creating new socket...");
  
  try {
    sendSystemMessagetoPebble("Connecting...");
    socket = new WebSocket(wsServer);//, 'echo-protocol');
    socket.binaryType = "arraybuffer";

    socket.onopen = function() {
      console.log("Connection Established!");
      sendSystemMessagetoPebble("CONNECTED");
      var username = Pebble.getAccountToken();
      username = "Pebble " + username.slice(username.length - 5);
      socket.send(JSON.stringify({"user":username}));
    };

    socket.onmessage = function(msg) {
//       console.log("onmessage = " + JSON.stringify(msg.data));
//       console.log(JSON.parse(JSON.stringify(msg)));
//       console.log("onmessage = " + JSON.stringify(msg));
      if(typeof msg.data == "string") {
        try {
          var obj = JSON.parse(msg.data);
          //if (typeof obj.user !== 'undefined')
          var usr = obj.user || "someone";
          var say = obj.says || "something";
          console.log("Text Message from " + usr +": '" + say + "'");
          sendUserMessagetoPebble(usr, say);
        } catch (e) {
          sendMessagetoPebble(msg.data);
        }
      }

      if(msg.data instanceof ArrayBuffer) {
        //if(typeof msg.data == "object") {
        var array = new Uint8Array(msg.data);
        //console.log("Array length = " + array.length);
        //sendMessagetoPebble("DATA: ["+array+"]");
        var bytes = [];
        for(var i = 0; i<array.length; i++)
          bytes.push(array[i]);
        console.log("Data Message: [" + bytes + "]");
        //sendUserMessagetoPebble("DATA MESSAGE", "["+bytes+"]");
        if(array[0]!==tapID)
          sendStatustoPebble(1);
        //sendMessagetoPebble("DATA: ["+bytes+"]");
      }
    };

    socket.onclose = function() {
      sendSystemMessagetoPebble("CONNECTION CLOSED");
      socket = {};
    };

    socket.onerror = function(err) {
      console.log("socket error - err=" + JSON.stringify(err));
      sendErrorMessagetoPebble("Socket Error");
      socket = {};
    };

  } catch(err) {
    console.log("socket error=" + err);
    sendErrorMessagetoPebble("Socket Error");
    socket = {};
  } 

  //if(socket) 
    if(socket.readyState === SOCKET_OPEN)
      return true;
  
  if(socket.readyState === SOCKET_CONNECTING)
    console.log("Connection Being Established...");
  else {
    console.log("Connection Failed");
    sendErrorMessagetoPebble("Socket FAIL");
  }
  return false;
}

function is_connected() {
  switch(socket.readyState) {
    case SOCKET_OPEN:
      // Connection is established.  No need to log it every time a message is sent.
      return true;

    case SOCKET_CONNECTING:
      console.log("Still trying to connect...");
      sendErrorMessagetoPebble("[not connected yet]");
      return false;
  }
  console.log("Connection has been closed.");
  sendErrorMessagetoPebble("[Disconnected]");
  return false;
}

// function send_ws_message_old(msg) {
//   if(is_connected()) {
//     socket.send(JSON.stringify({"says":msg}));
//   }
// }

//function sendwsmsg(msg) {
function send_ws_message(msg) {
  if(socket.readyState === SOCKET_OPEN)
    socket.send(JSON.stringify({"says":msg}));
  else
    sendErrorMessagetoPebble("[Not Connected]");
}
        
// function send_ws_message(msg) {
//   if(establish_connection()) {
//     // HAS an established connection
//     //var pos = parseInt(Math.random()*100);
//     //socket.send("Updated Position: " + pos);
//     //sendStatustoPebble(pos);
//     //socket.send(JSON.stringify({"user":"Pebble" + Pebble.getAccountToken()}));
//     //socket.send(JSON.stringify({"user":"guy1", "says":"Hello There"}));
//     //socket.send(JSON.stringify({"user":"guy1", "says":"Hello There"}));
//     socket.send(JSON.stringify({"says":msg}));
//     //sendMessagetoPebble("Me: Hello There.");
//   } else {
//     sendSystemMessagetoPebble("[No WS Connection]");
//   }
// }


function send_ws_bytes() {
  if(establish_connection()) {
    var message = new Uint8Array([0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89]);
    socket.send(message);
  } else {
    sendErrorMessagetoPebble("[No WS Connection]");
  }
}


function send_ws_tap() {
  if(socket.readyState === SOCKET_OPEN)
    socket.send(new Uint8Array([tapID]));

//   if(establish_connection()) {
//     var message = new Uint8Array([1]);
//     socket.send(message);
//   }
  //else sendErrorMessagetoPebble("[No WS Connection]");
}
