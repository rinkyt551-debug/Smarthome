// ============================================================
//  Smart Home dashboard - script.js
//
//  The page keeps the whole house in ONE object called "home".
//  Flow of every click:
//      button click -> change "home" -> redraw the page -> send "home" to server.cpp
//  server.cpp checks the values and saves them in state.txt.
//  Every 3 seconds the page also asks the server for state.txt, so changes
//  made in the console program (smarthome.exe) appear here too.
// ============================================================

// ---------- 1. THE DATA ----------
var home = {
  light: { on: true, level: 50 },
  fan:   { on: true, speed: 0 },
  ac:    { on: true, temp: 24, mode: 1, timer: 0 },   // mode: 0 ECO, 1 NORMAL, 2 TURBO
  tv:    { on: true, volume: 20, channel: 1 }
};
var labels = { light: "Living room light", fan: "Ceiling fan", ac: "Smart AC", tv: "Living room TV" };
var MODE_NAMES = ["ECO", "NORMAL", "TURBO"];
var savePending = false;      // true while a save is on its way to the server

// ---------- 2. TALKING TO THE SERVER (same text format as state.txt) ----------
// Each row is [device, setting, key used in state.txt].
// To add a new setting, add one row here (and one rule in server.cpp).
var FIELDS = [
  ["light", "on", "light_on"], ["light", "level", "light_level"],
  ["fan", "on", "fan_on"],     ["fan", "speed", "fan_speed"],
  ["ac", "on", "ac_on"],       ["ac", "temp", "ac_temp"],
  ["ac", "mode", "ac_mode"],   ["ac", "timer", "ac_timer"],
  ["tv", "on", "tv_on"],       ["tv", "volume", "tv_volume"],
  ["tv", "channel", "tv_channel"]
];

// home -> text such as "light_on 1\nlight_level 50\n..."
function homeToText() {
  return FIELDS.map(function (f) { return f[2] + " " + Number(home[f[0]][f[1]]); }).join("\n") + "\n";
}

// text from the server -> home
function textToHome(text) {
  text.split("\n").forEach(function (line) {
    var parts = line.trim().split(/\s+/);
    FIELDS.forEach(function (f) {
      if (f[2] !== parts[0] || isNaN(parseInt(parts[1], 10))) return;
      var number = parseInt(parts[1], 10);
      home[f[0]][f[1]] = (f[1] === "on") ? number === 1 : number;   // "on" is true/false, the rest are numbers
    });
  });
}

function setConnection(ok) {
  document.getElementById("connection-label").textContent = ok ? "C++ SERVER CONNECTED" : "SERVER DISCONNECTED";
  document.getElementById("backend-status").textContent = ok ? "CONNECTED" : "OFFLINE";
}

function saveToServer() {
  savePending = true;
  setText("save-state", "SAVING…");
  fetch("/api/state", { method: "POST", body: homeToText() })
    .then(function (response) {
      return response.text().then(function (reply) {
        if (!response.ok) throw new Error(reply);            // the server refused the values
        savePending = false; setText("save-state", "SAVED"); setConnection(true);
      });
    })
    .catch(function (error) {
      savePending = false; setText("save-state", "NOT SAVED"); setConnection(false);
      showMessage(error.message || "Could not reach the C++ server. Start it with run.bat.", true);
    });
}

function loadFromServer(firstTime) {
  fetch("/api/state")
    .then(function (response) { if (!response.ok) throw new Error("server"); return response.text(); })
    .then(function (text) {
      if (savePending) return;                               // don't overwrite a change that is being saved
      textToHome(text);
      render();
      setConnection(true);
      if (firstTime) { setText("save-state", "READY"); showMessage("Connected. Device settings loaded from the C++ server.", false); }
    })
    .catch(function () {
      setConnection(false);
      setText("save-state", "OFFLINE");
      if (firstTime) showMessage("Cannot reach the C++ server. Start run.bat, choose the web dashboard and open http://127.0.0.1:8080", true);
    });
}

// ---------- 3. SHOWING THINGS ON THE PAGE ----------
function setText(id, value) { document.getElementById(id).textContent = value; }

function showMessage(message, isError) {
  document.querySelector(".message-row").classList.toggle("error", !!isError);
  setText("message-symbol", isError ? "!" : "✓");
  setText("message", message);
}

function toFahrenheit(c) { return Math.round((c * 9 / 5 + 32) * 10) / 10; }

// Copies the numbers in "home" onto the page.
function render() {
  var active = 0;
  ["light", "fan", "ac", "tv"].forEach(function (name) {
    var isOn = home[name].on;
    document.getElementById(name).classList.toggle("off", !isOn);   // "off" class greys the card
    setText(name + "-status", isOn ? "ON" : "OFF");
    setText(name + "-power", isOn ? "Switch off" : "Switch on");
    if (isOn) active++;
  });
  setText("active-count", active + " of 4 devices active");

  setText("light-level", home.light.level);
  document.getElementById("light-track").style.width = home.light.level + "%";

  setText("fan-speed", home.fan.speed);
  document.querySelectorAll(".speed-meter i").forEach(function (bar, index) {
    bar.classList.toggle("active", index < home.fan.speed);
  });
  var presets = { 1: "low", 3: "medium", 5: "high" };               // which Low/Med/High button is lit
  document.querySelectorAll("[data-speed]").forEach(function (button) {
    button.classList.toggle("chosen", button.getAttribute("data-speed") === presets[home.fan.speed]);
  });

  setText("ac-temp", home.ac.temp);
  setText("ac-fahrenheit", toFahrenheit(home.ac.temp) + " °F");
  setText("ac-timer", home.ac.timer);
  document.querySelectorAll("[data-mode]").forEach(function (button) {
    button.classList.toggle("chosen", Number(button.getAttribute("data-mode")) === home.ac.mode);
  });

  setText("tv-channel", home.tv.channel);
  setText("tv-volume", home.tv.volume);
}

// ---------- 4. THE BUTTONS ----------
function limit(value, low, high) { return Math.max(low, Math.min(high, value)); }

// A device that is OFF refuses changes (same rule as needOn() in smarthome.cpp).
function needOn(name) {
  if (home[name].on) return true;
  showMessage("Switch on " + labels[name] + " before changing its settings.", true);
  return false;
}

// Connects a button to an action. The action returns false when nothing changed.
function connect(id, action) {
  document.getElementById(id).addEventListener("click", function () {
    if (action.call(this) === false) return;
    render();
    saveToServer();
  });
}

// Power buttons
["light", "fan", "ac", "tv"].forEach(function (name) {
  connect(name + "-power", function () {
    home[name].on = !home[name].on;
    showMessage(labels[name] + " switched " + (home[name].on ? "on" : "off") + ".", false);
  });
});

// Light: brightness 0 to 100, steps of 10
function changeLight(step) {
  if (!needOn("light")) return false;
  home.light.level = limit(home.light.level + step, 0, 100);
  showMessage("Brightness set to " + home.light.level + "%.", false);
}
connect("light-up", function () { return changeLight(10); });
connect("light-down", function () { return changeLight(-10); });

// Fan: speed 0 to 5
function setFan(speed) {
  if (!needOn("fan")) return false;
  home.fan.speed = limit(speed, 0, 5);
  showMessage("Fan speed set to " + home.fan.speed + ".", false);
}
connect("fan-up", function () { return setFan(home.fan.speed + 1); });
connect("fan-down", function () { return setFan(home.fan.speed - 1); });
document.querySelectorAll("[data-speed]").forEach(function (button) {
  var values = { low: 1, medium: 3, high: 5 };
  button.addEventListener("click", function () {
    if (setFan(values[this.getAttribute("data-speed")]) === false) return;
    render(); saveToServer();
  });
});

// AC: temperature 16 to 30, mode, sleep timer 0 to 720 minutes (steps of 15)
function changeTemp(step) {
  if (!needOn("ac")) return false;
  var wanted = home.ac.temp + step;
  if (wanted < 16 || wanted > 30) { showMessage("AC temperature must stay between 16 °C and 30 °C.", true); return false; }
  home.ac.temp = wanted;
  showMessage("Smart AC set to " + wanted + " °C.", false);
}
connect("ac-up", function () { return changeTemp(1); });
connect("ac-down", function () { return changeTemp(-1); });

document.querySelectorAll("[data-mode]").forEach(function (button) {
  button.addEventListener("click", function () {
    if (!needOn("ac")) return;
    home.ac.mode = Number(this.getAttribute("data-mode"));
    showMessage("AC mode set to " + MODE_NAMES[home.ac.mode] + ".", false);
    render(); saveToServer();
  });
});

function changeTimer(step) {
  if (!needOn("ac")) return false;
  var wanted = home.ac.timer + step;
  if (wanted < 0 || wanted > 720) { showMessage("AC timer must stay between 0 and 720 minutes.", true); return false; }
  home.ac.timer = wanted;
  showMessage(wanted ? "AC sleep timer set to " + wanted + " minutes." : "AC timer cleared.", false);
}
connect("ac-timer-up", function () { return changeTimer(15); });
connect("ac-timer-down", function () { return changeTimer(-15); });

// TV: volume 0 to 100, channel 1 to 999
function changeVolume(step) {
  if (!needOn("tv")) return false;
  home.tv.volume = limit(home.tv.volume + step, 0, 100);
  showMessage("TV volume is " + home.tv.volume + "%.", false);
}
function changeChannel(step) {
  if (!needOn("tv")) return false;
  var wanted = home.tv.channel + step;
  if (wanted < 1 || wanted > 999) { showMessage("TV channel must stay between 1 and 999.", true); return false; }
  home.tv.channel = wanted;
  showMessage("TV channel changed to " + wanted + ".", false);
}
connect("tv-vol-up", function () { return changeVolume(5); });
connect("tv-vol-down", function () { return changeVolume(-5); });
connect("tv-ch-up", function () { return changeChannel(1); });
connect("tv-ch-down", function () { return changeChannel(-1); });

// ---------- 5. START ----------
loadFromServer(true);
setInterval(function () { if (!savePending) loadFromServer(false); }, 3000);   // pick up console changes
