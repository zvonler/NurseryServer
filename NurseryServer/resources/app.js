
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
  } else {
    document.getElementById("time").innerHTML = "Not loaded yet";
  }
}
