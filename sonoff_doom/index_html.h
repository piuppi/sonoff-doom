#pragma once
// Web page. After editing, rerun: python wad2sonoff.py doom1.wad (it embeds a gzip copy in doom_assets.h)
const char INDEX_HTML[] PROGMEM = R"rawliteral(<!doctype html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1"><title>SONOFF DOOM 1.0 -- By Piuppi</title>
<style>
html{min-height:100%;background:radial-gradient(ellipse at 50% 0,#1c1e24 0,#121419 50%,#0a0b0e 100%) fixed}
body{margin:0;color:#f65;font:14px monospace;display:flex;flex-direction:column;align-items:center;user-select:none;min-height:100vh}
#top{position:relative;width:100%;display:flex;justify-content:center;align-items:center;margin:14px 0 10px;min-height:36px}
#ttl{max-width:94vw;display:block;color:#f44;font:bold 22px monospace;letter-spacing:3px}
#ttl canvas{height:auto;filter:drop-shadow(0 3px 0 #2a0000)}
#top a{position:absolute;right:16px;color:#fc6;text-decoration:none;border:1px solid #640;padding:2px 8px;background:#2a2a2a}
@media(max-width:900px){#top{flex-direction:column;gap:8px}#top a{position:static}}
#stats{width:min(96vw,768px);box-sizing:border-box;display:flex;flex-wrap:wrap;justify-content:center;column-gap:12px;row-gap:2px;margin-top:6px;font-size:11px;color:#f65;white-space:nowrap}
#stats span:empty{display:none}
#main{display:grid;grid-template-columns:1fr min(96vw,768px) 1fr;column-gap:22px;width:100%;align-items:start}
#col{grid-column:2;display:flex;flex-direction:column;align-items:center}
#leg{grid-column:3;justify-self:start}
@media(max-width:1480px){#main{grid-template-columns:1fr}#col,#leg{grid-column:1;justify-self:center}#leg{margin:10px 0 20px}}
#leg table{border-collapse:collapse;background:#2c2c2c;border:3px solid;border-color:#6a6a6a #1c1c1c #1c1c1c #6a6a6a;font:13px monospace;box-shadow:0 6px 22px #0007}
#leg th{text-align:left;color:#f55;background:#3b1414;padding:5px 10px;letter-spacing:2px}
#leg td{padding:3px 10px;color:#ccc;border-top:1px solid #3c3c3c}#leg td:first-child{color:#fc6;white-space:nowrap;text-align:right}
#dev{display:flex;gap:14px;margin-top:10px}
.db{padding:7px 14px;background:#444;border:3px solid;border-color:#999 #222 #222 #999;cursor:pointer;line-height:0}
.db.dn{border-color:#222 #999 #999 #222;background:#363636}.db canvas{width:auto;height:24px}
.db.on{background:#1d3a1d;border-color:#8c8 #131 #131 #8c8}
.db.pls,#bp.dn{background:#1d3a1d;border-color:#131 #8c8 #8c8 #131}
.db.on canvas,.db.pls canvas,#bp.dn canvas{filter:hue-rotate(110deg) brightness(1.4)}
#wrap{position:relative;width:min(96vw,768px)}
canvas{image-rendering:pixelated;display:block}
#c{width:100%;cursor:crosshair;border:2px solid #400;border-bottom:0;box-sizing:border-box;background:#1d1d1d}
#wrap{box-shadow:0 8px 26px #0008}
#mn{position:absolute;inset:0;width:100%;height:100%;cursor:pointer}
#cross{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);color:#ff0;pointer-events:none;font-size:20px}
#flash{position:absolute;inset:0;background:#fa0;opacity:0;pointer-events:none;transition:opacity .12s}
#sb{width:min(96vw,768px)}
#ms{color:#bbb;margin:4px}
</style></head><body>
<div id=top><div id=ttl><canvas id=tc height=12 style=display:none></canvas><span id=tf>SONOFF DOOM 1.0 -- By Piuppi</span></div><a href=/settings>SETTINGS</a></div>
<div id=main><div id=col>
<div id=wrap><canvas id=c width=128 height=80></canvas><div id=cross>+</div><div id=flash></div><canvas id=mn width=320 height=200></canvas></div>
<canvas id=sb width=320 height=32></canvas>
<div id=stats><span id=hp>HP 100</span><span id=k>KILLS 0</span><span id=fps>-- fps</span><span id=heap></span><span id=tm></span><span id=mu></span><span id=st>connecting...</span></div>
<div id=dev><button id=bp class=db title="30 ms pulse to the relay"><canvas width=60 height=12></canvas></button>
<button id=bs class=db title="load on / off"><canvas width=92 height=12></canvas></button></div>
<div id=ms>Loading...</div></div>
<aside id=leg><table>
<tr><th colspan=2>MOVE</th></tr>
<tr><td>W / S</td><td>forward / back</td></tr>
<tr><td>A / D</td><td>strafe (turn without mouse)</td></tr>
<tr><td>Mouse</td><td>turn (click the game first)</td></tr>
<tr><th colspan=2>COMBAT</th></tr>
<tr><td>Click / Space / Ctrl</td><td>fire</td></tr>
<tr><td>Wheel / 2 / 3</td><td>pistol / shotgun</td></tr>
<tr><td>E / Right click</td><td>open doors</td></tr>
<tr><th colspan=2>GAME</th></tr>
<tr><td>ESC</td><td>menu / pause</td></tr>
<tr><td>R</td><td>restart</td></tr>
<tr><td>M / N</td><td>music on-off / next track</td></tr>
<tr><th colspan=2>DEVICE</th></tr>
<tr><td>TOGGLE</td><td>30 ms pulse to the relay</td></tr>
<tr><td>SWITCH</td><td>load on / off</td></tr>
<tr><td>Sonoff key</td><td>toggle - hold 10 s = reset</td></tr>
</table></aside></div>
<script>
const W=128,H=80,$=i=>document.getElementById(i),c=$('c'),g=c.getContext('2d'),img=g.createImageData(W,H),d=img.data;
const R=new Uint8Array(256),G=new Uint8Array(256),B=new Uint8Array(256);
for(let i=0;i<256;i++){R[i]=G[i]=B[i]=i}   // grey until PLAYPAL arrives
for(let i=3;i<d.length;i+=4)d[i]=255;
const CFG={sens:+(localStorage.sdSensI||3),sfx:+(localStorage.sdSfx||12),mus:+(localStorage.sdMus2||8)};   // 0..9, 0..15, 0..15
function saveCfg(){localStorage.sdSensI=CFG.sens;localStorage.sdSfx=CFG.sfx;localStorage.sdMus2=CFG.mus;if(mg)mg.gain.value=CFG.mus/15;if(sg2)sg2.gain.value=CFG.sfx/15}
const sensVal=()=>0.25+CFG.sens*0.3;
let ws,mask=-1,keys=0,ac,sg2=null,locked=false,yaw=0,yawSent=null,mbtn=0,started=false,lockLostT=0,wheelT=0;
const RC=new Uint8Array(H);   // floor/ceiling colour per row (from the Sonoff)
function cur(){let k=keys;if(locked){const a=k&4,d=k&8;k&=~12;if(a)k|=16;if(d)k|=32}return k|mbtn}
function send(){const k=menuOpen()?0:cur();if(ws&&ws.readyState==1&&k!==mask){mask=k;ws.send('k'+k)}}
function wsSend(m){if(ws&&ws.readyState==1)ws.send(m)}

// ---- UI blob: palettes, status bar, menu graphics, font, sound effects (/stbar.bin) ----
const sb=$('sb'),sgc=sb.getContext('2d'),simg=sgc.createImageData(320,32),sd=simg.data,sbuf=new Uint8Array(320*32),TR=247;
let P=null,pal=0,GFX={},SFX={},st={hp:100,am:50,sh:8,wp:0,dead:0},dc=0,hurtT=0,hurtDir=0,ouch=0,look=1,lookT=0,fireT=0,barKey='';
function setPal(i){if(!P)return;pal=i;const o=i*768;for(let k=0;k<256;k++){R[k]=P[o+k*3];G[k]=P[o+k*3+1];B[k]=P[o+k*3+2]}}
const STEP=[7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767],IDX=[-1,-1,-1,-1,2,4,6,8];
function adpcm(b,n){const out=new Float32Array(n);let p=0,ix=0;
 for(let i=0;i<n;i++){const code=(b[i>>1]>>((i&1)*4))&15,s=STEP[ix];let dq=s>>3;
  if(code&4)dq+=s;if(code&2)dq+=s>>1;if(code&1)dq+=s>>2;
  p+=code&8?-dq:dq;if(p>32767)p=32767;else if(p<-32768)p=-32768;
  ix+=IDX[code&7];if(ix<0)ix=0;else if(ix>88)ix=88;out[i]=p/32768}return out}
async function loadGfx(){try{const r=await fetch('/stbar.bin'),b=new Uint8Array(await r.arrayBuffer()),dv=new DataView(b.buffer);
 const id=String.fromCharCode(b[0],b[1],b[2],b[3]);if(id!=='SDS2'&&id!=='SDST')throw 'format';
 const np=dv.getUint16(4,true);let o=6;P=b.slice(o,o+np*768);o+=np*768;setPal(0);
 const n=dv.getUint16(o,true);o+=2;
 for(let i=0;i<n;i++){let nm='';for(let j=0;j<8&&b[o+j];j++)nm+=String.fromCharCode(b[o+j]);o+=8;
  const w=dv.getUint16(o,true),h=dv.getUint16(o+2,true),lo=dv.getInt16(o+4,true),to=dv.getInt16(o+6,true);o+=8;
  GFX[nm]={w,h,lo,to,px:b.subarray(o,o+w*h)};o+=w*h}
 if(id==='SDS2'&&o<b.length){const ns=b[o++];
  for(let i=0;i<ns;i++){let nm='';for(let j=0;j<8&&b[o+j];j++)nm+=String.fromCharCode(b[o+j]);o+=8;
   const rate=dv.getUint16(o,true),cnt=dv.getUint32(o+2,true),nb=dv.getUint32(o+6,true);o+=10;
   SFX[nm]={rate,pcm:adpcm(b.subarray(o,o+nb),cnt),buf:null};o+=nb}}
 barKey='';}catch(e){console.log('ui',e)}}
function sfx(nm,gain=1,delay=0){if(!ac)return false;const s=SFX[nm];if(!s)return false;
 if(!sg2){sg2=ac.createGain();sg2.gain.value=CFG.sfx/15;sg2.connect(ac.destination)}
 if(!s.buf){s.buf=ac.createBuffer(1,s.pcm.length,s.rate);s.buf.getChannelData(0).set(s.pcm)}
 const src=ac.createBufferSource(),gn=ac.createGain();src.buffer=s.buf;gn.gain.value=gain;src.connect(gn);gn.connect(sg2);
 src.start(ac.currentTime+delay);return true}

// ---- status bar (Doom STBAR + Doomguy face) ----
function blitTo(buf,bw,bh,nm,x,y){const g=GFX[nm];if(!g)return;x-=g.lo;y-=g.to;
 for(let j=0;j<g.h;j++){const yy=y+j;if(yy<0||yy>=bh)continue;
  for(let i=0;i<g.w;i++){const xx=x+i,v=g.px[j*g.w+i];if(xx>=0&&xx<bw&&v!==TR)buf[yy*bw+xx]=v}}}
const blit=(nm,x,y)=>blitTo(sbuf,320,32,nm,x,y);
function num(v,x,y,f,dg=3){const g=GFX[f+'0'];if(!g)return;const w=g.w;v=Math.max(0,v|0);
 if(!v){blit(f+'0',x-w,y);return}while(v&&dg--){x-=w;blit(f+(v%10),x,y);v=(v/10)|0}}
function faceName(now){if(st.dead)return 'STFDEAD0';
 const l=Math.min(4,((100-Math.min(100,Math.max(0,st.hp)))*5/101)|0);
 if(now<hurtT)return ouch?'STFOUCH'+l:hurtDir>0?'STFTR'+l+'0':hurtDir<0?'STFTL'+l+'0':'STFKILL'+l;
 if(fireT&&now-fireT>2000)return 'STFKILL'+l;
 if(now>lookT){look=(Math.random()*3)|0;lookT=now+500}
 return 'STFST'+l+look}
function drawBar(face){sbuf.fill(0);blit('STBAR',0,0);blit('STARMS',104,0);
 num(st.wp?st.sh:st.am,44,3,'STTNUM');num(st.hp,90,3,'STTNUM');blit('STTPRCNT',90,3);
 for(let i=0;i<6;i++)blit((i<=1?'STYSNUM':'STGNUM')+(i+2),111+(i%3)*12,4+((i/3)|0)*10);   // owned: 2 pistol, 3 shotgun
 blit(face,143,0);num(0,221,3,'STTNUM');blit('STTPRCNT',221,3);
 const cv=[st.am,st.sh,0,0],mx=[200,50,50,300],ys=[5,11,17,23];
 for(let i=0;i<4;i++){num(cv[i],288,ys[i],'STYSNUM');num(mx[i],314,ys[i],'STYSNUM')}
 for(let i=0;i<sbuf.length;i++){const v=sbuf[i],o=i*4;sd[o]=R[v];sd[o+1]=G[v];sd[o+2]=B[v];sd[o+3]=255}
 sgc.putImageData(simg,0,0)}

// ---- classic Doom menu (320x200 overlay) ----
const mn=$('mn'),mg2=mn.getContext('2d'),mimg=mg2.createImageData(320,200),md=mimg.data,mbuf=new Int16Array(320*200);
const SKILLS=['M_JKILL','M_ROUGH','M_HURT','M_ULTRA','M_NMARE'];
const MENUS={
 main:{x:97,y:64,title:[['M_DOOM',94,2]],items:['M_NGAME','M_OPTION','M_LOADG','M_SAVEG','M_RDTHIS','M_QUITG'],back:null},
 episode:{x:48,y:63,title:[['M_EPISOD',54,38]],items:['M_EPI1','M_EPI2','M_EPI3'],back:'main'},
 skill:{x:48,y:63,title:[['M_NEWG',96,14],['M_SKILL',54,38]],items:SKILLS,back:'episode',def:2},
 options:{x:60,y:37,step:32,title:[['M_OPTTTL',108,15]],items:['M_MSENS','M_SFXVOL','M_MUSVOL'],back:'main'},
 readme:{items:[],back:'main'}};
let menu={page:'main',item:0,msg:null},skullT=0,skull=0;
const SWSTRING='this is the shareware version of doom.\n\nyou need to order the entire trilogy.\n\npress a key.';
const NIGHTMARE='are you sure? this skill level\nisn\'t even remotely fair.\n\npress y or n.';
const README=['SONOFF DOOM -- BY PIUPPI','','RUNNING ON A SONOFF BASIC R2','( ESP8285, 80 KB RAM, 1 MB FLASH )','',
 'W S ........... FORWARD / BACK','A D ........... STRAFE','MOUSE ......... TURN','CLICK / SPACE . FIRE',
 'E / R-CLICK ... OPEN DOORS',
 'WHEEL / 2 3 ... CHANGE WEAPON','M / N ......... MUSIC / NEXT TRACK','ESC ........... MENU','','PRESS A KEY.'];
function menuOpen(){return !!menu.page}
function textW(s){let w=0;for(const ch of s.toUpperCase()){const g=GFX['STCFN'+String(ch.charCodeAt(0)).padStart(3,'0')];w+=g?g.w:4}return w}
function text(s,x,y){for(const ch of s.toUpperCase()){const g=GFX['STCFN'+String(ch.charCodeAt(0)).padStart(3,'0')];
 if(!g){x+=4;continue}blitTo(mbuf,320,200,'STCFN'+String(ch.charCodeAt(0)).padStart(3,'0'),x,y);x+=g.w}}
function textBlock(lines,y0){lines.forEach((l,i)=>text(l,160-(textW(l)>>1),y0+i*12))}
function thermo(x,y,width,dot){blitTo(mbuf,320,200,'M_THERML',x,y);let xx=x+8;
 for(let i=0;i<width;i++,xx+=8)blitTo(mbuf,320,200,'M_THERMM',xx,y);blitTo(mbuf,320,200,'M_THERMR',xx,y);
 blitTo(mbuf,320,200,'M_THERMO',x+8+dot*8,y)}
function drawMenu(now){mbuf.fill(-1);let dim=0;
 if(menu.page==='readme'){dim=1;textBlock(README,100-README.length*6)}
 else if(menu.page&&!menu.msg){const m=MENUS[menu.page],stp=m.step||16;   // like Doom: a message hides the menu
  for(const t of m.title)blitTo(mbuf,320,200,t[0],t[1],t[2]);
  m.items.forEach((it,i)=>blitTo(mbuf,320,200,it,m.x,m.y+i*stp));
  if(menu.page==='options'){thermo(m.x,m.y+16,10,CFG.sens);thermo(m.x,m.y+48,16,CFG.sfx);thermo(m.x,m.y+80,16,CFG.mus)}
  if(now>skullT){skull^=1;skullT=now+229}blitTo(mbuf,320,200,skull?'M_SKULL2':'M_SKULL1',m.x-32,m.y-5+menu.item*stp)}
 if(menu.msg){dim=1;const L=menu.msg.text.split('\n');textBlock(L,100-L.length*6)}
 for(let i=0;i<mbuf.length;i++){const v=mbuf[i],o=i*4;
  if(v<0){md[o]=md[o+1]=md[o+2]=0;md[o+3]=dim?150:0}else{md[o]=R[v];md[o+1]=G[v];md[o+2]=B[v];md[o+3]=255}}
 mg2.putImageData(mimg,0,0)}
function showMenu(open){mn.style.display=open?'block':'none';$('cross').style.display=open?'none':'block'}
function openMenu(){if(menu.page)return;menu={page:'main',item:MENUS.main.last||0,msg:null};sfx('DSSWTCHN');
 keys=0;mbtn=0;wsSend('P1');showMenu(true);send();hint()}
function closeMenu(){if(!started)return;if(menu.page==='main')MENUS.main.last=menu.item;menu.page=null;menu.msg=null;sfx('DSSWTCHX');
 wsSend('G');wsSend('P0');showMenu(false);mask=-1;send();hint()}
function goto(page){if(menu.page&&MENUS[menu.page])MENUS[menu.page].last=menu.item;   // remember position like Doom
 menu.page=page;const m=MENUS[page];menu.item=m.last!==undefined?m.last:(m.def||0);menu.msg=null}
function message(t,yn){menu.msg={text:t,yn}}
function startGame(sk){started=true;sfx('DSPISTOL');wsSend('G');wsSend('n'+sk);menu.page=null;menu.msg=null;showMenu(false);
 mask=-1;send();lockMouse();hint()}
function activate(){const p=menu.page,i=menu.item;
 if(p==='main'){sfx('DSPISTOL');
  if(i===0)goto('episode');else if(i===1)goto('options');
  else if(i===2||i===3)message('you can\'t '+(i===2?'load':'save')+' a game\non a sonoff relay!\n\npress a key.');
  else if(i===4)goto('readme');
  else message('are you sure you want to\nquit this great game?\n\npress y or n.',()=>{started=false;wsSend('Q');
   if(document.exitPointerLock)document.exitPointerLock();goto('main');hint()})}
 else if(p==='episode'){sfx('DSPISTOL');if(i===0)goto('skill');else message(SWSTRING)}
 else if(p==='skill'){if(i===4)message(NIGHTMARE,()=>startGame(4));else startGame(i)}
 else if(p==='options')adjust(1)}
function adjust(dir){const k=['sens','sfx','mus'][menu.item],mx=[9,15,15][menu.item];
 CFG[k]=Math.max(0,Math.min(mx,CFG[k]+dir));saveCfg();sfx('DSPSTOP')}
function back(){const b=menu.page&&MENUS[menu.page].back;if(b){sfx('DSSWTCHX');goto(b)}else closeMenu()}
function menuKey(e){audio();const k=e.code;
 if(menu.page==='cheat'){closeMenu();return}
 if(menu.msg){if(menu.msg.yn){if(k==='KeyY'){const f=menu.msg.yn;menu.msg=null;f()}else if(k==='KeyN'||k==='Escape'){menu.msg=null;sfx('DSSWTCHX')}}
  else{menu.msg=null;sfx('DSSWTCHX')}return}
 if(menu.page==='readme'){goto('main');sfx('DSSWTCHX');return}
 const n=MENUS[menu.page].items.length;
 if(k==='ArrowUp'||k==='KeyW'){menu.item=(menu.item+n-1)%n;sfx('DSPSTOP')}
 else if(k==='ArrowDown'||k==='KeyS'){menu.item=(menu.item+1)%n;sfx('DSPSTOP')}
 else if(menu.page==='options'&&(k==='ArrowLeft'||k==='KeyA'))adjust(-1);
 else if(menu.page==='options'&&(k==='ArrowRight'||k==='KeyD'))adjust(1);
 else if(k==='Enter'||k==='Space'||k==='NumpadEnter')activate();
 else if(k==='Escape'||k==='Backspace')back()}
function menuItemAt(e){const r=mn.getBoundingClientRect(),y=(e.clientY-r.top)*200/r.height;const m=MENUS[menu.page];
 if(!m||!m.items.length)return -1;const stp=m.step||16,i=Math.floor((y-m.y+2)/stp);return i>=0&&i<m.items.length?i:-1}
mn.onmousemove=e=>{if(menu.msg||menu.page==='readme')return;const i=menuItemAt(e);if(i>=0&&i!==menu.item){menu.item=i;sfx('DSPSTOP')}};
mn.onclick=e=>{audio();if(menu.page==='cheat'){closeMenu();return}if(menu.msg&&menu.msg.yn)return;if(menu.msg||menu.page==='readme'){menuKey({code:'Enter'});return}
 const i=menuItemAt(e);if(i>=0){menu.item=i;activate()}};
mn.oncontextmenu=e=>{e.preventDefault();if(menu.msg)menuKey({code:'Escape'});else back()};
function hint(){$('ms').textContent=menu.page?(started?'Menu - ESC to return to the game':'Choose NEW GAME to start (arrows + Enter or mouse)')
 :locked?'Mouse active - ESC = menu':'Click the game to use the mouse - ESC = menu'}

let lastT=performance.now();
function tick(now){const dt=(now-lastT)/1000;lastT=now;
 dc=Math.max(0,dc-35*dt);                               // Doom damagecount: -1 per tic
 const p=menu.page?0:(dc>=1?Math.min(7,((dc|0)+7)>>3)+1:0);   // red palettes 1..8
 if(P&&p!==pal)setPal(p);
 if(cur()&64){if(!fireT)fireT=now}else fireT=0;
 const f=faceName(now),key=f+'|'+st.hp+'|'+st.am+'|'+st.sh+'|'+st.wp+'|'+pal;
 if(key!==barKey&&Object.keys(GFX).length){barKey=key;drawBar(f)}
 if(menu.page&&Object.keys(GFX).length)drawMenu(now);
 requestAnimationFrame(tick)}
// ---- music: MUS tracks from the WAD (/music.bin), synthesized with WebAudio ----
let MUS=[],trk=+(localStorage.sdTrk||0),mOn=localStorage.sdMus!=='0',mg=null,seq=null,NB=null,CURVE=null,voices=0;
function musUI(){$('mu').textContent=!MUS.length?'music not available':(mOn?'\u266B '+MUS[trk].nm:'\u266B off')}
async function loadMusic(){try{const r=await fetch('/music.bin');if(!r.ok)throw r.status;
 const b=new Uint8Array(await r.arrayBuffer()),dv=new DataView(b.buffer);
 if(String.fromCharCode(b[0],b[1],b[2],b[3])!=='SDMU')throw 'formato';let o=5;
 for(let i=0;i<b[4];i++){let nm='';for(let j=0;j<8&&b[o+j];j++)nm+=String.fromCharCode(b[o+j]);
  const n=dv.getUint32(o+8,true);o+=12;const ev=parseMus(b.subarray(o,o+n));if(ev)MUS.push({nm,ev});o+=n}
 if(trk>=MUS.length)trk=0;musUI();startMusic()}catch(e){MUS=[];musUI()}}
// MUS -> [tick, type, ch, a, b]   (140 tick/s; ch 15 = percussion)
function parseMus(m){if(m[0]!==77||m[1]!==85||m[2]!==83||m[3]!==26)return null;
 let p=m[6]|m[7]<<8,t=0;const ev=[],vol=new Array(16).fill(127);
 while(p<m.length){const d=m[p++],ty=(d>>4)&7,ch=d&15;
  if(ty===0)ev.push([t,0,ch,m[p++]&127]);
  else if(ty===1){const n=m[p++];if(n&128)vol[ch]=m[p++]&127;ev.push([t,1,ch,n&127,vol[ch]])}
  else if(ty===2||ty===3)ev.push([t,ty,ch,m[p++]]);
  else if(ty===4){const c=m[p++],v=m[p++];ev.push([t,4,ch,c,v])}
  else if(ty===6)break;
  if(d&128){let dl=0,x;do{x=m[p++];dl=dl*128+(x&127)}while(x&128&&p<m.length);t+=dl}}
 ev.push([t,6]);return t>0?ev:null}
// GM program -> synth voice  [waves, lowpass, attack, decay, sustain, release, gain]
function voice(p){
 if(p>=29&&p<=31)return['dist',3000,.004,.6,.8,.06,.9];     // overdriven/distortion guitar (E1M1!)
 if(p>=32&&p<=39)return['bass',900,.004,.4,.7,.06,1.3];
 if(p<8)return['piano',4000,.004,.5,.25,.25,1];
 if(p<16)return['bell',6000,.002,.6,0,.4,.8];
 if(p<24)return['organ',2200,.01,.2,.9,.08,.6];
 if(p<29)return['clean',3200,.004,.5,.3,.2,.9];
 if(p<56)return['pad',2600,.08,.5,.8,.35,.6];
 if(p<72)return['brass',2200,.03,.3,.7,.12,.7];
 if(p<80)return['flute',5000,.03,.3,.8,.12,.8];
 if(p<88)return['lead',3500,.006,.3,.8,.08,.6];
 return['pad',2400,.15,.8,.7,.5,.5]}
function startMusic(){if(!ac||!MUS.length||!mOn||seq)return;
 if(!mg){mg=ac.createGain();mg.gain.value=CFG.mus/15;mg.connect(ac.destination)}
 seq={i:0,t0:ac.currentTime+.1,ch:[...Array(16)].map(()=>({prog:0,vol:127,exp:127,bend:0,notes:{}}))};
 seq.timer=setInterval(pump,25)}
function stopMusic(){if(!seq)return;clearInterval(seq.timer);const t=ac.currentTime;
 for(const c of seq.ch)for(const k in c.notes)noteOff(c.notes[k],t);seq=null}
function pump(){if(!seq)return;const ev=MUS[trk].ev,now=ac.currentTime,until=now+.2;
 for(let guard=0;guard<4000;guard++){const e=ev[seq.i];let at=seq.t0+e[0]/140;
  if(at<now-.1){seq.t0+=now-at;at=now}                     // tab was in background: resync, no burst
  if(at>until)break;
  if(e[1]===6){for(const c of seq.ch){for(const k in c.notes)noteOff(c.notes[k],at);c.notes={}}seq.t0=at;seq.i=0;continue}
  handle(e,Math.max(at,now));seq.i++}}
function handle(e,at){const c=seq.ch[e[2]];
 if(e[1]===0){const n=c.notes[e[3]];if(n){noteOff(n,at);delete c.notes[e[3]]}}
 else if(e[1]===1){if(e[2]===15){drum(e[3],e[4],c,at);return}
  const o=c.notes[e[3]];if(o)noteOff(o,at);const n=noteOn(c,e[3],e[4],at);if(n)c.notes[e[3]]=n;else delete c.notes[e[3]]}
 else if(e[1]===2){c.bend=(e[3]-128)/64;for(const k in c.notes)for(const o of c.notes[k].os)o.detune.setValueAtTime(c.bend*100+o.dt,at)}
 else if(e[1]===3){if(e[3]===10||e[3]===11){for(const k in c.notes)noteOff(c.notes[k],at);c.notes={}}}
 else if(e[1]===4){if(e[3]===0)c.prog=e[4];
  else if(e[3]===3||e[3]===5){if(e[3]===3)c.vol=e[4];else c.exp=e[4];   // volume/expression also on held notes (E1M2 fades in)
   const k=(c.vol/127)*(c.exp/127);for(const nn in c.notes){const n=c.notes[nn];if(!n.done){n.g.gain.cancelScheduledValues(at);
    n.g.gain.setTargetAtTime(n.base*k*(at>n.t0+n.atk?n.sus:1),at,.015)}}}}}
function distCurve(){if(!CURVE){CURVE=new Float32Array(1024);for(let i=0;i<1024;i++){const x=i/511.5-1;CURVE[i]=Math.tanh(x*4)}}return CURVE}
function noteOn(c,note,vel,at){if(voices>=28)return null;
 const [w,lp,atk,dec,sus,rel,gn]=voice(c.prog),f=440*Math.pow(2,(note-69)/12);
 const base=(vel/127)*.12*gn,amp=base*(c.vol/127)*(c.exp/127);if(base<=0)return null;   // volume 0 now = silent but alive
 const g=ac.createGain(),fl=ac.createBiquadFilter(),os=[];fl.type='lowpass';fl.frequency.value=lp;fl.connect(g);g.connect(mg);
 const mk=(type,dt,to,lvl=1)=>{const o=ac.createOscillator(),og=ac.createGain();o.type=type;o.frequency.value=f*(type==='sine'&&w==='bass'?.5:1);
  o.dt=dt;o.detune.value=c.bend*100+dt;og.gain.value=lvl;o.connect(og);og.connect(to);o.start(at);os.push(o)};
 if(w==='dist'){const pre=ac.createGain(),ws=ac.createWaveShaper();pre.gain.value=1.5;ws.curve=distCurve();pre.connect(ws);ws.connect(fl);
  mk('sawtooth',-8,pre,.5);mk('sawtooth',8,pre,.5)}
 else if(w==='bass'){mk('sawtooth',0,fl,.6);mk('sine',0,fl,.8)}
 else if(w==='piano'||w==='clean'){mk('triangle',0,fl);mk('sawtooth',4,fl,.25)}
 else if(w==='bell'||w==='flute')mk(w==='bell'?'sine':'triangle',0,fl);
 else if(w==='organ'||w==='lead')mk('square',0,fl,.6);
 else{mk('sawtooth',-6,fl,.5);mk('sawtooth',6,fl,.5)}
 g.gain.setValueAtTime(0,at);g.gain.linearRampToValueAtTime(amp,at+atk);g.gain.setTargetAtTime(amp*sus,at+atk,dec/3);
 voices++;return{g,os,rel,done:false,base,sus,atk,t0:at}}
function noteOff(n,at){if(n.done)return;n.done=true;const p=n.g.gain;
 if(p.cancelAndHoldAtTime)p.cancelAndHoldAtTime(at);else p.cancelScheduledValues(at);
 p.setTargetAtTime(0,at,n.rel/3);const end=at+n.rel*2+.05;for(const o of n.os)o.stop(end);
 n.os[0].onended=()=>{voices--}}
function noise(){if(!NB){NB=ac.createBuffer(1,ac.sampleRate,ac.sampleRate);const a=NB.getChannelData(0);for(let i=0;i<a.length;i++)a[i]=Math.random()*2-1}return NB}
function drum(k,vel,c,at){const amp=(vel/127)*(c.vol/127)*(c.exp/127)*.3;if(amp<=0)return;
 const tone=(f0,f1,dur,a)=>{const o=ac.createOscillator(),g=ac.createGain();o.frequency.setValueAtTime(f0,at);
  o.frequency.exponentialRampToValueAtTime(f1,at+dur);g.gain.setValueAtTime(a,at);g.gain.exponentialRampToValueAtTime(.001,at+dur);
  o.connect(g);g.connect(mg);o.start(at);o.stop(at+dur+.02)};
 const hiss=(type,fq,dur,a)=>{const s=ac.createBufferSource(),fl=ac.createBiquadFilter(),g=ac.createGain();s.buffer=noise();
  fl.type=type;fl.frequency.value=fq;g.gain.setValueAtTime(a,at);g.gain.exponentialRampToValueAtTime(.001,at+dur);
  s.connect(fl);fl.connect(g);g.connect(mg);s.start(at,Math.random()*.5);s.stop(at+dur+.02)};
 if(k===35||k===36)tone(160,45,.25,amp*1.8);                                   // kick
 else if(k>=37&&k<=40){hiss('bandpass',1800,.18,amp);tone(230,150,.08,amp*.6)} // snare / clap
 else if(k===42||k===44)hiss('highpass',7000,.05,amp*.45);                     // closed hi-hat
 else if(k===46)hiss('highpass',7000,.3,amp*.45);                              // open hi-hat
 else if(k===49||k===52||k===55||k===57)hiss('highpass',5000,1.2,amp*.5);      // crash
 else if(k===51||k===53||k===59)hiss('highpass',8000,.4,amp*.3);               // ride
 else if(k>=41&&k<=50)tone(70+(k-41)*16,45+(k-41)*8,.3,amp*1.2);               // toms
 else hiss('bandpass',3000,.08,amp*.4)}
function setTrack(i){if(!MUS.length)return;stopMusic();trk=(i+MUS.length)%MUS.length;localStorage.sdTrk=trk;musUI();startMusic()}
function toggleMusic(){mOn=!mOn;localStorage.sdMus=mOn?'1':'0';if(mOn)startMusic();else stopMusic();musUI()}
function boom(wp){$('flash').style.opacity=wp?.5:.35;setTimeout(()=>$('flash').style.opacity=0,wp?90:50);
 if(sfx(wp?'DSSHOTGN':'DSPISTOL',wp?1:.8))return;
 if(!ac)return;const len=wp?.45:.15,n=ac.sampleRate*len,b=ac.createBuffer(1,n,ac.sampleRate),a=b.getChannelData(0);   // fallback
 for(let i=0;i<n;i++)a[i]=(Math.random()*2-1)*Math.pow(1-i/n,wp?2:3);
 const s=ac.createBufferSource(),f=ac.createBiquadFilter(),gn=ac.createGain();f.type='lowpass';f.frequency.value=wp?1500:1200;gn.gain.value=wp?1.6:1;
 s.buffer=b;s.connect(f).connect(gn).connect(ac.destination);s.start()}
// ---- device: TOGGLE (30 ms pulse) and SWITCH (on/off), labels drawn with the Doom font ----
let relayOn=false,plsT=0;
const PULSE_SHOW=500;   // ms the TOGGLE stays green after a pulse: 30 ms alone is too short to see
function lbl(id,t){const cv=$(id).firstElementChild,w=cv.width,h=cv.height,x2=cv.getContext('2d');
 if(!P||!GFX.STCFN065){x2.clearRect(0,0,w,h);x2.fillStyle='#f44';x2.font='bold 10px monospace';x2.textAlign='center';x2.fillText(t,w/2,10);return}
 const b=new Int16Array(w*h).fill(-1);let x=(w-textW(t))>>1;
 for(const ch of t.toUpperCase()){const nm='STCFN'+String(ch.charCodeAt(0)).padStart(3,'0'),gl=GFX[nm];if(!gl){x+=4;continue}blitTo(b,w,h,nm,x,2);x+=gl.w}
 const im=x2.createImageData(w,h);for(let i=0;i<w*h;i++){const v=b[i];if(v>=0){im.data[i*4]=P[v*3];im.data[i*4+1]=P[v*3+1];im.data[i*4+2]=P[v*3+2];im.data[i*4+3]=255}}
 x2.putImageData(im,0,0)}
// page title in Doom style: the WAD's red pixel font, enlarged x3 (falls back to plain text until the font has arrived)
const VER='1.0',TITLE='SONOFF DOOM '+VER+' -- BY PIUPPI';
function drawTitle(){if(!P||!GFX.STCFN065)return;const cv=$('tc'),w=textW(TITLE)+4,h=12;
 const b=new Int16Array(w*h).fill(-1);let x=2;
 for(const ch of TITLE){const nm='STCFN'+String(ch.charCodeAt(0)).padStart(3,'0'),gl=GFX[nm];if(!gl){x+=4;continue}blitTo(b,w,h,nm,x,2);x+=gl.w}
 cv.width=w;cv.height=h;const x2=cv.getContext('2d'),im=x2.createImageData(w,h);
 for(let i=0;i<w*h;i++){const v=b[i];if(v>=0){im.data[i*4]=P[v*3];im.data[i*4+1]=P[v*3+1];im.data[i*4+2]=P[v*3+2];im.data[i*4+3]=255}}
 x2.putImageData(im,0,0);cv.style.width=Math.min(w*3,innerWidth*.94)+'px';cv.style.display='block';$('tf').style.display='none'}
function devUI(){lbl('bp','TOGGLE');lbl('bs','SWITCH: '+(relayOn?'ON':'OFF'));$('bs').classList.toggle('on',relayOn);drawTitle()}
addEventListener('resize',drawTitle);
// pressed look via pointer events (preventDefault on mousedown disables :active in some browsers)
for(const id of ['bp','bs']){const el=$(id);el.onmousedown=e=>e.preventDefault();el.tabIndex=-1;
 el.onpointerdown=()=>el.classList.add('dn');
 for(const ev of ['pointerup','pointerleave','pointercancel'])el.addEventListener(ev,()=>el.classList.remove('dn'))}
$('bp').onclick=()=>{audio();wsSend('Rp');sfx('DSSWTCHN')};
$('bs').onclick=()=>{audio();wsSend('Rt');sfx('DSSWTCHN')};
async function conn(){let t='';
 try{const r=await fetch('/token',{cache:'no-store'});if(!r.ok)throw r.status;t=(await r.text()).trim()}catch(e){$('st').textContent='offline';setTimeout(conn,2000);return}
 ws=new WebSocket('ws://'+location.hostname+':81/?t='+t);ws.binaryType='arraybuffer';
 ws.onopen=()=>{$('st').textContent='online';ws.send('p');yawSent=null;mask=-1;
  if(started){if(menu.page)wsSend('P1');else{started=false;goto('main');showMenu(true);hint()}}send()};
 ws.onclose=()=>{$('st').textContent='offline';setTimeout(conn,1000)};
 ws.onmessage=e=>{
  if(typeof e.data==='string'){const m=JSON.parse(e.data);
   if(m.ev==='fire'){boom(m.wp);st.am=m.am;st.sh=m.sh;
    if(m.kill)sfx('DSBGDTH1',1,.05);else if(m.hit)sfx('DSPOPAIN',.9,.05)}
   if(m.ev==='hurt'){dc=Math.min(100,dc+m.d);hurtDir=m.dir;ouch=m.d>20;hurtT=performance.now()+1000;st.hp=m.hp;sfx('DSPLPAIN')}
   if(m.ev==='wsw'&&m.wp===1)sfx('DSSGCOCK',.8);
   if(m.ev==='relay'){relayOn=!!m.on;devUI()}
   if(m.ev==='pulse'){const b=$('bp');b.classList.add('pls');clearTimeout(plsT);plsT=setTimeout(()=>b.classList.remove('pls'),PULSE_SHOW)}
   if(m.ev==='kicked'){keys=0;mbtn=0;send();
    menu={page:'main',item:0,msg:{text:'another browser took the game.\n\nresume (esc) or start a new game\nto take it back.\n\npress a key.'}};
    showMenu(true);if(document.exitPointerLock)document.exitPointerLock();hint()}
   if(m.ev==='door')sfx(m.o?'DSDOROPN':'DSDORCLS',Math.max(.15,Math.min(1,1.6/Math.max(1,m.d))));
   if(m.ev==='noway')sfx('DSNOWAY')||sfx('DSPLPAIN',.4);
   if(m.st){if(m.dead&&!st.dead)sfx('DSPLDETH');st.hp=m.hp;st.am=m.am;st.sh=m.sh;st.wp=m.wp;st.dead=m.dead;if(!m.dead)dc=0}
   if(m.fps!==undefined){$('fps').textContent=m.fps+' fps';$('heap').textContent='heap '+m.heap+' B';
    $('hp').textContent='HP '+m.hp;st.hp=m.hp;if(m.am!==undefined)st.am=m.am;if(m.sh!==undefined)st.sh=m.sh;$('k').textContent='KILLS '+m.kills;
    if(m.r!==undefined)$('tm').textContent='render '+m.r+' ms | send '+m.s+' ms | '+m.kbs+' KB/s'}
   return}
  const b=new Uint8Array(e.data),x0=b[0],n=b[1];let p=2;
  if(x0===255){if(!P)for(let i=0;i<256;i++){R[i]=b[2+i*3];G[i]=b[3+i*3];B[i]=b[4+i*3]}
   if(b.length>=770+H)RC.set(b.subarray(770,770+H));return}
  for(let i=0;i<n;i++){const x=x0+i,ys=b[p++],ye=ys+b[p++];
   for(let y=0;y<H;y++){const v=(y>=ys&&y<ye)?b[p++]:RC[y],o=(y*W+x)*4;d[o]=R[v];d[o+1]=G[v];d[o+2]=B[v]}}
  if(x0+n>=W)g.putImageData(img,0,0)}}
const KM={ArrowUp:1,KeyW:1,ArrowDown:2,KeyS:2,ArrowLeft:4,KeyA:4,ArrowRight:8,KeyD:8,Space:64,ControlLeft:64,ControlRight:64};
// ---- cheats: typed while playing, like Doom. Here they only get a popup from Piuppi ----
const CHEATS={IDDQD:'degreelessness mode... denied!\n\nthis relay can switch 10 amps,\nbut it can\'t make you a god.\n\n- piuppi\n\npress a key.',
 IDKFA:'very happy ammo? not today.\n\nall keys, all weapons, all ammo\nwon\'t fit in 80 kb of ram.\n\n- piuppi\n\npress a key.'};
let cheatBuf='';
function cheatKey(code){if(!/^Key[A-Z]$/.test(code))return false;cheatBuf=(cheatBuf+code[3]).slice(-5);
 const t=CHEATS[cheatBuf];if(!t)return false;cheatBuf='';keys=0;mbtn=0;send();wsSend('P1');
 menu={page:'cheat',item:0,msg:{text:t}};showMenu(true);sfx('DSSWTCHN');hint();return true}
function audio(){if(!ac)try{ac=new AudioContext()}catch(e){}
 if(ac&&ac.state==='suspended')ac.resume();startMusic()}
onkeydown=e=>{audio();
 if(e.code==='KeyM'&&!e.repeat){toggleMusic();return}
 if(e.code==='KeyN'&&!e.repeat&&!(menu.msg&&menu.msg.yn)){setTrack(trk+1);return}
 if(menu.page){if(!e.repeat||/Arrow|Key[WSAD]/.test(e.code))menuKey(e);e.preventDefault();return}
 if(e.code==='Escape'){if(performance.now()-lockLostT>400)openMenu();return}
 if(!e.repeat&&cheatKey(e.code)){e.preventDefault();return}
 if(e.code==='KeyE'&&!e.repeat){wsSend('u');return}
 if(e.code==='KeyR'&&!e.repeat){wsSend('r');return}
 if((e.code==='Digit2'||e.code==='Digit3')&&!e.repeat){wsSend('w'+(e.code==='Digit2'?0:1));return}
 if(KM[e.code]){keys|=KM[e.code];send();e.preventDefault()}};
onkeyup=e=>{if(KM[e.code]){keys&=~KM[e.code];send()}};
onblur=()=>{keys=0;mbtn=0;send()};
// raw mouse (no OS acceleration) where supported, normal pointer lock otherwise
function lockMouse(){if(locked)return;let r;try{r=c.requestPointerLock({unadjustedMovement:true})}catch(e){}
 if(r&&r.catch)r.catch(()=>{try{c.requestPointerLock()}catch(e){}});else if(!r&&document.pointerLockElement!==c){try{c.requestPointerLock()}catch(e){}}}
c.onclick=()=>{audio();if(!menu.page)lockMouse()};
document.addEventListener('pointerlockchange',()=>{const was=locked;locked=document.pointerLockElement===c;mbtn=0;
 if(was&&!locked){lockLostT=performance.now();if(started&&!menu.page)openMenu()}   // ESC while playing = menu, like Doom
 hint();mask=-1;send()});
// spikes: Chrome sometimes reports a huge bogus movementX in pointer lock -> ignore them
document.addEventListener('mousemove',e=>{if(!locked||menu.page)return;const m=e.movementX;if(Math.abs(m)>200)return;yaw+=m*sensVal()});
document.addEventListener('mousedown',e=>{if(!locked)return;if(menu.page==='cheat'){closeMenu();return}
 if(!menu.page&&e.button===0){mbtn=64;send()}if(!menu.page&&e.button===2)wsSend('u')});
c.oncontextmenu=e=>e.preventDefault();
document.addEventListener('mouseup',e=>{if(e.button===0&&mbtn){mbtn=0;send()}});
document.addEventListener('wheel',e=>{if(!locked||menu.page)return;e.preventDefault();const t=performance.now();
 if(t-wheelT<200||!e.deltaY)return;wheelT=t;wsSend(e.deltaY>0?'w+':'w-')},{passive:false});
// absolute yaw every 20 ms (only if changed): the Sonoff uses the difference, never a burst
setInterval(()=>{const v=Math.round(yaw);if(v!==yawSent&&ws&&ws.readyState==1&&ws.bufferedAmount<256){ws.send('a'+v);yawSent=v}},20);
setInterval(()=>{mask=-1;send()},1000);
showMenu(true);hint();
devUI();loadGfx().then(()=>{hint();devUI();return loadMusic()});requestAnimationFrame(tick);
conn();
</script></body></html>
)rawliteral";
