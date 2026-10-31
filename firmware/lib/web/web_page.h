#pragma once
// Single-page web app served at "/". Plain HTML/CSS/JS, no external assets.
static const char WEB_PAGE[] = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0b0f14"><meta name="apple-mobile-web-app-capable" content="yes">
<title>Blinds</title>
<style>
:root{--bg:#0b0f14;--s1:#151b23;--s2:#1f2731;--line:#2a3441;--tx:#e8ecf1;--mu:#8a96a6;--ac:#ffb547;--sky:#5ac8fa;--bad:#ff5a5f;--ok:#4cd964}
*{box-sizing:border-box}html,body{margin:0;background:var(--bg);color:var(--tx);font:15px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
main{max-width:560px;margin:0 auto;padding:16px 16px 90px}
header{display:flex;align-items:center;justify-content:space-between;margin:4px 0 14px}
h1{font-size:22px;margin:0;font-weight:700}h2{font-size:13px;text-transform:uppercase;letter-spacing:.08em;color:var(--mu);margin:22px 0 8px}
.pill{font-size:12px;color:var(--mu);background:var(--s1);border:1px solid var(--line);border-radius:99px;padding:4px 10px;white-space:nowrap}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--bad);margin-right:6px;vertical-align:1px}.dot.on{background:var(--ok)}
.card{background:var(--s1);border:1px solid var(--line);border-radius:16px;padding:16px;margin-bottom:12px}
.row{display:flex;gap:10px;align-items:center}.row>*{flex:1}.between{justify-content:space-between}
button{font:inherit;color:var(--tx);background:var(--s2);border:1px solid var(--line);border-radius:12px;padding:12px 10px;cursor:pointer;font-weight:600}
button:active{transform:scale(.98)}button.pri{background:var(--ac);color:#1b1306;border-color:var(--ac)}button.bad{color:var(--bad)}button.sm{padding:8px 10px;font-size:13px}
.big{display:flex;gap:18px;align-items:center}
#win{width:120px;height:150px;flex:none}
.pct{font-size:56px;font-weight:300;line-height:1}.pct small{font-size:22px;color:var(--mu)}
.sub{color:var(--mu);font-size:13px}
input[type=range]{width:100%;accent-color:var(--ac);height:32px}
input,select{font:inherit;color:var(--tx);background:var(--s2);border:1px solid var(--line);border-radius:10px;padding:9px 10px;width:100%}
input[type=checkbox]{width:20px;height:20px;accent-color:var(--ac);flex:none}
label.f{display:block;margin:10px 0 0}label.f span{display:block;color:var(--mu);font-size:12px;margin-bottom:4px}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:0 10px}
nav{position:fixed;bottom:0;left:0;right:0;display:flex;background:rgba(21,27,35,.94);backdrop-filter:blur(10px);border-top:1px solid var(--line);padding:6px 6px calc(6px + env(safe-area-inset-bottom))}
nav button{flex:1;background:none;border:0;color:var(--mu);font-size:12px;padding:8px 2px}nav button.on{color:var(--ac)}
.tab{display:none}.tab.on{display:block}
.sched{border-top:1px solid var(--line);padding:12px 0}.sched:first-child{border:0;padding-top:0}
.days{display:flex;gap:4px;margin-top:8px}.days button{flex:1;padding:6px 0;font-size:12px;border-radius:8px}.days button.on{background:var(--sky);color:#04121a;border-color:var(--sky)}
pre{white-space:pre-wrap;word-break:break-all;font-size:11px;color:var(--mu);max-height:300px;overflow:auto;background:var(--bg);padding:10px;border-radius:10px}
.kv{display:grid;grid-template-columns:auto 1fr;gap:4px 12px;font-size:13px}.kv b{color:var(--mu);font-weight:500}
.err{background:#3a1416;border-color:#5c1f22;color:#ffb3b5}
#toast{position:fixed;left:50%;top:14px;transform:translateX(-50%);background:var(--s2);border:1px solid var(--line);padding:10px 16px;border-radius:12px;opacity:0;transition:.25s;pointer-events:none;z-index:9}
#toast.on{opacity:1}
code{background:var(--bg);padding:2px 6px;border-radius:6px;font-size:12px;word-break:break-all}
</style></head><body><main>
<header><h1 id="host">Blinds</h1><span class="pill"><span class="dot" id="dot"></span><span id="clock">--:--</span></span></header>
<div id="errbox" class="card err" style="display:none"><div class="row between"><span id="errtxt"></span><button class="sm" style="flex:none" onclick="post('/api/clear')">Clear</button></div></div>

<section class="tab on" id="t-home">
 <div class="card">
  <div class="big">
   <svg id="win" viewBox="0 0 120 150"><rect x="4" y="4" width="112" height="142" rx="8" fill="#0e1620" stroke="#2a3441" stroke-width="3"/>
    <rect x="10" y="10" width="100" height="130" rx="4" fill="url(#sky)"/><defs><linearGradient id="sky" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="#3f7fbf"/><stop offset="1" stop-color="#ffb547"/></linearGradient></defs>
    <g id="blind"></g><rect x="6" y="6" width="108" height="6" rx="3" fill="#3a4554"/></svg>
   <div><div class="pct"><span id="pct">--</span><small>%</small></div><div class="sub" id="state">connecting...</div><div class="sub" id="next"></div></div>
  </div>
  <input type="range" id="slider" min="0" max="100" step="1">
  <div class="row"><button class="pri" onclick="act('/api/open')">Open</button><button onclick="act('/api/stop')">Stop</button><button class="pri" onclick="act('/api/close')">Close</button></div>
  <div class="row" style="margin-top:8px"><button class="sm" onclick="act('/api/open?gentle=1')">Gentle open</button><button class="sm" onclick="act('/api/close?gentle=1')">Gentle close</button><button class="sm" onclick="act('/api/position?value=50')">50%</button></div>
 </div>
 <div class="card" id="wx" style="display:none"><div class="row between"><div><div style="font-size:30px;font-weight:300" id="wxt"></div><div class="sub" id="wxd"></div></div><div class="sub" id="wxm" style="text-align:right"></div></div></div>
 <div class="card" id="buscard" style="display:none"><div class="row between"><b id="busname">Bus</b><button class="sm" style="flex:none" onclick="loadBus()">Refresh</button></div><div id="buslist" class="sub" style="margin-top:8px"></div></div>
</section>

<section class="tab" id="t-sched">
 <div class="card"><div class="row between"><div><b>Schedules</b><div class="sub" id="pausetxt"></div></div><button class="sm" style="flex:none" id="pausebtn"></button></div></div>
 <div class="card" id="schedlist"></div>
 <button class="pri" style="width:100%" onclick="saveSched()">Save schedules</button>
</section>

<section class="tab" id="t-cal">
 <div class="card"><b>1 · Measure motor ratio</b><div class="sub">Moves ~2 motor turns forward and back to learn steps per encoder tick (also fixes direction). Do this first, with the blind half open.</div>
  <div class="row" style="margin-top:10px"><button onclick="post('/api/calibrate?action=ratio')">Measure ratio</button></div><div class="sub" id="ratio" style="margin-top:6px"></div></div>
 <div class="card"><b>2 · Set end positions</b><div class="sub">Jog the blind, then save the closed and open positions.</div>
  <div class="row" style="margin-top:10px"><button onclick="jog(-8192)">&#9650;&#9650;</button><button onclick="jog(-1024)">&#9650;</button><button onclick="jog(1024)">&#9660;</button><button onclick="jog(8192)">&#9660;&#9660;</button></div>
  <div class="row" style="margin-top:8px"><button onclick="post('/api/stop')">Stop</button></div>
  <div class="row" style="margin-top:8px"><button class="pri" onclick="post('/api/calibrate?action=closed')">Set CLOSED here</button><button class="pri" onclick="post('/api/calibrate?action=open')">Set OPEN here</button></div>
  <div class="kv" style="margin-top:12px"><b>Encoder</b><span id="ticks">-</span><b>Closed</b><span id="ctick">-</span><b>Open</b><span id="otick">-</span></div>
  <label class="row" style="margin-top:12px;gap:8px"><input type="checkbox" id="motorReversed" style="flex:none"><span>Reverse motor direction (flips jog buttons)</span></label>
 </div>
</section>

<section class="tab" id="t-set">
 <div class="card"><b>Motion</b><div class="grid">
  <label class="f"><span>Max speed (steps/s)</span><input type="number" data-k="maxSpeed"></label>
  <label class="f"><span>Acceleration (steps/s²)</span><input type="number" data-k="acceleration"></label>
  <label class="f"><span>Gentle move (s)</span><input type="number" data-k="gentleSeconds"></label>
  <label class="f"><span>Stall tolerance (ticks)</span><input type="number" data-k="stallTolerance"></label>
  <label class="f"><span>Position tolerance (ticks)</span><input type="number" data-k="positionTolerance"></label>
  <label class="f"><span>Hold torque when idle</span><select data-k="holdWhenIdle" data-b><option value="0">No</option><option value="1">Yes</option></select></label>
 </div><div class="sub" style="margin-top:10px">TMC2209 UART (ignored in standalone mode):</div><div class="grid">
  <label class="f"><span>Run current (mA)</span><input type="number" data-k="runCurrent"></label>
  <label class="f"><span>Hold current (%)</span><input type="number" data-k="holdCurrentPct"></label>
  <label class="f"><span>Microsteps</span><select data-k="microsteps"><option>8</option><option>16</option><option>32</option><option>64</option></select></label>
  <label class="f"><span>Mode</span><select data-k="stealthChop" data-b><option value="1">StealthChop (silent)</option><option value="0">SpreadCycle</option></select></label>
 </div></div>
 <div class="card"><b>Display &amp; knob</b><div class="grid">
  <label class="f"><span>Brightness (5-255)</span><input type="number" data-k="brightness"></label>
  <label class="f"><span>Dim brightness</span><input type="number" data-k="dimBrightness"></label>
  <label class="f"><span>Dim after (s, 0=never)</span><input type="number" data-k="dimAfterS"></label>
  <label class="f"><span>Screen off after (s)</span><input type="number" data-k="offAfterS"></label>
  <label class="f"><span>Rotation</span><select data-k="rotation"><option value="0">0°</option><option value="1">90°</option><option value="2">180°</option><option value="3">270°</option></select></label>
  <label class="f"><span>Knob step (%)</span><input type="number" data-k="knobStepPct"></label>
  <label class="f"><span>Knob direction</span><select data-k="knobReversed" data-b><option value="0">Normal</option><option value="1">Reversed</option></select></label>
  <label class="f"><span>Clock</span><select data-k="clock24h" data-b><option value="1">24 h</option><option value="0">12 h</option></select></label>
 </div></div>
 <div class="card"><b>Location, weather &amp; bus</b><div class="grid">
  <label class="f"><span>Latitude</span><input type="number" step="0.0001" data-k="latitude"></label>
  <label class="f"><span>Longitude</span><input type="number" step="0.0001" data-k="longitude"></label>
 </div>
  <label class="f"><span>Time zone (POSIX TZ)</span><input data-k="timezone"></label>
  <div class="grid"><label class="f"><span>Weather</span><select data-k="weatherEnabled" data-b><option value="1">On</option><option value="0">Off</option></select></label>
  <label class="f"><span>Red bus stop code</span><input data-k="busStop" placeholder="PA433"></label></div>
  <label class="f"><span>Only these services (comma separated, empty = all)</span><input data-k="busServices" placeholder="506, 507"></label>
 </div>
 <div class="card"><b>Network &amp; security</b>
  <label class="f"><span>Hostname (reboot to apply)</span><input data-k="hostname"></label>
  <label class="f"><span>API token (empty = no auth)</span><input data-k="apiToken" placeholder="unchanged" autocomplete="off"></label>
  <label class="f"><span>OTA password (reboot to apply)</span><input data-k="otaPassword" placeholder="unchanged" autocomplete="off"></label>
 </div>
 <button class="pri" style="width:100%" onclick="saveSettings()">Save settings</button>
</section>

<section class="tab" id="t-sys">
 <div class="card"><div class="kv" id="sysinfo"></div></div>
 <div class="card"><b>iPhone Shortcuts / automations</b><div class="sub" style="margin:6px 0">Use "Get contents of URL" with any of:</div><div id="urls" class="sub"></div></div>
 <div class="card"><b>Motor driver</b><pre id="driver">-</pre><button class="sm" onclick="loadDriver()">Refresh</button></div>
 <div class="card"><div class="row between"><b>Log</b><button class="sm" style="flex:none" onclick="loadLog()">Refresh</button></div><pre id="log"></pre></div>
 <div class="card"><b>Firmware update</b><div class="sub">Upload <code>.pio/build/blinds/firmware.bin</code></div><input type="file" id="fw" accept=".bin" style="margin:10px 0"><button onclick="upload()">Upload &amp; reboot</button><div class="sub" id="fwp"></div></div>
 <div class="card"><b>Access token for this browser</b><input id="tok" placeholder="only needed if an API token is set"><button class="sm" style="margin-top:8px" onclick="localStorage.tok=tok.value;toast('Saved')">Save</button></div>
 <div class="row"><button onclick="sys('reboot')">Reboot</button><button class="bad" onclick="confirm('Forget WiFi and open the setup portal?')&&sys('forget-wifi')">Forget WiFi</button></div>
 <div class="row" style="margin-top:10px"><button class="bad" onclick="confirm('Erase ALL settings and calibration?')&&sys('factory-reset')">Factory reset</button></div>
</section>
</main>
<nav><button class="on" data-t="home">Control</button><button data-t="sched">Schedules</button><button data-t="cal">Calibrate</button><button data-t="set">Settings</button><button data-t="sys">System</button></nav>
<div id="toast"></div>
<script>
const $=id=>document.getElementById(id);let S={},CFG={},SCH=[],drag=false,timer;
const DAYS=['M','T','W','T','F','S','S'];
function tk(u){const t=localStorage.tok;return t?u+(u.includes('?')?'&':'?')+'token='+encodeURIComponent(t):u}
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('on');clearTimeout(t._h);t._h=setTimeout(()=>t.classList.remove('on'),1800)}
async function req(u,o){try{const r=await fetch(tk(u),o);const ct=r.headers.get('content-type')||'';const d=ct.includes('json')?await r.json():await r.text();if(!r.ok)toast(d.message||('Error '+r.status));return d}catch(e){toast('Connection error');throw e}}
async function post(u){const d=await req(u,{method:'POST'});if(d&&d.message)toast(d.message);poll();return d}
function act(u){post(u)}
function jog(t){post('/api/jog?ticks='+t)}
function sys(a){post('/api/system?action='+a)}
function drawBlind(p){const g=$('blind'),h=Math.round(128*(100-p)/100);let s='<rect x="10" y="10" width="100" height="'+h+'" fill="#d9cdb8"/>';for(let y=18;y<10+h;y+=8)s+='<line x1="10" x2="110" y1="'+y+'" y2="'+y+'" stroke="#bfb29b" stroke-width="1"/>';s+='<rect x="8" y="'+(8+h)+'" width="104" height="5" rx="2" fill="#8c7f6a"/>';g.innerHTML=s}
async function poll(){clearTimeout(timer);try{S=await req('/api/status');render()}catch(e){$('dot').className='dot'}timer=setTimeout(poll,S.state=='moving'||S.state=='measuring'?400:1500)}
function render(){
 $('dot').className='dot on';$('host').textContent=S.hostname[0].toUpperCase()+S.hostname.slice(1);
 $('clock').textContent=S.time?S.time.slice(11,16):'no time';
 $('pct').textContent=S.calibrated?Math.round(S.position):'--';drawBlind(S.calibrated?S.position:100);
 if(!drag&&S.calibrated)$('slider').value=S.target!=null?S.target:Math.round(S.position);
 let st=!S.calibrated?'Not calibrated - see Calibrate':S.state=='moving'?(S.direction>0?'Opening':'Closing')+(S.target!=null?' to '+S.target+'%':''):S.state=='measuring'?'Measuring ratio...':'Idle';
 if(S.simulated)st+=' · simulated';$('state').textContent=st;
 $('next').textContent=S.schedulesPaused?'Schedules paused':S.next?'Next: '+S.next.text+' → '+S.next.position+'%':'';
 $('errbox').style.display=S.state=='error'?'':'none';$('errtxt').textContent='⚠ '+S.error;
 if(S.weather){$('wx').style.display='';$('wxt').textContent=Math.round(S.weather.temp)+'°';$('wxd').textContent=S.weather.text;$('wxm').innerHTML='↑'+Math.round(S.weather.max)+'° ↓'+Math.round(S.weather.min)+'°<br>'+S.weather.humidity+'% · '+Math.round(S.weather.wind)+' km/h'+(S.weather.precip>=0?'<br>rain '+S.weather.precip+'%':'')}
 $('ticks').textContent=S.ticks+' (raw '+S.rawAngle+', '+S.steps+' steps)';
 $('ratio').textContent='Current: '+S.stepsPerTick.toFixed(5)+' steps/tick'+(CFG.ratioMeasured?' (measured)':' (default guess)');
 $('pausetxt').textContent=S.schedulesPaused?'Paused - nothing will run':S.next?'Next: '+S.next.text:'No upcoming schedule';
 $('pausebtn').textContent=S.schedulesPaused?'Resume':'Pause all';$('pausebtn').onclick=()=>sys(S.schedulesPaused?'resume-schedules':'pause-schedules');
 const u=Math.floor(S.uptime/86400)+'d '+new Date(S.uptime*1000).toISOString().slice(11,19);
 $('sysinfo').innerHTML=['Firmware',S.version,'IP',S.wifi.ip,'WiFi',S.wifi.ssid+' ('+S.wifi.rssi+' dBm)','Uptime',u,'Free heap',(S.heap/1024).toFixed(0)+' KB','Driver UART',S.tmcUart?'connected':'standalone','Encoder',S.encoderOk?'OK':'NOT FOUND','Mode',S.simulated?'SIMULATED':'hardware'].map((x,i)=>i%2?'<span>'+x+'</span>':'<b>'+x+'</b>').join('');
 const b=location.origin;$('urls').innerHTML=['/api/open','/api/close','/api/open?gentle=1','/api/position?value=30','/api/toggle','/api/stop'].map(p=>'<div style="margin:4px 0"><code>'+tk(b+p)+'</code></div>').join('');
}
$('slider').oninput=()=>{drag=true;$('pct').textContent=$('slider').value};
$('slider').onchange=()=>{drag=false;post('/api/position?value='+$('slider').value)};
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{document.querySelectorAll('nav button,.tab').forEach(e=>e.classList.remove('on'));b.classList.add('on');$('t-'+b.dataset.t).classList.add('on');
 if(b.dataset.t=='sys'){loadLog();loadDriver()}if(b.dataset.t=='sched')loadSched();if(b.dataset.t=='set'||b.dataset.t=='cal')loadSettings()});
async function loadSettings(){CFG=await req('/api/settings');document.querySelectorAll('[data-k]').forEach(e=>{const v=CFG[e.dataset.k];if(v===undefined)return;e.value=e.hasAttribute('data-b')?(v?1:0):v});
 $('ctick').textContent=CFG.closedSet?CFG.closedTicks:'not set';$('otick').textContent=CFG.openSet?CFG.openTicks:'not set';$('motorReversed').checked=CFG.motorReversed}
$('motorReversed').onchange=async e=>{await req('/api/settings',{method:'POST',body:JSON.stringify({motorReversed:e.target.checked})});toast('Saved');loadSettings()};
async function saveSettings(){const o={};document.querySelectorAll('[data-k]').forEach(e=>{const k=e.dataset.k;let v=e.value;if((k=='apiToken'||k=='otaPassword')&&!v)return;
 if(e.hasAttribute('data-b'))v=v=='1';else if(e.type=='number'||k=='microsteps'||k=='rotation')v=Number(v);o[k]=v});
 CFG=await req('/api/settings',{method:'POST',body:JSON.stringify(o)});if(o.apiToken){localStorage.tok=o.apiToken}toast('Settings saved');loadSettings()}
function hhmm(m){return String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0')}
async function loadSched(){SCH=await req('/api/schedules');drawSched()}
function drawSched(){$('schedlist').innerHTML=SCH.map((s,i)=>`<div class="sched"><div class="row"><input type="checkbox" ${s.enabled?'checked':''} onchange="SCH[${i}].enabled=this.checked">
 <select onchange="SCH[${i}].trigger=this.value;SCH[${i}].minutes=this.value=='time'?420:0;drawSched()"><option value="time" ${s.trigger=='time'?'selected':''}>At time</option><option value="sunrise" ${s.trigger=='sunrise'?'selected':''}>Sunrise</option><option value="sunset" ${s.trigger=='sunset'?'selected':''}>Sunset</option></select>
 ${s.trigger=='time'?`<input type="time" value="${hhmm(s.minutes)}" onchange="const p=this.value.split(':');SCH[${i}].minutes=p[0]*60+ +p[1]">`:`<input type="number" value="${s.minutes}" title="offset minutes" onchange="SCH[${i}].minutes=+this.value">`}</div>
 <div class="row" style="margin-top:8px"><span class="sub" style="flex:none">To</span><input type="range" min="0" max="100" value="${s.position}" oninput="SCH[${i}].position=+this.value;this.nextElementSibling.textContent=this.value+'%'"><span style="flex:none;width:44px">${s.position}%</span>
 <label class="row" style="flex:none;gap:6px"><input type="checkbox" ${s.gentle?'checked':''} onchange="SCH[${i}].gentle=this.checked"><span class="sub">gentle</span></label></div>
 <div class="days">${DAYS.map((d,j)=>`<button class="${s.days&1<<j?'on':''}" onclick="SCH[${i}].days^=${1<<j};drawSched()">${d}</button>`).join('')}</div></div>`).join('')}
async function saveSched(){SCH=await req('/api/schedules',{method:'POST',body:JSON.stringify(SCH)});drawSched();toast('Schedules saved');poll()}
async function loadLog(){$('log').textContent=await req('/api/log');$('log').scrollTop=1e9}
async function loadDriver(){$('driver').textContent=await req('/api/driver')}
async function loadBus(){const b=await req('/api/bus');if(!b.stop){$('buscard').style.display='none';return}$('buscard').style.display='';$('busname').textContent='🚌 '+(b.name||b.stop);
 $('buslist').innerHTML=b.arrivals.length?b.arrivals.map(a=>`<div class="row between" style="margin:4px 0"><b style="color:var(--tx);flex:none;width:60px">${a.service}</b><span>${a.min==a.max?a.min:a.min+'-'+a.max} min</span><span style="text-align:right">${(a.meters/1000).toFixed(1)} km</span></div>`).join(''):(b.message||'Loading...')}
function upload(){const f=$('fw').files[0];if(!f)return toast('Choose a .bin file');const x=new XMLHttpRequest(),fd=new FormData();fd.append('firmware',f);
 x.upload.onprogress=e=>$('fwp').textContent='Uploading '+Math.round(e.loaded/e.total*100)+'%';x.onload=()=>{$('fwp').textContent=x.responseText;setTimeout(()=>location.reload(),8000)};x.open('POST',tk('/update'));x.send(fd)}
$('tok').value=localStorage.tok||'';poll();loadSettings();loadBus();setInterval(()=>{if($('t-home').classList.contains('on'))loadBus()},30000);
</script></body></html>)HTML";
