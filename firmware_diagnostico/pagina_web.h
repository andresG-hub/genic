// ============================================================================
//  pagina_web.h  -  Interfaz web embebida (servida desde el ESP32)
// ----------------------------------------------------------------------------
//  Pagina unica (SPA) almacenada en PROGMEM. Consume los endpoints locales:
//    GET  /estado    -> config actual + estado del sensor/wifi/firebase
//    POST /config    -> guarda fruta + estado  (form-urlencoded o query)
//    GET  /medir     -> toma medicion, devuelve JSON con las 18 bandas
//  Funciona tanto en modo AP (sin internet) como en STA.
// ============================================================================
#ifndef PAGINA_WEB_H
#define PAGINA_WEB_H

#include <Arduino.h>

const char PAGINA_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Diagnostico Espectral de Frutas</title>
<style>
  :root{--bg:#0f172a;--card:#1e293b;--accent:#22c55e;--txt:#e2e8f0;--mut:#94a3b8;}
  *{box-sizing:border-box;font-family:system-ui,Segoe UI,Roboto,sans-serif;}
  body{margin:0;background:var(--bg);color:var(--txt);padding:16px;}
  h1{font-size:1.3rem;margin:0 0 4px;}
  .sub{color:var(--mut);font-size:.85rem;margin-bottom:16px;}
  .card{background:var(--card);border-radius:14px;padding:16px;margin-bottom:14px;
        box-shadow:0 4px 14px rgba(0,0,0,.3);}
  label{display:block;font-size:.8rem;color:var(--mut);margin:8px 0 4px;}
  select,button{width:100%;padding:12px;border-radius:10px;border:none;font-size:1rem;}
  select{background:#0b1220;color:var(--txt);border:1px solid #334155;}
  button{background:var(--accent);color:#04210f;font-weight:700;margin-top:12px;cursor:pointer;}
  button.sec{background:#334155;color:var(--txt);}
  button:disabled{opacity:.5;cursor:not-allowed;}
  .row{display:flex;gap:10px;}
  .row>div{flex:1;}
  pre{background:#0b1220;border:1px solid #334155;border-radius:10px;padding:12px;
      overflow:auto;font-size:.8rem;max-height:320px;white-space:pre-wrap;}
  .pill{display:inline-block;padding:3px 10px;border-radius:999px;font-size:.75rem;}
  .ok{background:#064e3b;color:#6ee7b7;} .bad{background:#7f1d1d;color:#fca5a5;}
  .diag{font-size:1.6rem;font-weight:800;text-align:center;padding:14px;border-radius:12px;margin-top:6px;}
  .d-SANA{background:#064e3b;color:#6ee7b7;} .d-BOTRYTIS{background:#4c1d95;color:#ddd6fe;}
  .d-ANTRACNOSIS{background:#78350f;color:#fcd34d;} .d-PODRIDA{background:#7f1d1d;color:#fca5a5;}
  .d-DESCONOCIDO{background:#334155;color:#e2e8f0;}
  small{color:var(--mut);}
</style>
</head>
<body>
  <h1>&#127827; Diagnostico Espectral</h1>
  <div class="sub">LilyGO T-Display + AS7265x (18 bandas, 410-940 nm)</div>

  <div class="card">
    <div>Estado del equipo:
      <span id="stSensor" class="pill bad">sensor?</span>
      <span id="stWifi" class="pill bad">wifi?</span>
      <span id="stFb" class="pill bad">firebase?</span>
    </div>
  </div>

  <div class="card">
    <div class="row">
      <div>
        <label>Fruta</label>
        <select id="fruta">
          <option value="fresa">Fresa</option>
          <option value="mango">Mango</option>
          <option value="uva">Uva</option>
          <option value="arandano">Arandano</option>
          <option value="otra">Otra</option>
        </select>
      </div>
      <div>
        <label>Estado (etiqueta)</label>
        <select id="estado">
          <option value="sana">Sana</option>
          <option value="botrytis">Botrytis</option>
          <option value="antracnosis">Antracnosis</option>
          <option value="podrida">Podrida</option>
        </select>
      </div>
    </div>
    <button class="sec" onclick="guardarConfig()">Guardar configuracion</button>
    <button id="btnMedir" onclick="medir()">&#128300; MEDIR</button>
    <small id="msg"></small>
  </div>

  <div class="card">
    <label>Diagnostico del modelo embebido</label>
    <div id="diag" class="diag d-DESCONOCIDO">--</div>
  </div>

  <div class="card">
    <label>Respuesta JSON</label>
    <pre id="salida">Esperando medicion...</pre>
  </div>

<script>
function setPill(id, ok, txt){
  const e=document.getElementById(id);
  e.textContent=txt; e.className='pill '+(ok?'ok':'bad');
}
async function refrescarEstado(){
  try{
    const r=await fetch('/estado'); const j=await r.json();
    setPill('stSensor', j.sensor_ok, j.sensor_ok?'sensor OK':'sensor FAIL');
    setPill('stWifi',  j.wifi_ok,   j.wifi_ok? (j.ip||'wifi OK'):'sin wifi');
    setPill('stFb',    j.firebase_ok,j.firebase_ok?'firebase ON':'firebase OFF');
    if(j.fruta)  document.getElementById('fruta').value=j.fruta;
    if(j.estado) document.getElementById('estado').value=j.estado;
  }catch(e){ /* silencioso */ }
}
async function guardarConfig(){
  const fruta=document.getElementById('fruta').value;
  const estado=document.getElementById('estado').value;
  const body='fruta='+encodeURIComponent(fruta)+'&estado='+encodeURIComponent(estado);
  const r=await fetch('/config',{method:'POST',
    headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
  document.getElementById('msg').textContent = r.ok ? 'Configuracion guardada.' : 'Error al guardar.';
}
async function medir(){
  const btn=document.getElementById('btnMedir');
  btn.disabled=true; btn.textContent='Midiendo...';
  document.getElementById('msg').textContent='';
  try{
    const r=await fetch('/medir'); const j=await r.json();
    document.getElementById('salida').textContent=JSON.stringify(j,null,2);
    const d=(j.diagnostico||'DESCONOCIDO');
    const de=document.getElementById('diag');
    de.textContent=d; de.className='diag d-'+d;
  }catch(e){
    document.getElementById('salida').textContent='Error: '+e;
  }finally{
    btn.disabled=false; btn.textContent='\u{1F52C} MEDIR';
  }
}
refrescarEstado();
setInterval(refrescarEstado, 4000);
</script>
</body>
</html>
)rawliteral";

// ============================================================================
//  Portal de configuracion WiFi (se sirve en modo AP "GENIC-Setup")
//  Endpoints:
//    GET  /wifiscan    -> JSON [{ssid,rssi,sec}]
//    POST /wificonnect -> form ssid+pass ; conecta y responde {ok, ip, msg}
//    GET  /wifistatus  -> {conectado, ssid, ip}
// ============================================================================
const char PAGINA_WIFI[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>GENIC - Configurar WiFi</title>
<style>
  :root{--bg:#0f172a;--card:#1e293b;--accent:#ffc95c;--txt:#e2e8f0;--mut:#94a3b8;}
  *{box-sizing:border-box;font-family:system-ui,Segoe UI,Roboto,sans-serif;}
  body{margin:0;background:var(--bg);color:var(--txt);padding:16px;}
  h1{font-size:1.25rem;margin:0 0 2px;color:var(--accent);}
  .sub{color:var(--mut);font-size:.85rem;margin-bottom:16px;}
  .card{background:var(--card);border-radius:14px;padding:16px;margin-bottom:14px;
        box-shadow:0 4px 14px rgba(0,0,0,.3);}
  label{display:block;font-size:.8rem;color:var(--mut);margin:10px 0 4px;}
  select,input,button{width:100%;padding:12px;border-radius:10px;border:none;font-size:1rem;}
  select,input{background:#0b1220;color:var(--txt);border:1px solid #334155;}
  button{background:var(--accent);color:#3a2a00;font-weight:800;margin-top:14px;cursor:pointer;}
  button.sec{background:#334155;color:var(--txt);font-weight:600;}
  button:disabled{opacity:.5;cursor:not-allowed;}
  .row{display:flex;gap:8px;align-items:center;}
  .chk{display:flex;gap:8px;align-items:center;margin-top:8px;font-size:.85rem;color:var(--mut);}
  .chk input{width:auto;}
  .msg{margin-top:10px;font-size:.9rem;}
  .ok{color:#6ee7b7;} .bad{color:#fca5a5;}
  .bars{color:var(--accent);font-family:monospace;}
</style>
</head>
<body>
  <h1>&#128246; GENIC WiFi</h1>
  <div class="sub">Elige tu red y escribe la contrasena</div>

  <div class="card">
    <div class="row">
      <label style="margin:0">Redes disponibles</label>
    </div>
    <select id="ssid"><option value="">— pulsa Escanear —</option></select>
    <button class="sec" id="btnScan" onclick="escanear()">&#128260; Escanear redes</button>

    <label>Contrasena</label>
    <input id="pass" type="password" placeholder="clave de la red" autocomplete="off">
    <div class="chk">
      <input type="checkbox" id="ver" onchange="document.getElementById('pass').type=this.checked?'text':'password'">
      <span>Mostrar contrasena</span>
    </div>

    <button id="btnConn" onclick="conectar()">&#9989; Conectar</button>
    <div id="msg" class="msg"></div>
  </div>

  <div class="card">
    <label>Estado</label>
    <div id="estado" class="msg">Consultando...</div>
  </div>

<script>
function bars(r){ // rssi -> barras
  if(r>=-55) return '||||'; if(r>=-65) return '|||.'; if(r>=-75) return '||..'; return '|...';
}
async function escanear(){
  const b=document.getElementById('btnScan'); b.disabled=true; b.textContent='Escaneando...';
  try{
    const r=await fetch('/wifiscan'); const list=await r.json();
    const sel=document.getElementById('ssid'); sel.innerHTML='';
    if(!list.length){ sel.innerHTML='<option value="">(sin redes)</option>'; }
    list.forEach(n=>{
      const o=document.createElement('option'); o.value=n.ssid;
      o.textContent=(n.sec?'\uD83D\uDD12 ':'   ')+n.ssid+'   '+bars(n.rssi);
      sel.appendChild(o);
    });
  }catch(e){ document.getElementById('msg').textContent='Error al escanear'; }
  finally{ b.disabled=false; b.textContent='\uD83D\uDD04 Escanear redes'; }
}
async function conectar(){
  const ssid=document.getElementById('ssid').value;
  const pass=document.getElementById('pass').value;
  const m=document.getElementById('msg');
  if(!ssid){ m.className='msg bad'; m.textContent='Elige una red primero.'; return; }
  const b=document.getElementById('btnConn'); b.disabled=true; b.textContent='Conectando...';
  m.className='msg'; m.textContent='Conectando a '+ssid+'...';
  try{
    const body='ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass);
    const r=await fetch('/wificonnect',{method:'POST',
      headers:{'Content-Type':'application/x-www-form-urlencoded'},body});
    const j=await r.json();
    if(j.ok){ m.className='msg ok'; m.textContent='Conectado. IP: '+j.ip+' — ya puedes cerrar esta pagina.'; }
    else   { m.className='msg bad'; m.textContent='No se pudo conectar: '+(j.msg||'revisa la clave'); }
  }catch(e){ m.className='msg bad'; m.textContent='Error de conexion.'; }
  finally{ b.disabled=false; b.textContent='\u2705 Conectar'; refrescar(); }
}
async function refrescar(){
  try{
    const r=await fetch('/wifistatus'); const j=await r.json();
    const e=document.getElementById('estado');
    if(j.conectado){ e.className='msg ok'; e.textContent='Conectado a '+j.ssid+' (IP '+j.ip+')'; }
    else { e.className='msg bad'; e.textContent='No conectado'; }
  }catch(e){}
}
refrescar(); escanear();
</script>
</body>
</html>
)rawliteral";

#endif // PAGINA_WEB_H
