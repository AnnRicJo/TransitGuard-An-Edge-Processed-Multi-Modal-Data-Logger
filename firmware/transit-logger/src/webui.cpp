#include "webui.h"
#include "config.h"
#include "settings.h"
#include "sensors.h"
#include "event_log.h"
#include "tamper.h"
#include "rtc_time.h"
#include "buzzer.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>

WebUiServer WebUi;
static WebServer server(WEBSERVER_PORT);

static volatile bool s_sleepRequested = false;
static uint32_t s_sleepRequestTimestamp = 0;

static DNSServer dnsServer;
static const byte DNS_PORT = 53;
static const char *MDNS_HOSTNAME = "transitlogger";

static const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang='en' data-theme='dark'><head>
<meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>
<meta name='color-scheme' content='dark light'>
<title>TransitGuard &middot; Smart Cargo Monitoring</title>
<style>
:root{
--bg:#0c1014;--bg2:#11171d;--card:#141b22;--card2:#182029;--line:#22303c;
--tx:#e6edf3;--tx2:#93a4b3;--tx3:#61758a;--inputbg:#0d1318;--track:#1e2831;
--ok:#31c56c;--warn:#e8a72c;--crit:#e2513f;--info:#3aa0d9;
--r:12px;--r-sm:8px;--sp:14px;--fs:15px;
--font:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;
}
html[data-theme='light']{
--bg:#f2f5f8;--bg2:#ffffff;--card:#ffffff;--card2:#eef3f7;--line:#d5dee6;
--tx:#16212b;--tx2:#4a5d6d;--tx3:#748899;--inputbg:#ffffff;--track:#dde5ec;
--ok:#1a9b52;--warn:#b57b0d;--crit:#c33823;--info:#1c7cb5;
}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--tx);font-family:var(--font);
font-size:var(--fs);line-height:1.45;-webkit-font-smoothing:antialiased}
.wrap{max-width:1180px;margin:0 auto;padding:16px 16px 48px}
.skip{position:absolute;left:-9999px;top:0;background:var(--card2);color:var(--tx);padding:10px 14px;
border-radius:var(--r-sm);z-index:99}
.skip:focus{left:12px;top:12px}
header{display:flex;flex-wrap:wrap;gap:12px;align-items:center;justify-content:space-between;
padding:14px 16px;background:var(--bg2);border:1px solid var(--line);border-radius:var(--r)}
.brand{display:flex;align-items:center;gap:12px}
.logo{width:38px;height:38px;border-radius:9px;border:1px solid var(--line);background:var(--card2);
display:flex;align-items:center;justify-content:center;color:var(--info)}
h1{margin:0;font-size:1.15rem;letter-spacing:.4px;font-weight:650}
.sub{margin:0;font-size:.72rem;text-transform:uppercase;letter-spacing:1.6px;color:var(--tx3)}
.chips{display:flex;flex-wrap:wrap;gap:8px;align-items:center}
.chip{display:flex;align-items:center;gap:6px;font-size:.75rem;color:var(--tx2);
background:var(--card);border:1px solid var(--line);border-radius:999px;padding:5px 11px;
font-variant-numeric:tabular-nums}
.chip svg{width:13px;height:13px;flex:none;stroke:currentColor;fill:none;stroke-width:1.8}
.dot{width:8px;height:8px;border-radius:50%;background:var(--tx3);flex:none}
.dot.ok{background:var(--ok);box-shadow:0 0 0 3px rgba(49,197,108,.14)}
.dot.warn{background:var(--warn);box-shadow:0 0 0 3px rgba(232,167,44,.14)}
.dot.crit{background:var(--crit);box-shadow:0 0 0 3px rgba(226,81,63,.14)}
.dot.live{animation:pulse 2s ease-in-out infinite}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.35}}
.iconbtn{width:32px;height:32px;padding:0;justify-content:center}
.iconbtn svg{width:16px;height:16px;stroke:currentColor;fill:none;stroke-width:1.8}
.banner{margin-top:var(--sp);border:1px solid var(--line);border-radius:var(--r);padding:16px 18px;
background:var(--bg2);display:flex;flex-wrap:wrap;gap:14px;align-items:center;justify-content:space-between;
transition:border-color .25s,background .25s}
.banner.ok{border-color:rgba(49,197,108,.4);background:linear-gradient(90deg,rgba(49,197,108,.07),transparent 60%)}
.banner.warn{border-color:rgba(232,167,44,.45);background:linear-gradient(90deg,rgba(232,167,44,.08),transparent 60%)}
.banner.crit{border-color:rgba(226,81,63,.5);background:linear-gradient(90deg,rgba(226,81,63,.1),transparent 60%);
animation:alert 2.2s ease-in-out infinite}
@keyframes alert{0%,100%{box-shadow:0 0 0 0 rgba(226,81,63,0)}50%{box-shadow:0 0 0 5px rgba(226,81,63,.12)}}
.banner h2{margin:0;font-size:1.5rem;letter-spacing:1px;display:flex;align-items:center;gap:10px}
.banner p{margin:3px 0 0;font-size:.83rem;color:var(--tx2)}
.grid{display:grid;gap:var(--sp);margin-top:var(--sp);grid-template-columns:repeat(auto-fit,minmax(238px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:14px 15px;
transition:border-color .3s,background .3s}
.card.flash{border-color:rgba(58,160,217,.55);background:var(--card2)}
.card h3{margin:0 0 2px;font-size:.7rem;letter-spacing:1.4px;text-transform:uppercase;color:var(--tx3);
display:flex;align-items:center;gap:7px;font-weight:600}
.card h3 svg{width:15px;height:15px;stroke:currentColor;fill:none;stroke-width:1.7;flex:none}
.val{font-size:1.75rem;font-weight:650;font-variant-numeric:tabular-nums;letter-spacing:-.5px;margin-top:6px}
.val small{font-size:.85rem;color:var(--tx2);font-weight:500;margin-left:3px}
.meta{font-size:.76rem;color:var(--tx3);margin-top:4px;font-variant-numeric:tabular-nums}
.spark{display:block;width:100%;height:34px;margin-top:8px;overflow:visible}
.spark path{fill:none;stroke:var(--info);stroke-width:1.6;stroke-linejoin:round;stroke-linecap:round}
.spark path.area{fill:rgba(58,160,217,.12);stroke:none}
.tag{display:inline-block;margin-top:8px;font-size:.68rem;letter-spacing:1px;text-transform:uppercase;
padding:3px 9px;border-radius:6px;border:1px solid var(--line);color:var(--tx2)}
.tag.ok{color:var(--ok);border-color:rgba(49,197,108,.35);background:rgba(49,197,108,.08)}
.tag.warn{color:var(--warn);border-color:rgba(232,167,44,.35);background:rgba(232,167,44,.08)}
.tag.crit{color:var(--crit);border-color:rgba(226,81,63,.4);background:rgba(226,81,63,.1)}
.tag.info{color:var(--info);border-color:rgba(58,160,217,.35);background:rgba(58,160,217,.08)}
.tag.stale{opacity:.55}
.bar{height:6px;border-radius:4px;background:var(--track);overflow:hidden;margin-top:10px}
.bar i{display:block;height:100%;width:0;background:var(--ok);transition:width .5s ease,background .3s}
.section{margin-top:20px}
.section>h2{font-size:.78rem;letter-spacing:1.6px;text-transform:uppercase;color:var(--tx3);
margin:0 0 10px;font-weight:600}
.panel{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:16px}
.cols{display:grid;gap:var(--sp);grid-template-columns:repeat(auto-fit,minmax(300px,1fr))}
.toolbar{display:flex;flex-wrap:wrap;gap:9px;align-items:center;margin:0 0 12px}
.toolbar input[type=search]{width:auto;flex:1 1 180px;margin-top:0}
.evt{max-height:340px;overflow:auto;border:1px solid var(--line);border-radius:9px;background:var(--bg2)}
.evt table{width:100%;border-collapse:collapse;font-size:.8rem;font-variant-numeric:tabular-nums}
.evt th{position:sticky;top:0;background:var(--card2);color:var(--tx3);text-align:left;
font-size:.66rem;letter-spacing:1.1px;text-transform:uppercase;padding:8px 10px;
border-bottom:1px solid var(--line);cursor:pointer;user-select:none;white-space:nowrap}
.evt th[aria-sort=ascending]::after{content:' \2191'}
.evt th[aria-sort=descending]::after{content:' \2193'}
.evt td{padding:7px 10px;border-bottom:1px solid var(--line);color:var(--tx2);vertical-align:top}
.evt tr:last-child td{border-bottom:none}
.evt td.k{color:var(--tx);white-space:nowrap}
.evt td.ico svg{width:14px;height:14px;stroke:currentColor;fill:none;stroke-width:1.7}
.empty{padding:18px;text-align:center;color:var(--tx3);font-size:.83rem}
fieldset{border:1px solid var(--line);border-radius:10px;padding:12px 14px 16px;margin:0 0 14px}
legend{font-size:.68rem;letter-spacing:1.3px;text-transform:uppercase;color:var(--info);padding:0 6px}
label{display:block;margin-top:10px;font-size:.8rem;color:var(--tx)}
label .hint{display:block;font-size:.72rem;color:var(--tx3);margin-top:2px;font-weight:400}
input{width:100%;padding:9px 10px;margin-top:6px;border-radius:var(--r-sm);border:1px solid var(--line);
background:var(--inputbg);color:var(--tx);font-size:.9rem;font-family:inherit}
input:focus-visible,button:focus-visible,a:focus-visible,th:focus-visible{outline:2px solid var(--info);
outline-offset:2px}
input:focus{border-color:var(--info)}
input[aria-invalid=true]{border-color:var(--crit)}
.err{display:block;color:var(--crit);font-size:.72rem;margin-top:4px;min-height:0}
.pwrow{position:relative}
.pwrow button{position:absolute;right:6px;bottom:6px;padding:5px 9px;font-size:.72rem}
.btns{display:flex;flex-wrap:wrap;gap:9px;margin-top:var(--sp)}
button,a.btn{display:inline-flex;align-items:center;gap:7px;padding:10px 15px;border-radius:var(--r-sm);
border:1px solid var(--line);background:var(--card2);color:var(--tx);font-size:.86rem;font-family:inherit;
cursor:pointer;text-decoration:none;transition:transform .08s,background .2s,border-color .2s}
button svg,a.btn svg{width:15px;height:15px;stroke:currentColor;fill:none;stroke-width:1.8}
button:hover,a.btn:hover{border-color:var(--info)}
button:active,a.btn:active{transform:translateY(1px)}
button:disabled{opacity:.55;cursor:progress}
button.primary{background:rgba(58,160,217,.16);border-color:rgba(58,160,217,.5);color:var(--info)}
button.danger{background:rgba(226,81,63,.14);border-color:rgba(226,81,63,.45);color:var(--crit)}
button.amber{background:rgba(232,167,44,.14);border-color:rgba(232,167,44,.45);color:var(--warn)}
.note{font-size:.76rem;color:var(--tx3);margin:8px 0 0}
.hidden{display:none!important}
.sr{position:absolute;width:1px;height:1px;overflow:hidden;clip:rect(0 0 0 0);white-space:nowrap}
#toasts{position:fixed;left:50%;bottom:18px;transform:translateX(-50%);display:flex;flex-direction:column;
gap:8px;z-index:60;width:min(92vw,420px)}
.toast{background:var(--card2);border:1px solid var(--line);border-left-width:3px;border-radius:9px;
padding:10px 13px;font-size:.84rem;animation:in .2s ease;display:flex;gap:10px;align-items:flex-start}
.toast.ok{border-left-color:var(--ok)}.toast.err{border-left-color:var(--crit)}
.toast.info{border-left-color:var(--info)}
.toast button{padding:2px 7px;font-size:.72rem;margin-left:auto}
@keyframes in{from{opacity:0;transform:translateY(8px)}to{opacity:1;transform:none}}
#modal{position:fixed;inset:0;background:rgba(4,7,10,.72);display:flex;align-items:center;
justify-content:center;padding:18px;z-index:70}
.dlg{background:var(--card);border:1px solid var(--line);border-radius:var(--r);padding:20px;
max-width:400px;width:100%}
.dlg h4{margin:0 0 8px;font-size:1rem}
.dlg p{margin:0;color:var(--tx2);font-size:.86rem}
footer{margin-top:22px;text-align:center;font-size:.72rem;color:var(--tx3);letter-spacing:.6px}
.spin{width:12px;height:12px;border-radius:50%;border:2px solid rgba(127,127,127,.3);
border-top-color:var(--info);animation:sp .7s linear infinite;display:inline-block}
@keyframes sp{to{transform:rotate(360deg)}}
.stale-overlay{opacity:.55;filter:saturate(.4)}
@media(max-width:560px){.wrap{padding:12px 12px 40px}.banner h2{font-size:1.25rem}.val{font-size:1.5rem}}
@media(prefers-reduced-motion:reduce){*{animation:none!important;transition:none!important}}
@media print{header .chips,.btns,.toolbar,#toasts,#modal,footer{display:none!important}
body{background:#fff;color:#000}.card,.panel{border-color:#999}}
</style></head><body>
<a class='skip' href='#main'>Skip to dashboard</a>
<svg class='sr' aria-hidden='true'><defs>
<symbol id='i-temp' viewBox='0 0 24 24'><path d='M14 14.8V5a2 2 0 1 0-4 0v9.8a4 4 0 1 0 4 0Z'/></symbol>
<symbol id='i-light' viewBox='0 0 24 24'><path d='M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9V16h7v-2.1A6 6 0 0 0 12 3Z'/></symbol>
<symbol id='i-pres' viewBox='0 0 24 24'><circle cx='12' cy='12' r='9'/><path d='M12 12 16 8M12 12h.01'/></symbol>
<symbol id='i-acc' viewBox='0 0 24 24'><path d='M3 12h4l3-7 4 14 3-7h4'/></symbol>
<symbol id='i-compass' viewBox='0 0 24 24'><circle cx='12' cy='12' r='10'/><polygon points='16.24 7.76 14.12 14.12 7.76 16.24 9.88 9.88 16.24 7.76'/></symbol>
<symbol id='i-batt' viewBox='0 0 24 24'><rect x='2' y='7' width='16' height='10' rx='2'/><path d='M21 10v4'/></symbol>
<symbol id='i-shield' viewBox='0 0 24 24'><path d='M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6l7-3Z'/></symbol>
<symbol id='i-clock' viewBox='0 0 24 24'><circle cx='12' cy='12' r='9'/><path d='M12 7v5l3 2'/></symbol>
<symbol id='i-wifi' viewBox='0 0 24 24'><path d='M2 8.5a15 15 0 0 1 20 0M5.5 12a10 10 0 0 1 13 0M9 15.5a5 5 0 0 1 6 0M12 19h.01'/></symbol>
<symbol id='i-cog' viewBox='0 0 24 24'><circle cx='12' cy='12' r='3'/><path d='M12 2v3M12 19v3M2 12h3M19 12h3M4.9 4.9l2.1 2.1M17 17l2.1 2.1M19.1 4.9 17 7M7 17l-2.1 2.1'/></symbol>
<symbol id='i-refresh' viewBox='0 0 24 24'><path d='M20 11A8 8 0 0 0 6.3 6.3L4 8.5M4 5v4h4M4 13a8 8 0 0 0 13.7 4.7L20 15.5M20 19v-4h-4'/></symbol>
<symbol id='i-down' viewBox='0 0 24 24'><path d='M12 4v11M7.5 11 12 15.5 16.5 11M5 19h14'/></symbol>
<symbol id='i-warn' viewBox='0 0 24 24'><path d='M12 4 2.5 20h19L12 4ZM12 10v4M12 17h.01'/></symbol>
<symbol id='i-check' viewBox='0 0 24 24'><path d='M4 12.5 9.5 18 20 6'/></symbol>
<symbol id='i-moon' viewBox='0 0 24 24'><path d='M20 14A8.5 8.5 0 0 1 10 4a8.5 8.5 0 1 0 10 10Z'/></symbol>
<symbol id='i-sun' viewBox='0 0 24 24'><circle cx='12' cy='12' r='4'/><path d='M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M19.1 4.9l-1.4 1.4M6.3 17.7l-1.4 1.4'/></symbol>
<symbol id='i-pause' viewBox='0 0 24 24'><path d='M9 5v14M15 5v14'/></symbol>
<symbol id='i-play' viewBox='0 0 24 24'><path d='M7 4l12 8-12 8V4Z'/></symbol>
<symbol id='i-chart' viewBox='0 0 24 24'><path d='M4 19h16M7 16V9M12 16V5M17 16v-5'/></symbol>
</defs></svg>

<div class='wrap'>
<header>
 <div class='brand'><div class='logo'><svg width='20' height='20' stroke='currentColor' fill='none' stroke-width='1.7'><use href='#i-shield'/></svg></div>
  <div><h1>TransitGuard</h1><p class='sub'>Smart Cargo Monitoring System</p></div></div>
 <div class='chips'>
  <span class='chip'><i class='dot' id='dLink'></i><span id='cLink'>Connecting</span></span>
  <span class='chip'><svg aria-hidden='true'><use href='#i-clock'/></svg><span id='cTime'>--:--</span></span>
  <span class='chip'><svg aria-hidden='true'><use href='#i-cog'/></svg><span id='cMode'>Wi-Fi dashboard</span></span>
  <span class='chip'><svg aria-hidden='true'><use href='#i-batt'/></svg><span id='cBatt'>--%</span></span>
  <span class='chip'><svg aria-hidden='true'><use href='#i-wifi'/></svg><span id='cRtt'>-- ms</span></span>
  <button class='iconbtn' id='btnPause' title='Pause live updates (P)' aria-label='Pause live updates'><svg aria-hidden='true'><use href='#i-pause' id='pauseIcon'/></svg></button>
  <button class='iconbtn' id='btnTheme' title='Toggle light / dark theme (T)' aria-label='Toggle light or dark theme'><svg aria-hidden='true'><use href='#i-sun' id='themeIcon'/></svg></button>
 </div>
</header>

<main id='main'>
<div class='banner' id='banner' role='status' aria-live='polite'>
 <div><h2 id='bTitle'><svg width='22' height='22' stroke='currentColor' fill='none' stroke-width='1.8' aria-hidden='true'><use href='#i-clock' id='bIcon'/></svg><span id='bTitleTx'>INITIALISING</span></h2>
  <p id='bSub'>Contacting device&hellip;</p></div>
 <div style='text-align:right'><div class='meta'>Events this transit</div>
  <div class='val' id='bEvents' style='margin-top:0'>--</div></div>
</div>

<div class='grid'>
 <div class='card' id='cardOri'><h3><svg aria-hidden='true'><use href='#i-compass'/></svg> Orientation / Tilt</h3>
  <div class='val' id='vOri' style='font-size:1.35rem'>--</div>
  <div class='meta'>Pitch: <b id='vPitch' style='color:var(--tx)'>--</b>&deg; &middot; Roll: <b id='vRoll' style='color:var(--tx)'>--</b>&deg;</div>
  <span class='tag ok' id='tOri'>Leveled</span>
  <div class='btns' style='margin-top:8px'><button id='btnCalib' class='primary' style='padding:6px 10px;font-size:0.75rem'><svg aria-hidden='true'><use href='#i-check'/></svg> Zero Level</button></div></div>

 <div class='card' id='cardAcc'><h3><svg aria-hidden='true'><use href='#i-acc'/></svg> Dynamic Shock &amp; Axes</h3>
  <div class='val'><span id='vAcc'>--</span><small>g</small></div>
  <div class='meta'>X: <span id='vAx'>--</span> &middot; Y: <span id='vAy'>--</span> &middot; Z: <span id='vAz'>--</span> g</div>
  <svg class='spark' id='spAcc' viewBox='0 0 100 34' preserveAspectRatio='none' aria-hidden='true'></svg>
  <span class='tag' id='tAcc'>--</span></div>

 <div class='card' id='cardTemp'><h3><svg aria-hidden='true'><use href='#i-temp'/></svg> Temperature</h3>
  <div class='val'><span id='vTemp'>--</span><small>&deg;C</small></div>
  <div class='meta'>Threshold <span id='thTemp'>--</span> &deg;C &middot; min <span id='mnTemp'>--</span> / max <span id='mxTemp'>--</span></div>
  <svg class='spark' id='spTemp' viewBox='0 0 100 34' preserveAspectRatio='none' aria-hidden='true'></svg>
  <span class='tag' id='tTemp'>--</span></div>

 <div class='card' id='cardLux'><h3><svg aria-hidden='true'><use href='#i-light'/></svg> Ambient light</h3>
  <div class='val'><span id='vLux'>--</span><small>lx</small></div>
  <div class='meta'>Threshold <span id='thLux'>--</span> lx &middot; max <span id='mxLux'>--</span></div>
  <svg class='spark' id='spLux' viewBox='0 0 100 34' preserveAspectRatio='none' aria-hidden='true'></svg>
  <span class='tag' id='tLux'>--</span></div>

 <div class='card' id='cardPres'><h3><svg aria-hidden='true'><use href='#i-pres'/></svg> Pressure</h3>
  <div class='val'><span id='vPres'>--</span><small>hPa</small></div>
  <div class='meta'>Est. altitude <span id='vAlt'>--</span> m</div>
  <svg class='spark' id='spPres' viewBox='0 0 100 34' preserveAspectRatio='none' aria-hidden='true'></svg>
  <span class='tag info'>Barometric</span></div>

 <div class='card' id='cardBatt'><h3><svg aria-hidden='true'><use href='#i-batt'/></svg> Battery</h3>
  <div class='val'><span id='vBattP'>--</span><small>%</small></div>
  <div class='meta'><span id='vBattV'>--</span> V &middot; <span id='battTrend'>trend --</span></div>
  <div class='bar'><i id='battBar'></i></div></div>

 <div class='card' id='cardTamp'><h3><svg aria-hidden='true'><use href='#i-shield'/></svg> Tamper seal</h3>
  <div class='val' id='vTamp' style='font-size:1.25rem'>--</div>
  <div class='meta'>Tamper-evidence foil latch</div>
  <span class='tag' id='tTamp'>--</span>
  <div class='btns hidden' id='tampAck'><button class='danger' id='btnTamper'><svg aria-hidden='true'><use href='#i-check'/></svg> Acknowledge &amp; clear</button></div></div>
</div>

<div class='section'><h2>Recent events</h2>
 <div class='panel'>
  <div class='toolbar'>
   <button id='btnEvents'><svg aria-hidden='true'><use href='#i-refresh'/></svg> Load event log</button>
   <label class='sr' for='evtSearch'>Filter event log</label>
   <input type='search' id='evtSearch' placeholder='Filter rows&hellip;' autocomplete='off'>
   <button id='btnExport' disabled><svg aria-hidden='true'><use href='#i-down'/></svg> Export filtered CSV</button>
   <a class='btn' href='/download' id='btnCsv' download><svg aria-hidden='true'><use href='#i-down'/></svg> Raw CSV</a>
   <button class='danger' id='btnReset'><svg aria-hidden='true'><use href='#i-warn'/></svg> Reset for next transit</button>
  </div>
  <div class='evt' id='evtBox'><div class='empty'>Event log not loaded yet &mdash; press &ldquo;Load event log&rdquo;.</div></div>
  <p class='note' id='evtNote'>Rows are read directly from the on-device CSV log (<code>/download</code>); the newest entries are shown first. Click a column header to sort.</p>
 </div>
</div>

<div class='section'><h2>Configuration</h2>
<div class='cols'>
 <div class='panel'>
  <form id='cfg' method='POST' action='/settings' novalidate>
  <fieldset><legend>Directional Shock (Multiples of g)</legend>
   <div style='display:flex;gap:10px;'>
     <div style='flex:1'><label for='inAx'>X Threshold (g)<input type='number' step='0.1' name='th_ax' id='inAx' value='2.0'></label></div>
     <div style='flex:1'><label for='inAy'>Y Threshold (g)<input type='number' step='0.1' name='th_ay' id='inAy' value='2.0'></label></div>
     <div style='flex:1'><label for='inAz'>Z Threshold (g)<input type='number' step='0.1' name='th_az' id='inAz' value='2.5'></label></div>
   </div>
   <label for='inMot'>Hardware Motion Sensitivity (Wakeup Engine)
    <span class='hint'>1 = ultra sensitive, 255 = insensitive (recommended: 20-40).</span>
    <input type='number' step='1' min='1' max='255' name='motion' id='inMot' value='30' aria-describedby='eMot'></label>
    <span class='err' id='eMot'></span>
  </fieldset>

  <fieldset><legend>Tilt Limits (Degrees)</legend>
   <div style='display:flex;gap:10px;'>
     <div style='flex:1'><label for='inPitch'>Max Pitch (&plusmn;&deg;)<input type='number' step='1' name='th_pitch' id='inPitch' value='45'></label></div>
     <div style='flex:1'><label for='inRoll'>Max Roll (&plusmn;&deg;)<input type='number' step='1' name='th_roll' id='inRoll' value='45'></label></div>
   </div>
  </fieldset>

  <fieldset><legend>Environment &amp; Detection</legend>
   <label for='inLux'>Ambient light threshold
    <span class='hint'>Lux level that flags enclosure opening.</span>
    <input type='number' step='1' min='0' name='lux' id='inLux' value='50' aria-describedby='eLux'></label>
    <span class='err' id='eLux'></span>
   <label for='inTemp'>Temperature threshold
    <span class='hint'>Upper limit in &deg;C for cargo.</span>
    <input type='number' step='0.1' name='temp' id='inTemp' value='45.0' aria-describedby='eTemp'></label>
    <span class='err' id='eTemp'></span>
  </fieldset>

  <fieldset><legend>Logging</legend>
   <label for='inInt'>Logging interval (seconds)
    <span class='hint'>Seconds between routine readings.</span>
    <input type='number' step='1' name='logintvl' id='inInt' min='10' max='86400' value='60' aria-describedby='eInt'></label>
    <span class='err' id='eInt'></span>
  </fieldset>

  <div class='btns'>
   <button type='submit' class='primary'><svg aria-hidden='true'><use href='#i-check'/></svg> Save &amp; start logging</button>
   <button type='button' id='btnSync'><svg aria-hidden='true'><use href='#i-clock'/></svg> Sync time from browser</button>
  </div>
  <p class='note'>Saving drops the Wi-Fi dashboard and arms deep-sleep logging mode immediately.</p>
  </form>
 </div>

 <div class='panel'>
  <h3 style='margin:0 0 6px;font-size:.7rem;letter-spacing:1.4px;text-transform:uppercase;color:var(--tx3)'>Power</h3>
  <p class='note' style='margin-top:0'>Drops the device back into low-power deep-sleep mode.</p>
  <div class='btns'><button class='amber' id='btnSleep'><svg aria-hidden='true'><use href='#i-moon'/></svg> Go to sleep now</button></div>
  <hr style='border:none;border-top:1px solid var(--line);margin:16px 0'>
  <h3 style='margin:0 0 6px;font-size:.7rem;letter-spacing:1.4px;text-transform:uppercase;color:var(--tx3)'>Link</h3>
  <p class='note' style='margin-top:0'>Reachable at <code>http://transitlogger.local</code>.</p>
  <label for='inPoll' style='margin-top:10px'>Refresh interval
   <span class='hint'>Poll rate for <code>/status</code>.</span>
   <input type='number' id='inPoll' min='1' max='60' step='1' value='2'></label>
  <p class='note'>Last update: <span id='lastUpd'>never</span> <span id='lastRel'></span></p>
  <p class='note'>Uptime: <span id='sessUp'>0s</span> &middot; polls <span id='pollCount'>0</span> &middot; errors <span id='errCount'>0</span></p>
  <details style='margin-top:10px'><summary class='note' style='cursor:pointer'>Keyboard shortcuts</summary>
   <p class='note'><b>R</b> reload events &middot; <b>P</b> pause/resume &middot; <b>T</b> theme &middot;
   <b>S</b> focus settings &middot; <b>Esc</b> close dialog</p></details>
 </div>
</div></div>
</main>

<footer>TransitGuard &middot; embedded dashboard served locally &middot; no internet required</footer>
</div>
<div id='toasts' role='region' aria-live='polite' aria-label='Notifications'></div>
<div id='modal' class='hidden' role='dialog' aria-modal='true' aria-labelledby='mTitle' aria-describedby='mText'>
 <div class='dlg'>
 <h4 id='mTitle'>Confirm</h4><p id='mText'></p>
 <div class='btns'><button id='mYes' class='danger'>Continue</button><button id='mNo'>Cancel</button></div>
</div></div>
<script>
var $=function(i){return document.getElementById(i)};
var LS='tg.';
function store(k,v){try{localStorage.setItem(LS+k,v)}catch(e){}}
function load(k,d){try{var v=localStorage.getItem(LS+k);return v===null?d:v}catch(e){return d}}

/* ---------- theme ---------- */
function setTheme(t){document.documentElement.setAttribute('data-theme',t);store('theme',t);
 $('themeIcon').setAttribute('href',t==='dark'?'#i-sun':'#i-moon');}
setTheme(load('theme',window.matchMedia&&window.matchMedia('(prefers-color-scheme: light)').matches?'light':'dark'));
$('btnTheme').onclick=function(){setTheme(document.documentElement.getAttribute('data-theme')==='dark'?'light':'dark');};

/* ---------- toasts ---------- */
function toast(msg,kind,action){
 var t=document.createElement('div');t.className='toast '+(kind||'info');
 var s=document.createElement('span');s.textContent=msg;t.appendChild(s);
 if(action){var b=document.createElement('button');b.textContent=action.label;
  b.onclick=function(){t.remove();action.fn();};t.appendChild(b);}
 $('toasts').appendChild(t);
 setTimeout(function(){if(t.parentNode)t.remove()},action?9000:4200);
 return t;
}

/* ---------- modal ---------- */
var pending=null,lastFocus=null;
function confirmAction(text,fn){
 pending=fn;lastFocus=document.activeElement;
 $('mText').textContent=text;$('modal').classList.remove('hidden');$('mYes').focus();
}
function closeModal(){$('modal').classList.add('hidden');pending=null;if(lastFocus)lastFocus.focus();}
$('mNo').onclick=closeModal;
$('mYes').onclick=function(){var f=pending;closeModal();if(f)f();};
$('modal').addEventListener('keydown',function(e){
 if(e.key==='Escape'){closeModal();return;}
 if(e.key!=='Tab')return;
 var f=[$('mYes'),$('mNo')];var i=f.indexOf(document.activeElement);
 e.preventDefault();f[(i+(e.shiftKey?f.length-1:1))%f.length].focus();
});

function flash(id){var c=$(id);if(!c)return;c.classList.add('flash');
 setTimeout(function(){c.classList.remove('flash')},420);}
function num(v){var n=parseFloat(v);return isNaN(n)?null:n;}
function setTag(el,cls,txt){el.className='tag '+cls;el.textContent=txt;}
function esc(s){return String(s).replace(/[&<>"]/g,function(c){
 return c==='&'?'&amp;':c==='<'?'&lt;':c==='>'?'&gt;':'&quot;';});}

/* ---------- network with hard abort timeout ---------- */
function req(url,opts,ms){
 opts=opts||{};ms=ms||2500;
 if(typeof AbortController==='function'){
  var ac=new AbortController();opts.signal=ac.signal;
  var to=setTimeout(function(){ac.abort()},ms);
  return fetch(url,opts).then(function(r){clearTimeout(to);return r;},
   function(e){clearTimeout(to);throw new Error('Request timed out');});
 }
 return fetch(url,opts);
}
function post(url,body){
 return req(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
  body:body||''},4000).then(function(r){
   if(!r.ok&&r.status!==303)throw new Error('HTTP '+r.status);return r;});
}

/* ---------- history / sparklines ---------- */
var HIST_MAX=60;
var hist=JSON.parse(load('hist','{}')||'{}');
['temp','lux','pres','acc','batt'].forEach(function(k){if(!Array.isArray(hist[k]))hist[k]=[];});
var extremes=JSON.parse(load('ext','{}')||'{}');
function pushHist(k,v){if(v===null)return;hist[k].push(v);if(hist[k].length>HIST_MAX)hist[k].shift();}
function saveHist(){store('hist',JSON.stringify(hist));store('ext',JSON.stringify(extremes));}
function track(k,v){if(v===null)return;
 if(extremes[k+'Min']===undefined||v<extremes[k+'Min'])extremes[k+'Min']=v;
 if(extremes[k+'Max']===undefined||v>extremes[k+'Max'])extremes[k+'Max']=v;}
function spark(id,arr,color){
 var el=$(id);if(!el)return;
 if(arr.length<2){el.innerHTML='';return;}
 var mn=Math.min.apply(null,arr),mx=Math.max.apply(null,arr),rg=(mx-mn)||1;
 var W=100,H=34,pts=[];
 for(var i=0;i<arr.length;i++){
  var x=(i/(arr.length-1))*W, y=H-2-((arr[i]-mn)/rg)*(H-6);
  pts.push(x.toFixed(2)+','+y.toFixed(2));
 }
 var d='M'+pts.join(' L');
 el.innerHTML="<path class='area' d='"+d+' L'+W+','+H+' L0,'+H+" Z'/><path d='"+d+"' style='stroke:"+(color||'var(--info)')+"'/>";
}

/* ---------- telemetry render ---------- */
var lastGood=null,lastGoodAt=0;
function render(j,stale){
 var thLux=num($('inLux').value),thTemp=num($('inTemp').value),warn=false,crit=false;

 var t=num(j.tempC);$('vTemp').textContent=(t===null?'--':t.toFixed(1));
 $('thTemp').textContent=(thTemp===null?'--':thTemp);
 track('temp',t);
 $('mnTemp').textContent=extremes.tempMin===undefined?'--':extremes.tempMin.toFixed(1);
 $('mxTemp').textContent=extremes.tempMax===undefined?'--':extremes.tempMax.toFixed(1);
 if(t!==null&&thTemp!==null){if(t>thTemp){setTag($('tTemp'),'crit','Above threshold');crit=true;}
  else if(t>thTemp-2){setTag($('tTemp'),'warn','Near threshold');warn=true;}
  else setTag($('tTemp'),'ok','Within limits');}

 var lx=num(j.lux);$('vLux').textContent=(lx===null?'--':lx.toFixed(0));
 $('thLux').textContent=(thLux===null?'--':thLux);
 track('lux',lx);
 $('mxLux').textContent=extremes.luxMax===undefined?'--':extremes.luxMax.toFixed(0);
 if(lx!==null&&thLux!==null){if(lx>thLux){setTag($('tLux'),'crit','Light ingress');crit=true;}
  else setTag($('tLux'),'ok','Enclosure dark');}

 var p=num(j.pressureHPa);$('vPres').textContent=(p===null?'--':p.toFixed(0));
 var a=num(j.altitudeM);$('vAlt').textContent=(a===null?'--':a.toFixed(1));

 var g=num(j.accelG);$('vAcc').textContent=(g===null?'--':g.toFixed(2));
 $('vAx').textContent=(num(j.accelX)===null?'--':num(j.accelX).toFixed(2));
 $('vAy').textContent=(num(j.accelY)===null?'--':num(j.accelY).toFixed(2));
 $('vAz').textContent=(num(j.accelZ)===null?'--':num(j.accelZ).toFixed(2));
 track('acc',g);
 if(g!==null){if(g>1.8){setTag($('tAcc'),'crit','Shock event');crit=true;}
  else if(g>1.2){setTag($('tAcc'),'warn','Motion');warn=true;}
  else setTag($('tAcc'),'ok','Stable');}

 $('vOri').textContent=j.orientation||'--';
 $('vPitch').textContent=num(j.pitch)===null?'--':num(j.pitch).toFixed(1);
 $('vRoll').textContent=num(j.roll)===null?'--':num(j.roll).toFixed(1);

 var bp=num(j.battPct);$('vBattP').textContent=(bp===null?'--':bp.toFixed(0));
 $('vBattV').textContent=(num(j.battV)===null?'--':num(j.battV).toFixed(2));
 $('cBatt').textContent=(bp===null?'--':bp.toFixed(0))+'%';
 if(bp!==null){var bb=$('battBar');bb.style.width=Math.max(0,Math.min(100,bp))+'%';
  bb.style.background=bp<15?'var(--crit)':(bp<35?'var(--warn)':'var(--ok)');
  var prev=hist.batt.length?hist.batt[0]:null;
  if(prev!==null&&hist.batt.length>3){var d=bp-prev;
   $('battTrend').textContent='trend '+(d>0.5?'\u2191':(d<-0.5?'\u2193':'\u2192'))+' '+d.toFixed(1)+'%';}
  if(bp<15)crit=true;else if(bp<35)warn=true;}

 var tam=(j.tamper===true||j.tamper==='true');
 $('vTamp').textContent=tam?'TAMPER DETECTED':'SEAL INTACT';
 setTag($('tTamp'),tam?'crit':'ok',tam?'Action required':'Safe');
 $('tampAck').classList.toggle('hidden',!tam);
 if(tam)crit=true;

 $('cTime').textContent=j.time||'--';
 $('bEvents').textContent=(j.events===undefined?'--':j.events);

 if(!stale){
  pushHist('temp',t);pushHist('lux',lx);pushHist('pres',p);pushHist('acc',g);pushHist('batt',bp);
  saveHist();
 }
 spark('spTemp',hist.temp,crit?'var(--crit)':'var(--info)');
 spark('spLux',hist.lux);spark('spPres',hist.pres);
 spark('spAcc',hist.acc,'var(--warn)');

 var b=$('banner');
 b.className='banner '+(crit?'crit':(warn?'warn':'ok'))+(stale?' stale-overlay':'');
 $('bTitleTx').textContent=(crit?'CRITICAL':(warn?'WARNING':'SAFE'))+(stale?' (last known)':'');
 $('bIcon').setAttribute('href',crit?'#i-warn':(warn?'#i-warn':'#i-check'));
 $('bSub').textContent=stale?'Device unreachable \u2014 showing cached data.'
  :(crit?'One or more parameters exceed safe thresholds.'
  :(warn?'A monitored parameter is approaching threshold.'
  :'All monitored cargo parameters are nominal.'));

 if(!stale)['cardTemp','cardLux','cardPres','cardAcc','cardBatt','cardTamp','cardOri'].forEach(flash);
 if(!stale){lastGoodAt=Date.now();store('last',JSON.stringify(j));
  $('lastUpd').textContent=new Date().toLocaleTimeString();}
}

/* ---------- fast polling + watchdog ---------- */
var fails=0,paused=false,timer=null,pollMs=parseInt(load('pollMs','2000'),10)||2000,
    polls=0,errs=0,inFlight=false,lastReqSent=0;

$('inPoll').value=Math.round(pollMs/1000);
$('inPoll').onchange=function(){
 var s=Math.min(60,Math.max(1,parseInt(this.value,10)||2));
 this.value=s;pollMs=s*1000;store('pollMs',pollMs);schedule(0);
};

function link(state,txt){$('dLink').className='dot '+state+(state==='ok'&&!paused?' live':'');
 $('cLink').textContent=txt;}

function schedule(ms){
 if(timer)clearTimeout(timer);
 if(paused)return;
 timer=setTimeout(poll,ms===undefined?pollMs:ms);
}

function poll(){
 if(paused||inFlight)return;
 inFlight=true;
 lastReqSent=Date.now();
 var t0=Date.now();

 req('/status',{cache:'no-store'},2500)
  .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
  .then(function(j){
   inFlight=false;
   var rtt=Date.now()-t0;polls++;
   $('cRtt').textContent=rtt+' ms';
   $('pollCount').textContent=polls;
   fails=0;lastGood=j;
   link('ok','Connected');
   render(j,false);
   schedule();
  })
  .catch(function(e){
   inFlight=false;
   fails++;errs++;$('errCount').textContent=errs;
   link(fails>2?'crit':'warn',fails>2?'Unreachable':'Reconnecting\u2026');
   var cached=lastGood||JSON.parse(load('last','null')||'null');
   if(cached)render(cached,true);
   schedule(Math.min(5000,pollMs*(fails>2?2:1)));
  });
}

/* Hard watchdog: un-sticks any stalled request after 3.5s */
setInterval(function(){
 if(!paused&&inFlight&&Date.now()-lastReqSent>3500){
  inFlight=false;
  fails++;
  link('warn','Retrying...');
  poll();
 }
},1000);

$('btnPause').onclick=function(){
 paused=!paused;
 $('pauseIcon').setAttribute('href',paused?'#i-play':'#i-pause');
 if(paused){if(timer)clearTimeout(timer);inFlight=false;link('warn','Paused');}
 else{fails=0;schedule(0);}
};

/* ---------- event log ---------- */
var evtHead=null,evtRows=[],sortCol=-1,sortDir=1;
function iconFor(line){var s=line.toLowerCase();
 if(s.indexOf('tamper')>=0)return 'i-shield';
 if(s.indexOf('motion')>=0||s.indexOf('shock')>=0||s.indexOf('accel')>=0)return 'i-acc';
 if(s.indexOf('temp')>=0)return 'i-temp';
 if(s.indexOf('lux')>=0||s.indexOf('light')>=0)return 'i-light';
 if(s.indexOf('batt')>=0||s.indexOf('low')>=0)return 'i-batt';
 return 'i-chart';}

function renderEvents(){
 var q=$('evtSearch').value.trim().toLowerCase();
 var rows=evtRows.filter(function(r){return !q||r.join(',').toLowerCase().indexOf(q)>=0;});
 if(sortCol>=0){
  rows=rows.slice().sort(function(a,b){
   var x=a[sortCol]||'',y=b[sortCol]||'';
   var nx=parseFloat(x),ny=parseFloat(y);
   if(!isNaN(nx)&&!isNaN(ny))return (nx-ny)*sortDir;
   return x.localeCompare(y)*sortDir;});
 }
 if(!rows.length){$('evtBox').innerHTML="<div class='empty'>No rows match \u201C"+esc(q)+"\u201D.</div>";
  $('btnExport').disabled=true;return;}
 var h="<table><thead><tr><th scope='col'><span class='sr'>Type</span></th>";
 var cols=evtHead||['Entry'];
 for(var i=0;i<cols.length;i++)
  h+="<th scope='col' tabindex='0' data-c='"+i+"'"+(sortCol===i?" aria-sort='"+(sortDir>0?'ascending':'descending')+"'":"")+">"+esc(cols[i])+"</th>";
 h+='</tr></thead><tbody>';
 for(var r2=0;r2<rows.length;r2++){
  var c=rows[r2];
  h+="<tr><td class='ico'><svg aria-hidden='true'><use href='#"+iconFor(c.join(','))+"'/></svg></td>";
  for(var k=0;k<c.length;k++)h+="<td class='"+(k===0?'k':'')+"'>"+esc(c[k])+"</td>";
  h+="</tr>";}
 h+='</tbody></table>';
 $('evtBox').innerHTML=h;
 $('btnExport').disabled=false;
 $('evtNote').textContent='Showing '+rows.length+' logged entries (newest first). Click header to sort.';
 Array.prototype.forEach.call($('evtBox').querySelectorAll('th[data-c]'),function(th){
  function go(){var c=+th.getAttribute('data-c');
   if(sortCol===c)sortDir=-sortDir;else{sortCol=c;sortDir=1;}renderEvents();}
  th.onclick=go;
  th.onkeydown=function(e){if(e.key==='Enter'||e.key===' '){e.preventDefault();go();}};
 });
}

function loadEvents(){
 var b=$('btnEvents');b.disabled=true;b.innerHTML="<i class='spin'></i> Loading";
 req('/download',{cache:'no-store'},6000).then(function(r){
  if(r.status===404)throw new Error('No log file on device.');
  if(!r.ok)throw new Error('HTTP '+r.status);return r.text();})
 .then(function(txt){
  var rows=txt.split(/\r?\n/).filter(function(l){return l.trim().length});
  if(!rows.length){$('evtBox').innerHTML="<div class='empty'>Log is empty.</div>";
   evtRows=[];$('btnExport').disabled=true;return;}
  var head=rows[0].split(','),body=rows.slice(1);
  evtHead=head;
  evtRows=body.slice(-500).reverse().map(function(l){return l.split(',');});
  sortCol=-1;sortDir=1;
  renderEvents();})
 .catch(function(e){$('evtBox').innerHTML="<div class='empty'>"+esc(e.message)+"</div>";})
 .then(function(){b.disabled=false;b.innerHTML="<svg aria-hidden='true'><use href='#i-refresh'/></svg> Load event log";});
}
$('btnEvents').onclick=loadEvents;
$('evtSearch').oninput=function(){renderEvents();};

$('btnExport').onclick=function(){
 var q=$('evtSearch').value.trim().toLowerCase();
 var rows=evtRows.filter(function(r){return !q||r.join(',').toLowerCase().indexOf(q)>=0;});
 var csv=(evtHead?evtHead.join(',')+'\n':'')+rows.map(function(r){return r.join(',')}).join('\n');
 var url=URL.createObjectURL(new Blob([csv],{type:'text/csv'}));
 var a=document.createElement('a');a.href=url;
 a.download='transitguard_'+new Date().toISOString().slice(0,19).replace(/[:T]/g,'-')+'.csv';
 document.body.appendChild(a);a.click();a.remove();
};

/* ---------- actions ---------- */
$('btnCalib').onclick=function(){
 confirmAction('Zero-calibrate level orientation and accelerometer baseline now? Ensure unit is resting flat.',function(){
  post('/calibrate').then(function(r){return r.text();}).then(function(msg){
   toast(msg,'ok');fails=0;schedule(0);
  }).catch(function(e){toast('Calibration failed: '+e.message,'err');});
 });
};

$('btnReset').onclick=function(){
 confirmAction('Clear all logged transit data on device?',function(){
  post('/reset').then(function(){toast('Log cleared.','ok');
   evtRows=[];$('btnExport').disabled=true;
   $('evtBox').innerHTML="<div class='empty'>Event log cleared.</div>";
   hist={temp:[],lux:[],pres:[],acc:[],batt:[]};extremes={};saveHist();
   fails=0;schedule(0);}).catch(function(e){toast('Reset failed: '+e.message,'err');});
 });
};

$('btnTamper').onclick=function(){
 confirmAction('Acknowledge and reset tamper state?',function(){
  post('/tamper/clear').then(function(){toast('Tamper cleared.','ok');fails=0;schedule(0);})
  .catch(function(e){toast('Failed: '+e.message,'err');});
 });
};

$('btnSleep').onclick=function(){
 confirmAction('Drop into low power logging mode now?',function(){
  post('/sleep').then(function(){toast('Entering low power logging.','ok');})
  .catch(function(){toast('Sleep armed.','info');});
 });
};

$('btnSync').onclick=function(){
 var d=new Date(),epoch=Math.floor(d.getTime()/1000),tz=-d.getTimezoneOffset()*60;
 req('/synctime',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
  body:'epoch='+epoch+'&tz='+tz},4000).then(function(r){return r.text();})
 .then(function(m){toast(m||'Time synchronised.','ok');fails=0;schedule(0);})
 .catch(function(e){toast('Sync failed: '+e.message,'err');});
};

$('cfg').addEventListener('submit',function(ev){
 ev.preventDefault();
 confirmAction('Save settings and start low-power transit logging mode?',function(){
  var parts=['lux='+encodeURIComponent($('inLux').value),
             'temp='+encodeURIComponent($('inTemp').value),
             'motion='+encodeURIComponent($('inMot').value),
             'logintvl='+encodeURIComponent($('inInt').value),
             'th_ax='+encodeURIComponent($('inAx').value),
             'th_ay='+encodeURIComponent($('inAy').value),
             'th_az='+encodeURIComponent($('inAz').value),
             'th_pitch='+encodeURIComponent($('inPitch').value),
             'th_roll='+encodeURIComponent($('inRoll').value)];
  post('/settings',parts.join('&'))
   .then(function(){toast('Saved! Entering deep-sleep logging mode...','ok');})
   .catch(function(e){toast('Save error: '+e.message,'err');});
 });
});

/* ---------- IMMEDIATE STARTUP SEQUENCE ---------- */
window.addEventListener('DOMContentLoaded', function(){
 link('warn', 'Connecting...');
 poll();
 setTimeout(loadEvents, 1200);
});
</script></body></html>
)rawliteral";

/* ===================================================================== */
void WebUiServer::start() {
    const DeviceSettings &s = Settings.get();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(s.apSsid, s.apPass);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/status", HTTP_GET, handleStatusJson);
    server.on("/settings", HTTP_GET, handleSettingsGet);
    server.on("/settings", HTTP_POST, handleSettingsPost);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/synctime", HTTP_POST, handleSyncTime);
    server.on("/calibrate", HTTP_POST, handleCalibrate);
    server.on("/tamper/clear", HTTP_POST, handleTamperClear);
    server.on("/sleep", HTTP_POST, handleSleep);
    server.onNotFound(handleNotFound);

    server.begin();
    startDns();
    startMdns();
    _running = true;

    Serial.print(F("[WebUi] AP started, IP="));
    Serial.println(WiFi.softAPIP());
}

void WebUiServer::stop() {
    stopMdns();
    stopDns();
    server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    _running = false;
}

void WebUiServer::handle() {
    if (!_running) return;
    dnsServer.processNextRequest();
    server.handleClient();
}

void WebUiServer::startDns() {
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
}

void WebUiServer::stopDns() {
    dnsServer.stop();
}

void WebUiServer::startMdns() {
    if (MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", WEBSERVER_PORT);
    }
}

void WebUiServer::stopMdns() {
    MDNS.end();
}

uint8_t WebUiServer::connectedClients() const {
    return WiFi.softAPgetStationNum();
}

bool WebUiServer::consumeSleepRequest() {
    if (!s_sleepRequested || (millis() - s_sleepRequestTimestamp < 350)) {
        return false;
    }
    s_sleepRequested = false;
    return true;
}

void WebUiServer::handleRoot() {
    server.send_P(200, "text/html", INDEX_HTML);
}

void WebUiServer::handleStatusJson() {
    SensorReadings r = Sensors.readAll();
    String json = "{";
    json += "\"lux\":" + String(r.ambientLux, 0) + ",";
    json += "\"battPct\":" + String(r.batteryPercent) + ",";
    json += "\"battV\":" + String(r.batteryVoltage, 2) + ",";
    json += "\"tempC\":" + String(r.temperatureC, 1) + ",";
    json += "\"pressureHPa\":" + String(r.pressureHPa, 0) + ",";
    json += "\"altitudeM\":" + String(r.altitudeM, 1) + ",";
    json += "\"accelX\":" + String(r.accelX, 2) + ",";
    json += "\"accelY\":" + String(r.accelY, 2) + ",";
    json += "\"accelZ\":" + String(r.accelZ, 2) + ",";
    json += "\"accelG\":" + String(r.accelMagnitude_g, 2) + ",";
    json += "\"pitch\":" + String(r.pitchDeg, 1) + ",";
    json += "\"roll\":" + String(r.rollDeg, 1) + ",";
    json += "\"orientation\":\"" + String(r.orientation) + "\",";
    json += "\"time\":\"" + RtcTime.nowFormatted() + "\",";
    json += "\"events\":" + String((unsigned)EventLog.entryCount()) + ",";
    json += "\"tamper\":" + String(Tamper.isLatched() ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

void WebUiServer::handleCalibrate() {
    Sensors.calibrateZero();
    BuzzerDev.beepShort();
    server.send(200, "text/plain", "Level orientation calibrated successfully!");
}

void WebUiServer::handleSettingsGet() {
    server.sendHeader("Location", "/");
    server.send(303);
}

void WebUiServer::handleSettingsPost() {
    float lux   = server.arg("lux").toFloat();
    float temp  = server.arg("temp").toFloat();
    Settings.setBasicThresholds(lux, temp);

    float ax    = server.hasArg("th_ax") ? server.arg("th_ax").toFloat() : 2.0f;
    float ay    = server.hasArg("th_ay") ? server.arg("th_ay").toFloat() : 2.0f;
    float az    = server.hasArg("th_az") ? server.arg("th_az").toFloat() : 2.5f;
    float pitch = server.hasArg("th_pitch") ? server.arg("th_pitch").toFloat() : 45.0f;
    float roll  = server.hasArg("th_roll") ? server.arg("th_roll").toFloat() : 45.0f;
    Settings.setDirectionalThresholds(ax, ay, az, pitch, roll);

    uint8_t motion = (uint8_t)constrain(server.arg("motion").toInt(), 1, 255);
    Settings.setMotionThreshold(motion);

    uint32_t logIntvl = (uint32_t)constrain((long)server.arg("logintvl").toInt(),
                                             (long)MIN_LOGGING_INTERVAL_SEC,
                                             (long)MAX_LOGGING_INTERVAL_SEC);
    Settings.setLoggingInterval(logIntvl);

    BuzzerDev.beepShort();

    server.send(200, "text/plain", "OK");
    s_sleepRequested = true;
    s_sleepRequestTimestamp = millis();
}

void WebUiServer::handleDownload() {
    if (!LittleFS.exists(LOG_FILE_PATH)) {
        server.send(404, "text/plain", "No log file yet");
        return;
    }
    File f = LittleFS.open(LOG_FILE_PATH, "r");
    if (!f) {
        server.send(500, "text/plain", "Failed to open log file");
        return;
    }
    server.sendHeader("Content-Disposition", "attachment; filename=transit_log.csv");
    server.streamFile(f, "text/csv");
    f.close();
}

void WebUiServer::handleReset() {
    EventLog.resetLog();
    BuzzerDev.beepShort();
    server.send(200, "text/plain", "Reset OK");
}

void WebUiServer::handleSyncTime() {
    if (server.hasArg("epoch")) {
        time_t epoch = (time_t)strtoul(server.arg("epoch").c_str(), NULL, 10);
        int32_t tzSec = server.hasArg("tz") ? server.arg("tz").toInt() : 0;
        
        if (epoch > 1000000000UL) {
            RtcTime.setEpochTime(epoch, tzSec);
            BuzzerDev.beepShort();
            server.send(200, "text/plain", "Time synced successfully!");
            return;
        }
    }
    server.send(400, "text/plain", "Invalid time data");
}

void WebUiServer::handleTamperClear() {
    Tamper.clearLatch();
    BuzzerDev.beepShort();
    server.send(200, "text/plain", "Tamper cleared");
}

void WebUiServer::handleSleep() {
    server.send(200, "text/plain", "Entering deep-sleep mode.");
    s_sleepRequested = true;
    s_sleepRequestTimestamp = millis();
}

void WebUiServer::handleNotFound() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/");
    server.send(302, "text/plain", "");
}