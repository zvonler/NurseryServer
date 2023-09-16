
window.onload = init;

function init() {
    showStatus(null);
    setInterval(refresh, 5000);
}

function refresh() {
    var r = new XMLHttpRequest();
    r.open("GET", "http://nursery-devel.local/status");
    r.onreadystatechange = function() {
      if (this.readyState == 4 && this.status == 200) {
          showStatus(this);
      }
  };

    r.send()
}

function showStatus(response) {
  if (response) {
    var parsed_json = JSON.parse(response.responseText);
    document.getElementById("time").innerHTML = parsed_json["time"];
    var brightness = parsed_json["brightness"];
    document.getElementById("light_status").innerHTML = brightness ? "ON" : "OFF";
    document.getElementById("last_light_time").innerHTML = parsed_json["last_light_time"];
    document.getElementById("brightness").innerHTML = parsed_json["brightness"];
    document.getElementById("door_status").innerHTML = parsed_json["door_status"];
    document.getElementById("last_door_time").innerHTML = parsed_json["last_door_time"];
    document.getElementById("last_motion_time").innerHTML = parsed_json["last_motion_time"];
    document.getElementById("temperature").innerHTML = parsed_json["temperature"] + " F";
    document.getElementById("humidity").innerHTML = parsed_json["humidity"] = " %";
    document.getElementById("server_uptime").innerHTML = parsed_json["server_uptime"];
  } else {
    document.getElementById("time").innerHTML = "Waiting for status";
    document.getElementById("light_status").innerHTML = "UNKNOWN";
    document.getElementById("last_light_time").innerHTML = "N/A";
    document.getElementById("brightness").innerHTML = "UNKNOWN";
    document.getElementById("door_status").innerHTML = "UNKNOWN";
    document.getElementById("last_door_time").innerHTML = "N/A";
    document.getElementById("last_motion_time").innerHTML = "N/A";
    document.getElementById("temperature").innerHTML = "N/A";
    document.getElementById("humidity").innerHTML = "N/A";
    document.getElementById("server_uptime").innerHTML = "N/A";
  }
}

function send_get(url) {
  var r = new XMLHttpRequest();
  r.open("GET", url);
  r.send();
}

function off() {
  send_get("http://nursery-devel.local/off");
}

function brighter() {
    send_get("http://nursery-devel.local/brighter");
}

function dimmer() {
    send_get("http://nursery-devel.local/dimmer");
}

function wake() {
    send_get("http://nursery-devel.local/wake");
}
