#pragma once
// EUNO Client UI – WS :81, HTTP :80
// 4 pagine full-screen: Home • Setup • Visual • Network

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#0b1b2b">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="EUNO">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<title>EUNO Autopilot</title>
<style>
/* ====== EUNO Brand palette ====== */
:root{
  --navy:#0b1b2b;          /* background */
  --navy-2:#0f2438;        /* cards / bars */
  --navy-3:#15334f;        /* borders */
  --blue:#2a77bf;          /* step buttons */
  --red:#e04747;           /* errors */
  --white:#ffffff;         /* text */
  --muted:#bcd0e6;         /* secondary text */
  --green:#16a34a;         /* ON / connected */
  --ring:#3aa0ff;          /* focus */
  --radius:16px;
  --hdrH:56px;
  --navH:60px;
  --mono:ui-monospace,Menlo,Consolas,"Courier New",monospace;
}

/* ====== Base ====== */
*{box-sizing:border-box;margin:0;padding:0}
html,body{height:100%}
body{
  background:var(--navy);color:var(--white);
  font:16px/1.45 system-ui,-apple-system,Segoe UI,Roboto,Arial;
  -webkit-font-smoothing:antialiased
}

/* ====== Header ====== */
header{
  height:var(--hdrH);display:flex;align-items:center;justify-content:space-between;
  padding:0 14px;background:var(--navy-2);border-bottom:1px solid var(--navy-3);
  position:fixed;inset:0 0 auto 0;z-index:10
}
.brand{display:flex;align-items:center;gap:10px;font-weight:800;letter-spacing:.02em}
.brand .dot{width:10px;height:10px;border-radius:50%;background:var(--blue);box-shadow:0 0 0 3px rgba(58,160,255,.25)}
.status{display:flex;align-items:center;gap:8px}
.led-dot{
  width:10px;height:10px;border-radius:50%;
  box-shadow:0 0 0 3px rgba(0,0,0,.15);
}
.led-ok{background:var(--green)}
.led-err{background:#ff4d4d}
@keyframes blink{0%,100%{opacity:1}50%{opacity:.2}}
.blink{animation:blink 1s infinite}

/* ====== Viewport / Slides (full-screen) ====== */
.viewport{position:fixed;inset:var(--hdrH) 0 var(--navH) 0;overflow:hidden}
.slides{display:flex;width:400%;height:100%;transition:transform .25s ease}
.slide{width:100%;height:100%;overflow:auto;padding:16px}

/* ====== Cards & layout ====== */
.card{
  background:var(--navy-2);border:1px solid var(--navy-3);border-radius:var(--radius);
  padding:16px;max-width:720px;margin:0 auto;box-shadow:0 4px 16px rgba(0,0,0,.25)
}
.grid3{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px}
.grid2{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.row{display:flex;gap:12px;flex-wrap:wrap;align-items:center}
.sep{height:1px;background:var(--navy-3);margin:14px 0}
.stack{display:flex;flex-direction:column;gap:6px}
.lbl{color:var(--muted);font-size:12px;text-transform:uppercase;letter-spacing:.05em;font-weight:750}
.val{font:clamp(28px,6vw,44px)/1.05 var(--mono);font-weight:900}

/* Inputs & Buttons */
input,select,button{
  border-radius:12px;border:1px solid var(--navy-3);background:transparent;color:var(--white);
  padding:12px 14px;font-weight:650;min-height:44px
}
:where(input,select,button):focus-visible{outline:3px solid var(--ring);outline-offset:2px}
button{cursor:pointer;transition:transform .06s ease}
button:active{transform:translateY(1px)}
.btn-lg{padding:16px 18px;font-size:20px;border-radius:14px;min-width:86px;font-weight:900;text-transform:uppercase;letter-spacing:.02em}

/* Buttons color rules */
.btn-step{background:var(--blue);color:#000;border-color:#23639f}
.btn-on{  background:var(--green);color:#000;border-color:#12873d}
.btn-off{ background:#e04747;color:#000;border-color:#b83b3b}
.btn-acc{ background:var(--blue);color:#000;border-color:#23639f}
.btn-ghost{background:transparent}
.w80{width:80px}.w90{width:90px}.w120{width:140px}

/* ====== Visual tab: Angular Progress Bar ====== */
.visual-wrap{max-width:720px;margin:0 auto}
.angular-progress{width:100%;height:170px;position:relative;margin-bottom:16px}
.progress-track{
  position:absolute;top:70px;left:10%;right:10%;height:12px;
  background:var(--navy-3);border-radius:6px
}
.progress-fill{
  position:absolute;top:70px;left:50%;right:50%;height:12px;border-radius:6px;
  background:linear-gradient(90deg,var(--green),var(--blue),#ff4d4d);
  transition:all .3s ease;box-shadow:0 0 10px rgba(42,119,191,.35)
}
.progress-center{
  position:absolute;top:66px;left:50%;width:3px;height:20px;background:var(--green);transform:translateX(-50%)
}
.progress-center::after{
  content:"";position:absolute;inset:-4px;background:var(--green);border-radius:2px;opacity:.25
}
.progress-labels{
  position:absolute;top:105px;left:0;right:0;display:flex;justify-content:space-between;padding:0 15%
}
.progress-label{ text-align:center;font-family:var(--mono);font-size:14px;font-weight:700;color:var(--muted)}
.progress-label.current{ color:var(--green);font-size:16px}
.progress-label.target{  color:var(--blue);font-size:16px}
.progress-value{
  position:absolute;top:22px;left:0;right:0;text-align:center;font:32px/1 var(--mono);font-weight:900;color:var(--blue);
  text-shadow:0 0 10px rgba(42,119,191,.35)
}
.progress-scale{ position:absolute;top:50px;left:10%;right:10%;height:10px}
.scale-mark{ position:absolute;top:0;width:1px;height:6px;background:var(--muted);opacity:.5}
.scale-mark.major{ height:10px;width:2px;opacity:.7}
.scale-label{
  position:absolute;top:12px;font-size:10px;color:var(--muted);transform:translateX(-50%);font-family:var(--mono)
}

/* Visual stats box */
.stats{
  display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin-top:8px;padding:12px;
  background:#0a1626;border-radius:12px;border:1px solid var(--navy-3)
}
.stat-item{text-align:center}
.stat-value{font-family:var(--mono);font-size:18px;font-weight:900;color:var(--white)}
.stat-label{font-size:11px;color:var(--muted);text-transform:uppercase;letter-spacing:.1em;margin-top:2px}

/* Visual controls (manual test) */
.controls{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-top:14px}
.control-group{display:flex;flex-direction:column;gap:6px}

/* ====== Bottom nav ====== */
.nav{position:fixed;inset:auto 0 0 0;height:var(--navH);background:var(--navy-2);border-top:1px solid var(--navy-3)}
.nav .bar{height:100%;display:grid;grid-template-columns:repeat(4,1fr);max-width:920px;margin:0 auto}
.nav button{border:none;border-radius:0;padding:8px 6px;font-weight:800;color:var(--muted);background:transparent}
.nav button.active{color:var(--white);box-shadow:inset 0 -3px 0 var(--blue)}

/* ====== Scan / status boxes ====== */
.scan-panel{border:1px dashed var(--navy-3);border-radius:var(--radius);padding:14px;margin-top:10px;background:#0d2035}
.status-box{
  display:flex;align-items:center;justify-content:space-between;gap:12px;
  border:1px solid var(--navy-3);border-radius:12px;padding:12px;background:#0a1626;margin-top:12px
}
.status-box .title{font-weight:800;letter-spacing:.03em}
.status-pill{display:flex;align-items:center;gap:8px}
.status-pill .led-dot{box-shadow:none}
</style>
</head>
<body>

<header>
  <div class="brand"><span class="dot"></span> EUNO Autopilot</div>
  <div class="status" title="WebSocket status">
    <span id="wsLed" class="led-dot led-err blink" aria-label="Disconnected"></span>
  </div>
</header>

<div class="viewport" id="viewport">
  <div class="slides" id="slides">

    <!-- ===== PAGE 1: HOME ===== -->
    <section class="slide">
      <div class="card">
        <div class="grid3">
          <div class="stack"><div class="lbl">Heading</div><div class="val" id="hdg_big">—</div></div>
          <div class="stack"><div class="lbl">Command</div><div class="val" id="cmd_big">—</div></div>
          <div class="stack"><div class="lbl">Error</div><div class="val" id="err_big">—</div></div>
        </div>
        <div class="grid2" style="margin-top:10px">
          <div class="stack"><div class="lbl">COG</div><div class="val" id="cog_big">—</div></div>
          <div class="stack"><div class="lbl">SOG</div><div class="val" id="sog_big">—</div></div>
        </div>

        <div class="sep"></div>

        <!-- Order: ON/OFF, then -1 +1, then -10 +10 -->
        <div class="row" style="justify-content:center">
          <button id="btnToggle" class="btn-lg btn-off" onclick="toggle()">OFF</button>
        </div>

        <div class="row" style="justify-content:center;margin-top:10px">
          <button class="btn-lg btn-step" onclick="delta(-1)">−1</button>
          <button class="btn-lg btn-step" onclick="delta(+1)">+1</button>
        </div>

        <div class="row" style="justify-content:center;margin-top:10px">
          <button class="btn-lg btn-step" onclick="delta(-10)">−10</button>
          <button class="btn-lg btn-step" onclick="delta(+10)">+10</button>
        </div>
      </div>
    </section>

    <!-- ===== PAGE 2: SETUP ===== -->
    <section class="slide">
      <div class="card">
        <div class="grid2">
          <div>
            <div class="stack kv">
              <div class="lbl">Compass</div><div id="hdg_c">—</div>
              <div class="lbl">Fusion</div><div id="hdg_f">—</div>
              <div class="lbl">Experimental</div><div id="hdg_e">—</div>
              <div class="lbl">ADV</div><div id="hdg_a">—</div>
              <div class="lbl">SOG</div><div id="sog">—</div>
              <div class="lbl">EXT BRG</div><div id="extbrg">OFF</div>
            </div>
          </div>

          <div class="stack">
            <div class="row">
              <label class="lbl">Mode</label>
              <select id="selMode" onchange="setMode(this.value)">
                <option>COMPASS</option>
                <option>FUSION</option>
                <option>EXPERIMENTAL</option>
                <option>ADV</option>
                <option>OPENPLOTTER</option>
              </select>
              <button class="btn-acc" onclick="send('$PEUNO,CMD,CAL=MAG')">CAL MAG</button>
              <button class="btn-acc" onclick="send('$PEUNO,CMD,CAL=GYRO')">CAL GYRO</button>
              <button class="btn-acc" onclick="cgps()">CAL C-GPS</button>
              <button class="btn-acc" onclick="extbrgOn()">EXT BRG ON</button>
              <button class="btn-acc" onclick="extbrgOff()">EXT BRG OFF</button>
            </div>
            <div class="lbl" style="opacity:.9">Commands: $PEUNO,CMD,...</div>
          </div>
        </div>

        <div class="sep"></div>

        <div class="row">
          <label class="lbl">V_min</label><input id="V_min" class="w80" type="number" value="40">
          <label class="lbl">V_max</label><input id="V_max" class="w80" type="number" value="255">
          <label class="lbl">E_min</label><input id="E_min" class="w80" type="number" value="3">
          <label class="lbl">E_max</label><input id="E_max" class="w80" type="number" value="60">
          <label class="lbl">Deadband</label><input id="Deadband" class="w80" type="number" value="2">
          <label class="lbl">T_pause</label><input id="T_pause" class="w80" type="number" value="0">
          <label class="lbl">T_response</label><input id="T_risposta" class="w80" type="number" value="1">
          <button class="btn-acc" onclick="applyParams()">APPLY</button>
        </div>
        <div class="lbl" style="opacity:.9;margin-top:8px">Forward: $PEUNO,CMD,SET,key=value</div>
      </div>
    </section>

    <!-- ===== PAGE 3: VISUAL (Angular Progress Bar) ===== -->
    <section class="slide">
      <div class="card visual-wrap">
        <div class="angular-progress">
          <div class="progress-value" id="ap_value">+0°</div>
          <div class="progress-scale" id="ap_scale"></div>
          <div class="progress-track"></div>
          <div class="progress-fill" id="ap_fill"></div>
          <div class="progress-center"></div>
          <div class="progress-labels">
            <div class="progress-label current" id="ap_current">HDG —</div>
            <div class="progress-label target"  id="ap_target">CMD —</div>
          </div>
        </div>

        <div class="stats">
          <div class="stat-item"><div class="stat-value" id="ap_hdg">—</div><div class="stat-label">Heading</div></div>
          <div class="stat-item"><div class="stat-value" id="ap_cmd">—</div><div class="stat-label">Command</div></div>
          <div class="stat-item"><div class="stat-value" id="ap_err">—</div><div class="stat-label">Deviation</div></div>
          <div class="stat-item"><div class="stat-value" id="ap_state">—</div><div class="stat-label">Status</div></div>
        </div>

        <!-- manual test inputs -->
        <div class="controls">
          <div class="control-group">
            <label class="lbl" for="ap_in_hdg">Heading (HDG)</label>
            <input type="number" id="ap_in_hdg" value="120" min="0" max="360" step="1">
          </div>
          <div class="control-group">
            <label class="lbl" for="ap_in_cmd">Command (CMD)</label>
            <input type="number" id="ap_in_cmd" value="120" min="0" max="360" step="1">
          </div>
        </div>
      </div>
    </section>

    <!-- ===== PAGE 4: NETWORK ===== -->
    <section class="slide">
      <div class="card">
        <div class="row">
          <label class="lbl">OP SSID</label><input id="ssid" class="w120" type="text" placeholder="OpenPlotter-AP">
          <label class="lbl">OP PASS</label><input id="pass" class="w120" type="password" placeholder="password">
          <button class="btn-acc" onclick="saveNet()">SAVE & REBOOT STA</button>
        </div>
        <div class="lbl" style="opacity:.9;margin-top:8px">POST /api/net with ssid, pass</div>

        <div class="scan-panel">
          <div class="row">
            <button id="scanBtn" class="btn-acc">Scan networks</button>
            <span id="scanLbl" class="lbl" style="opacity:.95"></span>
          </div>
          <div class="row" style="margin-top:10px">
            <label class="lbl">Found SSIDs</label>
            <select id="ssidSelect" style="min-width:220px"></select>
            <button id="ssidUseBtn" class="btn-acc">Use selected SSID</button>
          </div>
          <small class="lbl" style="display:block;margin-top:8px;opacity:.85">If available, uses /api/scan (POST to start, then GET polling).</small>
        </div>

        <!-- BT RING status box -->
        <div class="status-box" id="ringBox">
          <div class="title">BT RING</div>
          <div class="status-pill">
            <span id="ringLed" class="led-dot led-err blink" aria-label="Disconnected"></span>
            <span id="ringText" class="lbl" style="text-transform:none">Disconnected</span>
          </div>
        </div>

        <div class="sep"></div>
        <div class="row"><div class="lbl">Console</div></div>
        <pre id="log" style="white-space:pre-wrap;max-height:260px;overflow:auto;background:#0b111d;border:1px solid var(--navy-3);padding:10px;border-radius:8px"></pre>
      </div>
    </section>

  </div>
</div>

<nav class="nav">
  <div class="bar">
    <button id="tab0" class="active" onclick="go(0)">Home</button>
    <button id="tab1" onclick="go(1)">Setup</button>
    <button id="tab2" onclick="go(2)">Visual</button>
    <button id="tab3" onclick="go(3)">Network</button>
  </div>
</nav>

<script>
/* ===== Slider ===== */
const slides = document.getElementById('slides');
let page = 0;
function go(i){
  page = Math.max(0, Math.min(3, i));
  slides.style.transform = `translateX(-${page*25}%)`;
  for(let j=0;j<4;j++){
    const b=document.getElementById('tab'+j);
    if (b) b.classList.toggle('active', j===page);
  }
}
let sx=0, dx=0, touching=false;
const vp=document.getElementById('viewport');
vp.addEventListener('touchstart',e=>{ touching=true; sx=e.touches[0].clientX; });
vp.addEventListener('touchmove',e=>{ if(!touching)return; dx=e.touches[0].clientX - sx; });
vp.addEventListener('touchend',()=>{ if(!touching)return; touching=false; if(Math.abs(dx)>50){ if(dx<0) go(page+1); else go(page-1);} dx=0; });
window.addEventListener('keydown',e=>{ if(e.key==='ArrowRight') go(page+1); if(e.key==='ArrowLeft') go(page-1); });

/* ===== State + WS ===== */
let ws, logEl = document.getElementById('log');
let hdg = 0, cmd = 0, err = 0, cog = 0, sog = 0, motor = false;

function log(s){
  const at = new Date().toLocaleTimeString();
  logEl.textContent += '['+at+'] '+s+'\\n';
  logEl.scrollTop = logEl.scrollHeight;
}

/* WS LED helpers */
function setWsLed(connected){
  const led = document.getElementById('wsLed');
  if(!led) return;
  led.classList.remove('led-ok','led-err','blink');
  if(connected){ led.classList.add('led-ok'); led.setAttribute('aria-label','Connected'); }
  else{ led.classList.add('led-err','blink'); led.setAttribute('aria-label','Disconnected'); }
}

function wsConnect(){
  const host = location.hostname || window.location.host.split(':')[0];
  const url  = (location.protocol==='https:'?'wss://':'ws://') + host + ':81/';
  log('WS → '+url);
  try{ ws = new WebSocket(url); }catch(e){ log('WS error init '+e.message); setWsLed(false); return; }
  ws.onopen  = _=> { log('WS connected'); setWsLed(true); };
  ws.onclose = _=> { log('WS closed, retry 2s'); setWsLed(false); setTimeout(wsConnect,2000); };
  ws.onerror = _=> { log('WS error'); setWsLed(false); };
  ws.onmessage = ev=>{
    const t = (ev.data || '').trim();
    if (t.startsWith('$AUTOPILOT')) parseTelem(t);
    else if (t.startsWith('LOG:'))   log(t.slice(4));
    else                              log(t);
  };
}
function send(s){ if(ws && ws.readyState===1){ ws.send(s); log('> '+s); } }

/* ===== Commands ===== */
function delta(v){ const s = v>0?('+'+v):v; send('$PEUNO,CMD,DELTA='+s); cmd = (cmd + v + 360)%360; paintHome(); updateVisual(); }
function toggle(){ send('$PEUNO,CMD,TOGGLE=1'); motor = !motor; paintHome(); }
function setMode(m){ send('$PEUNO,CMD,MODE='+m); const el=document.getElementById('mode'); if(el) el.textContent=m; }
function applyParams(){
  ['V_min','V_max','E_min','E_max','Deadband','T_pause','T_risposta'].forEach(k=>{
    const v = document.getElementById(k).value; send('$PEUNO,CMD,SET,'+k+'='+v);
  });
}
function cgps(){ send('$PEUNO,CMD,CAL=CGPS'); send('ACTION:C-GPS'); }
function extbrgOn(){  send('$PEUNO,CMD,EXTBRG=ON');  send('ACTION:EXTBRG=ON'); }
function extbrgOff(){ send('$PEUNO,CMD,EXTBRG=OFF'); send('ACTION:EXTBRG=OFF'); }
async function saveNet(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value.trim();
  if(!ssid||!pass){ log('Missing ssid/pass'); return; }
  try{
    const res = await fetch('/api/net',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});
    if(!res.ok){ log('HTTP '+res.status); return; }
    log('Saved. Rebooting STA...');
  }catch(e){ log('NET save error '+e.message); }
}

/* ===== Telemetry ===== */
function kv(line){
  const m={}; line.split(',').slice(1).forEach(p=>{ const i=p.indexOf('='); if(i>0) m[p.slice(0,i)]=p.slice(i+1); });
  return m;
}
function parseTelem(line){
  const m = kv(line);

  // Mode/Motor
  if (m.MODE){ const el=document.getElementById('mode'); if(el) el.textContent=m.MODE; }
  if (m.MOTOR){ motor = (m.MOTOR==='ON'); }

  // HDG/CMD/ERR
  if (m.HEADING)      hdg = ((parseFloat(m.HEADING) || 0) + 360) % 360;
  else if (m.HDG)     hdg = ((parseFloat(m.HDG)     || 0) + 360) % 360;

  if (m.COMMAND)      cmd = ((parseFloat(m.COMMAND) || 0) + 360) % 360;
  else if (m.CMD)     cmd = ((parseFloat(m.CMD)      || 0) + 360) % 360;

  if (m.ERROR)        err = Math.round(parseFloat(m.ERROR)||0);
  else if (m.ERR)     err = Math.round(parseFloat(m.ERR)||0);

  // COG / SOG
  if (m.GPS_HEADING)  cog = parseFloat(m.GPS_HEADING) || 0;
  else if (m.COG)     cog = parseFloat(m.COG) || 0;

  if (m.GPS_SPEED)    sog = parseFloat(m.GPS_SPEED) || 0;
  else if (m.SOG)     sog = parseFloat(m.SOG) || 0;

  // Heading sources
  ['C','F','E','A'].forEach(k=>{
    const key='HDG_'+k, el=document.getElementById('hdg_'+k.toLowerCase());
    if (m[key] && el) el.textContent = Math.round(parseFloat(m[key])||0)+'°';
  });

  // External bearing
  const ext = m.EXTBRG || m.EXT_BR || m.EXTERNAL_BRG;
  if (ext){ const el=document.getElementById('extbrg'); if(el) el.textContent = ext; }
  if (m.E_tol && !m.Deadband) m.Deadband = m.E_tol; // alias

  ['V_min','V_max','E_min','E_max','Deadband','T_pause','T_risposta'].forEach(id=>{
    if (m[id] !== undefined) {
      const el = document.getElementById(id);
      // non sovrascrive se stai editando quel campo
      if (el && document.activeElement !== el) el.value = m[id];
    }
  });
  paintHome(); updateVisual();
}

/* ===== UI paint (HOME) ===== */
const norm=a=>(a%360+360)%360;
const shortDiff=(a,b)=>{ let d=norm(b)-norm(a); if(d>180)d-=360; if(d<-180)d+=360; return d; }

function paintHome(){
  const e = Math.round(shortDiff(hdg,cmd));
  document.getElementById('hdg_big').textContent = Math.round(norm(hdg))+'°';
  document.getElementById('cmd_big').textContent = Math.round(norm(cmd))+'°';
  document.getElementById('err_big').textContent = (e>0?'+':'')+e+'°';
  document.getElementById('cog_big').textContent = Math.round(norm(cog))+'°';
  document.getElementById('sog_big').textContent = (sog||0).toFixed(1)+' kn';

  const sEl=document.getElementById('sog'); if(sEl) sEl.textContent = (sog||0).toFixed(1)+' kn';

  // ON/OFF button visual
  const btn = document.getElementById('btnToggle');
  btn.textContent = motor?'ON':'OFF';
  btn.classList.toggle('btn-on',  motor);
  btn.classList.toggle('btn-off', !motor);
}

/* ===== Visual (Angular Progress Bar) ===== */
function initScale(){
  const scale=document.getElementById('ap_scale'); scale.innerHTML='';
  for(let i=-30;i<=30;i+=5){
    const mark=document.createElement('div');
    const major = (i%10===0);
    mark.className='scale-mark'+(major?' major':'');
    mark.style.left=`calc(50% + ${i*1.2}%)`;
    if(major && i!==0){
      const label=document.createElement('div');
      label.className='scale-label';
      label.textContent=Math.abs(i)+'°';
      label.style.left=`calc(50% + ${i*1.2}%)`;
      scale.appendChild(label);
    }
    scale.appendChild(mark);
  }
}
function errorInfo(err){
  const a=Math.abs(err);
  if(a<=5)  return {color:'var(--green)', label:'Excellent'};
  if(a<=15) return {color:'var(--blue)',  label:'Good'};
  if(a<=30) return {color:'#f59e0b',      label:'Warning'};
  return {color:'#ff4d4d',               label:'Critical'};
}
function updateVisual(){
  const errVal = Math.round(shortDiff(hdg,cmd));
  const info = errorInfo(errVal);

  const vEl=document.getElementById('ap_value');
  vEl.textContent=(errVal>0?'+':'')+errVal+'°';
  vEl.style.color = info.color;

  const fill=document.getElementById('ap_fill');
  const w = Math.min(50, Math.abs(errVal)*1.2); // max 50%
  if(errVal>=0){ fill.style.left='50%'; fill.style.right=(50-w)+'%'; }
  else{ fill.style.left=(50-w)+'%'; fill.style.right='50%'; }

  document.getElementById('ap_current').textContent='HDG '+Math.round(norm(hdg))+'°';
  document.getElementById('ap_target').textContent='CMD '+Math.round(norm(cmd))+'°';
  document.getElementById('ap_hdg').textContent   = Math.round(norm(hdg))+'°';
  document.getElementById('ap_cmd').textContent   = Math.round(norm(cmd))+'°';
  document.getElementById('ap_err').textContent   = (errVal>0?'+':'')+errVal+'°';
  document.getElementById('ap_err').style.color   = info.color;
  document.getElementById('ap_state').textContent = info.label;
  document.getElementById('ap_state').style.color = info.color;

  // keep manual inputs in sync (if not focused)
  if(document.activeElement.id!=='ap_in_hdg') document.getElementById('ap_in_hdg').value = Math.round(norm(hdg));
  if(document.activeElement.id!=='ap_in_cmd') document.getElementById('ap_in_cmd').value = Math.round(norm(cmd));
}
document.getElementById('ap_in_hdg').addEventListener('input',e=>{
  hdg = norm(parseInt(e.target.value)||0); paintHome(); updateVisual();
});
document.getElementById('ap_in_cmd').addEventListener('input',e=>{
  cmd = norm(parseInt(e.target.value)||0); paintHome(); updateVisual();
});

/* ===== BT RING status (placeholder, ready for telemetry) ===== */
function setRingStatus(connected){
  const led = document.getElementById('ringLed');
  const txt = document.getElementById('ringText');
  if(!led||!txt) return;
  led.classList.remove('led-ok','led-err','blink');
  if(connected){ led.classList.add('led-ok'); txt.textContent='Connected'; }
  else{ led.classList.add('led-err','blink'); txt.textContent='Disconnected'; }
}
// call this later from telemetry parser when disponibile:
//   setRingStatus(true/false);

/* ===== Network scan (optional /api/scan) ===== */
(function(){
  const btnScan  = document.getElementById('scanBtn');
  const lblScan  = document.getElementById('scanLbl');
  const selSSID  = document.getElementById('ssidSelect');
  const btnUse   = document.getElementById('ssidUseBtn');
  const inputSSID= document.getElementById('ssid');

  function escapeHtml(s){ return (s||"").replace(/[&<>"']/g, m => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m])) }

  async function pollScan(){
    try{
      const res = await fetch("/api/scan",{cache:"no-store"});
      if (res.status==202){ if(lblScan) lblScan.textContent="Scanning..."; setTimeout(pollScan,600); return; }
      if (!res.ok) throw new Error("HTTP "+res.status);
      const list = await res.json();
      list.sort((a,b)=>(b.rssi||-999)-(a.rssi||-999));
      const seen=new Set(), uniq=[]; for(const ap of list){ if(ap&&ap.ssid&&!seen.has(ap.ssid)){ seen.add(ap.ssid); uniq.push(ap);} }
      if(selSSID){
        selSSID.innerHTML='<option value="">—</option>'+uniq.map(ap=>`<option value="${escapeHtml(ap.ssid)}">${escapeHtml(ap.ssid)} (${ap.rssi||'?' } dBm)</option>`).join('');
      }
      if(lblScan) lblScan.textContent="Scan complete";
    }catch(e){ if(lblScan) lblScan.textContent="Scan error"; }
  }
  btnScan && btnScan.addEventListener('click', async ()=>{
    if(lblScan) lblScan.textContent="Starting scan…";
    try{ await fetch("/api/scan",{method:'POST'}); pollScan(); }catch(e){ if(lblScan) lblScan.textContent="Start error"; }
  });
  btnUse && btnUse.addEventListener('click', ()=>{
    const chosen = selSSID?selSSID.value:"";
    inputSSID.value = chosen;
    if(lblScan) lblScan.textContent = chosen?`SSID copied: ${chosen}`:'';
  });
  selSSID && selSSID.addEventListener('change', ()=>{ inputSSID.value = selSSID.value||""; });
})();


/* ===== Init ===== */
function init(){
  initScale();
  setWsLed(false);
  setRingStatus(false);
  paintHome(); updateVisual(); wsConnect();
}
window.addEventListener('load', init);
</script>

</body>
</html>)rawliteral";
