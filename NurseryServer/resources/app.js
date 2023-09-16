
window.onload = init;

function init() {
  showStatus(null);
  refresh();
  setInterval(update, 1000);
}

var last_refresh_tm = 0;
const REFRESH_INTERVAL = 5000;

function update() {
  var now = Date.now();
  if (now - last_refresh_tm > REFRESH_INTERVAL) {
    refresh();
  }
}

function send_get(uri, onreadystatechange) {
  var url = "http://nursery-devel.local/" + uri;
  var r = new XMLHttpRequest();
  r.open("GET", url);
  if (onreadystatechange != null) {
    r.onreadystatechange = () => { onreadystatechange(r); };
  }
  r.send();
}

function refresh() {
  last_refresh_tm = Date.now();
  send_get("status", (req) => {
    if (req.readyState == 4 && req.status == 200) {
      showStatus(req);
    } else {
      showStatus(null);
    }
  });
}

function showStatus(response) {
  var placeholder = document.getElementById("placeholder");
  var status_panel = document.getElementById("status_panel");

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
    document.getElementById("humidity").innerHTML = parsed_json["humidity"] + " %";
    document.getElementById("server_uptime").innerHTML = parsed_json["server_uptime"];

    placeholder.className = "hide";
    status_panel.className = "show";
  } else {
    document.getElementById("time").innerHTML = "N/A";
    document.getElementById("light_status").innerHTML = "N/A";
    document.getElementById("last_light_time").innerHTML = "N/A";
    document.getElementById("brightness").innerHTML = "N/A";
    document.getElementById("door_status").innerHTML = "N/A";
    document.getElementById("last_door_time").innerHTML = "N/A";
    document.getElementById("last_motion_time").innerHTML = "N/A";
    document.getElementById("temperature").innerHTML = "N/A";
    document.getElementById("humidity").innerHTML = "N/A";
    document.getElementById("server_uptime").innerHTML = "N/A";

    placeholder.className = "show";
    status_panel.className = "hide";
  }
}

function off() {
  send_get("off");
}

function brighter() {
  send_get("brighter");
}

function dimmer() {
  send_get("dimmer");
}

function wake() {
  send_get("wake");
}
