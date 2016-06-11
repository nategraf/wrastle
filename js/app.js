Pebble.addEventListener("ready", function(e) {
  console.log("Wrastle Comms Ready");
});

Pebble.addEventListener("appmessage", function(e) {
  var req = new XMLHttpRequest();
  
  req.open("POST", "https://wrastle-nategraf.c9.io"+e.payload.PATH_KEY, true);
  
  delete e.payload.PATH_KEY;
  var message = JSON.stringify(e.payload);
  
  //Send the proper header information along with the request
  req.setRequestHeader("Content-type", "application/json");
  req.setRequestHeader("Content-length", message.length);
  req.setRequestHeader("Connection", "close");

  req.onload = function () {
    // do something to response
    console.log("Sent message!");
  };
  req.send(message);
});