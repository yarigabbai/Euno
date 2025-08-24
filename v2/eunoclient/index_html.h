#pragma once
// UI locale per EUNO Client – compatibile con:
// - WebSocket su :81 (ws://<host>:81/)
// - Comandi formattati: $PEUNO,CMD,... (DELTA/TOGGLE/MODE/SET/...)
// - Telemetria: $AUTOPILOT,MODE=...,MOTOR=...,HDG=...,HDG_C=...,HDG_F=...,HDG_E=...,HDG_A=...,SOG=...
// - Salvataggio credenziali OP: POST /api/net (ssid, pass)

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>EUNO Autopilot – Client</title>
<style>
  :root{
    --bg:#0b0f13; --card:#111722; --border:#1c2433;
    --fg:#e7f0ff; --muted:#9fb0c7; --acc:#53b2ff;
    --ok:#2ecc71; --bad:#ff5c5c; --warn:#ffb84d;
  }
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,Segoe UI,Roboto,Helvetica,Arial}
  header{display:flex;align-items:center;gap:12px;justify-content:space-between;padding:14px 16px;border-bottom:1px solid var(--border);background:#0f141c}
  h1{margin:0;font-size:16px;font-weight:600}
  .pill{font:12px/1 mono;background:#0e1622;color:var(--muted);padding:6px 8px;border:1px solid var(--border);border-radius:9px}
  main{padding:14px;display:grid;gap:14px;grid-template-columns:repeat(auto-fit,minmax(280px,1fr))}
  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:14px}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  .sep{height:1px;background:var(--border);margin:12px 0}
  label{color:var(--muted);font-size:12px}
  input,select,button{border-radius:10px;border:1px solid var(--border);background:#0e1520;color:var(--fg);padding:10px 12px}
  button{cursor:pointer}
  button:active{transform:translateY(1px)}
  .good{background:rgba(46,204,113,.12)}
  .bad{background:rgba(255,92,92,.12)}
  .hint{color:var(--muted);font-size:12px}
  .val{font:22px/1.2 mono}
  .kv{display:grid;grid-template-columns:120px 1fr;gap:6px 10px;margin-top:8px}
  pre{white-space:pre-wrap;max-height:260px;overflow:auto;background:#0d141d;border:1px solid var(--border);padding:10px;border-radius:8px}
  .grid2{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .w90{width:90px}
  .w120{width:120px}
  canvas{width:100%;max-width:320px;height:auto;aspect-ratio:1;border-radius:12px;border:1px solid var(--border);background:#0e141c}
</style>
</head>
<body>
<header>
  <h1>EUNO Autopilot – Client</h1>
  <div class="pill" id="net">NET: —</div>
</header>

<main>
  <!-- STATO / TELEMETRIA -->
  <section class="card">
    <div class="row" style="justify-content:space-between">
      <div><div class="hint">Heading</div><div class="val" id="hdg">—</div></div>
      <div><div class="hint">Mode</div><div class="val" id="mode">—</div></div>
      <div><div class="hint">Motor</div><div class="val" id="motor">—</div></div>
    </div>
    <div class="sep"></div>
    <div class="grid2">
      <div>
        <div class="kv">
          <label>Compass</label><div id="hdg_c">—</div>
          <label>Fusion</label> <div id="hdg_f">—</div>
          <label>Experimental</label><div id="hdg_e">—</div>
          <label>ADV</label><div id="hdg_a">—</div>
          <label>SOG</label><div id="sog">—</div>
          <label>EXT BRG</label><div id="extbrg">OFF</div>
        </div>
      </div>
      <div style="display:flex;align-items:center;justify-content:center">
        <canvas id="rose" width="320" height="320"></canvas>
      </div>
    </div>
  </section>

  <!-- COMANDI PRINCIPALI -->
  <section class="card">
    <div class="row">
      <button class="good" onclick="send('$PEUNO,CMD,TOGGLE=1')">ON/OFF</button>
      <button onclick="send('$PEUNO,CMD,DELTA=-10')">−10</button>
      <button onclick="send('$PEUNO,CMD,DELTA=-1')">−1</button>
      <button onclick="send('$PEUNO,CMD,DELTA=+1')">+1</button>
      <button onclick="send('$PEUNO,CMD,DELTA=+10')">+10</button>
    </div>
    <div class="sep"></div>
    <div class="row">
      <label>Mode</label>
      <select id="selMode" onchange="setMode(this.value)">
        <option>COMPASS</option>
        <option selected>FUSION</option>
        <option>EXPERIMENTAL</option>
        <option>ADV</option>
        <option>OPENPLOTTER</option>
      </select>
      <button onclick="send('$PEUNO,CMD,CAL=MAG')">CAL MAG</button>
      <button onclick="send('$PEUNO,CMD,CAL=GYRO')">CAL GYRO</button>
      <span class="hint">Comandi: $PEUNO,CMD,...</span>
    </div>
    <div class="sep"></div>
    <div class="row">
      <label>EXT BRG</label>
      <button onclick="send('$PEUNO,CMD,EXTBRG=ON')">ON</button>
      <button class="bad" onclick="send('$PEUNO,CMD,EXTBRG=OFF')">OFF</button>
    </div>
  </section>

  <!-- PARAMETRI (SET, salvati dal client in EEPROM) -->
  <section class="card">
    <div class="row">
      <label>V_min</label><input id="V_min" class="w90" type="number" value="100">
      <label>V_max</label><input id="V_max" class="w90" type="number" value="255">
      <label>E_min</label><input id="E_min" class="w90" type="number" value="5">
      <label>E_max</label><input id="E_max" class="w90" type="number" value="40">
      <label>Deadband</label><input id="Deadband" class="w90" type="number" value="1">
      <label>T_pause</label><input id="T_pause" class="w90" type="number" value="0">
      <label>T_risposta</label><input id="T_risposta" class="w90" type="number" value="10">
      <button onclick="applyParams()">APPLY</button>
    </div>
    <div class="hint">Inoltra: $PEUNO,CMD,SET,key=value</div>
  </section>

  <!-- RETE (salva SSID/PASS OP → EEPROM → riavvio in STA) -->
  <section class="card">
    <div class="row">
      <label>OP SSID</label><input id="ssid" class="w120" type="text" placeholder="OpenPlotter-AP">
      <label>OP PASS</label><input id="pass" class="w120" type="password" placeholder="password">
      <button onclick="saveNet()">SAVE & REBOOT STA</button>
    </div>
    <div class="hint">POST /api/net con ssid, pass</div>
  </section>

  <!-- CONSOLE -->
  <section class="card">
    <div class="row"><div class="hint">Console</div></div>
    <pre id="log"></pre>
  </section>
</main>

<script>
let ws, netEl = document.getElementById('net'), logEl = document.getElementById('log');
let lastHDG = 0;

function log(s){
  const at = new Date().toLocaleTimeString();
  logEl.textContent += '['+at+'] '+s+'\\n';
  logEl.scrollTop = logEl.scrollHeight;
}

function wsConnect(){
  // Connessione WS: host corrente su porta :81
  const host = location.hostname || window.location.host.split(':')[0];
  const url  = (location.protocol==='https:'?'wss://':'ws://') + host + ':81/';
  ws = new WebSocket(url);

  ws.onopen = _=>{
    netEl.textContent = 'NET: WS OK';
    netEl.style.background = 'rgba(46,204,113,.12)';
    log('WS connected '+url);
  };
  ws.onclose = _=>{
    netEl.textContent = 'NET: WS OFF';
    netEl.style.background = '';
    log('WS closed, retry in 2s');
    setTimeout(wsConnect, 2000);
  };
  ws.onerror = _=> log('WS error');
  ws.onmessage = ev=>{
    const t = ev.data || '';
    if(t.startsWith('$AUTOPILOT')) parseTelem(t);
    else if(t.startsWith('LOG:')) log(t.slice(4));
    else log(t);
  };
}

function send(s){
  if(ws && ws.readyState===1){ ws.send(s); log('> '+s); }
}

function setMode(m){
  send('$PEUNO,CMD,MODE='+m);
  document.getElementById('mode').textContent = m;
}

function applyParams(){
  const keys = ['V_min','V_max','E_min','E_max','Deadband','T_pause','T_risposta'];
  keys.forEach(k=>{
    const v = document.getElementById(k).value;
    send('$PEUNO,CMD,SET,'+k+'='+v);
  });
}

async function saveNet(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value.trim();
  if(!ssid || !pass){ log('Missing ssid/pass'); return; }
  try{
    const body = new URLSearchParams({ssid, pass}).toString();
    const r = await fetch('/api/net',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    const t = await r.text();
    log('< '+t);
  }catch(e){ log('HTTP error'); }
}

// ── TELEMETRIA
function kv(line){
  const m = {};
  line.split(',').slice(1).forEach(p=>{
    const i=p.indexOf('='); if(i>0){ m[p.slice(0,i)] = p.slice(i+1); }
  });
  return m;
}
function parseTelem(line){
  const m = kv(line);
  if(m.MODE)   document.getElementById('mode').textContent  = m.MODE;
  if(m.MOTOR)  document.getElementById('motor').textContent = m.MOTOR;
  if(m.HDG){   document.getElementById('hdg').textContent   = m.HDG+'°'; lastHDG = parseFloat(m.HDG)||0; drawRose(); }
  if(m.HDG_C)  document.getElementById('hdg_c').textContent = m.HDG_C+'°';
  if(m.HDG_F)  document.getElementById('hdg_f').textContent = m.HDG_F+'°';
  if(m.HDG_E)  document.getElementById('hdg_e').textContent = m.HDG_E+'°';
  if(m.HDG_A)  document.getElementById('hdg_a').textContent = m.HDG_A+'°';
  if(m.SOG)    document.getElementById('sog').textContent   = m.SOG+' kn';
  if(m.EXTBRG) document.getElementById('extbrg').textContent= m.EXTBRG+'°';
}

// ── ROSA SEMPLICE
function drawRose(){
  const c = document.getElementById('rose'); if(!c) return;
  const ctx = c.getContext('2d');
  const w = c.width, h = c.height; const cx = w/2, cy=h/2, r=Math.min(cx,cy)-16;

  ctx.clearRect(0,0,w,h);

  // cerchio
  ctx.beginPath(); ctx.arc(cx,cy,r,0,Math.PI*2); ctx.strokeStyle='#223043'; ctx.lineWidth=2; ctx.stroke();

  // tacche principali
  const ticks = [0,90,180,270];
  ctx.strokeStyle='#2a3a50'; ctx.lineWidth=2;
  ticks.forEach(a=>{
    const rad=(a-90)*Math.PI/180;
    const x1=cx+Math.cos(rad)*(r-14), y1=cy+Math.sin(rad)*(r-14);
    const x2=cx+Math.cos(rad)*r,      y2=cy+Math.sin(rad)*r;
    ctx.beginPath(); ctx.moveTo(x1,y1); ctx.lineTo(x2,y2); ctx.stroke();
  });

  // N
  ctx.fillStyle='#9fb0c7'; ctx.font='14px system-ui'; ctx.textAlign='center'; ctx.textBaseline='middle';
  ctx.fillText('N',cx,cy-r+22);

  // ago (heading)
  const a=(lastHDG-90)*Math.PI/180;
  const xh=cx+Math.cos(a)*(r-30), yh=cy+Math.sin(a)*(r-30);
  ctx.strokeStyle='#49adff'; ctx.lineWidth=4;
  ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(xh,yh); ctx.stroke();

  // puntale
  ctx.beginPath(); ctx.arc(cx,cy,5,0,Math.PI*2); ctx.fillStyle='#49adff'; ctx.fill();
}

window.addEventListener('load', ()=>{ wsConnect(); drawRose(); });
</script>
</body>
</html>)rawliteral";
