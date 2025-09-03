#pragma once
// UI locale per EUNO Client – WS :81, HTTP :80
// Mostra HERO (Heading/Command/Error/COG/SOG + tasti grandi) e bussola con 2 lancette.

const char INDEX_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#0b0f13">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="EUNO">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="apple-touch-icon" href="/icon-192.png">

<title>EUNO Autopilot – Client</title>
<style>
  :root{
    --bg:#0b0f13; --card:#111722; --border:#1c2433;
    --fg:#e7f0ff; --muted:#9fb0c7; --acc:#53b2ff;
    --ok:#2ecc71; --bad:#ff5c5c; --cmd:#ffa657;
  }
  *{box-sizing:border-box}
  body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.4 system-ui,Segoe UI,Roboto,Helvetica,Arial}

  header{display:flex;align-items:center;gap:12px;justify-content:space-between;padding:14px 16px;border-bottom:1px solid var(--border);background:#0f141c;position:sticky;top:0;z-index:10}
  h1{margin:0;font-size:16px;font-weight:600}
  .pill{font:12px/1 mono;background:#0e1622;color:var(--muted);padding:6px 8px;border:1px solid var(--border);border-radius:9px}

  /* CONTENITORE: limita la larghezza della pagina (niente allargamenti) */
  main{
    padding:14px;
    display:grid;
    gap:14px;
    grid-template-columns:1fr;
    max-width:480px;           /* <— blocco anti-allargamento */
    margin-inline:auto;
    width:100%;
  }

  .card{background:var(--card);border:1px solid var(--border);border-radius:12px;padding:14px;width:100%}
  .row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
  .sep{height:1px;background:var(--border);margin:12px 0}

  label{color:var(--muted);font-size:12px}
  input,select,button{border-radius:12px;border:1px solid var(--border);background:#0e1520;color:var(--fg);padding:12px 14px}
  button{cursor:pointer}
  button:active{transform:translateY(1px)}
  .good{background:rgba(46,204,113,.12)}
  .bad{background:rgba(255,92,92,.12)}
  .hint{color:var(--muted);font-size:12px}

  .val{font:clamp(24px,6vw,42px)/1.1 mono}
  .lbl{color:var(--muted);font-size:13px;margin-bottom:6px}

  /* HERO pulita: niente regole rotte, gap ridotto */
  .hero-grid{display:grid;gap:6px}
  .hero-row2{display:grid;gap:6px;margin-top:6px}

  .btn-row{display:flex;gap:12px;flex-wrap:wrap;justify-content:center}
  .btn-lg{padding:18px 20px;font-size:22px;border-radius:16px;min-width:86px}

  /* CONSENTE ALLE COLONNE DI STRINGERSI (niente allargamenti) */
  .stack{display:grid;gap:6px;min-width:0}

  /* Griglie interne comprimibili */
  .kv{display:grid;grid-template-columns:minmax(0,120px) minmax(0,1fr);gap:6px 10px;margin-top:8px}
  .grid2{display:grid;grid-template-columns:minmax(0,1fr) minmax(0,1fr);gap:12px}

  .w90{width:90px} .w120{width:120px}

  /* Canvas sempre entro viewport */
  canvas{width:100%;max-width:380px;height:auto;aspect-ratio:1;border-radius:12px;border:1px solid var(--border);background:#0e141c;margin:auto}

  /* Log: evita che stringhe lunghe allarghino la pagina */
  #log{word-break:break-word;overflow-wrap:anywhere}
</style>


</head>
<body>
<header>
  <h1>EUNO Autopilot – Client</h1>
  <div class="pill" id="net">NET: —</div>
</header>

<main>
  <!-- HERO -->
<section class="card">
  <!-- Riga 1: 3 campi -->
  <div class="hero-grid" style="grid-template-columns:repeat(3,minmax(0,1fr));gap:6px">
    <div class="stack">
      <div class="lbl">Heading</div>
      <div class="val" id="hdg_big">—</div>
    </div>
    <div class="stack">
      <div class="lbl">Command</div>
      <div class="val" id="cmd_big">—</div>
    </div>
    <div class="stack">
      <div class="lbl">Error</div>
      <div class="val" id="err_big">—</div>
    </div>
  </div>

  <!-- Riga 2: 2 campi -->
  <div class="hero-grid" style="grid-template-columns:repeat(2,minmax(0,1fr));gap:6px;margin-top:6px">
    <div class="stack">
      <div class="lbl">COG</div>
      <div class="val" id="cog_big">—</div>
    </div>
    <div class="stack">
      <div class="lbl">SOG</div>
      <div class="val" id="sog_big">—</div>
    </div>
  </div>

  <div class="sep"></div>

  <!-- Riga 3: 2 tasti -->
  <div class="btn-row" style="justify-content:center;gap:12px">
    <button class="btn-lg bad" onclick="delta(-10)">−10</button>
    <button class="btn-lg bad" onclick="delta(-1)">−1</button>
  </div>

  <!-- Riga 4: 2 tasti -->
  <div class="btn-row" style="justify-content:center;gap:12px;margin-top:8px">
    <button class="btn-lg bad" onclick="delta(+1)">+1</button>
    <button class="btn-lg bad" onclick="delta(+10)">+10</button>
  </div>

  <!-- Riga 5: 1 tasto -->
  <div class="btn-row" style="justify-content:center;gap:12px;margin-top:8px">
    <button class="btn-lg good" onclick="toggle()">ON/OFF</button>
  </div>
</section>


  <!-- Bussola con due lancette: HDG (azzurra) e CMD (arancione) -->
  <section class="card">
    <div class="row" style="justify-content:space-between;margin-bottom:8px">
      <div class="hint">Rosa: <span style="color:#49adff">HDG</span> • <span style="color:#ffa657">CMD</span></div>
      <div class="hint">Mode: <span id="mode">—</span> • Motor: <span id="motor">—</span></div>
    </div>
    <canvas id="rose" width="380" height="380"></canvas>
  </section>

  <!-- Dettagli (scorrendo) -->
  <section class="card">
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
      <div class="stack">
        <div class="row">
          <label>Mode</label>
          <select id="selMode" onchange="setMode(this.value)">
            <option>COMPASS</option>
            <option>FUSION</option>
            <option>EXPERIMENTAL</option>
            <option>ADV</option>
            <option>OPENPLOTTER</option>
          </select>
          <button onclick="send('$PEUNO,CMD,CAL=MAG')">CAL MAG</button>
          <button onclick="send('$PEUNO,CMD,CAL=GYRO')">CAL GYRO</button>
        </div>
        <div class="hint">Comandi: $PEUNO,CMD,...</div>
      </div>
    </div>
  </section>

  <!-- Parametri -->
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

  <!-- Rete -->
  <section class="card">
    <div class="row">
      <label>OP SSID</label><input id="ssid" class="w120" type="text" placeholder="OpenPlotter-AP">
      <label>OP PASS</label><input id="pass" class="w120" type="password" placeholder="password">
      <button onclick="saveNet()">SAVE & REBOOT STA</button>
    </div>
    <div class="hint">POST /api/net con ssid, pass</div>
  </section>

  <!-- Console -->
  <section class="card">
    <div class="row"><div class="hint">Console</div></div>
    <pre id="log" style="white-space:pre-wrap;max-height:260px;overflow:auto;background:#0d141d;border:1px solid var(--border);padding:10px;border-radius:8px"></pre>
  </section>
</main>

<script>
let ws, netEl = document.getElementById('net'), logEl = document.getElementById('log');
let hdg = 0, cmd = 0, err = 0;

function log(s){
  const at = new Date().toLocaleTimeString();
  logEl.textContent += '['+at+'] '+s+'\\n';
  logEl.scrollTop = logEl.scrollHeight;
}
function wsConnect(){
  const host = location.hostname || window.location.host.split(':')[0];
  const url  = (location.protocol==='https:'?'wss://':'ws://') + host + ':81/';
  ws = new WebSocket(url);
  ws.onopen = _=>{ netEl.textContent='NET: WS OK'; netEl.style.background='rgba(46,204,113,.12)'; log('WS connected '+url); };
  ws.onclose = _=>{ netEl.textContent='NET: WS OFF'; netEl.style.background=''; log('WS closed, retry 2s'); setTimeout(wsConnect,2000); };
  ws.onerror = _=> log('WS error');
ws.onmessage = ev=>{
  const t = (ev.data || '').trim();
  console.log("RAW FRAME:", t);
  if (t.startsWith('$AUTOPILOT')) parseTelem(t);
  else if (t.startsWith('LOG:'))  log(t.slice(4));
  else                             log(t);
};


}
function send(s){ if(ws && ws.readyState===1){ ws.send(s); log('> '+s); } }
function delta(v){ const s = v>0?('+'+v):v; send('$PEUNO,CMD,DELTA='+s); cmd = (cmd + v + 360)%360; paintHero(); drawRose(); }
function toggle(){ send('$PEUNO,CMD,TOGGLE=1'); }

function setMode(m){ send('$PEUNO,CMD,MODE='+m); document.getElementById('mode').textContent=m; }

function applyParams(){
  ['V_min','V_max','E_min','E_max','Deadband','T_pause','T_risposta'].forEach(k=>{
    const v = document.getElementById(k).value; send('$PEUNO,CMD,SET,'+k+'='+v);
  });
}
async function saveNet(){
  const ssid=document.getElementById('ssid').value.trim();
  const pass=document.getElementById('pass').value.trim();
  if(!ssid||!pass){ log('Missing ssid/pass'); return; }
  try{
    const body = new URLSearchParams({ssid, pass}).toString();
    const r = await fetch('/api/net',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    const t = await r.text(); log('< '+t);
  }catch(e){ log('HTTP error'); }
}

// --- TELEMETRIA
function kv(line){
  const m={}; line.split(',').slice(1).forEach(p=>{ const i=p.indexOf('='); if(i>0) m[p.slice(0,i)]=p.slice(i+1); });
  return m;
}
function parseTelem(line){
  const m = kv(line);

  // Mode/Motor
  if (m.MODE)   document.getElementById('mode').textContent  = m.MODE;
  if (m.MOTOR)  document.getElementById('motor').textContent = m.MOTOR;

  // HEADING (accetta HEADING o HDG)
  if (m.HEADING)      hdg = ((parseFloat(m.HEADING) || 0) + 360) % 360;
  else if (m.HDG)     hdg = ((parseFloat(m.HDG)     || 0) + 360) % 360;
  document.getElementById('hdg_big').textContent = Math.round(hdg) + '°';

  // COMMAND (accetta COMMAND o CMD)
  if (m.COMMAND)      cmd = ((parseFloat(m.COMMAND) || 0) + 360) % 360;
  else if (m.CMD)     cmd = ((parseFloat(m.CMD)      || 0) + 360) % 360;

  // ERROR (accetta ERROR o ERR)
  if (m.ERROR)        err = parseInt(m.ERROR) || 0;
  else if (m.ERR)     err = parseInt(m.ERR)   || 0;

  // GPS/COG/SOG (accetta GPS_HEADING/GPS_SPEED o COG/SOG)
  if (m.GPS_HEADING)  document.getElementById('cog_big').textContent = Math.round(parseFloat(m.GPS_HEADING)||0) + '°';
  else if (m.COG)     document.getElementById('cog_big').textContent = Math.round(parseFloat(m.COG)||0) + '°';

  if (m.GPS_SPEED) {
    const sog = parseFloat(m.GPS_SPEED) || 0;
    document.getElementById('sog_big').textContent = sog.toFixed(1) + ' kn';
    const s = document.getElementById('sog'); if (s) s.textContent = sog.toFixed(1) + ' kn';
  } else if (m.SOG) {
    const sog = parseFloat(m.SOG) || 0;
    document.getElementById('sog_big').textContent = sog.toFixed(1) + ' kn';
    const s = document.getElementById('sog'); if (s) s.textContent = sog.toFixed(1) + ' kn';
  }

  // Se hai i dettagli HDG_C/F/E/A li lasci come sono qui sotto (ok)

  paintHero();
  drawRose();
}

function paintHero(){
  document.getElementById('cmd_big').textContent = (Math.round(cmd)+360)%360 + '°';
  document.getElementById('err_big').textContent = (err>0?'+':'') + err + '°';
}

// --- ROSA con 2 lancette (HDG e CMD)
function drawRose(){
  const c = document.getElementById('rose'); if(!c) return;
  const ctx = c.getContext('2d');
  const w=c.width, h=c.height, cx=w/2, cy=h/2, r=Math.min(cx,cy)-18;

  ctx.clearRect(0,0,w,h);

  // cerchio esterno
  ctx.beginPath(); ctx.arc(cx,cy,r,0,Math.PI*2); ctx.strokeStyle='#223043'; ctx.lineWidth=2; ctx.stroke();

  // tacche cardinali
  const ticks=[0,90,180,270];
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

  // ago HDG (azzurro)
  const aH=(hdg-90)*Math.PI/180;
  const xh=cx+Math.cos(aH)*(r-34), yh=cy+Math.sin(aH)*(r-34);
  ctx.strokeStyle='#49adff'; ctx.lineWidth=5; ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(xh,yh); ctx.stroke();
  ctx.beginPath(); ctx.arc(cx,cy,6,0,Math.PI*2); ctx.fillStyle='#49adff'; ctx.fill();

  // ago CMD (arancione)
  const aC=(cmd-90)*Math.PI/180;
  const xc=cx+Math.cos(aC)*(r-20), yc=cy+Math.sin(aC)*(r-20);
  ctx.strokeStyle='#ffa657'; ctx.lineWidth=3; ctx.setLineDash([6,6]);
  ctx.beginPath(); ctx.moveTo(cx,cy); ctx.lineTo(xc,yc); ctx.stroke();
  ctx.setLineDash([]);
}

window.addEventListener('load', ()=>{ wsConnect(); drawRose(); });
</script>
<!-- === PANNELLO: Scansiona reti Wi-Fi (STA) === -->
<section id="sta-scan-panel" style="margin:16px;padding:16px;border:1px solid #1c2433;border-radius:12px;background:#0b0f13;color:#e7f0ff">
  <h3 style="margin:0 0 10px 0;font-weight:600">Scansiona reti Wi-Fi (STA)</h3>

  <div style="display:flex;gap:8px;align-items:center;flex-wrap:wrap">
    <button id="netScanBtn" style="padding:8px 12px;border-radius:10px;border:1px solid #1c2433;background:#111722;cursor:pointer">
      Scansiona reti
    </button>
    <span id="netScanStatus" style="opacity:.8"></span>
  </div>

  <div style="margin-top:12px;display:flex;gap:8px;align-items:center;flex-wrap:wrap">
    <label for="ssidSelect" style="opacity:.9">SSID trovati:</label>
    <select id="ssidSelect" style="min-width:220px;padding:8px;border-radius:10px;border:1px solid #1c2433;background:#111722;color:#e7f0ff">
      <option value="">(premi “Scansiona reti”)</option>
    </select>

    <button id="ssidUseBtn" style="padding:8px 12px;border-radius:10px;border:1px solid #1c2433;background:#53b2ff;color:#0b0f13;cursor:pointer">
      Usa SSID selezionato
    </button>
    <span class="hint" style="opacity:.8">Questo copia l'SSID nel tuo campo SSID già esistente.</span>
  </div>

  <small style="display:block;margin-top:8px;opacity:.7">
    Suggerimento: se l’hotspot è a 5 GHz, imposta 2.4 GHz. La pagina resta reattiva durante la scansione.
  </small>
</section>

<script>
(() => {
  // Riferimenti agli elementi del nuovo pannello
  const btnScan   = document.getElementById('netScanBtn');
  const lblScan   = document.getElementById('netScanStatus');
  const selSSID   = document.getElementById('ssidSelect');
  const btnUse    = document.getElementById('ssidUseBtn');

  // Troviamo il TUO campo SSID esistente (quello che usi già per salvare)
  // Se nella tua pagina l'input SSID ha id diverso, cambia qui sotto 'ssid'.
  const inputSSID = document.getElementById('ssid');

  function escapeHtml(s){ return (s||"").replace(/[&<>"']/g, m => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[m])) }

  // Polling asincrono: interroga /api/scan finché non sono pronti i risultati
  async function pollScan(){
    try{
      const res = await fetch("/api/scan", {cache:"no-store"});
      // 202 = "scansione in corso" → riproviamo tra 600ms
      if (res.status === 202) {
        if (lblScan) lblScan.textContent = "Scansione...";
        setTimeout(pollScan, 600);
        return;
      }
      if (!res.ok) throw new Error("HTTP "+res.status);

      // 200 = risultati pronti
      const list = await res.json();
      // Ordina per potenza segnale (RSSI discendente) e deduplica SSID
      list.sort((a,b)=>(b.rssi||-999)-(a.rssi||-999));
      const seen = new Set(), uniq=[];
      for (const ap of list) {
        if (!ap || !ap.ssid) continue;
        if (seen.has(ap.ssid)) continue;
        seen.add(ap.ssid); uniq.push(ap);
      }
      if (!uniq.length){
        if (selSSID) selSSID.innerHTML = `<option value="">(nessuna rete trovata)</option>`;
        if (lblScan)  lblScan.textContent = "Nessuna rete trovata";
      } else {
        if (selSSID) selSSID.innerHTML = uniq.map(ap =>
          `<option value="${escapeHtml(ap.ssid)}">${escapeHtml(ap.ssid)} (${ap.rssi} dBm)</option>`
        ).join("");
        if (lblScan)  lblScan.textContent = `Trovate ${uniq.length} reti`;
      }
    } catch(err){
      console.error(err);
      if (lblScan)  lblScan.textContent = "Errore scansione";
      if (selSSID)  selSSID.innerHTML   = `<option value="">(errore)</option>`;
    } finally {
      if (btnScan) btnScan.disabled = false; // riabilita sempre il bottone
    }
  }

  // Avvia la scansione (non blocca la pagina)
  btnScan && btnScan.addEventListener('click', () => {
    btnScan.disabled = true;
    if (lblScan)  lblScan.textContent = "Avvio scansione...";
    if (selSSID)  selSSID.innerHTML   = `<option value="">(scansione in corso)</option>`;
    pollScan();
  });

  // Copia l'SSID selezionato nel TUO campo SSID (quello già usato dal tuo SAVE)
  btnUse && btnUse.addEventListener('click', () => {
    if (!inputSSID) { alert("Campo SSID non trovato nella pagina (id='ssid')."); return; }
    const chosen = selSSID ? selSSID.value : "";
    if (!chosen) { alert("Seleziona prima una rete."); return; }
    inputSSID.value = chosen;
    if (lblScan) lblScan.textContent = `SSID copiato: ${chosen}`;
  });

  // (Facoltativo) copia anche al cambio selezione, senza premere il bottone
  selSSID && selSSID.addEventListener('change', () => {
    if (!inputSSID) return;
    inputSSID.value = selSSID.value || "";
  });
})();
</script>


</body>
</html>)rawliteral";
