#ifndef STOPS_PAGE_H
#define STOPS_PAGE_H

#include <Arduino.h>

// /stops: station and weather location picker served by the setup portal.
// Talks to /api/stations, /api/search, /api/preview, /api/weather, /api/cities
// and /api/exit (see setup_portal.cpp).
static const char STOPS_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="fr"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Prochain Départ – Arrêts</title>
<style>
:root{--bg:#f4f4f2;--card:#fff;--fg:#1a1a1a;--muted:#6b6b6b;--line:#ddd;--accent:#1a1a1a;--accent-fg:#fff;--ok:#1e7b34;--err:#b3261e}
@media (prefers-color-scheme:dark){:root{--bg:#121212;--card:#1e1e1e;--fg:#eee;--muted:#9a9a9a;--line:#333;--accent:#eee;--accent-fg:#111;--ok:#6fcf86;--err:#f28b82}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.4 system-ui,-apple-system,sans-serif;padding:16px 16px 96px}
h1{font-size:1.3rem;margin:0 0 4px}
.sub{color:var(--muted);margin:0 0 16px;font-size:.9rem}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:14px;margin-bottom:14px}
.card h2{font-size:1rem;margin:0 0 8px;display:flex;gap:8px;align-items:center}
.card h2 .move{margin-left:auto;display:flex;gap:4px}.card h2 .move button{padding:4px 10px}
.num{background:var(--accent);color:var(--accent-fg);border-radius:50%;width:22px;height:22px;display:inline-flex;align-items:center;justify-content:center;font-size:.8rem}
.cur{color:var(--muted);font-size:.85rem;margin-bottom:10px;word-break:break-all}
.row{display:flex;gap:8px}
input,select{font:inherit;color:var(--fg);background:var(--bg);border:1px solid var(--line);border-radius:8px;padding:10px;width:100%;min-width:0}
button{font:inherit;border:0;border-radius:8px;padding:10px 14px;background:var(--accent);color:var(--accent-fg);cursor:pointer;white-space:nowrap}
button.ghost{background:transparent;color:var(--fg);border:1px solid var(--line)}
label{display:block;font-size:.8rem;color:var(--muted);margin:10px 0 4px}
.list{margin-top:8px;display:flex;flex-direction:column;gap:6px}
.item{display:flex;flex-direction:column;align-items:flex-start;gap:4px;text-align:left;background:var(--bg);color:var(--fg);border:1px solid var(--line);white-space:normal}
.item small{color:var(--muted)}
.badges{display:flex;flex-wrap:wrap;gap:4px}
.badge{font-size:.75rem;font-weight:700;border-radius:4px;padding:1px 6px;background:#888;color:#fff}
.msg{font-size:.85rem;color:var(--muted);margin-top:6px}
.msg.err{color:var(--err)}
details{margin-top:10px}summary{color:var(--muted);font-size:.85rem;cursor:pointer}
.bar{position:fixed;left:0;right:0;bottom:0;background:var(--card);border-top:1px solid var(--line);padding:12px 16px;display:flex;gap:8px;align-items:center}
.bar .msg{flex:1;margin:0}
a{color:inherit}
</style></head><body>
<h1>Prochain Départ</h1>
<p class="sub">Cherchez un arrêt, puis choisissez la ligne et la direction pour chaque panneau. Les grands panneaux vont dans la colonne de gauche, les compacts à droite au-dessus de la météo. <a href="/wifi">Changer de WiFi</a> · <a href="/update">Mettre à jour le logiciel</a></p>
<div id="slots"></div>
<div id="weather"></div>
<div class="bar"><span id="status" class="msg"></span><button class="ghost" id="exit">Terminer</button><button id="save">Enregistrer</button></div>
<script>
const SLOTS=4;
const TYPES=["RER","METRO","TRAM","BUS"];
let stations=[],weather=null,weatherDirty=false,dirty=false;
const $=s=>document.querySelector(s);
function el(tag,props,...kids){const e=document.createElement(tag);Object.assign(e,props||{});for(const k of kids)if(k!=null)e.append(k);return e}
// The e-ink fonts are ASCII only
const ascii=s=>s.normalize("NFD").replace(/[̀-ͯ]/g,"").replace(/[’‘]/g,"'").replace(/[^\x20-\x7e]/g,"");
const shortName=n=>n.replace(/\s*\([^)]*\)\s*$/,"");
const hex=c=>/^[0-9a-f]{6}$/i.test(c||"")?"#"+c:null;
function modeType(m){m=ascii(m||"").toLowerCase();if(/rer|train|transilien|ter/.test(m))return"RER";if(m.startsWith("metro"))return"METRO";if(m.startsWith("tram"))return"TRAM";return"BUS"}
function badge(l){const b=el("span",{className:"badge",textContent:l?l.code:"?"});if(l){if(hex(l.color))b.style.background=hex(l.color);if(hex(l.text))b.style.color=hex(l.text)}return b}
function status(t,err){const s=$("#status");s.textContent=t;s.className="msg"+(err?" err":"")}
function describe(s){return s&&s.ref?`${s.type} · ${s.ref}${s.filter?" · filtre "+s.filter:""}`:"Non configuré"}
async function api(url,opt){const r=await fetch(url,opt);let j=null;try{j=await r.json()}catch(e){}if(!r.ok)throw Object.assign(new Error((j&&j.error)||r.status),{code:r.status});return j}

function render(){
  const root=$("#slots");root.replaceChildren();
  for(let i=0;i<SLOTS;i++){
    const st=stations[i]||(stations[i]={name:"",ref:"",type:i?"BUS":"RER",filter:"",large:!i});
    const cur=el("div",{className:"cur",textContent:describe(st)});
    const name=el("input",{value:st.name,maxLength:23,placeholder:"Titre affiché",oninput:()=>{st.name=ascii(name.value);touch()}});
    const q=el("input",{type:"search",placeholder:"Nom de l'arrêt, ex. Mairie d'Ivry",enterKeyHint:"search"});
    const list=el("div",{className:"list"}),msg=el("div",{className:"msg"});
    const go=el("button",{textContent:"Chercher",onclick:()=>search(q.value,list,msg,i,refresh)});
    q.onkeydown=e=>{if(e.key==="Enter")go.click()};
    const ref=el("input",{value:st.ref,placeholder:"STIF:StopPoint:Q:12345:",oninput:()=>{st.ref=ref.value.trim();touch()}});
    const type=el("select",{onchange:()=>{st.type=type.value;touch()}},...TYPES.map(t=>el("option",{value:t,textContent:t,selected:st.type===t})));
    const filter=el("input",{value:st.filter,placeholder:"Quai (RER) ou STIF:Line::C01234:",oninput:()=>{st.filter=filter.value.trim();touch()}});
    const size=el("select",{onchange:()=>{st.large=size.value==="1";touch()}},
      el("option",{value:"1",textContent:"Grand : colonne de gauche, avec les noms des trains",selected:!!st.large}),
      el("option",{value:"0",textContent:"Compact : colonne de droite",selected:!st.large}));
    const clear=el("button",{className:"ghost",textContent:"Vider ce panneau",onclick:()=>{Object.assign(st,{name:"",ref:"",filter:""});refresh()}});
    function refresh(){cur.textContent=describe(st);name.value=st.name;ref.value=st.ref;type.value=st.type;filter.value=st.filter;size.value=st.large?"1":"0";touch()}
    const move=d=>{const j=i+d;if(j<0||j>=SLOTS)return;[stations[i],stations[j]]=[stations[j],stations[i]];render();touch()};
    root.append(el("section",{className:"card"},
      el("h2",{},el("span",{className:"num",textContent:i+1}),"Panneau "+(i+1),
        el("span",{className:"move"},el("button",{className:"ghost",textContent:"↑",title:"Monter",disabled:i===0,onclick:()=>move(-1)}),
          el("button",{className:"ghost",textContent:"↓",title:"Descendre",disabled:i===SLOTS-1,onclick:()=>move(1)}))),cur,
      el("div",{className:"row"},q,go),msg,list,
      el("label",{textContent:"Titre affiché sur l'écran"}),name,
      el("label",{textContent:"Taille du panneau"}),size,
      el("details",{},el("summary",{textContent:"Réglages avancés"}),
        el("label",{textContent:"MonitoringRef"}),ref,el("label",{textContent:"Type"}),type,
        el("label",{textContent:"Filtre"}),filter,el("div",{style:"margin-top:10px"},clear))));
  }
}
function touch(){dirty=true;status("Modifications non enregistrées")}

function renderWeather(){
  const cur=el("div",{className:"cur"}),list=el("div",{className:"list"}),msg=el("div",{className:"msg"});
  const show=()=>{cur.textContent=weather.name+(weather.lat!=null?` · ${(+weather.lat).toFixed(3)}, ${(+weather.lon).toFixed(3)}`:" · recherche par nom")};
  const name=el("input",{value:weather.name,maxLength:23,placeholder:"Nom affiché",oninput:()=>{weather.name=ascii(name.value);weatherDirty=true;show();touch()}});
  const q=el("input",{type:"search",placeholder:"Ville, ex. Vincennes",enterKeyHint:"search"});
  const go=el("button",{textContent:"Chercher",onclick:async()=>{
    const text=q.value.trim();list.replaceChildren();
    if(text.length<2){msg.textContent="Tapez au moins 2 lettres.";return}
    msg.className="msg";msg.textContent="Recherche…";
    try{
      const cities=await api("/api/cities?q="+encodeURIComponent(text));
      msg.textContent=cities.length?"Choisissez la ville :":"Aucune ville trouvée.";
      for(const c of cities)list.append(el("button",{className:"item",onclick:()=>{
          weather={name:ascii(c.name).slice(0,23),lat:c.lat,lon:c.lon};weatherDirty=true;
          name.value=weather.name;list.replaceChildren();msg.textContent="Sélectionné : "+weather.name+". Pensez à enregistrer.";show();touch()}},
        el("strong",{textContent:c.name}),el("small",{textContent:[c.state,c.country].filter(Boolean).join(", ")})));
    }catch(e){msg.className="msg err";msg.textContent=e.code===503?"Recherche indisponible : l'appareil n'est pas connecté à Internet.":"Erreur de recherche ("+e.message+")."}
  }});
  q.onkeydown=e=>{if(e.key==="Enter")go.click()};
  show();
  $("#weather").replaceChildren(el("section",{className:"card"},
    el("h2",{},el("span",{className:"num",textContent:"☀"}),"Météo"),cur,
    el("div",{className:"row"},q,go),msg,list,
    el("label",{textContent:"Nom affiché sous la météo"}),name));
}

async function search(text,list,msg,slot,refresh){
  text=text.trim();list.replaceChildren();
  if(text.length<2){msg.textContent="Tapez au moins 2 lettres.";return}
  msg.className="msg";msg.textContent="Recherche…";
  try{
    const stops=await api("/api/search?q="+encodeURIComponent(text));
    msg.textContent=stops.length?"Choisissez l'arrêt :":"Aucun arrêt trouvé.";
    for(const s of stops)list.append(el("button",{className:"item",onclick:()=>directions(s,list,msg,slot,refresh)},
      el("strong",{textContent:s.name}),el("div",{className:"badges"},...s.lines.map(badge))));
  }catch(e){msg.className="msg err";msg.textContent=e.code===503?"Recherche indisponible : l'appareil n'est pas connecté à Internet. Configurez d'abord le WiFi.":"Erreur de recherche ("+e.message+")."}
}

async function directions(stop,list,msg,slot,refresh){
  list.replaceChildren();msg.className="msg";msg.textContent="Chargement des prochains passages…";
  const area=`STIF:StopArea:SP:${stop.id}:`,lineOf=id=>stop.lines.find(l=>l.id===id);
  const pick=(ref,line,platform)=>{
    const st=stations[slot],train=platform&&modeType(line&&line.mode)==="RER";
    if(!st.ref)st.large=!!train;
    st.ref=ref;st.type=train?"RER":modeType(line&&line.mode);
    st.filter=train?platform:(line?`STIF:Line::${line.id}:`:"");
    st.name=ascii(`${line?line.code+" ":""}${shortName(stop.name)}`).slice(0,23);
    list.replaceChildren();msg.textContent="Sélectionné : "+st.name+". Pensez à enregistrer.";refresh();
  };
  let groups=[];
  try{groups=await api("/api/preview?ref="+encodeURIComponent(area))}catch(e){}
  msg.textContent=groups.length?"Choisissez la ligne et la direction :":"Aucun passage en ce moment, choisissez une ligne :";
  for(const g of groups){const l=lineOf(g.line)||{id:g.line,code:g.line};
    list.append(el("button",{className:"item",onclick:()=>pick(g.ref,l,g.platform)},
      el("div",{},badge(l)," → "+(g.dests.join(", ")||"?")),
      el("small",{textContent:`${g.platform?"Quai "+g.platform+" · ":""}${g.n} passage${g.n>1?"s":""} à venir`})));}
  if(stop.lines.length){list.append(el("div",{className:"msg",textContent:"Toutes directions :"}));
    list.append(el("div",{className:"badges"},...stop.lines.map(l=>{const b=el("button",{className:"ghost",onclick:()=>pick(area,l,"")},badge(l));b.style.padding="6px";return b})));}
}

$("#save").onclick=async()=>{
  status("Enregistrement…");
  try{await api("/api/stations",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(stations.map(s=>s.ref?{name:s.name,ref:s.ref,type:s.type,filter:s.filter,large:!!s.large}:{}))});
    if(weatherDirty){await api("/api/weather",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(weather)});weatherDirty=false}
    dirty=false;status("Enregistré ✓ Touchez « Terminer » pour revenir aux horaires.")}
  catch(e){status("Échec de l'enregistrement ("+e.message+")",true)}
};
$("#exit").onclick=async()=>{
  if(dirty&&!confirm("Quitter sans enregistrer ?"))return;
  try{await api("/api/exit",{method:"POST"})}catch(e){}
  document.body.replaceChildren(el("h1",{textContent:"C'est réglé !"}),el("p",{className:"sub",textContent:"L'écran revient aux horaires. Vous pouvez fermer cette page."}));
};
Promise.all([api("/api/stations"),api("/api/weather")]).then(([s,w])=>{stations=s;weather=w;render();renderWeather()}).catch(()=>status("Impossible de lire la configuration",true));
</script></body></html>)HTML";

#endif
