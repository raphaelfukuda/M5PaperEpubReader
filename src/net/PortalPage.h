#pragma once

#include "AppConfig.h"

#if M5EPUB_ENABLE_WEB_PORTAL
#include <pgmspace.h>

static const char kPortalPage[] PROGMEM = R"PORTAL(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>M5Paper Library</title><style>
:root{color-scheme:light dark;--bg:#f5f3ed;--fg:#171717;--line:#777}*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);font:16px Georgia,serif}main{max-width:760px;margin:auto;padding:18px}
h1{font-size:1.65rem;margin:.3rem 0 1rem}.data,button,input{font:14px ui-monospace,monospace}button{background:transparent;color:inherit;border:1px solid;padding:.65rem;cursor:pointer}
button:focus-visible,input:focus-visible,a:focus-visible{outline:3px double currentColor;outline-offset:3px}.bar{display:flex;gap:.5rem;flex-wrap:wrap;margin:1rem 0}
#crumb a{color:inherit}.drop{border:2px dashed var(--line);padding:1.2rem;text-align:center}.drop.over{border-style:solid}
ul{list-style:none;padding:0;border-top:1px solid}li{display:grid;grid-template-columns:1fr auto auto;gap:.6rem;align-items:center;border-bottom:1px solid var(--line);padding:.75rem 0}
.name{overflow-wrap:anywhere}.folder .name{font-weight:bold}.meta{font:13px ui-monospace,monospace}progress{width:100%;height:1rem}.job{padding:.6rem 0}
@media(max-width:380px){main{padding:12px}li{grid-template-columns:1fr auto}.meta{grid-column:1}.bar button{flex:1}}
@media(prefers-color-scheme:dark){:root{--bg:#171717;--fg:#eee;--line:#aaa}}
@media(prefers-reduced-motion:no-preference){button{transition:background .15s}button:hover{background:#8883}}
</style></head><body><main><h1 id="title">M5Paper Library</h1><nav id="crumb"></nav>
<div class="bar"><button id="mkdir">New folder</button><button id="pick">Upload EPUB</button><input id="files" type="file" accept=".epub,application/epub+zip" multiple hidden></div>
<div id="drop" class="drop">Drop EPUB files here</div><p class="data" id="space"></p><ul id="list"></ul><section id="jobs"></section></main>
<script>
const pt=navigator.language.toLowerCase().startsWith('pt');const t=pt?{title:'Biblioteca do M5Paper',folder:'Nova pasta',upload:'Enviar EPUB',drop:'Arraste arquivos EPUB aqui',free:'livres',empty:'Pasta vazia',del:'Apagar',confirm:'Apagar este item?',name:'Nome da pasta:',failed:'Falha'}:{title:'M5Paper Library',folder:'New folder',upload:'Upload EPUB',drop:'Drop EPUB files here',free:'free',empty:'Empty folder',del:'Delete',confirm:'Delete this item?',name:'Folder name:',failed:'Failed'};
let path='/';const $=s=>document.querySelector(s);Object.assign($('#title'),{textContent:t.title});$('#mkdir').textContent=t.folder;$('#pick').textContent=t.upload;$('#drop').textContent=t.drop;
const natural=(a,b)=>a.name.localeCompare(b.name,undefined,{numeric:true,sensitivity:'base'});const size=n=>n<1024?n+' B':n<1048576?(n/1024).toFixed(1)+' KiB':(n/1048576).toFixed(1)+' MiB';
function crumbs(){let parts=path.split('/').filter(Boolean),html='<a href="#" data-p="/">/</a>',p='';for(const x of parts){p+='/'+x;html+=' / <a href="#" data-p="'+p+'">'+esc(x)+'</a>'}$('#crumb').innerHTML=html;$('#crumb').querySelectorAll('a').forEach(a=>a.onclick=e=>{e.preventDefault();path=a.dataset.p;load()})}
const esc=s=>s.replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
async function load(){crumbs();let r=await fetch('/api/list?path='+encodeURIComponent(path),{cache:'no-store'}),d=await r.json();if(!r.ok){alert(d.error);return}$('#space').textContent=size(d.freeKiB*1024)+' '+t.free;let a=d.entries.sort((x,y)=>x.directory!==y.directory?x.directory?-1:1:natural(x,y));$('#list').innerHTML=a.length?a.map(x=>'<li class="'+(x.directory?'folder':'file')+'"><a class="name" href="#">'+(x.directory?'[+] ':'')+esc(x.name)+'</a><span class="meta">'+(x.directory?'':size(x.size))+'</span><button>'+t.del+'</button></li>').join(''):'<li>'+t.empty+'</li>';[...$('#list').children].forEach((li,i)=>{if(!a[i])return;li.querySelector('.name').onclick=e=>{e.preventDefault();if(a[i].directory){path=(path==='/'?'':path)+'/'+a[i].name;load()}};li.querySelector('button').onclick=()=>remove(a[i])})}
async function remove(x){if(!confirm(t.confirm))return;let p=(path==='/'?'':path)+'/'+x.name,r=await fetch('/api/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'path='+encodeURIComponent(p)});if(!r.ok)alert((await r.json()).error);load()}
$('#mkdir').onclick=async()=>{let n=prompt(t.name);if(!n)return;let r=await fetch('/api/mkdir',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'path='+encodeURIComponent(path)+'&name='+encodeURIComponent(n)});if(!r.ok)alert((await r.json()).error);load()};
$('#pick').onclick=()=>$('#files').click();$('#files').onchange=()=>queue([...$('#files').files]);let drop=$('#drop');for(const ev of ['dragenter','dragover'])drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.add('over')});for(const ev of ['dragleave','drop'])drop.addEventListener(ev,e=>{e.preventDefault();drop.classList.remove('over')});drop.addEventListener('drop',e=>queue([...e.dataTransfer.files]));
async function queue(files){for(const f of files.filter(x=>x.name.toLowerCase().endsWith('.epub'))){let div=document.createElement('div');div.className='job data';div.innerHTML=esc(f.name)+'<progress max="100" value="0"></progress><span>0%</span>';$('#jobs').append(div);try{await upload(f,div)}catch(e){div.querySelector('span').textContent=t.failed+': '+e.message}}load()}
function upload(f,row){return new Promise((ok,no)=>{let x=new XMLHttpRequest(),form=new FormData();form.append('file',f,f.name);x.open('POST','/api/upload');x.setRequestHeader('X-Dest-Path',path);x.setRequestHeader('X-File-Size',f.size);x.upload.onprogress=e=>{if(e.lengthComputable){let p=Math.round(e.loaded*100/e.total);row.querySelector('progress').value=p;row.querySelector('span').textContent=p+'%'}};x.onload=()=>x.status<300?ok():no(Error(x.responseText));x.onerror=()=>no(Error('network'));x.send(form)})}load();
</script></body></html>)PORTAL";
#endif
