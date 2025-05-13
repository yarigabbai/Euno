#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>EUNO Autopilot</title>
  <style>
    body {
      font-family: Arial, sans-serif;
      background-color: #111;
      color: #fff;
      margin: 0;
      padding: 10px;
    }
    h1 {
      text-align: center;
    }
    .container {
      width: 100%;
      max-width: 600px;
      margin: 0 auto;
      padding: 10px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(100px, 1fr));
      gap: 10px;
      margin-bottom: 20px;
    }
    .box {
      background: #222;
      padding: 10px;
      border: 1px solid #444;
      border-radius: 8px;
      text-align: center;
    }
    .label {
      font-size: 14px;
      color: #aaa;
      margin-bottom: 5px;
    }
    .value {
      font-size: 24px;
      color: #0f0;
    }
    .red {
      background: #400;
    }
    .green {
      background: #040;
    }
    .buttons {
      display: flex;
      flex-wrap: wrap;
      justify-content: center;
      margin-bottom: 20px;
    }
    .buttons button {
      font-size: 18px;
      padding: 10px;
      margin: 5px;
      border-radius: 8px;
      border: none;
      cursor: pointer;
      min-width: 80px;
    }
    .blue {
      background: #007bff;
      color: #fff;
    }
    .orange {
      background: #f39c12;
      color: #fff;
    }
    .yellow {
      background: #f1c40f;
      color: #000;
    }
    #paramSection {
      display: none;
      background-color: #222;
      border: 1px solid #444;
      border-radius: 8px;
      padding: 10px;
      margin-bottom: 20px;
    }
    .param-row {
      margin: 10px 0;
    }
    .param-row label {
      display: inline-block;
      width: 120px;
      text-align: right;
      margin-right: 10px;
      color: #ccc;
    }
    .param-row input[type="range"] {
      width: 150px;
      vertical-align: middle;
    }
    #paramConfirm {
      margin-top: 10px;
      color: #0f0;
      min-height: 1.2em;
    }
    #otaBanner {
      display: none;
      margin: 20px auto;
      padding: 15px;
      background-color: #4CAF50;
      color: #fff;
      font-size: 22px;
      font-weight: bold;
      border-radius: 8px;
      width: 80%;
      text-align: center;
    }
    .progress-bar {
      width: 100%;
      background-color: #333;
      border-radius: 5px;
      margin-top: 10px;
      height: 20px;
    }
    .progress-bar-fill {
      height: 20px;
      background-color: #4CAF50;
      border-radius: 5px;
      width: 0%;
      transition: width 0.3s;
    }
  </style>
</head>
<body>
  <h1>EUNO Autopilot</h1>

  <div id="otaBanner"></div>

  <div class="container">
    <div class="grid">
      <div class="box">
        <div class="label">Heading</div>
        <div id="heading" class="value">N/A</div>
      </div>
      <div class="box">
        <div class="label">Cmd</div>
        <div id="cmd" class="value">N/A</div>
      </div>
      <div class="box">
        <div class="label">Err</div>
        <div id="err" class="value">N/A</div>
      </div>
      <div class="box">
        <div class="label">GPS</div>
        <div id="gps" class="value">N/A</div>
      </div>
      <div class="box">
        <div class="label">Spd</div>
        <div id="spd" class="value">N/A</div>
      </div>
      <div id="statusBox" class="box red">
        <div class="value">OFF</div>
      </div>
    </div>

    <div id="menu" class="buttons">
      <button class="blue" onclick="send('ACTION:-1')">-1</button>
      <button class="blue" onclick="send('ACTION:+1')">+1</button>
      <button class="blue" onclick="send('ACTION:-10')">-10</button>
      <button class="blue" onclick="send('ACTION:+10')">+10</button>
      <button class="orange" onclick="send('ACTION:TOGGLE')">ON/OFF</button>
      <button class="orange" onclick="toggleMenu()">MENU</button>
    </div>

    <div id="menu2" class="buttons" style="display:none;">
      <button class="yellow" onclick="send('ACTION:CAL')">CALIB</button>
      <button class="yellow" onclick="send('ACTION:GPS')">GPS</button>
      <button class="yellow" onclick="send('ACTION:C-GPS')">C-GPS</button>
      <button class="blue" onclick="toggleParamSection()">Parametri</button>
      <button class="yellow" onclick="send('ACTION:EXT_BRG')">EXTBRG</button>
      <button class="yellow" onclick="send('ACTION:CAL-GYRO')">GYRO</button>
      <button class="blue" onclick="toggleMenu()">MENU</button>
    </div>

    <div id="paramSection">
      <h3>Regolazione Parametri</h3>
      <div class="param-row">
        <label for="V_min">V_min:</label>
        <input type="range" id="V_min" min="0" max="255" value="100" oninput="updateLabel('V_min')">
        <span id="V_min_val">100</span>
      </div>
      <div class="param-row">
        <label for="V_max">V_max:</label>
        <input type="range" id="V_max" min="0" max="255" value="150" oninput="updateLabel('V_max')">
        <span id="V_max_val">150</span>
      </div>
      <div class="param-row">
        <label for="E_min">E_min:</label>
        <input type="range" id="E_min" min="0" max="360" value="20" oninput="updateLabel('E_min')">
        <span id="E_min_val">20</span>
      </div>
      <div class="param-row">
        <label for="E_max">E_max:</label>
        <input type="range" id="E_max" min="0" max="360" value="80" oninput="updateLabel('E_max')">
        <span id="E_max_val">80</span>
      </div>
      <div class="param-row">
        <label for="Deadband">Deadband:</label>
        <input type="range" id="Deadband" min="0" max="50" value="10" oninput="updateLabel('Deadband')">
        <span id="Deadband_val">10</span>
      </div>
      <div class="param-row">
        <label for="T_risposta">T resp [s]:</label>
        <input type="range" id="T_risposta" min="3" max="12" value="8" oninput="updateLabel('T_risposta')">
        <span id="T_risposta_val">8</span>
      </div>
      <div class="param-row">
        <label for="T_pause">T_pause:</label>
        <input type="range" id="T_pause" min="0" max="9" value="0" oninput="updateLabel('T_pause')">
        <span id="T_pause_val">0</span>
      </div>
      <button class="blue" onclick="sendAllParams()">Invia Parametri</button>
      <div id="paramConfirm"></div>
    </div>

    <!-- Sezione OTA Aggiornata -->
    <h2>OTA WebSocket Update</h2>
    <div style="margin-bottom: 20px;">
      <input type="file" id="otaFile">
      <button class="blue" onclick="startOTA('ap')">Aggiorna AP</button>
      <button class="blue" onclick="startOTA('client')">Aggiorna Client</button>
    </div>
    <div id="otaWsStatus" style="margin-top:10px;color:#0f0;"></div>
    <div class="progress-bar">
      <div id="otaProgress" class="progress-bar-fill"></div>
    </div>

    <div id="serialConsole" style="background-color:#111; color:#0f0; padding:10px; border:1px solid #444; border-radius:8px; height:150px; overflow:auto; font-family:monospace; font-size:12px;">
      <strong>Console seriale</strong><br>
      <div id="serialLog" style="height:200px; overflow-y:auto; background:black; color:lime; padding:10px; font-family:monospace; font-size:14px;">
        <b>Console WebSocket:</b><br>
      </div>
    </div>

  </div>

  <script>
    const ws = new WebSocket("ws://" + location.hostname + ":81/");

    function appendToConsole(text) {
      const log = document.getElementById("serialLog");
      const newLine = document.createElement("div");
      newLine.textContent = text;
      log.appendChild(newLine);
      log.scrollTop = log.scrollHeight;
    }

    ws.onopen = function() {
      console.log("WebSocket connesso");
      ws.send("GET_PARAMS");
    };

    ws.onmessage = function(evt) {
      const msg = evt.data;
      console.log("WS:", msg);

      if (msg.startsWith("LOG:")) {
        appendToConsole(msg.substring(4));
        return;
      }

      if (msg.startsWith("$AUTOPILOT")) {
        const parts = msg.split(',');
        const get = (key) => {
          let el = parts.find(p => p.startsWith(key + "="));
          return el ? el.split('=')[1] : "N/A";
        };
        document.getElementById("heading").innerText = get("HEADING");
        document.getElementById("cmd").innerText     = get("COMMAND");
        document.getElementById("err").innerText     = get("ERROR");
        document.getElementById("gps").innerText     = get("GPS_HEADING");
        document.getElementById("spd").innerText     = get("GPS_SPEED");
      }
      else if (msg.startsWith("PARAMS:")) {
        const paramsStr = msg.substring(7);
        const params = paramsStr.split(',');
        params.forEach(param => {
          const [name, value] = param.split('=');
          const slider = document.getElementById(name);
          const valueLabel = document.getElementById(name + "_val");
          if (slider && valueLabel) {
            slider.value = value;
            valueLabel.innerText = value;
          }
        });
        document.getElementById("paramConfirm").innerText = "Parametri sincronizzati";
        setTimeout(() => {
          document.getElementById("paramConfirm").innerText = "";
        }, 3000);
      }
      else if (msg.startsWith("PARAM_UPDATED:")) {
        const data = msg.substring("PARAM_UPDATED:".length);
        const [pName, pVal] = data.split("=");
        if(document.getElementById(pName)) {
          document.getElementById(pName).value = pVal;
          document.getElementById(pName + "_val").innerText = pVal;
        }
        document.getElementById("paramConfirm").innerText =
          "Parametro aggiornato: " + pName + " = " + pVal;
        setTimeout(() => {
          document.getElementById("paramConfirm").innerText = "";
        }, 3000);
      }
      else if (msg.startsWith("OTA_PROGRESS:")) {
        const progress = msg.substring(13);
        document.getElementById("otaProgress").style.width = progress + "%";
        document.getElementById("otaWsStatus").innerHTML =
          `<div style="color:#4CAF50;">Aggiornamento in corso: ${progress}%</div>`;
      }
      else if (msg === "OTA_COMPLETE_AP") {
        showOtaBanner("OTA AP completato con successo!");
      }
      else if (msg === "OTA_COMPLETE_CLIENT") {
        showOtaBanner("OTA Client completato con successo!");
      }
      else if (msg === "MOTOR:ON") {
        const el = document.getElementById("statusBox");
        const txt = el.querySelector(".value");
        txt.innerText = "ON";
        el.className = "box green";
      }
      else if (msg === "MOTOR:OFF") {
        const el = document.getElementById("statusBox");
        const txt = el.querySelector(".value");
        txt.innerText = "OFF";
        el.className = "box red";
      }
    };

    function showOtaBanner(text) {
      const banner = document.getElementById("otaBanner");
      banner.innerHTML = text;
      banner.style.display = "block";
      setTimeout(() => {
        banner.style.display = "none";
      }, 5000);
    }

    function send(cmd) {
      if (ws.readyState === WebSocket.OPEN) {
        ws.send(cmd);
      }
    }

    function toggleStatus() {
      const el = document.getElementById("statusBox");
      const txt = el.querySelector(".value");
      if (txt.innerText === "ON") {
        txt.innerText = "OFF";
        el.className = "box red";
      } else {
        txt.innerText = "ON";
        el.className = "box green";
      }
    }

    function toggleMenu() {
      const m1 = document.getElementById("menu");
      const m2 = document.getElementById("menu2");
      m1.style.display = (m1.style.display === "none") ? "flex" : "none";
      m2.style.display = (m2.style.display === "none") ? "flex" : "none";
    }

    function toggleParamSection() {
      const ps = document.getElementById("paramSection");
      ps.style.display = (ps.style.display === "none") ? "block" : "none";
    }

    function updateLabel(paramId) {
      const slider = document.getElementById(paramId);
      document.getElementById(paramId + "_val").innerText = slider.value;
    }

    function sendAllParams() {
      const paramList = ["V_min", "V_max", "E_min", "E_max", "Deadband", "T_risposta", "T_pause"];
      for (let i = 0; i < paramList.length; i++) {
        const param = paramList[i];
        const val = document.getElementById(param).value;
        send(`SET:${param}=${val}`);
      }
    }

   function startOTA(target) {
    const fileInput = document.getElementById("otaFile");
    if (!fileInput.files.length) return;

    const file = fileInput.files[0];
    const chunkSize = 1024;
    let offset = 0;
    
    ws.send(target === 'client' ? "$OTA_CLIENT:" + file.size : "$OTA_AP:" + file.size);

    const reader = new FileReader();
    reader.onload = function(e) {
        if (ws.readyState === WebSocket.OPEN) {
            ws.send(e.target.result);
            offset += e.target.result.byteLength;
            
            // Aggiorna progresso
            const percent = Math.floor((offset / file.size) * 100);
            document.getElementById("otaProgress").style.width = percent + "%";
            document.getElementById("otaWsStatus").innerText = 
                `Caricamento ${target}: ${percent}%`;
            
            // Prossimo chunk
            if (offset < file.size) {
                setTimeout(() => {
                    const nextChunk = file.slice(offset, offset + chunkSize);
                    reader.readAsArrayBuffer(nextChunk);
                }, 20);
            } else {
                ws.send("OTA_END");
                document.getElementById("otaWsStatus").innerText = 
                    "Completato! Riavvio...";
            }
        }
    };
    
    // Avvia
    const firstChunk = file.slice(0, chunkSize);
    reader.readAsArrayBuffer(firstChunk);
}
      
      function updateProgress(offset, total, target) {
        const percent = Math.floor((offset / total) * 100);
        const targetStr = target === 'client' ? "Client" : "AP";
        document.getElementById("otaWsStatus").innerText = 
          `Invio OTA ${targetStr}: ${percent}%`;
        document.getElementById("otaProgress").style.width = percent + "%";
      }
      
        function sendNextChunk() {
        if (offset >= file.size) {
            document.getElementById("otaWsStatus").innerText = 
                "Invio completato. Attendere riavvio...";
            return;
        }
      
      sendNextChunk();
    }
  </script>
</body>
</html>
)rawliteral";

#endif