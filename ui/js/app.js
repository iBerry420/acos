(function(){
"use strict";

var IC={
chat:'<path d="M7.9 20A9 9 0 1 0 4 16.1L2 22z"/>',
folder:'<path d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2z"/>',
activity:'<polyline points="22 12 18 12 15 21 9 3 6 12 2 12"/>',
gear:'<circle cx="12" cy="12" r="3"/><path d="M12 1v4m0 14v4M4.93 4.93l2.83 2.83m8.48 8.48l2.83 2.83M1 12h4m14 0h4M4.93 19.07l2.83-2.83m8.48-8.48l2.83-2.83"/>',
server:'<rect x="2" y="2" width="20" height="8" rx="2"/><rect x="2" y="14" width="20" height="8" rx="2"/><circle cx="6" cy="6" r="1" fill="currentColor" stroke="none"/><circle cx="6" cy="18" r="1" fill="currentColor" stroke="none"/>',
send:'<line x1="22" y1="2" x2="11" y2="13"/><polygon points="22 2 15 22 11 13 2 9 22 2"/>',
stop:'<rect x="6" y="6" width="12" height="12" rx="2"/>',
menu:'<line x1="4" y1="6" x2="20" y2="6"/><line x1="4" y1="12" x2="20" y2="12"/><line x1="4" y1="18" x2="20" y2="18"/>',
xic:'<line x1="18" y1="6" x2="6" y2="18"/><line x1="6" y1="6" x2="18" y2="18"/>',
chevR:'<polyline points="9 18 15 12 9 6"/>',
chevD:'<polyline points="6 9 12 15 18 9"/>',
file:'<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/>',
copy:'<rect x="9" y="9" width="13" height="13" rx="2"/><path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>',
check:'<polyline points="20 6 9 17 4 12"/>',
user:'<path d="M20 21v-2a4 4 0 0 0-4-4H8a4 4 0 0 0-4 4v2"/><circle cx="12" cy="7" r="4"/>',
logout:'<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><polyline points="16 17 21 12 16 7"/><line x1="21" y1="12" x2="9" y2="12"/>',
edit:'<path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"/><path d="M18.5 2.5a2.12 2.12 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"/>',
save:'<path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"/><polyline points="17 21 17 13 7 13 7 21"/><polyline points="7 3 7 8 15 8"/>',
brain:'<path d="M12 2a7 7 0 0 0-7 7c0 3.25 2.76 6.43 5 8.37V20h4v-2.63c2.24-1.94 5-5.12 5-8.37a7 7 0 0 0-7-7z"/><path d="M10 20v2m4-2v2"/>',
wrench:'<path d="M14.7 6.3a1 1 0 0 0 0 1.4l1.6 1.6a1 1 0 0 0 1.4 0l3.77-3.77a6 6 0 0 1-7.94 7.94l-6.91 6.91a2.12 2.12 0 0 1-3-3l6.91-6.91a6 6 0 0 1 7.94-7.94l-3.76 3.76z"/>',
dot:'<circle cx="12" cy="12" r="5" fill="currentColor" stroke="none"/>',
clock:'<circle cx="12" cy="12" r="10"/><polyline points="12 6 12 12 16 14"/>',
plus:'<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>',
note:'<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z"/><polyline points="14 2 14 8 20 8"/><line x1="16" y1="13" x2="8" y2="13"/><line x1="16" y1="17" x2="8" y2="17"/>',
context:'<path d="M21 15a2 2 0 0 1-2 2H7l-4 4V5a2 2 0 0 1 2-2h14a2 2 0 0 1 2 2z"/><line x1="9" y1="9" x2="15" y2="9"/><line x1="9" y1="13" x2="13" y2="13"/>',
key:'<path d="M21 2l-2 2m-7.61 7.61a5.5 5.5 0 1 1-7.78 7.78 5.5 5.5 0 0 1 7.78-7.78zm0 0L15.5 7.5m0 0l3 3L22 7l-3-3m-3.5 3.5L19 4"/>',
trash:'<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>',
log:'<path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/>',
image:'<rect x="3" y="3" width="18" height="18" rx="2"/><circle cx="8.5" cy="8.5" r="1.5"/><polyline points="21 15 16 10 5 21"/>',
upload:'<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="17 8 12 3 7 8"/><line x1="12" y1="3" x2="12" y2="15"/>',
video:'<polygon points="23 7 16 12 23 17 23 7"/><rect x="1" y="5" width="15" height="14" rx="2"/>',
refresh:'<polyline points="23 4 23 10 17 10"/><path d="M20.49 15a9 9 0 1 1-2.12-9.36L23 10"/>',
download:'<path d="M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4"/><polyline points="7 10 12 15 17 10"/><line x1="12" y1="15" x2="12" y2="3"/>',
play:'<polygon points="5 3 19 12 5 21 5 3"/>',
plus:'<line x1="12" y1="5" x2="12" y2="19"/><line x1="5" y1="12" x2="19" y2="12"/>',
trash:'<polyline points="3 6 5 6 21 6"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"/>',
chevron:'<polyline points="9 18 15 12 9 6"/>',
zap:'<polygon points="13 2 3 14 12 14 11 22 21 10 12 10 13 2"/>',
globe:'<circle cx="12" cy="12" r="10"/><line x1="2" y1="12" x2="22" y2="12"/><path d="M12 2a15.3 15.3 0 0 1 4 10 15.3 15.3 0 0 1-4 10 15.3 15.3 0 0 1-4-10 15.3 15.3 0 0 1 4-10z"/>',
code:'<polyline points="16 18 22 12 16 6"/><polyline points="8 6 2 12 8 18"/>',
users:'<path d="M17 21v-2a4 4 0 0 0-4-4H5a4 4 0 0 0-4 4v2"/><circle cx="9" cy="7" r="4"/><path d="M23 21v-2a4 4 0 0 0-3-3.87"/><path d="M16 3.13a4 4 0 0 1 0 7.75"/>',
shield:'<path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/>',
lock:'<rect x="3" y="11" width="18" height="11" rx="2"/><path d="M7 11V7a5 5 0 0 1 10 0v4"/>',
search:'<circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/>',
book:'<path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/><line x1="8" y1="7" x2="16" y2="7"/><line x1="8" y1="11" x2="14" y2="11"/>',
grid:'<rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/><rect x="14" y="14" width="7" height="7"/>',
chevLeft:'<polyline points="15 18 9 12 15 6"/>',
pause:'<rect x="6" y="4" width="4" height="16"/><rect x="14" y="4" width="4" height="16"/>',
home:'<path d="M3 9l9-7 9 7v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><polyline points="9 22 9 12 15 12 15 22"/>',
dot:'<circle cx="12" cy="12" r="3"/>'
};
function ic(n,s){s=s||18;return '<svg width="'+s+'" height="'+s+'" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'+(IC[n]||'')+'</svg>';}

/* ── State ────────────────────────────────────────────── */
var S={
token:localStorage.getItem('avacli_token')||'',
user:null,route:'',model:'',models:[],session:'',mode:'agent',
messages:[],streaming:false,streamStart:0,status:null,
fileTree:null,filePath:'',fileContent:'',fileEditing:false,
sidebarOpen:false,
useHistory:localStorage.getItem('ava_use_history')!=='false',
notes:(function(){try{return JSON.parse(localStorage.getItem('ava_notes')||'[]');}catch(e){return[];}})(),
usageData:null,
hasXaiKey:false,
extraModel:'',
maxOutputTokens:parseInt(localStorage.getItem('ava_max_output_tokens'))||0,
contextTokenLimit:parseInt(localStorage.getItem('ava_context_token_limit'))||0,
reasoningEffort:localStorage.getItem('ava_reasoning_effort')||'high',
lastTokenUsage:null,
toolLog:[]
};
window.setModelFromSelect=function(el){
if(!el||typeof el.value!=='string')return;
S.model=el.value;
api('/api/settings',{method:'POST',body:{model:S.model}}).then(function(r){
}).catch(function(){});
updateMediaSettings();
render();
setTimeout(function(){scrollToBottom(true);},120);
};
var chatAbort=null,autoScroll=true,rafPending=false;
var expandedDirs={},treePaths=[];
var openPopover=null;

/* ── Utilities ────────────────────────────────────────── */
function esc(s){if(!s)return '';return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;').replace(/"/g,'&quot;');}

function api(path,opts){
opts=opts||{};
var silent=!!opts.silent;delete opts.silent;
var h=Object.assign({},opts.headers||{});
var t=localStorage.getItem('avacli_token');if(t)h['Authorization']='Bearer '+t;
if(S.token)h['Authorization']='Bearer '+S.token;
if(opts.body&&typeof opts.body==='object'&&!(opts.body instanceof FormData)){h['Content-Type']='application/json';opts.body=JSON.stringify(opts.body);}
opts.headers=h;
return fetch(path,opts).then(function(r){
if(r.status===401){S.token='';localStorage.removeItem('ava_token');localStorage.removeItem('avacli_token');if(location.pathname!=='/login')location.reload();return Promise.reject(new Error('unauthorized'));}
if(!r.ok)return r.text().then(function(t){var msg='Request failed';try{var j=JSON.parse(t);if(j&&j.error)msg=j.error;}catch(e){if(t)msg=t;}throw new Error(msg);});
return r.json();
}).catch(function(e){if(e.name!=='AbortError'&&!silent)toast(e.message,'error');throw e;});
}

function toast(msg,type){
type=type||'info';
var c=document.getElementById('toasts');if(!c)return;
var el=document.createElement('div');
el.className='toast toast-'+type;el.textContent=msg;
c.appendChild(el);
setTimeout(function(){el.style.opacity='0';el.style.transition='opacity .3s';setTimeout(function(){el.remove();},300);},3500);
}

/* ── Custom Modal System ──────────────────────────────── */
function avaConfirm(msg,onOk,opts){
opts=opts||{};
var danger=opts.danger!==false;
var title=opts.title||'Confirm';
var okText=opts.okText||(danger?'Delete':'Confirm');
var root=document.getElementById('avaModal');if(!root)return;
root.innerHTML='<div class="ava-modal-backdrop" onclick="if(event.target===this)avaCloseModal()"><div class="ava-modal">'+
'<div class="ava-modal-head">'+esc(title)+'</div>'+
'<div class="ava-modal-body">'+esc(msg)+'</div>'+
'<div class="ava-modal-foot"><button class="ava-modal-cancel" onclick="avaCloseModal()">Cancel</button>'+
'<button class="'+(danger?'ava-modal-danger':'ava-modal-ok')+'" id="avaModalOk">'+esc(okText)+'</button></div></div></div>';
var okBtn=document.getElementById('avaModalOk');
if(okBtn)okBtn.onclick=function(){avaCloseModal();if(onOk)onOk();};
setTimeout(function(){var bd=root.querySelector('.ava-modal-backdrop');if(bd)bd.classList.add('show');},10);
}
window.avaConfirm=avaConfirm;
function avaPrompt(msg,defaultVal,onSubmit,opts){
opts=opts||{};
var title=opts.title||'Input';
var okText=opts.okText||'Submit';
var inputType=opts.inputType||'text';
var root=document.getElementById('avaModal');if(!root)return;
root.innerHTML='<div class="ava-modal-backdrop" onclick="if(event.target===this)avaCloseModal()"><div class="ava-modal">'+
'<div class="ava-modal-head">'+esc(title)+'</div>'+
'<div class="ava-modal-body">'+esc(msg)+'<input type="'+inputType+'" id="avaModalInput" value="'+esc(defaultVal||'')+'"></div>'+
'<div class="ava-modal-foot"><button class="ava-modal-cancel" onclick="avaCloseModal()">Cancel</button>'+
'<button class="ava-modal-ok" id="avaModalOk">'+esc(okText)+'</button></div></div></div>';
var inp=document.getElementById('avaModalInput');
var okBtn=document.getElementById('avaModalOk');
function doSubmit(){var val=inp?inp.value:'';avaCloseModal();if(onSubmit)onSubmit(val);}
if(okBtn)okBtn.onclick=doSubmit;
if(inp)inp.onkeydown=function(e){if(e.key==='Enter')doSubmit();if(e.key==='Escape')avaCloseModal();};
setTimeout(function(){var bd=root.querySelector('.ava-modal-backdrop');if(bd)bd.classList.add('show');if(inp){inp.focus();inp.select();}},10);
}
window.avaPrompt=avaPrompt;
function avaCloseModal(){
var root=document.getElementById('avaModal');if(!root)return;
var bd=root.querySelector('.ava-modal-backdrop');
if(bd){bd.classList.remove('show');setTimeout(function(){root.innerHTML='';},200);}
else{root.innerHTML='';}
}
window.avaCloseModal=avaCloseModal;

function fmtNum(n){if(n==null)return '--';if(n>=1e6)return (n/1e6).toFixed(1)+'M';if(n>=1e3)return (n/1e3).toFixed(1)+'k';return String(n);}
function fmtDate(ts){var d=new Date(ts*1000);return d.toLocaleDateString('en-US',{month:'short',day:'numeric'});}
function fmtTime(ts){var d=new Date(ts*1000);return d.toLocaleTimeString('en-US',{hour:'2-digit',minute:'2-digit'});}

function closePopovers(){
openPopover=null;
document.querySelectorAll('.popover.show').forEach(function(el){el.classList.remove('show');});
}
function restoreToolbarPopover(id){
if(openPopover!==id)return;
requestAnimationFrame(function(){
var pop=document.getElementById(id);
if(pop)pop.classList.add('show');
});
}
document.addEventListener('click',function(e){
if(openPopover&&!e.target.closest('.toolbar-group')){closePopovers();}
});

/* ── Markdown ─────────────────────────────────────────── */
function md(src){
if(!src)return '';
var html='',inCode=false,codeLang='',codeLines=[],inUl=false,inOl=false,inTable=false,tableRows=[];
var lines=src.split('\n');
function e(t){return t.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
function inl(t){
var r=e(t);
r=r.replace(/!\[([^\]]*)\]\(([^)]*\.(?:mp4|webm|mov))\)/gi,function(_,alt,u){return '<div class="media-embed"><video src="'+u+'" controls preload="metadata" class="media-video"><\/video><div class="media-caption">'+alt+'<\/div><\/div>';});
r=r.replace(/!\[([^\]]*)\]\(([^)]+)\)/g,function(_,alt,u){return '<div class="media-embed"><img src="'+u+'" alt="'+alt+'" class="media-img" onclick="openLightbox(this.src)"><div class="media-caption">'+alt+'<\/div><\/div>';});
r=r.replace(/`([^`]+)`/g,'<code class="ilc">$1</code>');
r=r.replace(/\*\*(.+?)\*\*/g,'<strong>$1</strong>');
r=r.replace(/\*([^*]+)\*/g,'<em>$1</em>');
r=r.replace(/~~(.+?)~~/g,'<del>$1</del>');
r=r.replace(/\[([^\]]+)\]\(([^)]*)\)/g,'<a href="$2" target="_blank" rel="noopener">$1</a>');
return r;
}
function closeList(){if(inUl){html+='</ul>';inUl=false;}if(inOl){html+='</ol>';inOl=false;}}
function closeTable(){
if(!inTable)return;
inTable=false;
if(tableRows.length<1)return;
html+='<table>';
var headerCells=tableRows[0].split('|').map(function(c){return c.trim();}).filter(Boolean);
html+='<thead><tr>';headerCells.forEach(function(c){html+='<th>'+inl(c)+'</th>';});html+='</tr></thead><tbody>';
for(var r=2;r<tableRows.length;r++){
var cells=tableRows[r].split('|').map(function(c){return c.trim();}).filter(Boolean);
html+='<tr>';cells.forEach(function(c){html+='<td>'+inl(c)+'</td>';});html+='</tr>';
}
html+='</tbody></table>';
tableRows=[];
}
for(var i=0;i<lines.length;i++){
var line=lines[i];
if(line.startsWith('```')){
if(inCode){closeList();closeTable();
if(codeLang==='mermaid'){var mid='mm'+Math.random().toString(36).slice(2,8);
html+='<div class="mermaid-block"><div class="mermaid-head"><span>mermaid</span></div><div class="mermaid-render" id="'+mid+'" data-mermaid-src="'+codeLines.join('\n').replace(/"/g,'&quot;')+'"></div></div>';
}else{var cid='cb'+Math.random().toString(36).slice(2,8);
html+='<div class="codeblock"><div class="cb-head"><span>'+e(codeLang||'code')+'</span><button onclick="copyCode(\''+cid+'\')">'+ic('copy',12)+' Copy</button></div><pre id="'+cid+'"><code>'+codeLines.join('\n')+'</code></pre></div>';}
inCode=false;codeLines=[];codeLang='';
}else{closeList();closeTable();inCode=true;codeLang=line.slice(3).trim();}
continue;}
if(inCode){codeLines.push(e(line));continue;}
if(line.indexOf('|')!==-1&&line.trim().startsWith('|')){
if(!inTable){closeList();inTable=true;tableRows=[];}
tableRows.push(line);continue;
}
if(inTable){closeTable();}
var hm=line.match(/^(#{1,4})\s+(.*)/);
if(hm){closeList();html+='<h'+hm[1].length+'>'+inl(hm[2])+'</h'+hm[1].length+'>';continue;}
if(/^---+$/.test(line.trim())){closeList();html+='<hr>';continue;}
if(/^>\s/.test(line)){closeList();html+='<blockquote>'+inl(line.slice(2))+'</blockquote>';continue;}
if(/^[-*]\s/.test(line)){if(!inUl){closeList();html+='<ul>';inUl=true;}html+='<li>'+inl(line.replace(/^[-*]\s/,''))+'</li>';continue;}
if(/^\d+\.\s/.test(line)){if(!inOl){closeList();html+='<ol>';inOl=true;}html+='<li>'+inl(line.replace(/^\d+\.\s/,''))+'</li>';continue;}
closeList();
if(line.trim())html+='<p>'+inl(line)+'</p>';
}
closeList();closeTable();
if(inCode&&codeLines.length){
if(codeLang==='mermaid'){var mid2='mm'+Math.random().toString(36).slice(2,8);
html+='<div class="mermaid-block"><div class="mermaid-head"><span>mermaid</span></div><div class="mermaid-render" id="'+mid2+'" data-mermaid-src="'+codeLines.join('\n').replace(/"/g,'&quot;')+'"></div></div>';
}else{var cid2='cb'+Math.random().toString(36).slice(2,8);
html+='<div class="codeblock"><div class="cb-head"><span>'+e(codeLang||'code')+'</span><button onclick="copyCode(\''+cid2+'\')">'+ic('copy',12)+' Copy</button></div><pre id="'+cid2+'"><code>'+codeLines.join('\n')+'</code></pre></div>';}}
return html;
}
window.copyCode=function(id){
var el=document.getElementById(id);if(!el)return;
navigator.clipboard.writeText(el.textContent).then(function(){toast('Copied','success');}).catch(function(){});
};
window.openLightbox=function(src){
var lb=document.createElement('div');lb.id='lightbox';lb.onclick=function(e){if(e.target===lb)lb.remove();};
if(/\.(mp4|webm|mov)(\?|$)/i.test(src))lb.innerHTML='<video src="'+src+'" controls autoplay style="max-width:90vw;max-height:90vh;border-radius:8px">';
else lb.innerHTML='<img src="'+src+'">';
document.body.appendChild(lb);
};
window.toggleMediaContext=function(msgIdx,mediaIdx){
var m=S.messages[msgIdx];if(!m||!m.media||!m.media[mediaIdx])return;
m.media[mediaIdx].inContext=!m.media[mediaIdx].inContext;
var btn=document.querySelector('[data-media-toggle="'+msgIdx+'-'+mediaIdx+'"]');
if(btn){btn.className='media-context-toggle'+(m.media[mediaIdx].inContext?' active':'');btn.textContent=m.media[mediaIdx].inContext?'In context':'Use in context';}
};

/* ── Assets ───────────────────────────────────────────── */
/* ── Tools Page ───────────────────────────────────────── */
function fetchTools(cb){
api('/api/tools').then(function(arr){
S.toolsList=Array.isArray(arr)?arr:[];if(cb)cb();else render();
}).catch(function(){S.toolsList=[];if(cb)cb();else render();});
}
function fetchVault(cb){
api('/api/vault').then(function(d){
S.vaultEntries=(d&&d.entries)||[];S.vaultExists=d&&d.vault_exists;if(cb)cb();else render();
}).catch(function(){S.vaultEntries=[];if(cb)cb();else render();});
}
function fetchApiRegistry(cb){
api('/api/api-registry').then(function(d){
S.apiRegistry=(d&&d.apis)||[];if(cb)cb();else render();
}).catch(function(){S.apiRegistry=[];if(cb)cb();else render();});
}

function renderTools(){
if(!S.toolsList){fetchTools();return '<div class="tools-layout"><div class="tools-body" style="display:flex;align-items:center;justify-content:center"><div class="spinner"></div></div></div>';}
var builtins=S.toolsList.filter(function(t){return t.category==='builtin';});
var aiTools=S.toolsList.filter(function(t){return t.category==='ai_created';});
var userTools=S.toolsList.filter(function(t){return t.category==='user_created';});

if(!S.toolsTab)S.toolsTab='ai_tools';
var tabs='<button class="tools-tab'+(S.toolsTab==='ai_tools'?' active':'')+'" onclick="setToolsTab(\'ai_tools\')">'+ic('zap',11)+' AI Tools ('+aiTools.length+')</button>';
tabs+='<button class="tools-tab'+(S.toolsTab==='builtin'?' active':'')+'" onclick="setToolsTab(\'builtin\')">Built-in ('+builtins.length+')</button>';
tabs+='<button class="tools-tab'+(S.toolsTab==='custom'?' active':'')+'" onclick="setToolsTab(\'custom\')">User ('+userTools.length+')</button>';
tabs+='<button class="tools-tab'+(S.toolsTab==='create'?' active':'')+'" onclick="setToolsTab(\'create\')">'+ic('plus',11)+' Create</button>';
tabs+='<button class="tools-tab'+(S.toolsTab==='vault'?' active':'')+'" onclick="setToolsTab(\'vault\')">'+ic('lock',11)+' Vault</button>';
tabs+='<button class="tools-tab'+(S.toolsTab==='apis'?' active':'')+'" onclick="setToolsTab(\'apis\')">'+ic('globe',11)+' APIs</button>';

var body='';
if(S.toolsTab==='ai_tools'){
body+='<div style="font-size:.75rem;color:var(--text-muted);margin-bottom:.75rem">Tools created by the AI during chat sessions. These are Python scripts the AI built to extend its own capabilities.</div>';
if(!aiTools.length){body+='<div style="color:var(--text-muted);text-align:center;padding:2rem">No AI-created tools yet. Ask the AI to create a tool during a chat session, or use the Create tab.</div>';}
else{for(var i=0;i<aiTools.length;i++)body+=renderToolItem(aiTools[i],i);}
}else if(S.toolsTab==='builtin'){
body+='<div style="font-size:.75rem;color:var(--text-muted);margin-bottom:.75rem">Built-in agent tools. These cannot be modified or disabled.</div>';
for(var i=0;i<builtins.length;i++)body+=renderToolItem(builtins[i],i);
}else if(S.toolsTab==='custom'){
if(!userTools.length){body+='<div style="color:var(--text-muted);text-align:center;padding:2rem">No user-created tools. Go to the Create tab to generate one.</div>';}
else{for(var ci=0;ci<userTools.length;ci++)body+=renderToolItem(userTools[ci],builtins.length+ci);}
}else if(S.toolsTab==='create'){
body+=renderToolCreateForm();
}else if(S.toolsTab==='vault'){
body+=renderVaultTab();
}else if(S.toolsTab==='apis'){
body+=renderApisTab();
}

return '<div class="tools-layout">'+
'<div class="tools-header"><h2>'+ic('wrench',18)+' Tool Forge</h2></div>'+
'<div class="tools-tabs" style="overflow-x:auto;scrollbar-width:thin">'+tabs+'</div>'+
'<div class="tools-body" id="toolsBody">'+body+'</div>'+
'</div>';
}

function renderToolItem(tool,idx){
var expanded=S.toolsExpanded===tool.name;
var cat=tool.category||'builtin';
var badgeCls=cat==='ai_created'?'custom':cat==='user_created'?'custom':'builtin';
var badgeLabel=cat==='ai_created'?'AI Created':cat==='user_created'?'User':'Built-in';
var badge='<span class="tool-item-badge '+badgeCls+'">'+badgeLabel+'</span>';
var desc=tool.description||'';
if(desc.length>80)desc=desc.slice(0,80)+'...';
var h='<div class="tool-item" data-tool-name="'+esc(tool.name)+'">';
h+='<div class="tool-item-head" onclick="toggleToolExpand(\''+esc(tool.name)+'\')">';
h+='<span class="tool-item-expand'+(expanded?' open':'')+'\'" id="toolExp_'+esc(tool.name)+'">'+ic('chevron',12)+'</span>';
h+='<span class="tool-item-name">'+esc(tool.name)+'</span>';
h+=badge;
h+='<span class="tool-item-desc">'+esc(desc)+'</span>';
h+='</div>';
h+='<div class="tool-item-detail" id="toolDetail_'+esc(tool.name)+'" style="display:'+(expanded?'block':'none')+'">';
h+='<div style="margin-bottom:.375rem;color:var(--text-secondary)">'+esc(tool.description||'No description')+'</div>';
h+='<div style="font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">Parameters</div>';
h+='<pre>'+esc(JSON.stringify(tool.parameters||{},null,2))+'</pre>';
if(tool.is_custom){
h+='<div class="tool-item-actions">';
h+='<button class="toolbar-btn" onclick="testToolPanel(\''+esc(tool.name)+'\')">' +ic('play',12)+' Test</button>';
h+='<button class="toolbar-btn" style="color:#f87171" onclick="deleteCustomTool(\''+esc(tool.name)+'\')">' +ic('trash',12)+' Delete</button>';
h+='</div>';
h+=renderToolTestPanel(tool);
}
h+='</div>';
h+='</div>';
return h;
}

function renderToolTestPanel(tool){
var testId='toolTest_'+tool.name;
var h='<div class="tool-test-panel" id="'+testId+'">';
h+='<label>Input JSON</label>';
h+='<textarea id="testInput_'+tool.name+'" placeholder=\'{"key":"value"}\'>'+(S.toolsTestInput&&S.toolsTestInput[tool.name]||'{}')+'</textarea>';
h+='<div style="display:flex;gap:.5rem;margin-top:.375rem">';
h+='<button class="toolbar-btn active" onclick="runToolTest(\''+esc(tool.name)+'\')">'+ic('play',10)+' Run</button>';
h+='<button class="toolbar-btn" onclick="genMockInput(\''+esc(tool.name)+'\')">'+ic('zap',10)+' Generate Mock</button>';
h+='</div>';
if(S.toolsTestResult&&S.toolsTestResult.name===tool.name){
var tr=S.toolsTestResult;
var cls=tr.exit_code===0?'success':'error';
h+='<div class="tool-test-output '+cls+'">'+esc(typeof tr.output==='object'?JSON.stringify(tr.output,null,2):(tr.output||tr.error||'No output'))+'</div>';
}
h+='</div>';
return h;
}

function renderToolCreateForm(){
var models=S.models.filter(function(m){return m.id.indexOf('imagine')<0;});
var h='<div class="tool-gen-form">';
h+='<div style="font-size:.875rem;font-weight:600;margin-bottom:.5rem">'+ic('zap',14)+' Generate a Python Tool</div>';
h+='<div style="font-size:.75rem;color:var(--text-muted);margin-bottom:.75rem">Describe what you want the tool to do and the AI will generate a Python script with structured input/output.</div>';
h+='<label>Tool Name (optional)</label>';
h+='<input id="toolGenName" placeholder="custom_my_tool" value="">';
h+='<label>Description *</label>';
h+='<textarea id="toolGenDesc" placeholder="Describe what this tool should do, what inputs it takes, and what it should output..."></textarea>';
h+='<label>Model</label>';
h+='<select id="toolGenModel">';
for(var mi=0;mi<models.length;mi++){
var defExtra=S.extraModel||'grok-4-1-non-reasoning';
var sel=models[mi].id===defExtra?' selected':'';
h+='<option value="'+models[mi].id+'"'+sel+'>'+esc(models[mi].label||models[mi].id)+'</option>';
}
h+='</select>';
h+='<div style="margin-top:.75rem">';
if(S.toolsGenBusy){
h+='<button class="toolbar-btn" disabled>'+ic('refresh',12)+' Generating...</button>';
}else{
h+='<button class="toolbar-btn active" onclick="generateTool()">'+ic('zap',12)+' Generate Tool</button>';
}
h+='</div>';
if(S.toolsGenResult){
h+='<div style="margin-top:.75rem;border:1px solid var(--border);border-radius:6px;padding:.5rem">';
h+='<div style="font-size:.75rem;font-weight:600;color:var(--accent-pale);margin-bottom:.25rem">Generated: '+esc(S.toolsGenResult.name)+'</div>';
h+='<div style="font-size:.6875rem;color:var(--text-secondary);margin-bottom:.375rem">'+esc(S.toolsGenResult.description)+'</div>';
h+='<div style="font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">Script</div>';
h+='<pre style="background:var(--bg-body);border:1px solid var(--border);border-radius:4px;padding:.5rem;font-size:.6875rem;max-height:200px;overflow:auto">'+esc(S.toolsGenResult.script)+'</pre>';
h+='</div>';
}
h+='</div>';
return h;
}

function renderVaultTab(){
if(!S.vaultEntries&&S.vaultEntries!==false){fetchVault();return '<div style="display:flex;align-items:center;justify-content:center;padding:2rem"><div class="spinner"></div></div>';}
var h='<div style="font-size:.75rem;color:var(--text-muted);margin-bottom:.75rem">Encrypted API key vault (AES-256-GCM). Keys are encrypted at rest and only decrypted when needed by <code style="color:var(--accent-pale)">call_api</code>.</div>';
h+='<div class="tool-gen-form" style="margin-bottom:1rem">';
h+='<div style="font-size:.875rem;font-weight:600;margin-bottom:.5rem">'+ic('plus',14)+' Add Secret</div>';
h+='<label>Key Name</label><input id="vaultName" placeholder="e.g. notion_api_key">';
h+='<label>Service</label><input id="vaultService" placeholder="e.g. Notion">';
h+='<label>Secret Value</label><input id="vaultValue" type="password" placeholder="Paste your API key here">';
h+='<div style="margin-top:.75rem"><button class="toolbar-btn active" onclick="vaultStore()">'+ic('lock',12)+' Encrypt & Store</button></div>';
h+='</div>';
if(!S.vaultEntries||!S.vaultEntries.length){
h+='<div style="color:var(--text-muted);text-align:center;padding:1rem">Vault is empty. Add API keys above or ask the AI to store them during a chat.</div>';
}else{
for(var i=0;i<S.vaultEntries.length;i++){
var e=S.vaultEntries[i];
h+='<div class="tool-item"><div class="tool-item-head">';
h+='<span class="tool-item-name">'+ic('lock',12)+' '+esc(e.name)+'</span>';
if(e.service)h+='<span class="tool-item-badge custom">'+esc(e.service)+'</span>';
h+='<span class="tool-item-desc">Updated: '+(e.updated_at||'—')+'</span>';
h+='<button class="toolbar-btn" style="color:#f87171;margin-left:auto" onclick="vaultRemove(\''+esc(e.name)+'\')">'+ic('trash',12)+'</button>';
h+='</div></div>';
}
}
return h;
}

function renderApisTab(){
if(!S.apiRegistry&&S.apiRegistry!==false){fetchApiRegistry();return '<div style="display:flex;align-items:center;justify-content:center;padding:2rem"><div class="spinner"></div></div>';}
var h='<div style="font-size:.75rem;color:var(--text-muted);margin-bottom:.75rem">Registered third-party APIs. The AI can research, configure, and call these APIs using encrypted vault credentials. Ask the AI to <code style="color:var(--accent-pale)">research_api</code> to discover new ones.</div>';
if(!S.apiRegistry||!S.apiRegistry.length){
h+='<div style="color:var(--text-muted);text-align:center;padding:2rem">No APIs registered yet. Ask the AI to research and set up an API during a chat session.</div>';
}else{
for(var i=0;i<S.apiRegistry.length;i++){
var api=S.apiRegistry[i];
var expanded=S.toolsExpanded===('api_'+api.slug);
h+='<div class="tool-item">';
h+='<div class="tool-item-head" onclick="toggleToolExpand(\'api_'+esc(api.slug)+'\')">';
h+='<span class="tool-item-expand'+(expanded?' open':'')+'">'+ic('chevron',12)+'</span>';
h+='<span class="tool-item-name">'+esc(api.name||api.slug)+'</span>';
h+='<span class="tool-item-badge '+(api.status==='active'?'builtin':'custom')+'">'+esc(api.status||'configured')+'</span>';
h+='<span class="tool-item-desc">'+esc(api.base_url)+'</span>';
h+='</div>';
if(expanded){
h+='<div class="tool-item-detail">';
h+='<div style="font-size:.75rem;color:var(--text-secondary);margin-bottom:.375rem"><strong>Auth:</strong> '+esc(api.auth_type)+' | <strong>Vault key:</strong> '+(api.vault_key_name?esc(api.vault_key_name):'<em>none</em>')+'</div>';
if(api.documentation_url)h+='<div style="font-size:.75rem;margin-bottom:.375rem"><a href="'+esc(api.documentation_url)+'" target="_blank" style="color:var(--accent-light)">Documentation</a></div>';
if(api.endpoints&&api.endpoints.length){
h+='<div style="font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">Endpoints ('+api.endpoints.length+')</div>';
h+='<pre>'+esc(JSON.stringify(api.endpoints,null,2))+'</pre>';
}
h+='<div class="tool-item-actions">';
h+='<button class="toolbar-btn" style="color:#f87171" onclick="removeApiReg(\''+esc(api.slug)+'\')">'+ic('trash',12)+' Remove</button>';
h+='</div>';
h+='</div>';
}
h+='</div>';
}
}
return h;
}

window.setToolsTab=function(t){S.toolsTab=t;render();};
window.toggleToolExpand=function(name){
 var wasExp=S.toolsExpanded===name;
 S.toolsExpanded=wasExp?null:name;
 // In-place DOM toggle — no full re-render to prevent scroll-to-top
 var detail=document.getElementById('toolDetail_'+name);
 var expBtn=document.getElementById('toolExp_'+name);
 if(detail)detail.style.display=S.toolsExpanded===name?'block':'none';
 if(expBtn)expBtn.className='tool-item-expand'+(S.toolsExpanded===name?' open':'');
 // Fallback if IDs not found (different tab rendering)
 if(!detail&&!expBtn)render();
};
if(!S.toolsTestInput)S.toolsTestInput={};

window.generateTool=function(){
var desc=(document.getElementById('toolGenDesc')||{}).value||'';
var name=(document.getElementById('toolGenName')||{}).value||'';
var model=(document.getElementById('toolGenModel')||{}).value||S.extraModel||'grok-4-1-non-reasoning';
if(!desc.trim()){toast('Please enter a description','error');return;}
S.toolsGenBusy=true;S.toolsGenResult=null;render();
api('/api/tools/generate',{method:'POST',body:{description:desc,name:name,model:model}})
.then(function(r){return r.json();}).then(function(d){
S.toolsGenBusy=false;
if(d.error){toast(d.error,'error');}
else{S.toolsGenResult=d;S.toolsList=null;toast('Tool generated: '+d.name,'success');}
render();
}).catch(function(e){S.toolsGenBusy=false;toast('Generation failed','error');render();});
};

window.runToolTest=function(name){
var inp=(document.getElementById('testInput_'+name)||{}).value||'{}';
S.toolsTestInput[name]=inp;
var parsed;try{parsed=JSON.parse(inp);}catch(e){toast('Invalid JSON input','error');return;}
S.toolsTestResult={name:name,running:true};render();
api('/api/tools/test',{method:'POST',body:{name:name,input:parsed}})
.then(function(r){return r.json();}).then(function(d){
d.name=name;S.toolsTestResult=d;render();
}).catch(function(){S.toolsTestResult={name:name,error:'Request failed'};render();});
};

window.genMockInput=function(name){
var tool=null;if(S.toolsList){for(var i=0;i<S.toolsList.length;i++){if(S.toolsList[i].name===name){tool=S.toolsList[i];break;}}}
if(!tool){toast('Tool not found','error');return;}
toast('Generating mock data...','info');
api('/api/tools/mock',{method:'POST',body:{name:name,description:tool.description,parameters:tool.parameters,model:S.extraModel||'grok-4-1-non-reasoning'}})
.then(function(r){return r.json();}).then(function(d){
if(d.error){toast(d.error,'error');return;}
S.toolsTestInput[name]=JSON.stringify(d,null,2);
var el=document.getElementById('testInput_'+name);
if(el)el.value=S.toolsTestInput[name];
toast('Mock data generated','success');
}).catch(function(){toast('Mock generation failed','error');});
};

window.deleteCustomTool=function(name){
avaConfirm('Delete custom tool "'+name+'"?',function(){
api('/api/tools/'+encodeURIComponent(name),{method:'DELETE'})
.then(function(r){return r.json();}).then(function(d){
if(d.ok){S.toolsList=null;toast('Tool deleted','success');fetchTools();}
else{toast(d.error||'Delete failed','error');}
}).catch(function(){toast('Delete failed','error');});
},{title:'Delete Tool',okText:'Delete'});
};

window.testToolPanel=function(name){
if(S.toolsExpanded===name){S.toolsExpanded=null;}
else{S.toolsExpanded=name;}
render();
};

window.vaultStore=function(){
var name=(document.getElementById('vaultName')||{}).value||'';
var service=(document.getElementById('vaultService')||{}).value||'';
var value=(document.getElementById('vaultValue')||{}).value||'';
if(!name.trim()||!value.trim()){toast('Name and value required','error');return;}
api('/api/vault',{method:'POST',body:{action:'store',name:name,service:service,value:value}})
.then(function(r){return r.json();}).then(function(d){
if(d.status==='ok'){toast('Key encrypted and stored','success');S.vaultEntries=null;fetchVault();
var el=document.getElementById('vaultValue');if(el)el.value='';
var n=document.getElementById('vaultName');if(n)n.value='';
var s=document.getElementById('vaultService');if(s)s.value='';
}else{toast(d.error||'Store failed','error');}
}).catch(function(){toast('Store failed','error');});
};

window.vaultRemove=function(name){
avaConfirm('Remove vault entry "'+name+'"?',function(){
api('/api/vault',{method:'POST',body:{action:'remove',name:name}})
.then(function(r){return r.json();}).then(function(d){
if(d.status==='ok'){toast('Removed','success');S.vaultEntries=null;fetchVault();}
else{toast(d.error||'Remove failed','error');}
}).catch(function(){toast('Remove failed','error');});
},{title:'Remove Vault Entry',okText:'Remove'});
};

window.removeApiReg=function(slug){
avaConfirm('Remove API "'+slug+'" from registry?',function(){
api('/api/api-registry/'+encodeURIComponent(slug),{method:'DELETE'})
.then(function(r){return r.json();}).then(function(d){
if(d.ok){toast('Removed','success');S.apiRegistry=null;fetchApiRegistry();}
else{toast(d.error||'Remove failed','error');}
}).catch(function(){toast('Remove failed','error');});
},{title:'Remove API',okText:'Remove'});
};

function fetchAssets(cb){
var params=[];
if(S.assetsFolder)params.push('folder='+encodeURIComponent(S.assetsFolder));
if(S.assetsSearch)params.push('q='+encodeURIComponent(S.assetsSearch));
var qs=params.length?'?'+params.join('&'):'';
api('/api/assets'+qs).then(function(arr){
S.assets=Array.isArray(arr)?arr:[];S.assetsLoaded=true;
collectAssetSelections();
if(cb)cb();else render();
}).catch(function(){S.assets=[];S.assetsLoaded=true;if(cb)cb();else render();});
}
function collectAssetSelections(){
var byUrl={};
for(var i=0;i<S.selectedAssets.length;i++)byUrl[S.selectedAssets[i].url]=true;
for(var j=0;j<S.messages.length;j++){
var m=S.messages[j];
if(m.media){for(var k=0;k<m.media.length;k++){if(m.media[k].inContext&&!byUrl[m.media[k].url]){
S.selectedAssets.push(m.media[k]);byUrl[m.media[k].url]=true;
}}}
}
}
window.toggleAssetSelection=function(url,type){
var idx=-1;
for(var i=0;i<S.selectedAssets.length;i++){if(S.selectedAssets[i].url===url){idx=i;break;}}
if(idx>=0){S.selectedAssets[idx].inContext=!S.selectedAssets[idx].inContext;}
else{S.selectedAssets.push({url:url,type:type||'image',inContext:true});}
var bar=document.getElementById('assetBar');
if(bar)bar.innerHTML=renderAssetBar();
};
window.removeAssetFromBar=function(url){
S.selectedAssets=S.selectedAssets.filter(function(a){return a.url!==url;});
var bar=document.getElementById('assetBar');if(bar)bar.innerHTML=renderAssetBar();
};
window.clearAssetSelections=function(){S.selectedAssets=[];var bar=document.getElementById('assetBar');if(bar)bar.innerHTML=renderAssetBar();};

function renderAssetBar(){
if(!S.selectedAssets.length)return '';
var activeCount=S.selectedAssets.filter(function(a){return a.inContext;}).length;
var h='<span class="asset-bar-label">'+ic('image',12)+' '+activeCount+'/'+S.selectedAssets.length+' active</span>';
for(var i=0;i<S.selectedAssets.length;i++){
var a=S.selectedAssets[i];
var isV=(a.type==='video');
var cls='asset-thumb'+(a.inContext?' selected':' dimmed');
h+='<div class="'+cls+'" onclick="toggleAssetSelection(\''+a.url+'\',\''+a.type+'\')" title="'+(a.inContext?'Click to exclude from context':'Click to include in context')+'">';
if(isV)h+='<video src="'+a.url+'" muted></video><span class="asset-type-badge">VID</span>';
else h+='<img src="'+(a.thumb_url||a.url)+'">';
if(a.inContext)h+='<div class="asset-check">'+ic('check',8)+'</div>';
h+='<button class="asset-remove" onclick="event.stopPropagation();removeAssetFromBar(\''+a.url+'\')">'+ic('xic',8)+'</button>';
h+='</div>';
}
return h;
}

function formatFileSize(b){if(b<1024)return b+'B';if(b<1024*1024)return (b/1024).toFixed(1)+'KB';return (b/(1024*1024)).toFixed(1)+'MB';}
function timeAgo(ts){if(!ts)return '';if(ts>1e12)ts=Math.floor(ts/1000);var s=Math.floor(Date.now()/1000-ts);if(s<0)return 'just now';if(s<60)return s+'s ago';if(s<3600)return Math.floor(s/60)+'m ago';if(s<86400)return Math.floor(s/3600)+'h ago';return Math.floor(s/86400)+'d ago';}

function assetThumbHtml(a){
if(a.type==='folder')return '<div style="display:flex;align-items:center;justify-content:center;height:100%;color:var(--text-muted)">'+ic('folder',40)+'</div>';
if(a.type==='video')return '<video src="'+a.url+'" muted preload="metadata"></video>';
if(a.type==='image')return '<img src="'+(a.thumb_url||a.url)+'" loading="lazy">';
var ext=(a.filename||'').split('.').pop().toLowerCase();
var codeExts=['py','js','ts','tsx','jsx','cpp','c','h','hpp','rs','go','java','rb','sh','css','scss','sql','lua','zig','swift','kt','cs'];
var icon=(codeExts.indexOf(ext)>=0)?'code':'file';
return '<div style="display:flex;flex-direction:column;align-items:center;justify-content:center;height:100%;color:var(--text-muted);gap:.25rem">'+ic(icon,32)+'<span style="font-size:.65rem;background:rgba(124,58,237,.3);color:var(--accent-pale);padding:1px 6px;border-radius:3px;font-weight:600">.'+esc(ext)+'</span></div>';
}

function renderAssets(){
if(!S.assetsLoaded){fetchAssets();return '<div class="assets-empty"><div class="spinner"></div> Loading assets...</div>';}
var filtered=S.assets.slice();
if(S.assetsFilter!=='all')filtered=filtered.filter(function(a){return a.type===S.assetsFilter;});

if(S.assetsSort==='date')filtered.sort(function(a,b){return (b.created_at||0)-(a.created_at||0);});
else if(S.assetsSort==='name')filtered.sort(function(a,b){return (a.filename||'').localeCompare(b.filename||'');});
else if(S.assetsSort==='size')filtered.sort(function(a,b){return (b.size||0)-(a.size||0);});

var filterBtns='';
filterBtns+='<button class="assets-filter-btn'+(S.assetsFilter==='all'?' active':'')+'" onclick="setAssetsFilter(\'all\')">All</button>';
filterBtns+='<button class="assets-filter-btn'+(S.assetsFilter==='image'?' active':'')+'" onclick="setAssetsFilter(\'image\')">'+ic('image',12)+' Images</button>';
filterBtns+='<button class="assets-filter-btn'+(S.assetsFilter==='video'?' active':'')+'" onclick="setAssetsFilter(\'video\')">'+ic('video',12)+' Videos</button>';
filterBtns+='<button class="assets-filter-btn'+(S.assetsFilter==='document'?' active':'')+'" onclick="setAssetsFilter(\'document\')">'+ic('file',12)+' Docs</button>';
filterBtns+='<button class="assets-filter-btn'+(S.assetsFilter==='code'?' active':'')+'" onclick="setAssetsFilter(\'code\')">'+ic('code',12)+' Code</button>';
filterBtns+='<select class="toolbar-select" style="font-size:.75rem" onchange="setAssetsSort(this.value)">';
filterBtns+='<option value="date"'+(S.assetsSort==='date'?' selected':'')+'>Newest</option>';
filterBtns+='<option value="name"'+(S.assetsSort==='name'?' selected':'')+'>Name</option>';
filterBtns+='<option value="size"'+(S.assetsSort==='size'?' selected':'')+'>Size</option>';
filterBtns+='</select>';
filterBtns+='<button class="assets-filter-btn" onclick="refreshAssets()" title="Refresh">'+ic('refresh',12)+'</button>';

var searchBar='<div style="display:flex;gap:.5rem;align-items:center;margin-bottom:.5rem;padding:0 .75rem">'+
'<input id="assetSearchInput" type="text" placeholder="Search assets..." value="'+esc(S.assetsSearch||'')+'" style="flex:1;background:var(--bg-elevated);border:1px solid var(--border);border-radius:var(--radius-sm);color:var(--text-primary);padding:.375rem .625rem;font-size:.8125rem;font-family:var(--font);outline:none" onkeydown="if(event.key===\'Enter\')doAssetSearch()">'+
'<button class="assets-filter-btn" onclick="doAssetSearch()">'+ic('search',12)+' Search</button>'+
(S.assetsSearch?'<button class="assets-filter-btn" onclick="clearAssetSearch()">'+ic('xic',10)+' Clear</button>':'')+
'<button class="assets-filter-btn" onclick="createAssetFolder()">'+ic('folder',12)+' New Folder</button>'+
'<button class="assets-filter-btn" onclick="uploadAssetFile()">'+ic('upload',12)+' Upload</button>'+
'<input type="file" id="assetFileUpload" style="display:none" onchange="handleAssetUpload(this)">'+
'</div>';

var breadcrumb='';
if(S.assetsFolder){
var parts=S.assetsFolder.split('/');
breadcrumb+='<div style="padding:0 .75rem .5rem;font-size:.8125rem;display:flex;align-items:center;gap:.25rem">';
breadcrumb+='<a href="#" onclick="event.preventDefault();navigateAssetFolder(\'\')" style="color:var(--accent-pale);text-decoration:none">Assets</a>';
var path='';
for(var bi=0;bi<parts.length;bi++){
path+=(bi?'/':'')+parts[bi];
breadcrumb+=' <span style="color:var(--text-muted)">/</span> ';
if(bi===parts.length-1)breadcrumb+='<span style="color:var(--text-primary)">'+esc(parts[bi])+'</span>';
else breadcrumb+='<a href="#" onclick="event.preventDefault();navigateAssetFolder(\''+esc(path)+'\')" style="color:var(--accent-pale);text-decoration:none">'+esc(parts[bi])+'</a>';
}
breadcrumb+='</div>';
}

var grid='';
if(!filtered.length){
grid='<div class="assets-empty">'+ic('image',32)+'<span>No assets found</span><span style="font-size:.8125rem">Generated and uploaded media will appear here</span></div>';
}else{
grid='<div class="assets-grid">';
for(var ai=0;ai<filtered.length;ai++){
var a=filtered[ai];
if(a.type==='folder'){
grid+='<div class="asset-card" ondragover="event.preventDefault();this.style.borderColor=\'var(--accent)\'" ondragleave="this.style.borderColor=\'\'" ondrop="event.preventDefault();this.style.borderColor=\'\';dropOnFolder(\''+esc(a.path||a.id)+'\',event)" onclick="navigateAssetFolder(\''+esc(a.path||a.id)+'\')">';
grid+='<div class="asset-card-thumb">'+assetThumbHtml(a)+'</div>';
grid+='<span class="asset-card-type">folder</span>';
grid+='<div class="asset-card-info"><div class="asset-card-name" title="'+esc(a.filename)+'">'+esc(a.filename)+'</div></div>';
grid+='</div>';
continue;
}
var isSelected=false;
for(var si=0;si<S.selectedAssets.length;si++){if(S.selectedAssets[si].url===a.url){isSelected=true;break;}}
grid+='<div class="asset-card'+(isSelected?' selected':'')+'" draggable="true" ondragstart="event.dataTransfer.setData(\'text/plain\',\''+esc(a.id)+'\')" onclick="assetCardClick(\''+esc(a.url)+'\',\''+esc(a.type)+'\',event)">';
grid+='<div class="asset-card-thumb">'+assetThumbHtml(a)+'</div>';
grid+='<span class="asset-card-type">'+esc(a.type)+'</span>';
var tagHtml='';
if(a.tags&&a.tags.length){for(var ti=0;ti<Math.min(a.tags.length,3);ti++)tagHtml+='<span style="font-size:.55rem;background:rgba(124,58,237,.2);color:var(--accent-pale);padding:0 3px;border-radius:2px">'+esc(a.tags[ti])+'</span>';}
var actions='<div class="asset-card-actions" onclick="event.stopPropagation()" style="position:absolute;top:.375rem;left:.375rem;display:flex;gap:2px">';
actions+='<button title="Rename" onclick="renameAsset(\''+esc(a.id)+'\',\''+esc(a.filename)+'\')" style="background:rgba(0,0,0,.7);border:none;color:#ccc;font-size:.6rem;padding:2px 4px;border-radius:3px;cursor:pointer">'+ic('edit',9)+'</button>';
actions+='<button title="Delete" onclick="deleteAsset(\''+esc(a.id)+'\')" style="background:rgba(0,0,0,.7);border:none;color:#f87171;font-size:.6rem;padding:2px 4px;border-radius:3px;cursor:pointer">'+ic('trash',9)+'</button>';
actions+='<button title="Analyze with AI" onclick="analyzeAsset(\''+esc(a.id)+'\')" style="background:rgba(0,0,0,.7);border:none;color:#a78bfa;font-size:.6rem;padding:2px 4px;border-radius:3px;cursor:pointer">'+ic('brain',9)+'</button>';
actions+='</div>';
grid+=actions;
grid+='<button class="asset-card-select'+(isSelected?' active':'')+'" onclick="event.stopPropagation();toggleAssetSelection(\''+esc(a.url)+'\',\''+esc(a.type)+'\');render()">'+(isSelected?ic('check',10)+' Selected':'Use in chat')+'</button>';
grid+='<div class="asset-card-info"><div class="asset-card-name" title="'+esc(a.filename)+'">'+esc(a.filename)+'</div>';
grid+='<div class="asset-card-meta"><span>'+formatFileSize(a.size||0)+'</span><span>'+timeAgo(a.created_at||0)+'</span>'+
(tagHtml?'<span style="display:flex;gap:2px">'+tagHtml+'</span>':'')+
'</div>';
if(a.description)grid+='<div onclick="event.stopPropagation();showAnalysisModal(\''+esc(a.id)+'\')" style="font-size:.6rem;color:var(--accent-pale);margin-top:2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;cursor:pointer" title="Click to view full analysis">'+ic('brain',8)+' '+esc(a.description)+'</div>';
grid+='</div></div>';
}
grid+='</div>';
}

return '<div style="display:flex;flex-direction:column;height:100%">'+
'<div class="assets-header"><h2>'+ic('image',18)+' Assets</h2><div class="assets-filters">'+filterBtns+'</div></div>'+
breadcrumb+searchBar+
'<div style="flex:1;overflow-y:auto;padding:0 .75rem .75rem">'+grid+'</div>'+
'</div>';
}
window.setAssetsFilter=function(f){S.assetsFilter=f;render();};
window.setAssetsSort=function(s){S.assetsSort=s;render();};
window.setAssetsNodeFilter=function(n){S.assetsNodeFilter=n;render();};
window.refreshAssets=function(){S.assetsLoaded=false;fetchAssets();};
window.assetCardClick=function(url,type,e){
if(e&&e.shiftKey){toggleAssetSelection(url,type);return;}
if(type==='image')openLightbox(url);
else if(type==='video')openLightbox(url);
else if(type==='document'||type==='code')window.open(url,'_blank');
};
window.deleteAsset=function(id){
avaConfirm('Delete this asset permanently?',function(){
api('/api/assets/'+encodeURIComponent(id),{method:'DELETE'}).then(function(){
toast('Asset deleted','success');S.assetsLoaded=false;fetchAssets();
}).catch(function(e){toast('Delete failed: '+(e.message||'error'),'error');});
},{title:'Delete Asset',okText:'Delete'});
};
window.renameAsset=function(id,currentName){
avaPrompt('New filename:',currentName,function(newName){
if(!newName||!newName.trim())return;
api('/api/assets/'+encodeURIComponent(id)+'/rename',{method:'POST',body:{name:newName.trim()}}).then(function(){
toast('Renamed','success');S.assetsLoaded=false;fetchAssets();
}).catch(function(e){toast('Rename failed: '+(e.message||'error'),'error');});
},{title:'Rename Asset',okText:'Rename'});
};
window.navigateAssetFolder=function(folder){
S.assetsFolder=folder||'';S.assetsLoaded=false;fetchAssets();
};
window.doAssetSearch=function(){
var inp=document.getElementById('assetSearchInput');
S.assetsSearch=inp?inp.value.trim():'';S.assetsLoaded=false;fetchAssets();
};
window.clearAssetSearch=function(){S.assetsSearch='';S.assetsLoaded=false;fetchAssets();};
window.createAssetFolder=function(){
avaPrompt('Folder name:','',function(name){
if(!name||!name.trim())return;
api('/api/assets/folder',{method:'POST',body:{name:name.trim(),folder:S.assetsFolder||''}}).then(function(){
toast('Folder created','success');S.assetsLoaded=false;fetchAssets();
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
},{title:'New Folder',okText:'Create'});
};
window.dropOnFolder=function(folderPath,e){
var src=e.dataTransfer.getData('text/plain');
if(!src)return;
api('/api/assets/move',{method:'POST',body:{src:src,dst:folderPath}}).then(function(){
toast('Moved','success');S.assetsLoaded=false;fetchAssets();
}).catch(function(e){toast('Move failed: '+(e.message||'error'),'error');});
};
window.uploadAssetFile=function(){
var inp=document.getElementById('assetFileUpload');if(inp)inp.click();
};
window.handleAssetUpload=function(inp){
if(!inp.files||!inp.files[0])return;
var file=inp.files[0];
if(file.size>50*1024*1024){toast('File too large (max 50MB)','error');return;}
var fd=new FormData();fd.append('file',file);
var h={};if(S.token)h['Authorization']='Bearer '+S.token;
toast('Uploading...','info');
fetch('/api/upload',{method:'POST',headers:h,body:fd}).then(function(r){return r.json();}).then(function(d){
toast('Uploaded: '+file.name,'success');inp.value='';S.assetsLoaded=false;fetchAssets();
}).catch(function(e){toast('Upload failed','error');});
};
window.analyzeAsset=function(id){
toast('Analyzing asset...','info');
api('/api/assets/'+encodeURIComponent(id)+'/analyze',{method:'POST',body:{}}).then(function(d){
toast('Analysis complete','success');S.assetsLoaded=false;fetchAssets();
setTimeout(function(){showAnalysisModal(id);},500);
}).catch(function(e){toast('Analysis failed: '+(e.message||'error'),'error');});
};
window.showAnalysisModal=function(id){
var asset=null;
if(S.assets)for(var i=0;i<S.assets.length;i++){if(S.assets[i].id===id){asset=S.assets[i];break;}}
if(!asset||!asset.description){toast('No analysis data yet. Click the brain icon to analyze.','info');return;}
var tags='';
if(asset.tags&&asset.tags.length){
for(var ti=0;ti<asset.tags.length;ti++){
tags+='<span style="display:inline-block;font-size:.75rem;background:rgba(124,58,237,.15);color:var(--accent-pale);padding:.125rem .5rem;border-radius:999px;margin:.125rem">'+esc(asset.tags[ti])+'</span>';
}}
var thumb='';
if(asset.type==='image')thumb='<div style="text-align:center;margin-bottom:1rem"><img src="'+esc(asset.url)+'" style="max-width:100%;max-height:300px;border-radius:var(--radius-sm);object-fit:contain"></div>';
else if(asset.type==='video')thumb='<div style="text-align:center;margin-bottom:1rem"><video src="'+esc(asset.url)+'" controls style="max-width:100%;max-height:300px;border-radius:var(--radius-sm)"></video></div>';
var modalHtml='<div class="ava-modal-backdrop" onclick="if(event.target===this)closeAnalysisModal()" style="position:fixed;top:0;left:0;right:0;bottom:0;background:rgba(0,0,0,.6);z-index:9999;display:flex;align-items:center;justify-content:center;padding:1rem">'+
'<div style="background:var(--bg-elevated);border:1px solid var(--border);border-radius:var(--radius-lg);max-width:600px;width:100%;max-height:85vh;display:flex;flex-direction:column;overflow:hidden">'+
'<div style="display:flex;align-items:center;justify-content:space-between;padding:1rem 1.25rem;border-bottom:1px solid var(--border)">'+
'<h3 style="margin:0;font-size:1rem;color:var(--accent-pale)">'+ic('brain',16)+' AI Analysis</h3>'+
'<button onclick="closeAnalysisModal()" style="background:none;border:none;color:var(--text-muted);cursor:pointer;padding:4px">'+ic('xic',18)+'</button></div>'+
'<div style="flex:1;overflow-y:auto;padding:1.25rem">'+
thumb+
'<div style="font-size:.8125rem;font-weight:600;color:var(--text-primary);margin-bottom:.25rem">'+esc(asset.filename)+'</div>'+
'<div style="font-size:.75rem;color:var(--text-muted);margin-bottom:1rem">'+esc(asset.type)+' &middot; '+formatFileSize(asset.size||0)+'</div>'+
'<div style="font-size:.875rem;color:var(--text-primary);line-height:1.7;white-space:pre-wrap;word-break:break-word">'+esc(asset.description)+'</div>'+
(tags?'<div style="margin-top:1rem;display:flex;flex-wrap:wrap;gap:.25rem">'+tags+'</div>':'')+
'</div>'+
'<div style="display:flex;justify-content:flex-end;gap:.5rem;padding:.75rem 1.25rem;border-top:1px solid var(--border)">'+
'<button class="btn-sm" onclick="closeAnalysisModal();analyzeAsset(\''+esc(id)+'\')">'+ic('refresh',12)+' Re-analyze</button>'+
'<button class="btn-ghost" onclick="closeAnalysisModal()">Close</button>'+
'</div></div></div>';
var existing=document.getElementById('analysisModalRoot');
if(existing)existing.remove();
var div=document.createElement('div');div.id='analysisModalRoot';
div.innerHTML=modalHtml;document.body.appendChild(div);
};
window.closeAnalysisModal=function(){
var el=document.getElementById('analysisModalRoot');if(el)el.remove();
};

/* ── Router ───────────────────────────────────────────── */
function getRoute(){return location.hash.slice(1)||'/chat';}
function navigate(h){if(h!==undefined)location.hash=h;S.route=getRoute();render();}
window.addEventListener('hashchange',function(){S.route=getRoute();render();});

function toggleSidebar(){
S.sidebarOpen=!S.sidebarOpen;
var sb=document.getElementById('sidebar'),ov=document.getElementById('sbOverlay');
if(sb)sb.classList.toggle('open',S.sidebarOpen);
if(ov)ov.classList.toggle('open',S.sidebarOpen);
}
function closeSidebar(){
S.sidebarOpen=false;
var sb=document.getElementById('sidebar'),ov=document.getElementById('sbOverlay');
if(sb)sb.classList.remove('open');
if(ov)ov.classList.remove('open');
}
window.toggleSidebar=toggleSidebar;window.closeSidebar=closeSidebar;

/* ── Swipe Gesture System ─────────────────────────── */
(function(){
 var tx=0,ty=0,tt=0;
 var THRESH=50,EDGE=40,MAXDT=400;
 function isMobile(){return window.innerWidth<=768;}
 document.addEventListener('touchstart',function(e){
 tx=e.touches[0].clientX;ty=e.touches[0].clientY;tt=Date.now();
 },{passive:true});
 document.addEventListener('touchend',function(e){
 if(!isMobile())return;
 var dx=e.changedTouches[0].clientX-tx;
 var dy=e.changedTouches[0].clientY-ty;
 var dt=Date.now()-tt;
 if(dt>MAXDT||Math.abs(dx)<THRESH||Math.abs(dy)>Math.abs(dx)*1.1)return;
 var right=(dx>0),left=(dx<0),fromLeft=(tx<EDGE);
 var tree=document.getElementById('fileTree');
 var treeOpen=tree&&tree.classList.contains('tree-open');
 if(right&&fromLeft){
 if(S.route==='/files'){
 if(!treeOpen){openFileTree();}
 else{
 // Tree already open — swipe again from edge opens sidebar
 closeFileTree();setTimeout(function(){if(!S.sidebarOpen)toggleSidebar();},50);
 }
 }else{
 if(!S.sidebarOpen)toggleSidebar();
 }
 }else if(left){
 if(S.sidebarOpen){closeSidebar();}
 else if(S.route==='/files'&&treeOpen){closeFileTree();}
 }
 },{passive:true});
})();
function openFileTree(){
 var t=document.getElementById('fileTree'),o=document.getElementById('fileTreeOverlay');
 if(t)t.classList.add('tree-open');
 if(o)o.classList.add('open');
}
function closeFileTree(){
 var t=document.getElementById('fileTree'),o=document.getElementById('fileTreeOverlay');
 if(t)t.classList.remove('tree-open');
 if(o)o.classList.remove('open');
}
window.openFileTree=openFileTree;window.closeFileTree=closeFileTree;

/* ── Render ───────────────────────────────────────────── */
window.render=render;
function render(){
var app=document.getElementById('app');if(!app)return;
S.route=getRoute();
app.className='app';
app.innerHTML=
'<div id="sbOverlay" class="sb-overlay'+(S.sidebarOpen?' open':'')+'" onclick="closeSidebar()"></div>'+
'<nav id="sidebar" class="sidebar'+(S.sidebarOpen?' open':'')+'">'+renderSidebar()+'</nav>'+
'<main id="main" class="main">'+renderPage()+'</main>';
if(S.route==='/'||S.route==='/chat')setupChat();
if(S.route==='/files')setupIdeAfterRender();
if(S.route==='/servers')loadServers();
if(S.route==='/system')setupSystemPage();
}

function renderSidebar(){
var links=[
{r:'/chat',ic:'chat',l:'Chat'},
{r:'/assets',ic:'image',l:'Assets'},
{r:'/tools',ic:'wrench',l:'Tools'},
{r:'/files',ic:'folder',l:'Files'},
{r:'/knowledge',ic:'book',l:'Knowledge Base'},
{r:'/apps',ic:'grid',l:'Apps'},
{r:'/services',ic:'activity',l:'Services'},
{r:'/usage',ic:'activity',l:'Usage'},
{r:'/system',ic:'brain',l:'System'},
{r:'/logs',ic:'log',l:'Logs'},
{r:'/servers',ic:'server',l:'Servers'},
{r:'/settings',ic:'gear',l:'Settings'}
];
var nav='';
for(var i=0;i<links.length;i++){
var lk=links[i],active=(S.route===lk.r)?'active':'';
nav+='<a href="#'+lk.r+'" class="sb-link '+active+'" onclick="closeSidebar()">'+ic(lk.ic)+' <span>'+lk.l+'</span></a>';
}
var stColor=S.status?'#4ade80':'#f87171';
var stText=S.status?'Connected':'Unknown';
var modelShort=S.model?(S.model.length>25?S.model.slice(0,25)+'...':S.model):'--';
var connDot='<span class="status-dot online" style="display:inline-block;width:6px;height:6px;border-radius:50%;background:#4ade80;margin-right:4px"></span>Local';
return '<div class="sb-head"><span class="sb-logo">Avacli</span><button class="sb-close" onclick="closeSidebar()">'+ic('xic',18)+'</button></div>'+
'<div class="sb-nav">'+nav+'</div>'+
'<div class="sb-foot">'+
'<div>Model: <span title="'+esc(S.model)+'">'+esc(modelShort)+'</span></div>'+
'<div>'+connDot+'</div>'+
'<div>Status: <span style="color:'+stColor+'">'+ic('dot',8)+' '+stText+'</span></div>'+
'</div>';
}

function renderPage(){
switch(S.route){
case '/':
case '/chat':return renderChat();
case '/assets':return renderAssets();
case '/tools':return renderTools();
case '/files':return renderFiles();
case '/knowledge':return renderKnowledge();
case '/apps':return renderApps();
case '/services':return renderServices();
case '/usage':return renderUsage();
case '/system':return renderSystem();
case '/logs':return renderLogs();
case '/servers':return renderServers();
case '/settings':return renderSettings();
default:return renderChat();
}
}

/* ── Dashboard / Home ──────────────────────────────────── */
var dashData=null;
function renderDashboard(){
if(!dashData){loadDashboard();return '<div class="dash-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';}
var d=dashData;
var hour=new Date().getHours();
var greeting=hour<12?'Good morning':hour<18?'Good afternoon':'Good evening';
var h='<div class="dash-wrap">';
h+='<div class="dash-greeting">'+greeting+'</div>';
h+='<div class="dash-sub">Avacli v2.0 &mdash; '+new Date().toLocaleDateString('en-US',{weekday:'long',month:'long',day:'numeric',year:'numeric'})+'</div>';
h+='<div class="dash-stats">';
h+='<div class="dash-stat"><div class="dash-stat-val">'+(d.apps||0)+'</div><div class="dash-stat-label">Apps</div></div>';
h+='<div class="dash-stat"><div class="dash-stat-val">'+(d.services||0)+'</div><div class="dash-stat-label">Services</div></div>';
h+='<div class="dash-stat"><div class="dash-stat-val">'+(d.sessions||0)+'</div><div class="dash-stat-label">Sessions</div></div>';
h+='<div class="dash-stat"><div class="dash-stat-val">'+(d.articles||0)+'</div><div class="dash-stat-label">Articles</div></div>';
h+='</div>';
h+='<div class="dash-section"><div class="dash-section-title">'+ic('grid',14)+' Quick Actions</div>';
h+='<div class="dash-cards">';
h+='<div class="dash-card" onclick="go(\'/chat\')"><div class="dash-card-icon">'+ic('chat',24)+'</div><div class="dash-card-title">Chat</div><div class="dash-card-desc">Start a conversation with the AI agent</div></div>';
h+='<div class="dash-card" onclick="go(\'/apps\')"><div class="dash-card-icon">'+ic('grid',24)+'</div><div class="dash-card-title">Apps</div><div class="dash-card-desc">Build and manage web applications</div></div>';
h+='<div class="dash-card" onclick="go(\'/files\')"><div class="dash-card-icon">'+ic('folder',24)+'</div><div class="dash-card-title">Files</div><div class="dash-card-desc">Browse and edit workspace files</div></div>';
h+='<div class="dash-card" onclick="go(\'/knowledge\')"><div class="dash-card-icon">'+ic('book',24)+'</div><div class="dash-card-title">Knowledge Base</div><div class="dash-card-desc">Research articles and saved notes</div></div>';
h+='<div class="dash-card" onclick="go(\'/services\')"><div class="dash-card-icon">'+ic('activity',24)+'</div><div class="dash-card-title">Services</div><div class="dash-card-desc">Background tasks and scheduled prompts</div></div>';
h+='<div class="dash-card" onclick="go(\'/servers\')"><div class="dash-card-icon">'+ic('server',24)+'</div><div class="dash-card-title">Servers</div><div class="dash-card-desc">Manage cloud infrastructure</div></div>';
h+='</div></div>';
h+='<div class="dash-section"><div class="dash-section-title">'+ic('activity',14)+' Recent Activity</div>';
if(d.activity&&d.activity.length){
h+='<div class="dash-activity">';
for(var i=0;i<d.activity.length;i++){
var a=d.activity[i];
var ts=new Date(a.timestamp).toLocaleTimeString('en-US',{hour12:false,hour:'2-digit',minute:'2-digit'});
h+='<div class="dash-activity-row"><span class="dash-activity-time">'+ts+'</span><span class="dash-activity-cat">'+esc(a.category)+'</span><span class="dash-activity-msg">'+esc(a.message)+'</span></div>';
}
h+='</div>';
}else{
h+='<div class="dash-empty">No recent activity</div>';
}
h+='</div></div>';
return h;
}
function loadDashboard(){
var counts={apps:0,services:0,sessions:0,articles:0,activity:[]};
var pending=4;
function done(){pending--;if(pending<=0){dashData=counts;render();}}
api('/api/apps').then(function(d){counts.apps=(d.apps||d||[]).length||0;}).catch(function(){}).then(done);
api('/api/services').then(function(d){counts.services=(d.services||d||[]).length||0;}).catch(function(){}).then(done);
api('/api/sessions').then(function(d){counts.sessions=(d.sessions||d||[]).length||0;}).catch(function(){}).then(done);
api('/api/logs?limit=20').then(function(d){
var entries=d.entries||d||[];
counts.activity=entries.slice(0,15);
if(d.entries)for(var i=0;i<entries.length;i++){
if(entries[i].category==='knowledge')counts.articles++;
}
}).catch(function(){}).then(done);
}
function setupDashboard(){}
window.go=function(r){location.hash=r;};

/* ── Chat Page ────────────────────────────────────────── */
function renderChat(){
var msgs='';
if(!S.messages.length){
msgs='<div class="empty">'+ic('chat',48)+'<p>Start a conversation</p><p style="font-size:.8125rem">Type a message below to begin chatting with the AI agent.</p></div>';
}else{
for(var i=0;i<S.messages.length;i++){msgs+=renderMessage(S.messages[i],i);}
}

var streamInfo='';
if(S.streaming){
var elapsed=Math.round((Date.now()-S.streamStart)/1000);
streamInfo='<div class="streaming-indicator"><div class="streaming-dot"></div> Generating'+(S.model?' with '+S.model.split('-').slice(0,2).join('-'):'')+' &middot; '+elapsed+'s</div>';
}

return '<div class="chat-layout">'+
'<div class="chat-main">'+
'<div class="chat-wrap">'+
'<div class="chat-msgs" id="chatMsgs" onscroll="onChatScroll()">'+msgs+'</div>'+
renderToolbar()+
renderMediaSettings()+
streamInfo+
'<div class="asset-bar" id="assetBar">'+renderAssetBar()+'</div>'+
'<div id="uploadPreview"></div>'+
'<div class="chat-input" style="position:relative">'+
'<div id="mentionPopup" class="mention-popup"></div>'+
'<div class="chat-input-row">'+
'<button class="upload-btn" onclick="triggerUpload()" title="Attach file">'+ic('upload',16)+'</button>'+
'<input type="file" id="fileUpload" style="display:none" onchange="handleUpload(this)">'+
'<textarea id="chatInput" rows="1" placeholder="Type a message... (use @ to mention a node)" onkeydown="chatKey(event)" oninput="autoGrow(this)"></textarea>'+
(S.streaming?
'<button class="btn-stop" onclick="stopChat()">'+ic('stop',16)+' Stop</button>':
'<button class="btn-send" onclick="sendMsg()">'+ic('send',16)+' Send</button>')+
'</div></div></div></div>'+
renderNodesPanel()+
'</div>'+
'<button class="nodes-toggle" onclick="toggleNodesPanel()" title="Nodes">'+ic('server',20)+'</button>'+
'<div class="remote-ui-overlay" id="remoteUiOverlay"></div>'+
'<div class="msg-actions" id="msgActions">'+
'<button class="msg-action-btn" data-action="copy" title="Copy">'+ic('copy',14)+'</button>'+
'<button class="msg-action-btn" data-action="exclude" title="Remove from context">'+ic('xic',14)+'</button>'+
'<button class="msg-action-btn" data-action="delete" title="Delete">'+ic('trash',14)+'</button>'+
'</div>';
}

function renderToolbar(){
var activeNotes=S.notes.filter(function(n){return n.enabled;}).length;
var noteBadge=activeNotes?'<span class="badge">'+activeNotes+'</span>':'';

return '<div class="toolbar">'+
'<div class="toolbar-group" style="position:relative">'+
'<button class="toolbar-btn" onclick="avaOpenToolbarPopover(event,\'histPop\')" title="Chat History">'+ic('clock',15)+' <span>History</span> '+ic('chevD',12)+'</button>'+
'<div class="popover" id="histPop">'+renderHistoryPopover()+'</div>'+
'</div>'+
'<div class="toolbar-group">'+
'<button class="toolbar-btn'+(S.useHistory?' active':'')+'" onclick="toggleContext()" title="Context: '+(S.useHistory?'ON':'OFF')+'">'+ic('context',15)+' <span>Context</span></button>'+
'</div>'+
'<div class="toolbar-group" style="position:relative">'+
'<button class="toolbar-btn" onclick="avaOpenToolbarPopover(event,\'notesPop\')" title="Notes">'+ic('note',15)+' <span>Notes</span> '+noteBadge+'</button>'+
'<div class="popover" id="notesPop">'+renderNotesPopover()+'</div>'+
'</div>'+
'<div class="toolbar-spacer"></div>'+
'<div class="toolbar-group toolbar-nodes-btn">'+
'<button class="toolbar-btn" onclick="toggleNodesPanel()" title="Nodes">'+ic('server',15)+' <span>Nodes</span></button>'+
'</div>'+
'<div class="toolbar-group" style="position:relative">'+
'<button class="toolbar-btn" onclick="avaOpenToolbarPopover(event,\'inputSettingsPop\')" title="Chat Settings">'+ic('gear',15)+' <span>'+(function(){var m=getModelMeta(S.model);return m?(m.name||m.id).split('-').slice(0,3).join('-'):'Model';})()+' '+ic('chevD',10)+'</span></button>'+
'<div class="popover" id="inputSettingsPop" style="min-width:260px">'+
'<div class="popover-header"><span>Chat Settings</span></div>'+
'<div style="padding:.5rem .75rem"><label style="display:block;font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">Model</label><select class="toolbar-select" id="modelSelect" onchange="setModelFromSelect(this)" style="width:100%;max-width:100%">'+renderModelOptions()+'</select></div>'+
renderReasoningEffortSelect(true)+
'<label style="display:flex;align-items:center;gap:.5rem;padding:.5rem .75rem;font-size:.8125rem;cursor:pointer;color:var(--text-secondary)">'+
'<input type="checkbox" '+(S.ctrlEnterSend!==false?'checked':'')+' onchange="toggleCtrlEnter(this.checked)"> Ctrl+Enter to send'+
'</label>'+
'<div style="padding:0 .75rem .5rem;font-size:.72rem;color:var(--text-muted)">'+(S.ctrlEnterSend!==false?'Enter adds new line':'Enter sends message')+'</div>'+
'<div style="padding:.25rem .75rem"><label style="display:block;font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">Max Output Tokens</label>'+
'<input type="number" class="set-input" id="maxOutputTokensInput" value="'+(S.maxOutputTokens||'')+'" placeholder="Default" min="1" max="131072" style="width:100%" onchange="setMaxOutputTokens(this.value)"></div>'+
'<div style="padding:.25rem .75rem .5rem"><label style="display:block;font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">Context Token Limit (warn threshold)</label>'+
'<input type="number" class="set-input" id="contextTokenLimitInput" value="'+(S.contextTokenLimit||'')+'" placeholder="Default" min="1000" max="1048576" style="width:100%" onchange="setContextTokenLimit(this.value)"></div>'+
(S.lastTokenUsage?'<div style="padding:0 .75rem .5rem;font-size:.6875rem;color:var(--text-muted)">Last: '+S.lastTokenUsage.prompt+' prompt + '+S.lastTokenUsage.completion+' completion = '+S.lastTokenUsage.total+' total'+(S.contextTokenLimit&&S.lastTokenUsage.total>S.contextTokenLimit*0.8?' <span style="color:#f59e0b">&#9888; Approaching limit</span>':'')+'</div>':'')+
'</div>'+
'</div>'+
'</div>';
}
window.toggleCtrlEnter=function(v){S.ctrlEnterSend=v;localStorage.setItem('ava_ctrl_enter',v?'true':'false');render();};
window.setMaxOutputTokens=function(v){var n=parseInt(v);S.maxOutputTokens=isNaN(n)?0:n;try{localStorage.setItem('ava_max_output_tokens',S.maxOutputTokens||'');}catch(x){}};
window.setContextTokenLimit=function(v){var n=parseInt(v);S.contextTokenLimit=isNaN(n)?0:n;try{localStorage.setItem('ava_context_token_limit',S.contextTokenLimit||'');}catch(x){}};
window.setReasoningEffort=function(v){S.reasoningEffort=v||'high';try{localStorage.setItem('ava_reasoning_effort',S.reasoningEffort);}catch(x){}};

function renderModelOptions(){
var chat=[],img=[],vid=[];
for(var i=0;i<S.models.length;i++){
var m=S.models[i];
if(m.type==='image_gen')img.push(m);
else if(m.type==='video_gen')vid.push(m);
else chat.push(m);
}
var o='';
if(chat.length){
o+='<optgroup label="Chat">';
for(var i=0;i<chat.length;i++){var m=chat[i];o+='<option value="'+esc(m.id)+'"'+(m.id===S.model?' selected':'')+'>'+esc(m.name||m.id)+(m.reasoning?' *':'')+'</option>';}
o+='</optgroup>';
}
if(img.length){
o+='<optgroup label="Image Generation">';
for(var i=0;i<img.length;i++){var m=img[i];o+='<option value="'+esc(m.id)+'"'+(m.id===S.model?' selected':'')+'>'+esc(m.name||m.id)+'</option>';}
o+='</optgroup>';
}
if(vid.length){
o+='<optgroup label="Video Generation">';
for(var i=0;i<vid.length;i++){var m=vid[i];o+='<option value="'+esc(m.id)+'"'+(m.id===S.model?' selected':'')+'>'+esc(m.name||m.id)+'</option>';}
o+='</optgroup>';
}
return o||'<option value="">No models</option>';
}
function renderExtraModelOptions(){
var o='';
for(var i=0;i<S.models.length;i++){
var m=S.models[i];
if(m.type==='image_gen'||m.type==='video_gen')continue;
o+='<option value="'+esc(m.id)+'"'+(m.id===S.extraModel?' selected':'')+'>'+esc(m.name||m.id)+(m.reasoning?' *':'')+'</option>';
}
return o;
}
window.setExtraModel=function(v){
S.extraModel=v;
api('/api/settings',{method:'POST',body:{extra_model:v}}).catch(function(){});
};

function renderMediaSettings(){
var m=getModelMeta(S.model);
if(!m)return '<div class="media-settings" id="mediaSettings"></div>';
if(m.type==='image_gen'){
var ms=S.mediaSettings||{};
return '<div class="media-settings visible" id="mediaSettings">'+
'<div class="media-opt"><label>'+ic('image',13)+' Ratio</label><select id="imgRatio" onchange="updateMediaSetting(\'ratio\',this.value)">'+
'<option value="1:1"'+((ms.ratio||'1:1')==='1:1'?' selected':'')+'>1:1</option>'+
'<option value="16:9"'+(ms.ratio==='16:9'?' selected':'')+'>16:9</option>'+
'<option value="9:16"'+(ms.ratio==='9:16'?' selected':'')+'>9:16</option>'+
'<option value="4:3"'+(ms.ratio==='4:3'?' selected':'')+'>4:3</option>'+
'<option value="3:4"'+(ms.ratio==='3:4'?' selected':'')+'>3:4</option>'+
'<option value="3:2"'+(ms.ratio==='3:2'?' selected':'')+'>3:2</option>'+
'<option value="2:3"'+(ms.ratio==='2:3'?' selected':'')+'>2:3</option>'+
'<option value="1:2"'+(ms.ratio==='1:2'?' selected':'')+'>1:2</option>'+
'<option value="2:1"'+(ms.ratio==='2:1'?' selected':'')+'>2:1</option>'+
'<option value="auto"'+(ms.ratio==='auto'?' selected':'')+'>Auto</option>'+
'</select></div>'+
'<div class="media-opt"><label>Resolution</label><select id="imgRes" onchange="updateMediaSetting(\'resolution\',this.value)">'+
'<option value="1k"'+((ms.resolution||'1k')==='1k'?' selected':'')+'>1k</option>'+
'<option value="2k"'+(ms.resolution==='2k'?' selected':'')+'>2k</option>'+
'</select></div>'+
'<div class="media-opt"><label>Quality</label><select id="imgQuality" onchange="updateMediaSetting(\'quality\',this.value)">'+
'<option value=""'+(!ms.quality?' selected':'')+'>Default</option>'+
'<option value="low"'+(ms.quality==='low'?' selected':'')+'>Low</option>'+
'<option value="medium"'+(ms.quality==='medium'?' selected':'')+'>Medium</option>'+
'<option value="high"'+(ms.quality==='high'?' selected':'')+'>High</option>'+
'</select></div>'+
'<div class="media-opt"><label>Count</label><select id="imgCount" onchange="updateMediaSetting(\'count\',this.value)">'+
'<option value="1"'+(ms.count==='1'||!ms.count?' selected':'')+'>1</option>'+
'<option value="2"'+(ms.count==='2'?' selected':'')+'>2</option>'+
'<option value="3"'+(ms.count==='3'?' selected':'')+'>3</option>'+
'<option value="4"'+(ms.count==='4'?' selected':'')+'>4</option>'+
'</select></div>'+
'</div>';
}
if(m.type==='video_gen'){
var ms=S.mediaSettings||{};
return '<div class="media-settings visible" id="mediaSettings">'+
'<div class="media-opt"><label>'+ic('video',13)+' Duration</label><select id="vidDur" onchange="updateMediaSetting(\'duration\',this.value)">'+
'<option value="5"'+((ms.duration||'5')==='5'?' selected':'')+'>5s</option>'+
'<option value="10"'+(ms.duration==='10'?' selected':'')+'>10s</option>'+
'<option value="15"'+(ms.duration==='15'?' selected':'')+'>15s</option>'+
'</select></div>'+
'<div class="media-opt"><label>Ratio</label><select id="vidRatio" onchange="updateMediaSetting(\'ratio\',this.value)">'+
'<option value="16:9"'+((ms.ratio||'16:9')==='16:9'?' selected':'')+'>16:9</option>'+
'<option value="9:16"'+(ms.ratio==='9:16'?' selected':'')+'>9:16</option>'+
'<option value="1:1"'+(ms.ratio==='1:1'?' selected':'')+'>1:1</option>'+
'</select></div>'+
'<div class="media-opt"><label>Resolution</label><select id="vidRes" onchange="updateMediaSetting(\'resolution\',this.value)">'+
'<option value="480p"'+(ms.resolution==='480p'?' selected':'')+'>480p</option>'+
'<option value="720p"'+((ms.resolution||'720p')==='720p'||ms.resolution==='720p'?' selected':'')+'>720p</option>'+
'</select></div>'+
'</div>';
}
return '<div class="media-settings" id="mediaSettings"></div>';
}

function getModelMeta(id){
if(!id)return null;
for(var i=0;i<S.models.length;i++){if(S.models[i].id===id)return S.models[i];}
return null;
}
function isMultiAgentModel(id){var m=getModelMeta(id);return m&&m.responsesApi;}
function renderReasoningEffortSelect(inPopover){
if(!isMultiAgentModel(S.model))return '';
var e=S.reasoningEffort||'high';
var opts=[
{v:'low',l:'Low (fewer agents)',d:'Faster, cheaper — fewer sub-agents'},
{v:'medium',l:'Medium',d:'Balanced speed and depth'},
{v:'high',l:'High (4 agents)',d:'4 parallel research agents'},
{v:'xhigh',l:'Max (16 agents)',d:'16 parallel agents — deepest research'}
];
var sel='';
for(var i=0;i<opts.length;i++){
sel+='<option value="'+opts[i].v+'"'+(e===opts[i].v?' selected':'')+' title="'+esc(opts[i].d)+'">'+esc(opts[i].l)+'</option>';
}
if(inPopover){
return '<div style="padding:.25rem .75rem"><label style="display:block;font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem">'+ic('brain',11)+' Agent Effort</label>'+
'<select class="toolbar-select" id="reasoningEffortSelect" onchange="setReasoningEffort(this.value)" style="width:100%;max-width:100%">'+sel+'</select>'+
'<div style="font-size:.65rem;color:var(--text-muted);margin-top:.2rem;padding:0 .125rem">'+(function(){for(var i=0;i<opts.length;i++){if(opts[i].v===e)return opts[i].d;}return '';})()+
'</div></div>';
}
return '<div class="set-row"><span class="set-label">'+ic('brain',14)+' Agent Effort</span><select class="set-input" id="reasoningEffortSelect" onchange="setReasoningEffort(this.value)">'+sel+'</select></div>'+
'<div style="font-size:.75rem;color:var(--text-muted);margin:-.25rem 0 .5rem .5rem">'+(function(){for(var i=0;i<opts.length;i++){if(opts[i].v===e)return opts[i].d;}return '';})()+
'</div>';
}

S.mediaSettings=(function(){try{return JSON.parse(localStorage.getItem('ava_media_settings')||'{}');}catch(e){return {};}})();
S.ctrlEnterSend=(function(){var v=localStorage.getItem('ava_ctrl_enter');return v===null?true:v==='true';})();
S.uploadFile=null;
S.assets=[];S.assetsLoaded=false;S.assetsFilter='all';S.assetsSort='date';S.assetsNodeFilter='all';
S.assetsFolder='';S.assetsSearch='';
S.selectedAssets=[];
S.toolsList=null;S.toolsTab='builtin';S.toolsGenBusy=false;S.toolsTestResult=null;S.toolsExpanded=null;

function updateMediaSettings(){
var el=document.getElementById('mediaSettings');
if(!el)return;
var m=getModelMeta(S.model);
if(m&&(m.type==='image_gen'||m.type==='video_gen')){
el.outerHTML=renderMediaSettings();
}else{
el.className='media-settings';el.innerHTML='';
}
}
window.updateMediaSetting=function(key,val){
S.mediaSettings[key]=val;
localStorage.setItem('ava_media_settings',JSON.stringify(S.mediaSettings));
};

window.triggerUpload=function(){
var inp=document.getElementById('fileUpload');if(inp)inp.click();
};
window.handleUpload=function(inp){
if(!inp.files||!inp.files[0])return;
var file=inp.files[0];
if(file.size>50*1024*1024){toast('File too large (max 50MB)','error');return;}
S.uploadFile=file;
var preview=document.getElementById('uploadPreview');
if(!preview)return;
if(file.type.startsWith('image/')){
var reader=new FileReader();
reader.onload=function(e){
preview.innerHTML='<div class="upload-row" style="padding:0 1.5rem"><img src="'+e.target.result+'" class="upload-preview"><button class="upload-remove" onclick="removeUpload()">'+ic('xic',12)+' Remove</button></div>';
};reader.readAsDataURL(file);
}else if(file.type.startsWith('video/')){
preview.innerHTML='<div class="upload-row" style="padding:0 1.5rem"><span style="font-size:.8125rem;color:var(--text-secondary)">'+ic('video',14)+' '+esc(file.name)+' ('+Math.round(file.size/1024)+'KB)</span><button class="upload-remove" onclick="removeUpload()">'+ic('xic',12)+' Remove</button></div>';
}
};
window.removeUpload=function(){
S.uploadFile=null;
var preview=document.getElementById('uploadPreview');if(preview)preview.innerHTML='';
var inp=document.getElementById('fileUpload');if(inp)inp.value='';
};

function renderHistoryPopover(){
return '<div class="popover-header"><span>Chat History</span><button onclick="newChat()">'+ic('plus',14)+' New Chat</button></div>'+
'<div id="histList"><div style="padding:.5rem;color:var(--text-muted);font-size:.8125rem">Loading...</div></div>';
}

function renderNotesPopover(){
var list='';
for(var i=0;i<S.notes.length;i++){
var n=S.notes[i];
list+='<div class="note-item"><span class="note-text">'+esc(n.text)+'</span>'+
'<button class="note-toggle '+(n.enabled?'on':'off')+'" onclick="toggleNote('+i+')"></button>'+
'<button class="note-del" onclick="deleteNote('+i+')">'+ic('xic',12)+'</button></div>';
}
if(!list)list='<div style="padding:.5rem;color:var(--text-muted);font-size:.8125rem">No notes yet</div>';

return '<div class="popover-header"><span>Notes</span><button onclick="showNoteInput()">'+ic('plus',14)+'</button></div>'+
list+
'<div class="notes-input-row" id="noteInputRow" style="display:none">'+
'<input class="notes-input" id="noteInput" placeholder="Type an instruction..." onkeydown="if(event.key===\'Enter\')saveNote()">'+
'<button class="notes-save-btn" onclick="saveNote()">'+ic('check',14)+'</button>'+
'</div>'+
'<div class="notes-hint">Active notes are sent with every message</div>';
}

function renderMessage(m,idx){
if(m.role==='user'){
var userBody=md(m.content);
if(m.media&&m.media.length){for(var mi=0;mi<m.media.length;mi++){var mm=m.media[mi];if(mm.type==='image'||mm.type==='image/png'||mm.type==='image/jpeg'){userBody+='<div style="margin-top:.5rem"><img src="'+esc(mm.url)+'" style="max-width:200px;max-height:200px;border-radius:8px;border:1px solid var(--border);cursor:zoom-in" alt="attached" onclick="openLightbox(this.src)"></div>';} else if(mm.type==='video'||mm.type==='video/mp4'){userBody+='<div style="margin-top:.5rem"><video src="'+esc(mm.url)+'" controls preload="metadata" style="max-width:200px;max-height:200px;border-radius:8px;border:1px solid var(--border)"></video></div>';}}}
var userLabel='You';
if(m.nodeTarget)userLabel='You '+ic('send',10)+' <span class="node-badge" style="vertical-align:middle">'+esc(m.nodeTarget)+'</span>';
return '<div class="msg msg-user'+(m.excluded?' excluded':'')+'" data-idx="'+idx+'"><div class="msg-label">'+userLabel+'</div><div class="msg-bubble">'+userBody+'</div></div>';
}
var isStreaming=S.streaming&&idx===S.messages.length-1;
if(!isStreaming&&!m.content&&!m.thinking&&(!m.tools||!m.tools.length)&&(!m.parts||!m.parts.length)&&!m.mediaStatus&&!m.media)return '';
var body='';
if(m.parts&&m.parts.length){
for(var pi=0;pi<m.parts.length;pi++){
var p=m.parts[pi];
if(p.type==='thinking'){
var tcc=p.text?p.text.length:0;
body+='<div class="think-block"><div class="think-head" onclick="toggleEl(this)">'+ic('brain',14)+' Thinking'+(p.done&&tcc?' ('+tcc+' chars)':p.done?'':' ...')+'</div><div class="think-body" style="display:none">'+esc(p.text||'')+'</div></div>';
}else if(p.type==='tool'){
var tl=m.tools[p.toolIdx];if(tl)body+=renderToolBlock(tl);
}else if(p.type==='ask_user'){
body+=renderAskUserBlock(m,idx,p.question);
}else if(p.type==='content'){body+=md(p.text||'');}
}
}else{
if(m.thinking){
body+='<div class="think-block"><div class="think-head" onclick="toggleEl(this)">'+ic('brain',14)+' Thinking'+(m.thinkingDone?'':' ...')+'</div><div class="think-body" style="display:none">'+esc(m.thinking)+'</div></div>';
}
if(m.tools&&m.tools.length){for(var t=0;t<m.tools.length;t++)body+=renderToolBlock(m.tools[t]);}
if(m.content)body+=md(m.content);
}
if(m.mediaStatus){
body+='<div class="media-progress"><div class="spinner"></div> '+(m.mediaStatus.status==='generating'?'Generating '+m.mediaStatus.type+'...':'Starting...')+'</div>';
}
if(m.usage){
var pt=m.usage.prompt_tokens||0,ct=m.usage.completion_tokens||0;
body+='<div class="usage-line">Tokens: '+(pt+ct).toLocaleString()+' (prompt: '+pt.toLocaleString()+', completion: '+ct.toLocaleString()+')</div>';
}
if(!body&&S.streaming&&idx===S.messages.length-1){body='<div class="spinner"></div>';}
if(m.media&&m.media.length){
var mediaDivs=body.match(/<div class="media-embed">/g)||[];
var mi=0;
body=body.replace(/<div class="media-embed">/g,function(){
var active=m.media[mi]&&m.media[mi].inContext;
var btn='<button class="media-context-toggle'+(active?' active':'')+'" data-media-toggle="'+idx+'-'+mi+'" onclick="event.stopPropagation();toggleMediaContext('+idx+','+mi+')">'+(active?'In context':'Use in context')+'</button>';
mi++;
return '<div class="media-embed" style="position:relative">'+btn;
});
}
var astLabel='Agent';
if(m.nodeSource)astLabel=ic('server',10)+' <span class="node-badge" style="vertical-align:middle">'+esc(m.nodeSource)+'</span>';
return '<div class="msg msg-ast'+(m.excluded?' excluded':'')+'" id="msg'+idx+'" data-idx="'+idx+'"><div class="msg-label">'+astLabel+'</div><div class="msg-bubble">'+body+'</div></div>';
}

window.toggleEl=function(head){
var body=head.nextElementSibling;if(!body)return;
body.style.display=body.style.display==='none'?'block':'none';
};

function renderAskUserBlock(m,idx,question){
var answered=m.askUser&&m.askUser.answered;
var h='<div class="ask-user-block">';
h+='<div class="ask-user-header">'+ic('user',14)+' Agent is asking you a question</div>';
h+='<div class="ask-user-question">'+esc(question)+'</div>';
if(answered){
h+='<div class="ask-user-answered">'+ic('check',12)+' Answered: '+esc(m.askUser.answer||'')+'</div>';
}else{
h+='<div class="ask-user-input-row"><input type="text" id="askUserInput'+idx+'" class="ask-user-input" placeholder="Type your answer..." onkeydown="if(event.key===\'Enter\'){event.preventDefault();submitAskUser('+idx+');}">';
h+='<button class="ask-user-btn" onclick="submitAskUser('+idx+')">'+ic('send',14)+' Reply</button></div>';
}
h+='</div>';
return h;
}
window.submitAskUser=function(idx){
var inp=document.getElementById('askUserInput'+idx);
if(!inp)return;
var answer=inp.value.trim();
if(!answer)return;
inp.disabled=true;
var btn=inp.parentElement.querySelector('.ask-user-btn');
if(btn){btn.disabled=true;btn.innerHTML=ic('check',14)+' Sent';}
api('/api/chat/ask_user',{method:'POST',body:{answer:answer}}).then(function(){
var m=S.messages[idx];
if(m&&m.askUser){m.askUser.answered=true;m.askUser.answer=answer;scheduleStreamUpdate(m);}
}).catch(function(e){
inp.disabled=false;
if(btn){btn.disabled=false;btn.innerHTML=ic('send',14)+' Reply';}
toast('Failed to send answer: '+e.message,'error');
});
};

function setupChat(){
var ta=document.getElementById('chatInput');
if(ta){ta.focus();var draft=localStorage.getItem('ava_draft');if(draft){ta.value=draft;ta.style.height='auto';ta.style.height=Math.min(ta.scrollHeight,150)+'px';}}
scrollToBottom(true);setupMsgActions();renderMermaidBlocks();installChatResizeObserver();
setupMentionPicker();
if(!S.nodesLoaded)loadChatNodes();
if(!window._wakeLockInstalled)initWakeLock();
}

/* Wake Lock — keeps screen on during streaming on mobile.
   Installed once globally to avoid stacking wrappers/listeners. */
var _wakeLock=null;
function initWakeLock(){
if(!('wakeLock' in navigator))return;
window._wakeLockInstalled=true;
function requestWakeLock(){
if(_wakeLock)return;
navigator.wakeLock.request('screen').then(function(lock){
_wakeLock=lock;
lock.addEventListener('release',function(){_wakeLock=null;});
}).catch(function(){});
}
function releaseWakeLock(){
if(_wakeLock){_wakeLock.release().catch(function(){});_wakeLock=null;}
}
document.addEventListener('visibilitychange',function(){
if(document.visibilityState==='hidden')releaseWakeLock();
else if(document.visibilityState==='visible'&&S.streaming)requestWakeLock();
});
window._wakeLockAcquire=requestWakeLock;
window._wakeLockRelease=releaseWakeLock;
}
function renderMermaidBlocks(){
if(typeof mermaid==='undefined')return;
var blocks=document.querySelectorAll('.mermaid-render[data-mermaid-src]');
blocks.forEach(function(el){
if(el.getAttribute('data-rendered'))return;
el.setAttribute('data-rendered','1');
var src=el.getAttribute('data-mermaid-src');
if(!src)return;
try{mermaid.render('mmd'+Math.random().toString(36).slice(2,8),src).then(function(r){
el.innerHTML=r.svg;
var svg=el.querySelector('svg');
if(svg){
svg.removeAttribute('width');
svg.removeAttribute('height');
svg.style.overflow='visible';
requestAnimationFrame(function(){
svg.querySelectorAll('foreignObject').forEach(function(fo){
var inner=fo.querySelector('.nodeLabel')||fo.querySelector('div')||fo.firstElementChild;
if(!inner)return;
var foH=parseFloat(fo.getAttribute('height'))||0;
var actualH=inner.scrollHeight||inner.offsetHeight||0;
if(actualH>foH+2){
var extra=actualH-foH+16;
fo.setAttribute('height',String(foH+extra));
var foY=parseFloat(fo.getAttribute('y'))||0;
fo.setAttribute('y',String(foY-extra/2));
var node=fo.closest('g');
if(node){var shape=node.querySelector('rect');if(shape&&shape.getAttribute('height')){
var sH=parseFloat(shape.getAttribute('height'))||0;var sY=parseFloat(shape.getAttribute('y'))||0;
shape.setAttribute('height',String(sH+extra));shape.setAttribute('y',String(sY-extra/2));
}}}
});
var bb=svg.getBBox();
if(bb&&bb.width>0){
var pad=40;
var nw=bb.width+pad*2,nh=bb.height+pad*2;
svg.setAttribute('viewBox',(bb.x-pad)+' '+(bb.y-pad)+' '+nw+' '+nh);
svg.dataset.naturalWidth=nw;
svg.dataset.naturalHeight=nh;
}
svg.removeAttribute('width');
svg.removeAttribute('height');
initPanZoom(el);
});
}
}).catch(function(err){el.innerHTML='<div class="mermaid-error">Diagram error: '+err.message+'</div>';});}
catch(err){el.innerHTML='<div class="mermaid-error">Diagram error: '+err.message+'</div>';}
});
}

function initPanZoom(container){
var svg=container.querySelector('svg');
if(!svg||container.querySelector('.pz-controls'))return;
var state={x:0,y:0,scale:1,dragging:false,startX:0,startY:0,lastDist:0};
svg.style.position='absolute';
svg.style.left='0';
svg.style.top='0';
svg.style.transformOrigin='0 0';
svg.style.cursor='grab';
svg.style.transition='none';
function applyTransform(){svg.style.transform='translate('+state.x+'px,'+state.y+'px) scale('+state.scale+')';}
function clampScale(s){return Math.max(0.15,Math.min(5,s));}

container.addEventListener('wheel',function(e){
e.preventDefault();
var rect=container.getBoundingClientRect();
var mx=e.clientX-rect.left, my=e.clientY-rect.top;
var delta=e.deltaY>0?0.9:1.1;
var ns=clampScale(state.scale*delta);
var ratio=ns/state.scale;
state.x=mx-(mx-state.x)*ratio;
state.y=my-(my-state.y)*ratio;
state.scale=ns;
applyTransform();
},{passive:false});

container.addEventListener('mousedown',function(e){
if(e.button!==0)return;
state.dragging=true;state.startX=e.clientX-state.x;state.startY=e.clientY-state.y;
svg.style.cursor='grabbing';
e.preventDefault();
});
window.addEventListener('mousemove',function(e){
if(!state.dragging)return;
state.x=e.clientX-state.startX;state.y=e.clientY-state.startY;
applyTransform();
});
window.addEventListener('mouseup',function(){
if(state.dragging){state.dragging=false;svg.style.cursor='grab';}
});

container.addEventListener('touchstart',function(e){
if(e.touches.length===1){
state.dragging=true;state.startX=e.touches[0].clientX-state.x;state.startY=e.touches[0].clientY-state.y;
}else if(e.touches.length===2){
state.dragging=false;
var dx=e.touches[0].clientX-e.touches[1].clientX;
var dy=e.touches[0].clientY-e.touches[1].clientY;
state.lastDist=Math.sqrt(dx*dx+dy*dy);
}
},{passive:true});
container.addEventListener('touchmove',function(e){
if(e.touches.length===1&&state.dragging){
e.preventDefault();
state.x=e.touches[0].clientX-state.startX;state.y=e.touches[0].clientY-state.startY;
applyTransform();
}else if(e.touches.length===2){
e.preventDefault();
var dx=e.touches[0].clientX-e.touches[1].clientX;
var dy=e.touches[0].clientY-e.touches[1].clientY;
var dist=Math.sqrt(dx*dx+dy*dy);
if(state.lastDist>0){
var rect=container.getBoundingClientRect();
var mx=(e.touches[0].clientX+e.touches[1].clientX)/2-rect.left;
var my=(e.touches[0].clientY+e.touches[1].clientY)/2-rect.top;
var ns=clampScale(state.scale*(dist/state.lastDist));
var ratio=ns/state.scale;
state.x=mx-(mx-state.x)*ratio;
state.y=my-(my-state.y)*ratio;
state.scale=ns;
applyTransform();
}
state.lastDist=dist;
}
},{passive:false});
container.addEventListener('touchend',function(e){
state.dragging=false;state.lastDist=0;
},{passive:true});

var ctrl=document.createElement('div');ctrl.className='pz-controls';
ctrl.innerHTML='<button class="pz-btn" onclick="pzZoom(this,1.3)" title="Zoom in">'+ic('plus',14)+'</button>'+
'<button class="pz-btn" onclick="pzZoom(this,0.7)" title="Zoom out">'+ic('minus',14)+'</button>'+
'<button class="pz-btn" onclick="pzReset(this)" title="Reset">'+ic('refresh',14)+'</button>'+
'<button class="pz-btn" onclick="pzExportPng(this)" title="Save PNG">'+ic('download',14)+'</button>';
container.style.position='relative';container.style.overflow='hidden';
container.appendChild(ctrl);
container._pzState=state;
container._pzApply=applyTransform;

requestAnimationFrame(function(){
var cRect=container.getBoundingClientRect();
var sW=parseFloat(svg.dataset.naturalWidth)||0;
var sH=parseFloat(svg.dataset.naturalHeight)||0;
if(!sW||!sH){try{var bb=svg.getBBox();sW=bb.width+80;sH=bb.height+80;}catch(e){sW=800;sH=600;}}
state._origW=sW;state._origH=sH;
svg.setAttribute('width',sW);
svg.setAttribute('height',sH);
if(sW>0&&sH>0&&cRect.width>0&&cRect.height>0){
var fit=Math.min(cRect.width/sW,cRect.height/sH,1);
state.scale=fit*0.92;
state.x=(cRect.width-sW*state.scale)/2;
state.y=Math.max(0,(cRect.height-sH*state.scale)/2);
applyTransform();
}
});
}

window.pzZoom=function(btn,factor){
var c=btn.closest('.mermaid-render')||btn.closest('.sys-mermaid-wrap');
if(!c)c=btn.parentElement.parentElement;
if(!c||!c._pzState)return;
var s=c._pzState;
var rect=c.getBoundingClientRect();
var mx=rect.width/2, my=rect.height/2;
var ns=Math.max(0.15,Math.min(5,s.scale*factor));
var ratio=ns/s.scale;
s.x=mx-(mx-s.x)*ratio;s.y=my-(my-s.y)*ratio;
s.scale=ns;c._pzApply();
};
window.pzReset=function(btn){
var c=btn.closest('.mermaid-render')||btn.closest('.sys-mermaid-wrap');
if(!c)c=btn.parentElement.parentElement;
if(!c||!c._pzState)return;
var s=c._pzState;
var cRect=c.getBoundingClientRect();
var sW=s._origW||1;var sH=s._origH||1;
if(sW>0&&sH>0&&cRect.width>0){
var fit=Math.min(cRect.width/sW,cRect.height/sH,1);
s.scale=fit*0.92;
s.x=(cRect.width-sW*s.scale)/2;
s.y=Math.max(0,(cRect.height-sH*s.scale)/2);
}else{s.x=0;s.y=0;s.scale=1;}
c._pzApply();
};
window.pzExportPng=function(btn){
var c=btn.closest('.mermaid-render')||btn.closest('.sys-mermaid-wrap');
if(!c)c=btn.parentElement.parentElement;
var svg=c?c.querySelector('svg'):null;
if(!svg){toast('No diagram to export','error');return;}
var origShapes=svg.querySelectorAll('rect,polygon,circle,ellipse');
var clone=svg.cloneNode(true);
clone.removeAttribute('style');
clone.setAttribute('xmlns','http://www.w3.org/2000/svg');
clone.setAttribute('xmlns:xlink','http://www.w3.org/1999/xlink');
var fos=Array.prototype.slice.call(clone.querySelectorAll('foreignObject'));
fos.forEach(function(fo){
var txt=(fo.textContent||'').trim();
if(!txt){fo.parentNode.removeChild(fo);return;}
var x=parseFloat(fo.getAttribute('x'))||0;
var y=parseFloat(fo.getAttribute('y'))||0;
var w=parseFloat(fo.getAttribute('width'))||200;
var h=parseFloat(fo.getAttribute('height'))||30;
var g=document.createElementNS('http://www.w3.org/2000/svg','g');
var lines=txt.split(/\n/).filter(function(l){return l.trim();});
if(lines.length===1&&txt.length>30){
var words=txt.split(/\s+/),cur='',wrapped=[];
words.forEach(function(word){
if((cur+' '+word).trim().length>35&&cur){wrapped.push(cur);cur=word;}
else cur=cur?cur+' '+word:word;
});
if(cur)wrapped.push(cur);
lines=wrapped;
}
var lh=16,totalH=lines.length*lh;
var startY=y+(h-totalH)/2+lh*0.75;
lines.forEach(function(line,i){
var t=document.createElementNS('http://www.w3.org/2000/svg','text');
t.setAttribute('x',x+w/2);
t.setAttribute('y',startY+i*lh);
t.setAttribute('text-anchor','middle');
t.setAttribute('fill','#e2e2e8');
t.setAttribute('font-family','Inter,system-ui,sans-serif');
t.setAttribute('font-size','13');
t.textContent=line.trim();
g.appendChild(t);
});
fo.parentNode.replaceChild(g,fo);
});
var edges=clone.querySelectorAll('.edgePaths path,.flowchart-link');
edges.forEach(function(p){
if(!p.getAttribute('stroke'))p.setAttribute('stroke','#a855f7');
if(!p.getAttribute('fill'))p.setAttribute('fill','none');
});
var markers=clone.querySelectorAll('marker path');
markers.forEach(function(m){if(!m.getAttribute('fill'))m.setAttribute('fill','#a855f7');});
var cloneShapes=clone.querySelectorAll('rect,polygon,circle,ellipse');
for(var si=0;si<cloneShapes.length&&si<origShapes.length;si++){
var cr=cloneShapes[si],orig=origShapes[si];
var cs=window.getComputedStyle(orig);
if(!cr.getAttribute('fill')){var f=cs.fill;if(f&&f!=='none'&&f!=='')cr.setAttribute('fill',f);}
if(!cr.getAttribute('stroke')){var s=cs.stroke;if(s&&s!=='none'&&s!=='')cr.setAttribute('stroke',s);}
}
var leftoverFo=clone.querySelectorAll('foreignObject');
for(var fi=leftoverFo.length-1;fi>=0;fi--)leftoverFo[fi].parentNode.removeChild(leftoverFo[fi]);
var vb=clone.getAttribute('viewBox');
var cw=800,ch=600;
if(vb){var p=vb.split(/[\s,]+/).map(Number);cw=p[2]||800;ch=p[3]||600;}
else{cw=parseFloat(clone.getAttribute('width'))||800;ch=parseFloat(clone.getAttribute('height'))||600;}
clone.setAttribute('width',cw);
clone.setAttribute('height',ch);
var bg=document.createElementNS('http://www.w3.org/2000/svg','rect');
bg.setAttribute('x',vb?vb.split(/[\s,]+/)[0]:'0');
bg.setAttribute('y',vb?vb.split(/[\s,]+/)[1]:'0');
bg.setAttribute('width',cw);bg.setAttribute('height',ch);
bg.setAttribute('fill','#0c0c18');
clone.insertBefore(bg,clone.firstChild);
var svgData='<?xml version="1.0" encoding="UTF-8"?>'+new XMLSerializer().serializeToString(clone);
svgData=svgData.replace(/<style[^>]*>[\s\S]*?<\/style>/gi,'');
var svgBlob=new Blob([svgData],{type:'image/svg+xml;charset=utf-8'});
var url=URL.createObjectURL(svgBlob);
var img=new Image();
img.onload=function(){
var scale=2;
var canvas=document.createElement('canvas');
canvas.width=cw*scale;canvas.height=ch*scale;
var ctx2=canvas.getContext('2d');
ctx2.fillStyle='#0c0c18';ctx2.fillRect(0,0,canvas.width,canvas.height);
ctx2.drawImage(img,0,0,canvas.width,canvas.height);
URL.revokeObjectURL(url);
canvas.toBlob(function(blob){
if(!blob){toast('PNG export failed','error');return;}
var a=document.createElement('a');a.href=URL.createObjectURL(blob);
a.download='agent-flow-'+(new Date().toISOString().slice(0,10))+'.png';
document.body.appendChild(a);a.click();document.body.removeChild(a);
setTimeout(function(){URL.revokeObjectURL(a.href);},500);
toast('PNG exported','success');
},'image/png');
};
img.onerror=function(){URL.revokeObjectURL(url);toast('PNG export failed - try zooming in first','error');};
img.src=url;
};
window.chatKey=function(e){
var ctrlSend=S.ctrlEnterSend!==false;
if(ctrlSend){if(e.key==='Enter'&&(e.ctrlKey||e.metaKey)){e.preventDefault();sendMsg();}}
else{if(e.key==='Enter'&&!e.shiftKey){e.preventDefault();sendMsg();}}
};
window.autoGrow=function(el){el.style.height='auto';el.style.height=Math.min(el.scrollHeight,150)+'px';try{localStorage.setItem('ava_draft',el.value);}catch(x){}scrollToBottom();};
window.onChatScroll=function(){
var el=document.getElementById('chatMsgs');if(!el)return;
autoScroll=el.scrollTop+el.clientHeight>=el.scrollHeight-50;
};
function scrollToBottom(force){
if(!force&&!autoScroll)return;
requestAnimationFrame(function(){requestAnimationFrame(function(){
var el=document.getElementById('chatMsgs');if(el)el.scrollTop=el.scrollHeight;
});});
}
var _chatResizeObs=null;
function installChatResizeObserver(){
if(_chatResizeObs)_chatResizeObs.disconnect();
var wrap=document.querySelector('.chat-wrap');if(!wrap||typeof ResizeObserver==='undefined')return;
_chatResizeObs=new ResizeObserver(function(){scrollToBottom();});
_chatResizeObs.observe(wrap);
}

/* ── Chat Persistence ─────────────────────────────────── */
function saveMessagesToLocal(){
if(!S.session)return;
try{
var lite=S.messages.map(function(m){
var c={role:m.role,content:m.content||''};
if(m.thinking)c.thinking=m.thinking;
if(m.thinkingDone)c.thinkingDone=true;
if(m.tools&&m.tools.length)c.tools=m.tools.map(function(t){return{name:t.name,args:t.args,result:t.result,success:t.success,done:t.done,duration:t.duration};});
if(m.parts&&m.parts.length)c.parts=m.parts;
if(m.media&&m.media.length)c.media=m.media;
if(m.usage)c.usage=m.usage;
if(m.excluded)c.excluded=true;
return c;
});
localStorage.setItem('ava_msgs_'+S.session,JSON.stringify(lite));
}catch(e){}
}
function loadMessagesFromLocal(sessionName){
try{
var d=localStorage.getItem('ava_msgs_'+sessionName);
if(d){var arr=JSON.parse(d);if(Array.isArray(arr)&&arr.length)return arr;}
}catch(e){}
return null;
}
function restoreMessagesFromApi(data){
var msgs=[];
if(!data||!data.messages)return msgs;
var toolResultMap={};
for(var i=0;i<data.messages.length;i++){
var m=data.messages[i];
if(m.role==='tool'&&m.tool_call_id)toolResultMap[m.tool_call_id]=m.content;
}
for(var i=0;i<data.messages.length;i++){
var m=data.messages[i];
if(m.role==='system'||m.role==='tool')continue;
var msg={role:m.role,content:m.content||'',thinking:'',thinkingDone:true,tools:[],parts:[],usage:null,media:null};
if(m.role==='assistant'&&m.tool_calls&&m.tool_calls.length){
for(var ti=0;ti<m.tool_calls.length;ti++){
var tc=m.tool_calls[ti];
var args={};try{args=JSON.parse(tc.arguments||'{}');}catch(x){}
var res=toolResultMap[tc.id]||null;
var success=true;
if(res){try{var rj=JSON.parse(res);if(rj.error)success=false;}catch(x){}}
msg.tools.push({name:tc.name,args:args,result:res,success:success,done:true,duration:0});
msg.parts.push({type:'tool',toolIdx:ti});
}
if(m.content)msg.parts.push({type:'content',text:m.content});
}
msgs.push(msg);
}
return msgs;
}

/* ── Chat Actions ─────────────────────────────────────── */
function doSendChat(msg,extraContextMedia){
var aMsg={role:'assistant',content:'',thinking:'',thinkingDone:false,tools:[],usage:null,media:null,mediaStatus:null,parts:[]};
S.messages.push(aMsg);
S.streaming=true;S.streamStart=Date.now();autoScroll=true;
if(window._wakeLockAcquire)window._wakeLockAcquire();
if(S.streamTimer)clearInterval(S.streamTimer);
S.streamTimer=setInterval(function(){
if(!S.streaming){clearInterval(S.streamTimer);S.streamTimer=null;return;}
var si=document.querySelector('.streaming-indicator');
if(si){var el=Math.round((Date.now()-S.streamStart)/1000);
si.innerHTML='<div class="streaming-dot"></div> Generating'+(S.model?' with '+S.model.split('-').slice(0,2).join('-'):'')+' &middot; '+el+'s';}
},1000);
render();
var headers={'Content-Type':'application/json'};
if(S.token)headers['Authorization']='Bearer '+S.token;
var activeNotes=S.notes.filter(function(n){return n.enabled;}).map(function(n){return n.text;});
var contextMediaArr=(extraContextMedia||[]).slice();
for(var si=0;si<S.selectedAssets.length;si++){
var sa=S.selectedAssets[si];
if(!sa.inContext)continue;
var dup=false;for(var di=0;di<contextMediaArr.length;di++){if(contextMediaArr[di].url===sa.url){dup=true;break;}}
if(!dup)contextMediaArr.push(sa);
}
for(var ci=0;ci<S.messages.length-1;ci++){
var cm=S.messages[ci];
if(cm.media){for(var cj=0;cj<cm.media.length;cj++){if(cm.media[cj].inContext){
var mdup=false;for(var mdi=0;mdi<contextMediaArr.length;mdi++){if(contextMediaArr[mdi].url===cm.media[cj].url){mdup=true;break;}}
if(!mdup)contextMediaArr.push(cm.media[cj]);
}}}
}
var chatBody={message:msg,model:S.model,session:S.session,mode:S.mode,use_history:S.useHistory,notes:activeNotes};
if(S.mediaSettings)chatBody.media_settings=S.mediaSettings;
if(contextMediaArr.length)chatBody.context_media=contextMediaArr;
if(S.maxOutputTokens>0)chatBody.max_tokens=S.maxOutputTokens;
if(isMultiAgentModel(S.model)&&S.reasoningEffort)chatBody.reasoning_effort=S.reasoningEffort;
chatAbort=new AbortController();
fetch('/api/chat',{
method:'POST',headers:headers,
body:JSON.stringify(chatBody),
signal:chatAbort.signal
}).then(function(res){
if(!res.ok)return res.text().then(function(t){throw new Error(t||'Chat failed');});
var reader=res.body.getReader(),dec=new TextDecoder(),buf='';
function handleSseLine(ln){
if(!ln.startsWith('data: '))return;
var d;try{d=JSON.parse(ln.slice(6));}catch(x){return;}
switch(d.type||''){
case 'content_delta':
aMsg.content+=(d.content||'');
var lp=aMsg.parts.length?aMsg.parts[aMsg.parts.length-1]:null;
if(!lp||lp.type!=='content'){aMsg.parts.push({type:'content',text:d.content||''});}
else{lp.text+=(d.content||'');}
break;
case 'thinking_delta':
aMsg.thinking+=(d.content||'');
var lpt=aMsg.parts.length?aMsg.parts[aMsg.parts.length-1]:null;
if(!lpt||lpt.type!=='thinking'){aMsg.parts.push({type:'thinking',text:d.content||'',done:false});}
else{lpt.text+=(d.content||'');}
break;
case 'thinking_done':
aMsg.thinkingDone=true;
for(var ti=aMsg.parts.length-1;ti>=0;ti--){if(aMsg.parts[ti].type==='thinking'){aMsg.parts[ti].done=true;break;}}
break;
case 'tool_start':
var toolObj={name:d.name||'tool',args:d.arguments||{},result:null,success:null,done:false,startTime:Date.now()};
aMsg.tools.push(toolObj);
aMsg.parts.push({type:'tool',toolIdx:aMsg.tools.length-1});
S.toolLog.push({name:d.name||'tool',args:d.arguments||{},result:null,success:null,done:false,time:new Date().toISOString(),duration:0});break;
case 'tool_done':
var tf=null;for(var j=aMsg.tools.length-1;j>=0;j--){if(aMsg.tools[j].name===(d.name||'')&&!aMsg.tools[j].done){tf=aMsg.tools[j];break;}}
if(tf){tf.result=d.result;tf.success=d.success!==false;tf.done=true;tf.duration=Date.now()-(tf.startTime||Date.now());}
for(var tli=S.toolLog.length-1;tli>=0;tli--){if(S.toolLog[tli].name===(d.name||'')&&!S.toolLog[tli].done){S.toolLog[tli].result=d.result;S.toolLog[tli].success=d.success!==false;S.toolLog[tli].done=true;S.toolLog[tli].duration=tf?tf.duration:0;break;}}
if(d.name==='summarize_and_new_chat'&&d.result){
try{var scr=typeof d.result==='string'?JSON.parse(d.result):d.result;
if(scr.session){
avaConfirm('The AI has created a summary session: '+scr.session+'. Switch to it now?',function(){
S.session=scr.session;localStorage.setItem('ava_last_session',scr.session);
S.messages=[];S.streaming=false;render();
loadSession(scr.session);
},{title:'New Session Available',okText:'Switch'});
}}catch(x){}
}
if(d.name&&(d.name.indexOf('image')>=0||d.name.indexOf('video')>=0)&&d.result){
try{var tr=typeof d.result==='string'?JSON.parse(d.result):d.result;
var turl=tr.url||'';var tarr=tr.images||tr.results||[];
if(turl){var ttype=/\.(mp4|webm|mov)(\?|$)/i.test(turl)?'video':'image';
if(!aMsg.media)aMsg.media=[];aMsg.media.push({url:turl,type:ttype,inContext:true});
S.selectedAssets.push({url:turl,type:ttype,inContext:true,thumb_url:turl});S.assetsLoaded=false;}
for(var tri=0;tri<tarr.length;tri++){var tu=tarr[tri].url||tarr[tri];if(typeof tu==='string'&&tu){
var tt=/\.(mp4|webm|mov)(\?|$)/i.test(tu)?'video':'image';
if(!aMsg.media)aMsg.media=[];aMsg.media.push({url:tu,type:tt,inContext:true});
S.selectedAssets.push({url:tu,type:tt,inContext:true,thumb_url:tu});S.assetsLoaded=false;}}
}catch(e){}}
break;
case 'ask_user':
aMsg.askUser={question:d.question||'',answered:false};
aMsg.parts.push({type:'ask_user',question:d.question||''});
break;
case 'media_start':
aMsg.mediaStatus={type:d.media_type||'image',status:'starting',prompt:d.prompt||''};break;
case 'media_progress':
if(aMsg.mediaStatus)aMsg.mediaStatus.status=d.status||'generating';break;
case 'media_done':
aMsg.mediaStatus=null;
if(d.urls&&d.urls.length){
if(!aMsg.media)aMsg.media=[];
for(var mi=0;mi<d.urls.length;mi++){
var murl=d.urls[mi];var mtype=murl.type||d.media_type||'image';
aMsg.media.push({url:murl.url,type:mtype,inContext:true});
S.selectedAssets.push({url:murl.url,type:mtype,inContext:true,thumb_url:murl.url});
}
var mc='';for(var mj=0;mj<d.urls.length;mj++){
var mu=d.urls[mj];
if((mu.type||d.media_type)==='video')mc+='![video]('+mu.url+')\n';
else mc+='![image]('+mu.url+')\n';
}
aMsg.content+=mc;
S.assetsLoaded=false;
}
if(d.result&&d.result.error&&!d.success)aMsg.content+='*Error: '+(d.result.error)+'*\n';
break;
case 'usage':aMsg.usage=d;
S.lastTokenUsage={prompt:d.prompt_tokens||0,completion:d.completion_tokens||0,total:(d.prompt_tokens||0)+(d.completion_tokens||0)};
break;
case 'session_init':
if(d.session){S.session=d.session;localStorage.setItem('ava_last_session',d.session);}
break;
case 'done':S.streaming=false;chatAbort=null;if(S.streamTimer){clearInterval(S.streamTimer);S.streamTimer=null;}if(window._wakeLockRelease)window._wakeLockRelease();if(d.session){S.session=d.session;localStorage.setItem('ava_last_session',d.session);}
saveMessagesToLocal();
if(!S.assetsLoaded)fetchAssets(function(){});
var ab=document.getElementById('assetBar');if(ab)ab.innerHTML=renderAssetBar();
break;
}
if(d.type==='tool_done')saveMessagesToLocal();
scheduleStreamUpdate(aMsg);
}
function flushSseBuffer(final){
var lines=buf.split('\n');
if(!final){buf=lines.pop()||'';}else{buf='';}
for(var i=0;i<lines.length;i++)handleSseLine(lines[i]);
}
function pump(){
return reader.read().then(function(r){
if(r.value)buf+=dec.decode(r.value,{stream:!r.done});
if(r.done){
flushSseBuffer(true);
if(S.streaming){
/* Stream closed without a done event — connection dropped while agent runs.
   Trigger the same reconnection polling as the catch handler. */
throw new Error('stream_disconnected');
}
chatAbort=null;render();
return;
}
flushSseBuffer(false);
return pump();
});
}
return pump();
}).catch(function(e){
if(e.name==='AbortError'){chatAbort=null;S.streaming=false;if(window._wakeLockRelease)window._wakeLockRelease();render();return;}
/* Stream broke unexpectedly — agent may still be running server-side.
   Poll /api/chat/status until it's no longer busy, then reload the session. */
toast('Connection lost — waiting for agent to finish...','warning');
chatAbort=null;
var pollDelay=2000,maxPoll=120,polls=0;
function pollStatus(){
polls++;
if(polls>maxPoll){S.streaming=false;if(window._wakeLockRelease)window._wakeLockRelease();toast('Agent timed out after reconnection attempt','error');render();return;}
var ph={};if(S.token)ph['Authorization']='Bearer '+S.token;
fetch('/api/chat/status',{headers:ph}).then(function(r){return r.json();}).then(function(d){
if(!d.busy){
S.streaming=false;if(window._wakeLockRelease)window._wakeLockRelease();
if(S.streamTimer){clearInterval(S.streamTimer);S.streamTimer=null;}
toast('Agent finished — reloading session','success');
api('/api/sessions/'+encodeURIComponent(S.session)).then(function(data){
if(!data)return;
S.messages=restoreMessagesFromApi(data);saveMessagesToLocal();autoScroll=true;render();
setTimeout(function(){scrollToBottom(true);},50);
}).catch(function(){render();});
return;
}
var si=document.querySelector('.streaming-indicator');
if(si)si.innerHTML='<div class="streaming-dot" style="background:var(--warning)"></div> Reconnecting &middot; poll '+polls;
setTimeout(pollStatus,Math.min(pollDelay*Math.pow(1.3,polls-1),15000));
}).catch(function(){
setTimeout(pollStatus,Math.min(pollDelay*Math.pow(1.3,polls-1),15000));
});
}
pollStatus();
});
}
window.sendMsg=function(){
var inp=document.getElementById('chatInput');if(!inp)return;
var msg=inp.value.trim();if(!msg||S.streaming)return;

// Detect @node mentions
var targetNode=null;
var cleanMsg=msg;
var atMatch=msg.match(/^@(\S+)\s+([\s\S]*)$/);
if(atMatch){
var mentionLabel=atMatch[1];
cleanMsg=atMatch[2];
for(var ni=0;ni<S.nodesList.length;ni++){
if((S.nodesList[ni].label||S.nodesList[ni].ip)===mentionLabel&&S.nodesList[ni].status==='connected'){
targetNode=S.nodesList[ni];break;
}
}
}

// If a node is selected in the sidebar, route to it
if(!targetNode&&S.activeNodeId){
for(var ni=0;ni<S.nodesList.length;ni++){
if(S.nodesList[ni].id===S.activeNodeId&&S.nodesList[ni].status==='connected'){
targetNode=S.nodesList[ni];break;
}
}
}

if(targetNode){
inp.value='';inp.style.height='auto';localStorage.removeItem('ava_draft');
S.messages.push({role:'user',content:msg,nodeTarget:targetNode.label});
sendToNode(targetNode,cleanMsg);
return;
}

if(!S.hasXaiKey){
S.messages.push({role:'user',content:msg});
S.messages.push({role:'assistant',content:'The xAI API Key has not been configured. Please go to [Settings](#/settings) to set your key to chat.',thinking:'',thinkingDone:true,tools:[],parts:[],temp:true});
fetch('/api/sessions/append',{method:'POST',body:JSON.stringify({session:S.session,role:'user',content:msg})});
inp.value='';inp.style.height='auto';localStorage.removeItem('ava_draft');
render();setTimeout(function(){scrollToBottom(true);},50);return;
}
inp.value='';inp.style.height='auto';
localStorage.removeItem('ava_draft');
if(S.uploadFile){
var fd=new FormData();fd.append('file',S.uploadFile);
var uh={};if(S.token)uh['Authorization']='Bearer '+S.token;
fetch('/api/upload',{method:'POST',headers:uh,body:fd}).then(function(r){return r.json();}).then(function(uj){
S.uploadFile=null;var preview=document.getElementById('uploadPreview');if(preview)preview.innerHTML='';
S.messages.push({role:'user',content:msg,media:[{url:uj.url,type:uj.type}]});
doSendChat(msg,[{url:uj.url,type:uj.type}]);
}).catch(function(e){toast('Upload failed: '+e.message,'error');S.messages.push({role:'user',content:msg});doSendChat(msg,[]);});
}else{
var ctxMedia=[];
for(var si=0;si<S.selectedAssets.length;si++){if(S.selectedAssets[si].inContext)ctxMedia.push(S.selectedAssets[si]);}
S.messages.push({role:'user',content:msg,media:ctxMedia.length?ctxMedia:undefined});
doSendChat(msg,[]);
}
};

function sendToNode(node,message){
S.streaming=true;S.streamStart=Date.now();
var aMsg={role:'assistant',content:'',thinking:'',thinkingDone:false,tools:[],parts:[],nodeSource:node.label};
S.messages.push(aMsg);
render();scrollToBottom(true);

var h={'Content-Type':'application/json'};
var t=localStorage.getItem('avacli_token');if(t)h['Authorization']='Bearer '+t;
if(S.token)h['Authorization']='Bearer '+S.token;

fetch('/api/nodes/'+encodeURIComponent(node.id)+'/chat',{
method:'POST',headers:h,
body:JSON.stringify({message:message,model:S.model||'',session:''})
}).then(function(res){
if(!res.ok)return res.text().then(function(t){throw new Error(t||'Node chat failed');});
var reader=res.body.getReader(),dec=new TextDecoder(),buf='';
function handleLine(ln){
if(!ln.startsWith('data: '))return;
var d;try{d=JSON.parse(ln.slice(6));}catch(x){return;}
if(d.type==='content_delta'){
aMsg.content+=(d.content||'');
var lp=aMsg.parts.length?aMsg.parts[aMsg.parts.length-1]:null;
if(!lp||lp.type!=='content'){aMsg.parts.push({type:'content',text:d.content||''});}
else{lp.text+=(d.content||'');}
}else if(d.type==='thinking_delta'){
aMsg.thinking+=(d.content||'');
var lpt=aMsg.parts.length?aMsg.parts[aMsg.parts.length-1]:null;
if(!lpt||lpt.type!=='thinking'){aMsg.parts.push({type:'thinking',text:d.content||'',done:false});}
else{lpt.text+=(d.content||'');}
}else if(d.type==='thinking_done'){
aMsg.thinkingDone=true;
for(var ti=aMsg.parts.length-1;ti>=0;ti--){if(aMsg.parts[ti].type==='thinking'){aMsg.parts[ti].done=true;break;}}
}else if(d.type==='tool_start'){
aMsg.tools.push({name:d.name||'tool',args:d.arguments||{},result:null,success:null,done:false,startTime:Date.now()});
aMsg.parts.push({type:'tool',toolIdx:aMsg.tools.length-1});
}else if(d.type==='tool_done'){
var tf=null;for(var j=aMsg.tools.length-1;j>=0;j--){if(aMsg.tools[j].name===(d.name||'')&&!aMsg.tools[j].done){tf=aMsg.tools[j];break;}}
if(tf){tf.result=d.result;tf.success=d.success!==false;tf.done=true;tf.duration=Date.now()-(tf.startTime||Date.now());}
}else if(d.type==='done'||d.type==='node_done'){
S.streaming=false;render();return;
}
scheduleStreamUpdate(aMsg);
}
function pump(){
return reader.read().then(function(r){
if(r.value)buf+=dec.decode(r.value,{stream:!r.done});
if(r.done){S.streaming=false;render();return;}
var lines=buf.split('\n');buf=lines.pop()||'';
for(var i=0;i<lines.length;i++)handleLine(lines[i]);
return pump();
});
}
return pump();
}).catch(function(e){
toast('Node error: '+e.message,'error');
if(!aMsg.content)aMsg.content='*Error reaching node '+node.label+': '+e.message+'*';
S.streaming=false;render();
});
}

function renderDiffBlock(diff){
var lines=diff.split('\n');
var html='<div class="diff-block">';
for(var i=0;i<lines.length;i++){
var l=lines[i];
var cls='diff-ctx';
if(l.indexOf('---')===0||l.indexOf('+++')===0)cls='diff-hdr';
else if(l.indexOf('@@')===0)cls='diff-hunk';
else if(l.indexOf('+')===0)cls='diff-add';
else if(l.indexOf('-')===0)cls='diff-del';
html+='<div class="'+cls+'">'+esc(l)+'</div>';
}
return html+'</div>';
}
function renderToolBlock(tl){
var sc=tl.done?(tl.success?'tool-ok':'tool-fail'):'tool-run';
var st=tl.done?(tl.success?'Success':'Failed'):'Running...';
var chev=tl.done?ic('chevR',10):ic('chevD',10);
var bodyDisplay=tl.done?'none':'block';
var args='';try{args=JSON.stringify(tl.args,null,2);}catch(x){args=String(tl.args);}
var res='';var diffHtml='';
if(tl.result!=null){
var resultObj=null;
try{resultObj=typeof tl.result==='string'?JSON.parse(tl.result):tl.result;}catch(x){}
if(resultObj&&resultObj.diff){
diffHtml=renderDiffBlock(resultObj.diff);
var summary=resultObj.path?(resultObj.edits_applied!=null?resultObj.edits_applied+' edit(s) applied to '+resultObj.path:'wrote '+resultObj.path):'';
res=summary;
}else{
try{res=typeof tl.result==='string'?tl.result:JSON.stringify(tl.result,null,2);}catch(x){res=String(tl.result);}
if(res.length>2000)res=res.slice(0,2000)+'...';
}
}
var outputSection='';
if(diffHtml){outputSection='<hr class="tool-sep"><div style="font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem;text-transform:uppercase;letter-spacing:.5px">Changes</div>'+(res?'<div style="font-size:.75rem;color:var(--text-secondary);margin-bottom:.25rem">'+esc(res)+'</div>':'')+diffHtml;}
else if(res){outputSection='<hr class="tool-sep"><div style="font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem;text-transform:uppercase;letter-spacing:.5px">Output</div><pre>'+esc(res)+'</pre>';}
return '<div class="tool-block"><div class="tool-head" onclick="toggleEl(this)">'+ic('wrench',14)+' <span class="tool-name">'+esc(tl.name)+'</span><span class="tool-status '+sc+'">'+st+'</span></div><div class="tool-body" style="display:'+bodyDisplay+'"><div style="font-size:.6875rem;color:var(--text-muted);margin-bottom:.25rem;text-transform:uppercase;letter-spacing:.5px">Input</div><pre>'+esc(args)+'</pre>'+outputSection+'</div></div>';
}
function scheduleStreamUpdate(aMsg){
if(rafPending)return;rafPending=true;
requestAnimationFrame(function(){
rafPending=false;
var idx=S.messages.length-1;
var el=document.getElementById('msg'+idx);
if(el){var bubble=el.querySelector('.msg-bubble');if(bubble){
var body='';
if(aMsg.parts&&aMsg.parts.length){
for(var pi=0;pi<aMsg.parts.length;pi++){
var p=aMsg.parts[pi];
if(p.type==='thinking'){
var tDone=p.done;
var charCount=p.text?p.text.length:0;
var countLabel=tDone&&charCount>0?' ('+charCount+' chars)':'';
body+='<div class="think-block"><div class="think-head" onclick="toggleEl(this)">'+ic('brain',14)+' Thinking'+(tDone?countLabel:' ...')+'</div><div class="think-body" style="display:'+(tDone?'none':'block')+'">'+esc(p.text||'')+'</div></div>';
}else if(p.type==='tool'){
var tl=aMsg.tools[p.toolIdx];
if(tl)body+=renderToolBlock(tl);
}else if(p.type==='ask_user'){
body+=renderAskUserBlock(aMsg,idx,p.question);
}else if(p.type==='content'){
body+=md(p.text||'');
}
}
}else{
if(aMsg.thinking)body+='<div class="think-block"><div class="think-head" onclick="toggleEl(this)">'+ic('brain',14)+' Thinking'+(aMsg.thinkingDone?'':' ...')+'</div><div class="think-body" style="display:none">'+esc(aMsg.thinking)+'</div></div>';
if(aMsg.tools&&aMsg.tools.length){for(var t=0;t<aMsg.tools.length;t++)body+=renderToolBlock(aMsg.tools[t]);}
if(aMsg.content)body+=md(aMsg.content);
}
if(aMsg.mediaStatus){
var ms=aMsg.mediaStatus;
body+='<div class="media-progress"><div class="spinner"></div> '+(ms.status==='generating'?'Generating '+ms.type+'...':'Starting '+ms.type+' generation...')+'</div>';
}
if(S.streaming&&!aMsg.mediaStatus&&aMsg.content)body+='<span class="blink-cursor"></span>';
if(!body)body='<div class="spinner"></div>';
bubble.innerHTML=body;
}}
var assetBarEl=document.getElementById('assetBar');
if(assetBarEl)assetBarEl.innerHTML=renderAssetBar();
renderMermaidBlocks();scrollToBottom();
// Update streaming timer
var si=document.querySelector('.streaming-indicator');
if(si&&S.streaming){
var elapsed=Math.round((Date.now()-S.streamStart)/1000);
si.innerHTML='<div class="streaming-dot"></div> Generating'+(S.model?' with '+S.model.split('-').slice(0,2).join('-'):'')+' &middot; '+elapsed+'s';
}
});
}

window.stopChat=function(){
if(chatAbort)chatAbort.abort();
var h={};if(S.token)h['Authorization']='Bearer '+S.token;
fetch('/api/chat/stop',{method:'POST',headers:h}).catch(function(){});
S.streaming=false;chatAbort=null;render();
};

/* ── Toolbar Actions ──────────────────────────────────── */
/* Named avaOpenToolbarPopover so inline handlers do not resolve to HTMLElement.togglePopover (Popover API). */
function repositionPopover(pop,rect){
if(!pop||!rect)return;
var popW=pop.offsetWidth,popH=pop.offsetHeight;
var x=rect.left,y=rect.top-popH-6;
if(x+popW>window.innerWidth)x=window.innerWidth-popW-8;
if(x<8)x=8;
if(y<8){y=rect.bottom+6;}
var maxY=window.innerHeight-popH-8;
if(y>maxY)y=maxY;
if(y<8)y=8;
pop.style.left=x+'px';pop.style.top=y+'px';
}
window.avaOpenToolbarPopover=function(e,id){
e.stopPropagation();
var pop=document.getElementById(id);if(!pop)return;
var isOpen=pop.classList.contains('show');
closePopovers();
if(!isOpen){
var btn=e.currentTarget||e.target.closest('.toolbar-btn')||e.target;
var rect=btn.getBoundingClientRect();
pop.style.left='';pop.style.right='';pop.style.top='';pop.style.bottom='';
pop.classList.add('show');
repositionPopover(pop,rect);
openPopover=id;
pop._btnRect=rect;
if(id==='histPop')loadSessions();
}
};

window.toggleContext=function(){
S.useHistory=!S.useHistory;
localStorage.setItem('ava_use_history',S.useHistory?'true':'false');
render();
};

window.newChat=function(){
if(S.session)try{localStorage.removeItem('ava_msgs_'+S.session);}catch(e){}
S.messages=[];S.session='';localStorage.removeItem('ava_last_session');closePopovers();render();
};

function loadSessions(){
var list=document.getElementById('histList');
api('/api/sessions').then(function(data){
if(!list)list=document.getElementById('histList');if(!list)return;
if(data===null){list.innerHTML='<div style="padding:.5rem;color:var(--text-muted);font-size:.8125rem">Unable to load sessions</div>';return;}
if(!Array.isArray(data)){list.innerHTML='<div style="padding:.5rem;color:#f87171;font-size:.8125rem">Unexpected sessions response</div>';return;}
if(!data.length){list.innerHTML='<div style="padding:.5rem;color:var(--text-muted);font-size:.8125rem">No sessions yet — send a message to create one</div>';return;}
data.sort(function(a,b){return (b.modified||0)-(a.modified||0);});
var html='';
for(var i=0;i<data.length;i++){
var s=data[i];
var isActive=s.name===S.session;
var enc=encodeURIComponent(s.name);
html+='<div class="hist-item'+(isActive?' active':'')+'" onclick="pickHistSession(\''+enc+'\')">'+
'<div class="hist-item-info"><div class="hist-item-name">'+esc(s.name)+'</div>'+
'<div class="hist-item-meta">'+(s.message_count||0)+' messages &middot; '+fmtDate(s.modified)+'</div></div>'+
'<div class="hist-item-actions">'+
'<button class="hist-item-rename" onclick="event.stopPropagation();renameHistSession(\''+enc+'\')" title="Rename">'+ic('edit',12)+'</button>'+
'<button class="hist-item-del" onclick="event.stopPropagation();deleteHistSession(\''+enc+'\')" title="Delete">'+ic('trash',12)+'</button>'+
'</div></div>';
}
list.innerHTML=html;
var pop=document.getElementById('histPop');
if(pop&&pop._btnRect)setTimeout(function(){repositionPopover(pop,pop._btnRect);},0);
}).catch(function(){
if(!list)list=document.getElementById('histList');
if(list)list.innerHTML='<div style="padding:.5rem;color:#f87171;font-size:.8125rem">Could not load sessions</div>';
});
}

window.loadSessions=loadSessions;
window.pickHistSession=function(enc){loadSession(decodeURIComponent(enc));};
window.deleteHistSession=function(enc){deleteSession(decodeURIComponent(enc));};
window.renameHistSession=function(enc){
var name=decodeURIComponent(enc);
var list=document.getElementById('histList');if(!list)return;
var items=list.querySelectorAll('.hist-item');
for(var i=0;i<items.length;i++){
var nameEl=items[i].querySelector('.hist-item-name');
if(nameEl&&nameEl.textContent===name){
var info=items[i].querySelector('.hist-item-info');
if(!info)break;
var oldHtml=info.innerHTML;
info.innerHTML='<div class="hist-rename-row"><input type="text" value="'+esc(name)+'" id="renameInput" onkeydown="if(event.key===\'Enter\')doRenameSession(\''+enc+'\');if(event.key===\'Escape\')loadSessions();" onclick="event.stopPropagation()">'+
'<button onclick="event.stopPropagation();doRenameSession(\''+enc+'\')">'+ic('check',14)+'</button>'+
'<button onclick="event.stopPropagation();loadSessions()">'+ic('xic',14)+'</button></div>';
var inp=document.getElementById('renameInput');if(inp){inp.focus();inp.select();}
break;
}
}
};
window.doRenameSession=function(enc){
var oldName=decodeURIComponent(enc);
var inp=document.getElementById('renameInput');if(!inp)return;
var newName=inp.value.trim();
if(!newName||newName===oldName){loadSessions();return;}
api('/api/sessions/rename',{method:'POST',body:{from:oldName,to:newName}}).then(function(data){
if(data&&data.renamed){
if(S.session===oldName){S.session=newName;localStorage.setItem('ava_last_session',newName);}
toast('Session renamed','info');
}
loadSessions();
}).catch(function(){loadSessions();});
};

window.loadSession=function(name){
S.session=name;
localStorage.setItem('ava_last_session', name);
var local=loadMessagesFromLocal(name);
if(local&&local.length){
S.messages=local;
autoScroll=true;closePopovers();render();
setTimeout(function(){scrollToBottom(true);},50);
return;
}
api('/api/sessions/'+encodeURIComponent(name)).then(function(data){
if(!data)return;
S.messages=restoreMessagesFromApi(data);
saveMessagesToLocal();
autoScroll=true;
closePopovers();render();
setTimeout(function(){scrollToBottom(true);},50);
}).catch(function(){});
};

window.deleteSession=function(name){
avaConfirm('Delete session "'+name+'"?',function(){
api('/api/sessions/'+encodeURIComponent(name),{method:'DELETE'}).then(function(){
try{localStorage.removeItem('ava_msgs_'+name);}catch(e){}
if(S.session===name){S.session='';S.messages=[];}
loadSessions();
}).catch(function(){});
},{title:'Delete Session',okText:'Delete'});
};

/* ── Notes Actions ────────────────────────────────────── */
window.showNoteInput=function(){
var row=document.getElementById('noteInputRow');if(row)row.style.display='flex';
var inp=document.getElementById('noteInput');if(inp)inp.focus();
};
window.saveNote=function(){
var inp=document.getElementById('noteInput');if(!inp||!inp.value.trim())return;
S.notes.push({text:inp.value.trim(),enabled:true});
localStorage.setItem('ava_notes',JSON.stringify(S.notes));
var showInputRow=document.getElementById('noteInputRow')&&document.getElementById('noteInputRow').style.display!=='none';
render();
restoreToolbarPopover('notesPop');
if(showInputRow){requestAnimationFrame(function(){
var row=document.getElementById('noteInputRow'),ni=document.getElementById('noteInput');
if(row)row.style.display='flex';if(ni){ni.value='';ni.focus();}
});}
};
window.toggleNote=function(i){
if(!S.notes[i])return;
S.notes[i].enabled=!S.notes[i].enabled;
localStorage.setItem('ava_notes',JSON.stringify(S.notes));
render();
restoreToolbarPopover('notesPop');
};
window.deleteNote=function(i){
if(!S.notes[i])return;
S.notes.splice(i,1);
localStorage.setItem('ava_notes',JSON.stringify(S.notes));
render();
restoreToolbarPopover('notesPop');
};

/* ── Nodes Panel ──────────────────────────────────────── */
S.nodesList=[];S.nodesLoaded=false;S.nodesPanelOpen=false;S.mentionActive=false;S.mentionIdx=0;S.activeNodeId=null;

function renderNodesPanel(){
var h='<div class="nodes-panel'+(S.nodesPanelOpen?' open':'')+'" id="nodesPanel">';
h+='<div class="nodes-panel-head"><span>'+ic('server',14)+' Nodes</span>';
h+='<button class="node-action-btn" onclick="refreshNodes()" title="Refresh">'+ic('refresh',14)+'</button></div>';
h+='<div class="nodes-panel-list" id="nodesPanelList">';
h+='<div class="node-item self" onclick="go(\'/servers\')"><div class="node-dot online"></div>';
h+='<div class="node-info"><div class="node-label">'+ic('dot',8)+' Local</div>';
h+='<div class="node-meta">'+esc(S.status?S.status.workspace:'this node')+'</div></div>';
h+='<div class="node-badge">self</div></div>';
if(!S.nodesLoaded){
h+='<div style="padding:.75rem;text-align:center"><div class="spinner" style="width:16px;height:16px;border-width:2px;margin:0 auto"></div></div>';
}else{
for(var i=0;i<S.nodesList.length;i++){
var n=S.nodesList[i];
var dotCls='offline';
if(n.status==='connected')dotCls=n.latency_ms>500?'warning':'online';
else if(n.status==='testing')dotCls='testing';
var active=S.activeNodeId===n.id?' active':'';
h+='<div class="node-item'+active+'" data-node-id="'+esc(n.id)+'" onclick="onNodeClick(\''+esc(n.id)+'\')">';
h+='<div class="node-dot '+dotCls+'"></div>';
h+='<div class="node-info"><div class="node-label">'+esc(n.label||n.ip)+'</div>';
h+='<div class="node-meta">'+esc(n.ip)+':'+esc(String(n.port||8080));
if(n.latency_ms>0)h+=' &middot; '+n.latency_ms+'ms';
if(n.version)h+=' &middot; v'+esc(n.version);
h+='</div></div>';
if(n.status==='connected')h+='<button class="node-action-btn" onclick="event.stopPropagation();openRemoteUI(\''+esc(n.id)+'\',\''+esc(n.label||n.ip)+'\')" title="Open remote UI">'+ic('globe',12)+'</button>';
h+='</div>';
}
if(!S.nodesList.length){
h+='<div style="padding:1rem;text-align:center;color:var(--text-muted);font-size:.75rem">No nodes yet.<br><a href="#/servers" style="color:var(--accent-light)">Add one</a></div>';
}
}
h+='</div></div>';
return h;
}

function loadChatNodes(){
api('/api/nodes',{silent:true}).then(function(d){
S.nodesList=(d&&d.nodes)?d.nodes:[];
S.nodesLoaded=true;
var list=document.getElementById('nodesPanelList');
if(list){
var panel=document.getElementById('nodesPanel');
if(panel)panel.outerHTML=renderNodesPanel();
}
}).catch(function(){S.nodesLoaded=true;});
}

window.refreshNodes=function(){
S.nodesLoaded=false;
var list=document.getElementById('nodesPanelList');
if(list)list.innerHTML='<div style="padding:.75rem;text-align:center"><div class="spinner" style="width:16px;height:16px;border-width:2px;margin:0 auto"></div></div>';
loadChatNodes();
};

window.onNodeClick=function(id){
if(S.activeNodeId===id){S.activeNodeId=null;}
else{S.activeNodeId=id;}
var items=document.querySelectorAll('.node-item[data-node-id]');
items.forEach(function(el){el.classList.toggle('active',el.getAttribute('data-node-id')===S.activeNodeId);});
};

window.toggleNodesPanel=function(){
S.nodesPanelOpen=!S.nodesPanelOpen;
var panel=document.getElementById('nodesPanel');
if(panel)panel.classList.toggle('open',S.nodesPanelOpen);
};

/* ── @mention picker ─────────────────────────────────── */
function setupMentionPicker(){
var inp=document.getElementById('chatInput');if(!inp)return;
var popup=document.getElementById('mentionPopup');if(!popup)return;

inp.addEventListener('input',function(){
var val=inp.value;
var cursor=inp.selectionStart;
var before=val.substring(0,cursor);
var atIdx=before.lastIndexOf('@');
if(atIdx<0||atIdx>0&&before[atIdx-1]!==' '&&before[atIdx-1]!=='\n'){
popup.classList.remove('show');S.mentionActive=false;return;
}
var query=before.substring(atIdx+1).toLowerCase();
var connected=S.nodesList.filter(function(n){return n.status==='connected';});
var matches=connected.filter(function(n){
return (n.label||n.ip).toLowerCase().indexOf(query)>=0;
});
if(!matches.length){popup.classList.remove('show');S.mentionActive=false;return;}
S.mentionActive=true;S.mentionIdx=0;S.mentionMatches=matches;S.mentionAtIdx=atIdx;
renderMentionPopup();
});
inp.addEventListener('keydown',function(e){
if(!S.mentionActive)return;
if(e.key==='ArrowDown'){e.preventDefault();S.mentionIdx=Math.min(S.mentionIdx+1,S.mentionMatches.length-1);renderMentionPopup();}
else if(e.key==='ArrowUp'){e.preventDefault();S.mentionIdx=Math.max(S.mentionIdx-1,0);renderMentionPopup();}
else if(e.key==='Enter'||e.key==='Tab'){
if(S.mentionActive&&S.mentionMatches.length){e.preventDefault();insertMention(S.mentionMatches[S.mentionIdx]);}
}
else if(e.key==='Escape'){popup.classList.remove('show');S.mentionActive=false;}
});
}

function renderMentionPopup(){
var popup=document.getElementById('mentionPopup');if(!popup)return;
var h='';
for(var i=0;i<S.mentionMatches.length;i++){
var n=S.mentionMatches[i];
var sel=i===S.mentionIdx?' selected':'';
var dotCls=n.latency_ms>500?'warning':'online';
h+='<div class="mention-item'+sel+'" onmousedown="insertMention(S.mentionMatches['+i+'])">';
h+='<div class="node-dot '+dotCls+'"></div>';
h+='<span>'+esc(n.label||n.ip)+'</span>';
if(n.latency_ms>0)h+='<span style="font-size:.6875rem;color:var(--text-muted);margin-left:auto">'+n.latency_ms+'ms</span>';
h+='</div>';
}
popup.innerHTML=h;
popup.classList.add('show');
}

window.insertMention=function(node){
var inp=document.getElementById('chatInput');if(!inp)return;
var popup=document.getElementById('mentionPopup');if(popup)popup.classList.remove('show');
S.mentionActive=false;
var val=inp.value;
var cursor=inp.selectionStart;
var before=val.substring(0,S.mentionAtIdx);
var after=val.substring(cursor);
var tag='@'+node.label+' ';
inp.value=before+tag+after;
inp.selectionStart=inp.selectionEnd=before.length+tag.length;
inp.focus();
};

/* ── Remote UI overlay ───────────────────────────────── */
window.openRemoteUI=function(nodeId,label){
var overlay=document.getElementById('remoteUiOverlay');if(!overlay)return;
overlay.innerHTML='<div class="remote-ui-bar">'+
'<button class="node-action-btn" onclick="closeRemoteUI()" style="padding:6px" title="Back">'+ic('chevLeft',18)+'</button>'+
'<div class="node-dot online"></div>'+
'<div class="node-label">'+esc(label)+'</div>'+
'<div class="node-badge">remote</div>'+
'<div style="flex:1"></div>'+
'<button class="node-action-btn" onclick="window.open(\'/api/nodes/'+esc(nodeId)+'/proxy/\',\'_blank\')" title="Open in new tab">'+ic('globe',14)+'</button>'+
'<button class="node-action-btn" onclick="closeRemoteUI()" title="Close">'+ic('xic',16)+'</button>'+
'</div>'+
'<iframe class="remote-ui-frame" src="/api/nodes/'+esc(nodeId)+'/proxy/"></iframe>';
overlay.classList.add('show');
};

window.closeRemoteUI=function(){
var overlay=document.getElementById('remoteUiOverlay');if(!overlay)return;
overlay.classList.remove('show');
setTimeout(function(){overlay.innerHTML='';},300);
};

/* ── Mobile swipe for nodes panel ────────────────────── */
(function(){
var touchStartX=0,touchStartY=0,swiping=false;
document.addEventListener('touchstart',function(e){
if(S.route!=='/chat')return;
var t=e.touches[0];touchStartX=t.clientX;touchStartY=t.clientY;swiping=false;
},{passive:true});
document.addEventListener('touchmove',function(e){
if(S.route!=='/chat')return;
var t=e.touches[0];
var dx=t.clientX-touchStartX;
var dy=t.clientY-touchStartY;
if(!swiping&&Math.abs(dx)>Math.abs(dy)&&Math.abs(dx)>20)swiping=true;
if(!swiping)return;
var panel=document.getElementById('nodesPanel');if(!panel)return;
if(!S.nodesPanelOpen&&dx<-40&&touchStartX>window.innerWidth*0.6){
S.nodesPanelOpen=true;panel.classList.add('open');
}else if(S.nodesPanelOpen&&dx>40){
S.nodesPanelOpen=false;panel.classList.remove('open');
}
},{passive:true});
})();

/* ── Message Actions (hover toolbar) ─────────────────── */
var hoveredMsgEl=null,lastActionClientY=0;
function setupMsgActions(){
var chat=document.getElementById('chatMsgs');
var toolbar=document.getElementById('msgActions');
if(!chat||!toolbar)return;
var isMobile=('ontouchstart' in window)||window.innerWidth<=600;
function posToolbar(msgEl,clientY){
var bubble=msgEl.querySelector('.msg-bubble');if(!bubble)return;
var rect=bubble.getBoundingClientRect();
var isUser=msgEl.classList.contains('msg-user');
var th=toolbar.offsetHeight||80;
var top=clientY!=null?clientY-Math.round(th/2):rect.top;
top=Math.max(rect.top,Math.min(top,rect.bottom-th));
var left=isUser?rect.left-38:rect.right+6;
if(left+40>window.innerWidth)left=rect.left-38;
if(left<4)left=4;
if(top<4)top=4;
if(top+th>window.innerHeight)top=window.innerHeight-th-4;
toolbar.style.top=top+'px';toolbar.style.left=left+'px';
}
if(!isMobile){
chat.addEventListener('mouseover',function(e){
var msgEl=e.target.closest('.msg');
if(!msgEl||msgEl===hoveredMsgEl)return;
hoveredMsgEl=msgEl;lastActionClientY=e.clientY;
posToolbar(msgEl,e.clientY);
toolbar.classList.remove('mobile-active');
toolbar.classList.add('visible');
});
chat.addEventListener('mousemove',function(e){
if(!hoveredMsgEl)return;
var msgEl=e.target.closest('.msg');
if(msgEl!==hoveredMsgEl)return;
lastActionClientY=e.clientY;
posToolbar(hoveredMsgEl,e.clientY);
});
chat.addEventListener('mouseleave',function(){
setTimeout(function(){if(!toolbar.matches(':hover'))hideMsgActions();},120);
});
toolbar.addEventListener('mouseleave',hideMsgActions);
chat.addEventListener('scroll',function(){
if(hoveredMsgEl)posToolbar(hoveredMsgEl,lastActionClientY);
});
}else{
var touchTimer=null,touchMsg=null,touchMoved=false,longFired=false;
chat.addEventListener('touchstart',function(e){
var msgEl=e.target.closest('.msg');if(!msgEl)return;
touchMsg=msgEl;touchMoved=false;longFired=false;
touchTimer=setTimeout(function(){
touchTimer=null;longFired=true;
hoveredMsgEl=touchMsg;
toolbar.classList.add('mobile-active');
toolbar.classList.add('visible');
},400);
},{passive:true});
chat.addEventListener('touchmove',function(){touchMoved=true;if(touchTimer){clearTimeout(touchTimer);touchTimer=null;}},{passive:true});
chat.addEventListener('touchend',function(){
if(touchTimer){clearTimeout(touchTimer);touchTimer=null;}
if(!touchMoved&&!longFired&&touchMsg){
if(hoveredMsgEl===touchMsg&&toolbar.classList.contains('visible')){hideMsgActions();}
else{hoveredMsgEl=touchMsg;toolbar.classList.add('mobile-active');toolbar.classList.add('visible');}
}
touchMsg=null;
});
document.addEventListener('touchstart',function(e){if(toolbar.classList.contains('visible')&&!e.target.closest('.msg-actions')&&!e.target.closest('.msg')){hideMsgActions();}},{passive:true});
}
toolbar.addEventListener('click',function(e){
var btn=e.target.closest('.msg-action-btn');
if(!btn||!hoveredMsgEl)return;
var action=btn.getAttribute('data-action');
var idx=parseInt(hoveredMsgEl.getAttribute('data-idx'),10);
if(action==='copy'){
var bubble=hoveredMsgEl.querySelector('.msg-bubble');
navigator.clipboard.writeText(bubble?bubble.innerText:'').then(function(){toast('Copied','success');}).catch(function(){});
}else if(action==='exclude'){
if(!isNaN(idx)&&S.messages[idx]){S.messages[idx].excluded=!S.messages[idx].excluded;hoveredMsgEl.classList.toggle('excluded');}
}else if(action==='delete'){
if(!isNaN(idx)){S.messages.splice(idx,1);hideMsgActions();render();
if(S.session){api('/api/sessions/delete-message',{method:'POST',body:{session:S.session,visible_index:idx},silent:true}).catch(function(){});}
}
}
});
}
function hideMsgActions(){
var toolbar=document.getElementById('msgActions');
if(toolbar){toolbar.classList.remove('visible');toolbar.classList.remove('mobile-active');}
hoveredMsgEl=null;
}

/* ── WebIDE / Files Page ──────────────────────────────── */
var openTabs=[],activeTab='',dirCache={},loadingDirs={};
function renderFiles(){
return '<div class="files-wrap" style="height:100%;position:relative">'+
  '<div id="fileTreeOverlay" class="file-tree-overlay" onclick="closeFileTree()"></div>'+
'<div class="file-tree" id="fileTree" style="width:240px;min-width:180px">'+
'<div style="padding:.5rem .75rem;border-bottom:1px solid var(--border);font-size:.75rem;font-weight:600;color:var(--accent-pale);display:flex;align-items:center;justify-content:space-between">'+
'<span>'+ic('folder',14)+' Explorer</span>'+
'<button style="background:none;border:none;color:var(--text-muted);cursor:pointer;padding:2px;display:flex;align-items:center;gap:4px" onclick="browseWorkspace()" title="Browse folder">'+ic('folder',12)+' Browse</button>'+
'</div>'+
'<div id="treeRoot" style="padding:.25rem;overflow-y:auto;flex:1"></div>'+
'</div>'+
'<div class="file-view" style="display:flex;flex-direction:column;flex:1;overflow:hidden;padding:0">'+
renderFileTabs()+
'<div id="editorArea" style="flex:1;overflow:auto;position:relative">'+
(activeTab?renderEditor():'<div class="empty" style="height:100%">'+ic('file',48)+'<p>Select a file to edit</p><p style="font-size:.8125rem">Browse the workspace in the file tree on the left</p></div>')+
'</div></div></div>';
}
function renderFileTabs(){
if(!openTabs.length)return '';
var tabs='';
for(var i=0;i<openTabs.length;i++){
var t=openTabs[i];
var isActive=t.path===activeTab;
var name=t.path.split('/').pop();
var modified=t.modified?' *':'';
tabs+='<div class="'+(isActive?'ide-tab active':'ide-tab')+'" onclick="switchTab(\''+esc(t.path).replace(/'/g,"\\'")+'\')">'+
'<span>'+esc(name)+modified+'</span>'+
'<button class="ide-tab-close" onclick="event.stopPropagation();closeTab(\''+esc(t.path).replace(/'/g,"\\'")+'\')">'+ic('xic',10)+'</button>'+
'</div>';
}
return '<div class="ide-tabs" style="display:flex;gap:0;border-bottom:1px solid var(--border);background:var(--bg-surface);overflow-x:auto;flex-shrink:0">'+tabs+'</div>';
}
function renderEditor(){
var tab=openTabs.find(function(t){return t.path===activeTab;});
if(!tab)return '<div class="empty">'+ic('file',48)+'<p>Select a file</p></div>';
if(tab.loading)return '<div style="display:flex;justify-content:center;align-items:center;height:100%"><div class="spinner"></div></div>';
var lines=(tab.content||'').split('\n');
var lineNums='';
for(var i=0;i<lines.length;i++){
lineNums+='<div class="ide-ln">'+(i+1)+'</div>';
}
return '<div class="ide-header" style="display:flex;align-items:center;justify-content:space-between;padding:.375rem .75rem;border-bottom:1px solid var(--border);background:var(--bg-surface);flex-shrink:0">'+
'<span class="file-path" style="font-size:.8125rem">'+esc(tab.path)+'</span>'+
'<div class="file-actions">'+
(tab.modified?'<button onclick="ideRevert()" title="Revert to original">'+ic('xic',14)+' Revert</button>':'')+
'<button onclick="ideUndo()" '+(tab.history&&tab.historyIndex>0?'':'disabled style="opacity:0.5"')+' title="Undo">Undo</button>'+
'<button onclick="ideRedo()" '+(tab.history&&tab.historyIndex>=0&&tab.historyIndex<tab.history.length-1?'':'disabled style="opacity:0.5"')+' title="Redo">Redo</button>'+
'<button onclick="ideSave()">'+ic('save',14)+' Save</button>'+
'</div></div>'+
'<div style="display:flex;flex:1;overflow:auto" id="ideScrollWrap">'+
'<div class="ide-line-numbers" id="ideLineNums" style="padding:.5rem .25rem;text-align:right;color:var(--text-muted);font-family:var(--mono);font-size:.8125rem;line-height:1.65;user-select:none;background:var(--bg-surface);border-right:1px solid var(--border);min-width:40px;flex-shrink:0">'+lineNums+'</div>'+
'<textarea class="ide-editor" id="ideTextarea" spellcheck="false" oninput="onIdeInput()" onscroll="syncIdeScroll()">'+esc(tab.content||'')+'</textarea>'+
'</div>';
}

function setupIdeAfterRender(){
renderFileTree();
if(window._pendingAppDir){
var targetDir=window._pendingAppDir;
var parts=targetDir.split('/');
var expanding='';
var chain=[];
for(var i=0;i<parts.length;i++){
expanding=expanding?(expanding+'/'+parts[i]):parts[i];
chain.push(expanding);
}
delete window._pendingAppDir;
function expandNext(idx){
if(idx>=chain.length){
if(typeof openFileTree==='function')openFileTree();
return;
}
expandedDirs[chain[idx]]=true;
if(!dirCache[chain[idx]]){
loadDir(chain[idx],function(){renderFileTree();expandNext(idx+1);});
}else{renderFileTree();expandNext(idx+1);}
}
if(!dirCache['']){loadDir('',function(){renderFileTree();expandNext(0);});}
else expandNext(0);
}
var ta=document.getElementById('ideTextarea');
if(ta){
ta.addEventListener('keydown',function(e){
if(e.key==='Tab'){e.preventDefault();var s=ta.selectionStart,en=ta.selectionEnd;ta.value=ta.value.substring(0,s)+'  '+ta.value.substring(en);ta.selectionStart=ta.selectionEnd=s+2;onIdeInput();}
if((e.ctrlKey||e.metaKey)&&e.key==='s'){e.preventDefault();ideSave();}
});
}
}

function renderFileTree(){
var root=document.getElementById('treeRoot');
if(!root)return;
if(!dirCache['']){
root.innerHTML='<div style="display:flex;justify-content:center;padding:1rem"><div class="spinner"></div></div>';
loadDir('',function(){renderFileTree();});
return;
}
root.innerHTML=renderDirEntries('');
}

function loadDir(dir,cb){
if(loadingDirs[dir])return;
loadingDirs[dir]=true;
api('/api/files/list?dir='+encodeURIComponent(dir)).then(function(d){
dirCache[dir]=(d&&d.entries)?d.entries:[];
delete loadingDirs[dir];
if(cb)cb();
}).catch(function(){dirCache[dir]=[];delete loadingDirs[dir];if(cb)cb();});
}

function renderDirEntries(dir){
var entries=dirCache[dir];
if(!entries)return '';
var html='';
for(var i=0;i<entries.length;i++){
var e=entries[i];
var p=e.path;
if(e.type==='dir'){
var isOpen=expandedDirs[p];
html+='<div><div class="tree-node" onclick="ideToggleDir(\''+esc(p).replace(/'/g,"\\'")+'\')">'+ic(isOpen?'chevD':'chevR',14)+' '+ic('folder',14)+' '+esc(e.name)+'</div>';
if(isOpen){
html+='<div class="tree-children">';
if(dirCache[p])html+=renderDirEntries(p);
else html+='<div style="padding:.25rem .5rem"><div class="spinner" style="width:12px;height:12px;border-width:1px"></div></div>';
html+='</div>';
}
html+='</div>';
}else{
var sel=activeTab===p?'selected':'';
html+='<div class="tree-node '+sel+'" onclick="ideOpenFile(\''+esc(p).replace(/'/g,"\\'")+'\')">'+ic('file',14)+' '+esc(e.name)+'</div>';
}
}
return html;
}

window.ideToggleDir=function(p){
expandedDirs[p]=!expandedDirs[p];
if(expandedDirs[p]&&!dirCache[p]){
loadDir(p,function(){renderFileTree();});
}
renderFileTree();
};

window.ideOpenFile=function(path){
var existing=openTabs.find(function(t){return t.path===path;});
if(existing){activeTab=path;render();setupIdeAfterRender();return;}
openTabs.push({path:path,content:'',original:'',modified:false,loading:true});
activeTab=path;
render();setupIdeAfterRender();
api('/api/files/read?path='+encodeURIComponent(path)).then(function(d){
var tab=openTabs.find(function(t){return t.path===path;});
if(tab){tab.content=d?d.content||'':'';tab.original=tab.content;tab.loading=false;tab.history=[tab.original];tab.historyIndex=0;tab.history=[tab.original];tab.historyIndex=0;}
render();setupIdeAfterRender();
}).catch(function(){
var tab=openTabs.find(function(t){return t.path===path;});
if(tab){tab.content='Error loading file';tab.original='';tab.loading=false;tab.history=[tab.original];tab.historyIndex=0;tab.history=[tab.original];tab.historyIndex=0;}
render();setupIdeAfterRender();
});
};

window.switchTab=function(path){activeTab=path;render();setupIdeAfterRender();};

window.closeTab=function(path){
var idx=openTabs.findIndex(function(t){return t.path===path;});
if(idx===-1)return;
if(openTabs[idx].modified){avaConfirm('Discard unsaved changes?',function(){openTabs.splice(idx,1);if(activeTab===path){activeTab=openTabs.length?openTabs[Math.min(idx,openTabs.length-1)].path:'';}render();setupIdeAfterRender();},{title:'Unsaved Changes',okText:'Discard',danger:true});return;}
openTabs.splice(idx,1);
if(activeTab===path){activeTab=openTabs.length?openTabs[Math.min(idx,openTabs.length-1)].path:'';}
render();setupIdeAfterRender();
};

window.onIdeInput=function(){
var ta=document.getElementById('ideTextarea');if(!ta)return;
var tab=openTabs.find(function(t){return t.path===activeTab;});
if(tab){
if(!tab.history) { tab.history = [tab.original]; tab.historyIndex = 0; }
tab.content=ta.value;tab.modified=tab.content!==tab.original;
if(window.ideHistoryTimer) clearTimeout(window.ideHistoryTimer);
window.ideHistoryTimer = setTimeout(function(){ pushIdeHistory(tab); }, 500);
}
var tabContainer=document.querySelector('.ide-tabs');
if(tabContainer)tabContainer.outerHTML=renderFileTabs();
updateLineNumbers();
};
window.pushIdeHistory = function(tab) {
var limit = typeof S.editorHistoryLimit === 'number' ? S.editorHistoryLimit : 25;
if(!tab.history) { tab.history = [tab.original]; tab.historyIndex = 0; }
var current = tab.historyIndex >= 0 && tab.historyIndex < tab.history.length ? tab.history[tab.historyIndex] : null;
if(current === tab.content) return;
if(tab.historyIndex < tab.history.length - 1) { tab.history = tab.history.slice(0, tab.historyIndex + 1); }
tab.history.push(tab.content);
if(tab.history.length > limit) tab.history.shift();
tab.historyIndex = tab.history.length - 1;
render();setupIdeAfterRender();
};


window.syncIdeScroll=function(){
var ta=document.getElementById('ideTextarea');
var ln=document.getElementById('ideLineNums');
if(ta&&ln)ln.scrollTop=ta.scrollTop;
};

function updateLineNumbers(){
var ta=document.getElementById('ideTextarea');
var ln=document.getElementById('ideLineNums');
if(!ta||!ln)return;
var count=(ta.value.match(/\n/g)||[]).length+1;
var html='';for(var i=1;i<=count;i++)html+='<div class="ide-ln">'+i+'</div>';
ln.innerHTML=html;
}

window.ideSave=function(){
var tab=openTabs.find(function(t){return t.path===activeTab;});
if(!tab)return;
api('/api/files/write',{method:'POST',body:{path:tab.path,content:tab.content}}).then(function(){
tab.original=tab.content;tab.modified=false;toast('File saved','success');render();setupIdeAfterRender();
}).catch(function(){});
};

window.ideRevert=function(){
var tab=openTabs.find(function(t){return t.path===activeTab;});
if(!tab)return;
tab.content=tab.original;tab.modified=false;
if(!tab.history)tab.history=[tab.original];
tab.history.push(tab.content);tab.historyIndex=tab.history.length-1;
render();setupIdeAfterRender();
};
window.ideUndo=function(){
var tab=openTabs.find(function(t){return t.path===activeTab;});
if(!tab||!tab.history||tab.historyIndex<=0)return;
tab.historyIndex--;tab.content=tab.history[tab.historyIndex];tab.modified=tab.content!==tab.original;
render();setupIdeAfterRender();
};
window.ideRedo=function(){
var tab=openTabs.find(function(t){return t.path===activeTab;});
if(!tab||!tab.history||tab.historyIndex>=tab.history.length-1)return;
tab.historyIndex++;tab.content=tab.history[tab.historyIndex];tab.modified=tab.content!==tab.original;
render();setupIdeAfterRender();
};

/* ── Usage Page ───────────────────────────────────────── */
if(!S.usageRange)S.usageRange='month';
var usageRangeOptions=[
{id:'hour',label:'Last Hour'},{id:'6hours',label:'6 Hours'},
{id:'12hours',label:'12 Hours'},{id:'day',label:'24 Hours'},
{id:'3days',label:'3 Days'},{id:'week',label:'Week'},
{id:'2weeks',label:'2 Weeks'},{id:'month',label:'Month'},
{id:'3months',label:'3 Months'},{id:'6months',label:'6 Months'},
{id:'year',label:'Year'},{id:'all',label:'All Time'}
];
function usageRangeParam(r){
var map={hour:'hours=1','6hours':'hours=6','12hours':'hours=12',day:'hours=24',
'3days':'days=3',week:'days=7','2weeks':'days=14',month:'days=30',
'3months':'days=90','6months':'days=180',year:'days=365',all:'days=0'};
return map[r]||'days=30';
}
window.setUsageRange=function(r){S.usageRange=r;S.usageData=null;render();};
function renderUsage(){
if(!S.usageData){loadUsageData();
return '<div class="usage-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';}

var d=S.usageData;
var totalInput=0,totalOutput=0;
for(var i=0;i<d.history.length;i++){totalInput+=(d.history[i].prompt_tokens||0);totalOutput+=(d.history[i].completion_tokens||0);}

var rangeLabel=usageRangeOptions.find(function(o){return o.id===S.usageRange;});
rangeLabel=rangeLabel?rangeLabel.label:'Month';
var rangeSelector='<div style="display:flex;gap:.375rem;align-items:center;flex-wrap:wrap">';
for(var ri=0;ri<usageRangeOptions.length;ri++){
var ro=usageRangeOptions[ri];
rangeSelector+='<button class="chart-tab'+(S.usageRange===ro.id?' active':'')+'" onclick="setUsageRange(\''+ro.id+'\')">'+ro.label+'</button>';
}
rangeSelector+='</div>';

return '<div class="usage-wrap">'+
'<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem;flex-wrap:wrap;gap:.5rem">'+
'<h2 style="font-size:1.25rem;color:var(--accent-pale)">'+ic('activity',20)+' Token Usage</h2>'+
rangeSelector+
'</div>'+
'<div class="usage-cards" style="grid-template-columns:repeat(auto-fit,minmax(160px,1fr))">'+
'<div class="usage-card"><div class="usage-card-value">'+fmtNum(totalInput)+'</div><div class="usage-card-label">Input Tokens</div></div>'+
'<div class="usage-card"><div class="usage-card-value">'+fmtNum(totalOutput)+'</div><div class="usage-card-label">Output Tokens</div></div>'+
'<div class="usage-card"><div class="usage-card-value">'+fmtNum(totalInput+totalOutput)+'</div><div class="usage-card-label">Total Tokens</div></div>'+
'<div class="usage-card"><div class="usage-card-value">'+d.history.length+'</div><div class="usage-card-label">Requests</div></div>'+
'</div>'+
'<div style="display:grid;grid-template-columns:1fr 1fr;gap:1rem;margin-bottom:1rem">'+
renderLineChart(d.history)+
renderBarChart(d.history)+
'</div>'+
renderToolLog()+
renderUsageTable(d.history)+
'</div>';
}

function renderToolLog(){
if(!S.toolLog.length)return '<div class="chart-container"><div class="chart-header"><span class="chart-title">Tool Usage</span><span style="font-size:.75rem;color:var(--text-muted)">'+S.toolLog.length+' calls</span></div><div style="padding:1.5rem;text-align:center;color:var(--text-muted);font-size:.875rem">No tool calls yet — send a message in agent mode to see tool usage here</div></div>';
var rows='';
var logReversed=S.toolLog.slice().reverse().slice(0,100);
for(var i=0;i<logReversed.length;i++){
var t=logReversed[i];
var args='';try{args=typeof t.args==='string'?t.args:JSON.stringify(t.args);}catch(x){args='';}
if(args.length>120)args=args.slice(0,120)+'...';
var res='';if(t.result!=null){try{res=typeof t.result==='string'?t.result:JSON.stringify(t.result);}catch(x){res='';}if(res.length>120)res=res.slice(0,120)+'...';}
var sc=t.done?(t.success?'tool-log-ok':'tool-log-fail'):'';
var st=t.done?(t.success?'OK':'Fail'):'...';
rows+='<div class="tool-log-item">'+
'<div><div class="tool-log-name">'+esc(t.name)+'</div><div class="tool-log-time">'+esc(t.time?t.time.replace('T',' ').slice(0,19):'')+(t.duration?' &middot; '+t.duration+'ms':'')+'</div></div>'+
'<div><div class="tool-log-args" title="'+esc(args)+'">'+esc(args)+'</div>'+(res?'<div class="tool-log-args" style="color:var(--text-muted);margin-top:2px" title="'+esc(res)+'">'+esc(res)+'</div>':'')+'</div>'+
'<div class="tool-log-status '+sc+'">'+st+'</div></div>';
}
return '<div class="chart-container"><div class="chart-header"><span class="chart-title">Tool Usage</span><span style="font-size:.75rem;color:var(--text-muted)">'+S.toolLog.length+' calls</span></div><div class="tool-log">'+rows+'</div></div>';
}

function loadUsageData(){
var param=usageRangeParam(S.usageRange||'month');
Promise.all([
api('/api/usage/history?'+param,{silent:true}).catch(function(){return [];}),
api('/api/tokens/usage',{silent:true}).catch(function(){return {};})
]).then(function(res){
S.usageData={
history:Array.isArray(res[0])?res[0]:[],
session:res[1]||{}
};
render();
});
}

function renderLineChart(history){
if(!history.length)return '<div class="chart-container"><div class="chart-header"><span class="chart-title">Usage Over Time</span></div><div style="color:var(--text-muted);text-align:center;padding:2rem;font-size:.875rem">No usage data yet</div></div>';

var byDay={};
for(var i=0;i<history.length;i++){
var r=history[i];
var dateKey=new Date((r.timestamp||0)*1000).toISOString().slice(0,10);
if(!byDay[dateKey])byDay[dateKey]={prompt:0,completion:0};
byDay[dateKey].prompt+=(r.prompt_tokens||0);
byDay[dateKey].completion+=(r.completion_tokens||0);
}
var days=Object.keys(byDay).sort();
if(days.length<2)days=[days[0]||'today',days[0]||'today'];

var maxVal=0;
for(var k=0;k<days.length;k++){
var v=byDay[days[k]];
if(v.prompt>maxVal)maxVal=v.prompt;
if(v.completion>maxVal)maxVal=v.completion;
}
if(maxVal===0)maxVal=1;

var w=700,h=200,pad=40,chartW=w-pad*2,chartH=h-pad*2;
var stepX=days.length>1?chartW/(days.length-1):chartW;

var promptPts=[],compPts=[];
for(var k=0;k<days.length;k++){
var x=pad+k*stepX;
var v=byDay[days[k]];
promptPts.push(x+','+(pad+chartH-chartH*(v.prompt/maxVal)));
compPts.push(x+','+(pad+chartH-chartH*(v.completion/maxVal)));
}

var gridLines='';
for(var g=0;g<=4;g++){
var gy=pad+chartH*g/4;
var gv=Math.round(maxVal*(1-g/4));
gridLines+='<line x1="'+pad+'" y1="'+gy+'" x2="'+(w-pad)+'" y2="'+gy+'" stroke="rgba(255,255,255,0.04)" stroke-width="1"/>';
gridLines+='<text x="'+(pad-8)+'" y="'+(gy+4)+'" fill="rgba(255,255,255,0.3)" font-size="10" text-anchor="end">'+fmtNum(gv)+'</text>';
}

var xLabels='';
var labelStep=Math.max(1,Math.floor(days.length/7));
for(var k=0;k<days.length;k+=labelStep){
var x=pad+k*stepX;
xLabels+='<text x="'+x+'" y="'+(h-5)+'" fill="rgba(255,255,255,0.3)" font-size="10" text-anchor="middle">'+days[k].slice(5)+'</text>';
}

var svg='<svg class="chart-svg" viewBox="0 0 '+w+' '+h+'" preserveAspectRatio="xMidYMid meet">'+
gridLines+xLabels+
'<polyline points="'+promptPts.join(' ')+'" fill="none" stroke="#7c3aed" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>'+
'<polyline points="'+compPts.join(' ')+'" fill="none" stroke="#a855f7" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" stroke-dasharray="4 3"/>'+
'</svg>';

return '<div class="chart-container">'+
'<div class="chart-header"><span class="chart-title">Usage Over Time</span></div>'+
svg+
'<div class="chart-legend">'+
'<div class="chart-legend-item"><div class="chart-legend-dot" style="background:#7c3aed"></div> Input</div>'+
'<div class="chart-legend-item"><div class="chart-legend-dot" style="background:#a855f7"></div> Output</div>'+
'</div></div>';
}

function renderBarChart(history){
if(!history.length)return '';

var byDay={};
for(var i=0;i<history.length;i++){
var r=history[i];
var dateKey=new Date((r.timestamp||0)*1000).toISOString().slice(0,10);
if(!byDay[dateKey])byDay[dateKey]={prompt:0,completion:0};
byDay[dateKey].prompt+=(r.prompt_tokens||0);
byDay[dateKey].completion+=(r.completion_tokens||0);
}
var days=Object.keys(byDay).sort().slice(-14);
if(!days.length)return '';

var maxVal=0;
for(var k=0;k<days.length;k++){
var total=byDay[days[k]].prompt+byDay[days[k]].completion;
if(total>maxVal)maxVal=total;
}
if(maxVal===0)maxVal=1;

var w=700,h=180,pad=40,chartW=w-pad*2,chartH=h-pad*2;
var barW=Math.min(24,chartW/days.length*0.6);
var gap=chartW/days.length;

var bars='',labels='';
for(var k=0;k<days.length;k++){
var v=byDay[days[k]];
var total=v.prompt+v.completion;
var barH=chartH*(total/maxVal);
var x=pad+k*gap+gap/2-barW/2;
var y=pad+chartH-barH;

var promptH=chartH*(v.prompt/maxVal);
var compH=barH-promptH;

bars+='<rect x="'+x+'" y="'+(y+compH)+'" width="'+barW+'" height="'+promptH+'" rx="2" fill="#7c3aed" opacity="0.8"/>';
bars+='<rect x="'+x+'" y="'+y+'" width="'+barW+'" height="'+compH+'" rx="2" fill="#a855f7" opacity="0.6"/>';
labels+='<text x="'+(x+barW/2)+'" y="'+(h-5)+'" fill="rgba(255,255,255,0.3)" font-size="9" text-anchor="middle">'+days[k].slice(8)+'</text>';
}

var gridLines='';
for(var g=0;g<=4;g++){
var gy=pad+chartH*g/4;
var gv=Math.round(maxVal*(1-g/4));
gridLines+='<line x1="'+pad+'" y1="'+gy+'" x2="'+(w-pad)+'" y2="'+gy+'" stroke="rgba(255,255,255,0.04)" stroke-width="1"/>';
gridLines+='<text x="'+(pad-8)+'" y="'+(gy+4)+'" fill="rgba(255,255,255,0.3)" font-size="10" text-anchor="end">'+fmtNum(gv)+'</text>';
}

return '<div class="chart-container">'+
'<div class="chart-header"><span class="chart-title">Daily Breakdown</span></div>'+
'<svg class="chart-svg" viewBox="0 0 '+w+' '+h+'" preserveAspectRatio="xMidYMid meet">'+
gridLines+bars+labels+
'</svg></div>';
}

function renderUsageTable(history){
if(!history.length)return '';
var recent=history.slice(-50).reverse();
var rows='';
for(var i=0;i<recent.length;i++){
var r=recent[i];
rows+='<tr><td>'+fmtDate(r.timestamp)+'</td><td>'+fmtTime(r.timestamp)+'</td><td title="'+esc(r.model||'')+'">'+(esc(r.model||'--').split('-').slice(0,3).join('-'))+'</td><td>'+(r.prompt_tokens||0).toLocaleString()+'</td><td>'+(r.completion_tokens||0).toLocaleString()+'</td></tr>';
}
return '<div class="chart-container"><div class="chart-header"><span class="chart-title">Recent Usage</span></div>'+
'<table class="usage-table"><thead><tr><th>Date</th><th>Time</th><th>Model</th><th>Input Tokens</th><th>Output Tokens</th></tr></thead><tbody>'+rows+'</tbody></table></div>';
}

/* ── Logs Page ─────────────────────────────────────── */
var logData=null,logAutoScroll=true,logFilter='all',logPollTimer=null,logShowTimestamp=true,logSearch='',logExpandedIdx=null;
function renderLogs(){
if(!logData){loadLogs();return '<div class="usage-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';}
var levelColors={debug:'rgba(255,255,255,0.3)',info:'#4ade80',warn:'#fbbf24',error:'#f87171'};
var catColors={chat:'#c4b5fd',node:'#38bdf8',settings:'#fbbf24',api:'#818cf8',relay:'#f472b6',system:'#94a3b8',error:'#f87171',deploy:'#38bdf8',knowledge:'#a78bfa',apps:'#34d399',services:'#f472b6',assets:'#fbbf24',db:'#818cf8',auth:'#c084fc'};
var filterBtns='<button class="chart-tab'+(logFilter==='all'?' active':'')+'" onclick="setLogFilter(\'all\')">All</button>';
['info','warn','error','debug','chat','auth','api','deploy','assets','knowledge','apps','services','settings','system'].forEach(function(f){
filterBtns+='<button class="chart-tab'+(logFilter===f?' active':'')+'" onclick="setLogFilter(\''+f+'\')">'+f+'</button>';
});
var entries=logData.entries||[];
var filtered=entries.filter(function(e){
if(logFilter!=='all'&&e.level!==logFilter&&e.category!==logFilter)return false;
if(logSearch){var s=logSearch.toLowerCase();if(e.message.toLowerCase().indexOf(s)<0&&e.category.toLowerCase().indexOf(s)<0)return false;}
return true;
});

var stats={info:0,warn:0,error:0,debug:0};
for(var si=0;si<entries.length;si++){var sl=entries[si].level;if(stats[sl]!==undefined)stats[sl]++;}
var statsHtml='<div style="display:flex;gap:1rem;margin-bottom:.75rem;font-size:.75rem">';
statsHtml+='<span style="color:#4ade80">'+ic('dot',6)+' '+stats.info+' info</span>';
statsHtml+='<span style="color:#fbbf24">'+ic('dot',6)+' '+stats.warn+' warn</span>';
statsHtml+='<span style="color:#f87171">'+ic('dot',6)+' '+stats.error+' error</span>';
statsHtml+='<span style="color:rgba(255,255,255,0.3)">'+ic('dot',6)+' '+stats.debug+' debug</span>';
statsHtml+='</div>';

var rows='';
for(var i=0;i<filtered.length;i++){
var e=filtered[i];
var ts=new Date(e.timestamp).toLocaleTimeString('en-US',{hour12:false,hour:'2-digit',minute:'2-digit',second:'2-digit'});
var ms=String(e.timestamp%1000).padStart(3,'0');
var dateStr=new Date(e.timestamp).toLocaleDateString();
var lc=levelColors[e.level]||'#e2e2e8';
var cc=catColors[e.category]||'#e2e2e8';
var hasDet=e.details&&typeof e.details==='object'&&Object.keys(e.details).length>0;
var isExpanded=logExpandedIdx===i;
rows+='<div onclick="toggleLogDetail('+i+')" style="cursor:pointer;padding:.3rem .5rem;font-family:var(--mono);font-size:.75rem;line-height:1.6;border-bottom:1px solid rgba(255,255,255,0.02);'+(isExpanded?'background:rgba(124,58,237,.05)':'')+'">'+
'<div style="display:flex;gap:.5rem;align-items:baseline">'+
(logShowTimestamp?'<span style="color:var(--text-muted);white-space:nowrap;min-width:90px">'+ts+'.'+ms+'</span>':'')+
'<span style="color:'+lc+';min-width:40px;font-weight:600;text-transform:uppercase;font-size:.625rem">'+esc(e.level)+'</span>'+
'<span style="color:'+cc+';min-width:70px;font-size:.6875rem">['+esc(e.category)+']</span>'+
'<span style="color:var(--text-primary);flex:1;word-break:break-all">'+esc(e.message)+(hasDet&&!isExpanded?' <span style="color:var(--text-muted);font-size:.625rem">'+ic('chevD',8)+'</span>':'')+'</span>'+
'</div>';
if(isExpanded&&hasDet){
rows+='<div style="margin:.375rem 0 .25rem 100px;padding:.5rem;background:var(--bg-body);border:1px solid var(--border);border-radius:var(--radius-sm);font-size:.6875rem;color:var(--text-secondary);white-space:pre-wrap;max-height:200px;overflow-y:auto">'+esc(JSON.stringify(e.details,null,2))+'</div>';
}
if(isExpanded){
rows+='<div style="margin-left:100px;font-size:.625rem;color:var(--text-muted)">'+dateStr+' '+ts+'.'+ms+' &middot; Level: '+esc(e.level)+' &middot; Category: '+esc(e.category)+'</div>';
}
rows+='</div>';
}
if(!rows)rows='<div style="padding:2rem;text-align:center;color:var(--text-muted);font-size:.875rem">No log entries'+(logFilter!=='all'?' matching filter "'+logFilter+'"':'')+(logSearch?' matching "'+esc(logSearch)+'"':'')+'</div>';
startLogPoll();
return '<div class="usage-wrap" style="max-width:100%">'+
'<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:.75rem;flex-wrap:wrap;gap:.5rem">'+
'<h2 style="font-size:1.25rem;color:var(--accent-pale)">'+ic('log',20)+' System Logs</h2>'+
'<div style="display:flex;gap:.5rem;align-items:center;flex-wrap:wrap">'+
'<input id="logSearchInput" type="text" placeholder="Search logs..." value="'+esc(logSearch)+'" style="background:var(--bg-elevated);border:1px solid var(--border);border-radius:var(--radius-sm);color:var(--text-primary);padding:.3rem .5rem;font-size:.75rem;font-family:var(--font);outline:none;width:160px" onkeydown="if(event.key===\'Enter\'){logSearch=this.value;render();}">'+
'<label style="font-size:.75rem;color:var(--text-muted);display:flex;align-items:center;gap:.25rem;cursor:pointer"><input type="checkbox" '+(logAutoScroll?'checked':'')+' onchange="logAutoScroll=this.checked"> Auto-scroll</label>'+
'<label style="font-size:.75rem;color:var(--text-muted);display:flex;align-items:center;gap:.25rem;cursor:pointer"><input type="checkbox" '+(logShowTimestamp?'checked':'')+' onchange="logShowTimestamp=this.checked;render()"> Time</label>'+
'<button class="chart-tab" onclick="exportLogs()">'+ic('download',12)+' Export</button>'+
'<button class="chart-tab" onclick="clearLogs()">Clear</button>'+
'<button class="chart-tab" onclick="logData=null;render()">'+ic('refresh',12)+'</button>'+
'</div></div>'+
statsHtml+
'<div style="display:flex;gap:.25rem;overflow-x:auto;-webkit-overflow-scrolling:touch;scrollbar-width:none;flex-wrap:nowrap;margin-bottom:.75rem;padding-bottom:2px">'+filterBtns+'</div>'+
'<div class="chart-container" style="padding:0;height:calc(100vh - 310px);min-height:300px;overflow-y:auto" id="logContainer">'+rows+'</div>'+
'<div style="margin-top:.5rem;font-size:.6875rem;color:var(--text-muted)">'+logData.total+' total entries &middot; Showing '+filtered.length+' &middot; Auto-refreshing every 3s</div>'+
'</div>';
}
function loadLogs(){api('/api/logs?limit=500',{silent:true}).then(function(d){logData=d||{entries:[],total:0};render();}).catch(function(){logData={entries:[],total:0};render();});}
window.setLogFilter=function(f){logFilter=f;logExpandedIdx=null;render();};
window.clearLogs=function(){api('/api/logs/clear',{method:'POST'}).then(function(){logData={entries:[],total:0};render();}).catch(function(){});};
window.toggleLogDetail=function(idx){logExpandedIdx=(logExpandedIdx===idx)?null:idx;render();};
window.exportLogs=function(){
if(!logData||!logData.entries)return;
var data=JSON.stringify(logData.entries,null,2);
var blob=new Blob([data],{type:'application/json'});
var a=document.createElement('a');a.href=URL.createObjectURL(blob);
a.download='avacli-logs-'+new Date().toISOString().slice(0,10)+'.json';
a.click();URL.revokeObjectURL(a.href);
toast('Logs exported','success');
};
function startLogPoll(){
if(logPollTimer)clearInterval(logPollTimer);
logPollTimer=setInterval(function(){
if(S.route!=='/logs'){clearInterval(logPollTimer);logPollTimer=null;return;}
api('/api/logs?limit=500',{silent:true}).then(function(d){
if(d&&S.route==='/logs'){logData=d;
var container=document.getElementById('logContainer');
var wasAtBottom=container&&(container.scrollTop+container.clientHeight>=container.scrollHeight-30);
render();
if(logAutoScroll&&wasAtBottom){var c2=document.getElementById('logContainer');if(c2)c2.scrollTop=c2.scrollHeight;}
}
}).catch(function(){});
},3000);
}

/* ── System Page ──────────────────────────────────────── */
var sysState={prompt:'',blueprints:[],activeId:null,editing:false,nodes:[],edges:[],bpName:'',bpDesc:'',loaded:false,mermaidSrc:'',promptDirty:false,flowDirty:false,
availableTools:null,availableModels:null,
ab:{desc:'',selectedTools:{},modelAssignments:{},generating:false,result:null,showToolPicker:false,showModelPicker:false}
};

function loadSystemData(){
api('/api/system/prompt',{silent:true}).then(function(d){
sysState.prompt=d.prompt||'';sysState.loaded=true;
if(!sysState.mermaidSrc&&sysState.prompt){
sysState.mermaidSrc=generateFlowchartFromPrompt(sysState.prompt);
}
renderSystemInner();
}).catch(function(){sysState.loaded=true;renderSystemInner();});
api('/api/system/blueprints',{silent:true}).then(function(arr){
sysState.blueprints=Array.isArray(arr)?arr:[];renderSystemInner();
}).catch(function(){});
if(!sysState.availableTools){
api('/api/system/tools',{silent:true}).then(function(d){
sysState.availableTools=Array.isArray(d)?d:[];
}).catch(function(){sysState.availableTools=[];});
}
if(!sysState.availableModels){
api('/api/models',{silent:true}).then(function(d){
sysState.availableModels=Array.isArray(d)?d:[];
}).catch(function(){sysState.availableModels=[];});
}
}

function renderSystem(){
if(!sysState.loaded){loadSystemData();return '<div class="sys-wrap" id="sysWrap"><div style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div></div>';}
return '<div class="sys-wrap" id="sysWrap">'+renderSystemInnerHtml()+'</div>';
}

function renderSystemInnerHtml(){
var h='<div class="sys-header"><h2>'+ic('brain',20)+' System Prompt & Agent Blueprints</h2>';
h+='<p class="sys-sub">Design and manage how the agent behaves. Build flowcharts, save blueprints, minimize tokens with maximum context.</p></div>';

h+='<div class="sys-grid">';

// Left: Prompt editor
h+='<div class="sys-panel sys-prompt-panel">';
h+='<div class="sys-panel-head"><span>'+ic('edit',14)+' Active System Prompt</span>';
h+='<div class="sys-panel-actions"><button class="sys-btn sys-btn-sm" onclick="sysGenMermaid()">'+ic('activity',12)+' Visualize</button>';
h+='<button class="sys-btn sys-btn-primary sys-btn-sm" onclick="sysSavePrompt()">'+ic('save',12)+' Save</button></div></div>';
h+='<textarea id="sysPromptEditor" class="sys-textarea" oninput="sysState.promptDirty=true" spellcheck="false">'+esc(sysState.prompt)+'</textarea>';
h+='<div class="sys-prompt-meta"><span id="sysTokenEst">~'+Math.round((sysState.prompt||'').length/4)+' tokens</span>';
h+='<span id="sysCharCount">'+((sysState.prompt||'').length)+' chars</span></div>';
h+='</div>';

// Right: Mermaid preview
h+='<div class="sys-panel sys-flow-panel">';
h+='<div class="sys-panel-head"><span>'+ic('activity',14)+' Agent Flow</span></div>';
h+='<div class="sys-mermaid-wrap" id="sysMermaidWrap">';
if(sysState.mermaidSrc){
h+='<div class="mermaid-render" data-mermaid-src="'+esc(sysState.mermaidSrc)+'"></div>';
}else{
h+='<div class="sys-flow-empty">'+ic('activity',32)+'<p>Click "Visualize" to generate a flowchart from the system prompt</p></div>';
}
h+='</div>';
h+='</div>';

h+='</div>'; // /sys-grid

// Agent Builder AI
h+='<div class="sys-section ab-section">';
h+='<div class="sys-section-head"><h3>'+ic('zap',16)+' Agent Builder AI</h3>';
h+='<span class="ab-badge">Describe an agent and AI builds the blueprint</span></div>';
h+='<div class="ab-form">';
h+='<textarea id="abDescInput" class="ab-desc-input" placeholder="Describe the agent you want to build...\n\nExample: An agent that creates 30 second video shorts based off recent news. It should research trending topics, generate a script, create images for each scene, then compile them into a short video with captions." rows="4">'+esc(sysState.ab.desc)+'</textarea>';

h+='<div class="ab-config">';
h+='<div class="ab-config-section">';
h+='<button class="sys-btn sys-btn-sm" onclick="abToggleToolPicker()">'+ic('wrench',12)+' '+(sysState.ab.showToolPicker?'Hide':'Select')+' Tools '+(Object.keys(sysState.ab.selectedTools).length?'('+Object.keys(sysState.ab.selectedTools).length+')':'')+'</button>';
h+='<button class="sys-btn sys-btn-sm" onclick="abToggleModelPicker()">'+ic('gear',12)+' '+(sysState.ab.showModelPicker?'Hide':'Assign')+' Models '+(Object.keys(sysState.ab.modelAssignments).length?'('+Object.keys(sysState.ab.modelAssignments).length+')':'')+'</button>';
h+='</div>';
h+='<button class="sys-btn sys-btn-primary ab-generate-btn'+(sysState.ab.generating?' disabled':'')+'" onclick="abGenerate()" '+(sysState.ab.generating?'disabled':'')+'>'+
(sysState.ab.generating?'<div class="spinner-sm"></div> Generating...':ic('zap',14)+' Generate Blueprint')+'</button>';
h+='</div>';

if(sysState.ab.showToolPicker){
h+='<div class="ab-tool-picker">';
h+='<div class="ab-picker-head">Select tools the agent should use (leave empty for AI to decide)</div>';
h+='<div class="ab-tool-grid">';
var tools=sysState.availableTools||[];
var toolCats={};
for(var ti=0;ti<tools.length;ti++){
var t=tools[ti];
var cat='General';
if(t.name.match(/read_file|search_files|list_dir|glob/))cat='Explore';
else if(t.name.match(/web_search|x_search|read_url/))cat='Web';
else if(t.name.match(/edit_file|write_file|undo_edit/))cat='Edit';
else if(t.name.match(/run_shell|run_tests/))cat='Execute';
else if(t.name.match(/generate_image|edit_image|generate_video|edit_video|extend_video/))cat='Media';
else if(t.name.match(/create_tool|modify_tool|delete_tool/))cat='Forge';
else if(t.name.match(/research_api|setup_api|call_api/))cat='API';
else if(t.name.match(/vault|memory|todo|note/))cat='Memory';
else if(t.name.match(/db_query/))cat='Database';
else if(t.name.match(/save_article|search_article/))cat='Knowledge';
if(!toolCats[cat])toolCats[cat]=[];
toolCats[cat].push(t);
}
var cats=Object.keys(toolCats).sort();
for(var ci=0;ci<cats.length;ci++){
h+='<div class="ab-tool-cat"><div class="ab-tool-cat-head">'+esc(cats[ci])+' <button class="ab-cat-toggle" onclick="abToggleCat(\''+esc(cats[ci])+'\')">all</button></div>';
for(var tti=0;tti<toolCats[cats[ci]].length;tti++){
var tool=toolCats[cats[ci]][tti];
var chk=sysState.ab.selectedTools[tool.name]?'checked':'';
h+='<label class="ab-tool-item'+(chk?' selected':'')+'"><input type="checkbox" '+chk+' onchange="abToggleTool(\''+esc(tool.name)+'\',this.checked)"> <code>'+esc(tool.name)+'</code></label>';
}
h+='</div>';
}
h+='</div></div>';
}

if(sysState.ab.showModelPicker){
h+='<div class="ab-model-picker">';
h+='<div class="ab-picker-head">Assign models to processing steps (leave empty for AI to decide)</div>';
var steps=['reasoning','text_generation','code_analysis','image_generation','video_generation','data_processing','search','custom'];
var models=sysState.availableModels||[];
h+='<div class="ab-model-grid">';
for(var si=0;si<steps.length;si++){
var step=steps[si];
var label=step.replace(/_/g,' ').replace(/\b\w/g,function(c){return c.toUpperCase();});
var assigned=sysState.ab.modelAssignments[step]||'';
h+='<div class="ab-model-row">';
h+='<span class="ab-model-label">'+label+'</span>';
h+='<select class="ab-model-select" onchange="abAssignModel(\''+step+'\',this.value)">';
h+='<option value="">Auto-detect</option>';
for(var mi=0;mi<models.length;mi++){
var m=models[mi];
var sel=assigned===m.id?' selected':'';
var badge=m.type==='image_gen'?' [img]':(m.type==='video_gen'?' [vid]':(m.reasoning?' [reason]':''));
h+='<option value="'+esc(m.id)+'"'+sel+'>'+esc(m.id)+badge+'</option>';
}
h+='</select></div>';
}
h+='</div></div>';
}

if(sysState.ab.result){
var r=sysState.ab.result;
h+='<div class="ab-result">';
h+='<div class="ab-result-head"><strong>'+esc(r.name||'Generated Blueprint')+'</strong>';
h+='<span class="ab-result-desc">'+esc(r.description||'')+'</span></div>';
if(r.tools&&r.tools.length){
h+='<div class="ab-result-tools"><span class="ab-result-label">Tools:</span> ';
for(var rti=0;rti<r.tools.length;rti++){h+='<code class="ab-tool-tag">'+esc(r.tools[rti])+'</code> ';}
h+='</div>';
}
if(r.model_assignments&&Object.keys(r.model_assignments).length){
h+='<div class="ab-result-models"><span class="ab-result-label">Model Routing:</span>';
var ma=r.model_assignments;
for(var mk in ma){h+='<div class="ab-model-tag"><span>'+esc(mk.replace(/_/g,' '))+'</span> '+ic('chevR',10)+' <code>'+esc(ma[mk])+'</code></div>';}
h+='</div>';
}
h+='<div class="ab-result-actions">';
h+='<button class="sys-btn sys-btn-sm" onclick="abLoadToEditor()">'+ic('edit',12)+' Load to Editor</button>';
h+='<button class="sys-btn sys-btn-primary sys-btn-sm" onclick="abSaveAsBlueprint()">'+ic('save',12)+' Save as Blueprint</button>';
h+='<button class="sys-btn sys-btn-primary sys-btn-sm" onclick="abDeployDirect()">'+ic('zap',12)+' Deploy Now</button>';
h+='</div></div>';
}

h+='</div></div>';

// Flow builder
h+='<div class="sys-section">';
h+='<div class="sys-section-head"><h3>'+ic('zap',16)+' Flow Builder</h3>';
h+='<button class="sys-btn sys-btn-sm" onclick="sysAddNode()">'+ic('plus',12)+' Add Node</button></div>';
h+='<div class="sys-flow-builder" id="sysFlowBuilder">';
h+=renderFlowBuilder();
h+='</div></div>';

// Blueprints
h+='<div class="sys-section">';
h+='<div class="sys-section-head"><h3>'+ic('grid',16)+' Saved Blueprints</h3>';
h+='<button class="sys-btn sys-btn-primary sys-btn-sm" onclick="sysSaveBlueprint()">'+ic('save',12)+' Save Current as Blueprint</button></div>';
h+='<div class="sys-blueprints" id="sysBlueprintList">';
if(!sysState.blueprints.length){
h+='<div class="sys-bp-empty">No blueprints yet. Save your current prompt and flow as a blueprint to get started.</div>';
}else{
for(var i=0;i<sysState.blueprints.length;i++){
var bp=sysState.blueprints[i];
h+='<div class="sys-bp-card" data-bp="'+esc(bp.id)+'">';
h+='<div class="sys-bp-card-head"><strong>'+esc(bp.name||'Untitled')+'</strong>';
h+=(bp.is_default?'<span class="sys-bp-badge">Default</span>':'');
h+='</div>';
h+='<div class="sys-bp-card-desc">'+esc(bp.description||'No description')+'</div>';
var bpMeta=((bp.prompt||'').length?'~'+Math.round(bp.prompt.length/4)+' tokens':'Empty')+' &middot; ';
bpMeta+=(bp.nodes&&bp.nodes.length?bp.nodes.length+' nodes':'No flow');
if(bp.tools&&bp.tools.length)bpMeta+=' &middot; '+bp.tools.length+' tools';
if(bp.model_assignments&&Object.keys(bp.model_assignments).length)bpMeta+=' &middot; '+Object.keys(bp.model_assignments).length+' model routes';
h+='<div class="sys-bp-card-meta">'+bpMeta+'</div>';
h+='<div class="sys-bp-card-actions">';
h+='<button class="sys-btn sys-btn-sm" data-bp-action="load" data-bp-id="'+esc(bp.id)+'" onclick="sysLoadBlueprint(this.dataset.bpId)">'+ic('download',12)+' Load</button>';
h+='<button class="sys-btn sys-btn-primary sys-btn-sm" data-bp-action="deploy" data-bp-id="'+esc(bp.id)+'" onclick="sysDeployBlueprint(this.dataset.bpId)">'+ic('zap',12)+' Deploy</button>';
h+='<button class="sys-btn sys-btn-danger sys-btn-sm" data-bp-action="delete" data-bp-id="'+esc(bp.id)+'" onclick="sysDeleteBlueprint(this.dataset.bpId)">'+ic('trash',12)+'</button>';
h+='</div></div>';
}}
h+='</div></div>';

return h;
}

function renderFlowBuilder(){
var h='<div class="sys-fb-nodes">';
var nodes=sysState.nodes||[];
var nodeTypes=[{v:'process',l:'Process'},{v:'tool',l:'Tool'},{v:'decision',l:'Decision'},{v:'io',l:'I/O'}];
var models=sysState.availableModels||[];
if(!nodes.length){
h+='<div class="sys-fb-hint">Add nodes to build a visual agent flow. Nodes become steps in the Mermaid diagram.</div>';
}
for(var i=0;i<nodes.length;i++){
var n=nodes[i];
h+='<div class="sys-fb-node" data-nidx="'+i+'">';
h+='<input type="text" class="sys-fb-input" value="'+esc(n.label||'')+'" placeholder="Label" onchange="sysUpdateNode('+i+',\'label\',this.value)">';
h+='<select class="sys-fb-select" onchange="sysUpdateNode('+i+',\'type\',this.value)">';
for(var ti=0;ti<nodeTypes.length;ti++){h+='<option value="'+nodeTypes[ti].v+'"'+(n.type===nodeTypes[ti].v?' selected':'')+'>'+nodeTypes[ti].l+'</option>';}
h+='</select>';
h+='<select class="sys-fb-select sys-fb-model" onchange="sysUpdateNode('+i+',\'model\',this.value)">';
h+='<option value="">No model</option>';
for(var mi=0;mi<models.length;mi++){
var m=models[mi];
h+='<option value="'+esc(m.id)+'"'+((n.model||'')===m.id?' selected':'')+'>'+esc(m.id)+'</option>';
}
h+='</select>';
h+='<button class="sys-fb-del" onclick="sysRemoveNode('+i+')">'+ic('xic',12)+'</button>';
h+='</div>';
}
h+='</div>';

h+='<div class="sys-fb-edges">';
h+='<div class="sys-fb-edge-head"><span>Connections</span><button class="sys-btn sys-btn-sm" onclick="sysAddEdge()">'+ic('plus',12)+' Add</button></div>';
var edges=sysState.edges||[];
for(var i=0;i<edges.length;i++){
var e=edges[i];
h+='<div class="sys-fb-edge">';
h+='<select class="sys-fb-select" onchange="sysUpdateEdge('+i+',\'from\',this.value)">';
h+='<option value="">From...</option>';
for(var ni=0;ni<nodes.length;ni++){h+='<option value="'+esc(nodes[ni].id)+'"'+(e.from===nodes[ni].id?' selected':'')+'>'+esc(nodes[ni].label||nodes[ni].id)+'</option>';}
h+='</select>';
h+='<span class="sys-fb-arrow">'+ic('chevR',14)+'</span>';
h+='<select class="sys-fb-select" onchange="sysUpdateEdge('+i+',\'to\',this.value)">';
h+='<option value="">To...</option>';
for(var ni=0;ni<nodes.length;ni++){h+='<option value="'+esc(nodes[ni].id)+'"'+(e.to===nodes[ni].id?' selected':'')+'>'+esc(nodes[ni].label||nodes[ni].id)+'</option>';}
h+='</select>';
h+='<input type="text" class="sys-fb-input sys-fb-elabel" value="'+esc(e.label||'')+'" placeholder="Label (opt)" onchange="sysUpdateEdge('+i+',\'label\',this.value)">';
h+='<button class="sys-fb-del" onclick="sysRemoveEdge('+i+')">'+ic('xic',12)+'</button>';
h+='</div>';
}
h+='</div>';

h+='<div style="margin-top:.75rem;text-align:right"><button class="sys-btn sys-btn-sm" onclick="sysPreviewFlow()">'+ic('activity',12)+' Preview Flow</button></div>';
return h;
}

function renderSystemInner(){
var wrap=document.getElementById('sysWrap');
if(wrap)wrap.innerHTML=renderSystemInnerHtml();
setupSystemPage();
}

function setupSystemPage(){
var ta=document.getElementById('sysPromptEditor');
if(ta){
ta.addEventListener('input',function(){
var est=document.getElementById('sysTokenEst');
var cc=document.getElementById('sysCharCount');
if(est)est.textContent='~'+Math.round(ta.value.length/4)+' tokens';
if(cc)cc.textContent=ta.value.length+' chars';
});
}
renderMermaidBlocks();
}

window.sysSavePrompt=function(){
var ta=document.getElementById('sysPromptEditor');
if(!ta)return;
api('/api/system/prompt',{method:'POST',body:{prompt:ta.value}}).then(function(){
sysState.prompt=ta.value;sysState.promptDirty=false;toast('System prompt saved','info');
}).catch(function(e){toast('Failed: '+e.message,'error');});
};

function generateFlowchartFromPrompt(text){
if(!text||!text.trim())return '';
function san(s){return String(s||'').replace(/"/g,"'").replace(/[<>[\]{}()#&]/g,'').replace(/\n/g,' ').substring(0,48);}

var sections=[],cur={t:'',b:[],tools:[],rules:[],steps:[],subs:[]},csub=null;
text.split('\n').forEach(function(line){
var m;
if((m=line.match(/^##\s+(.+)/))){
if(csub){cur.subs.push(csub);csub=null;}
if(cur.t||cur.b.length)sections.push(cur);
cur={t:m[1].trim(),b:[],tools:[],rules:[],steps:[],subs:[]};
}else if((m=line.match(/^\*\*(.+?):\*\*\s*$/))||(m=line.match(/^\*\*(.+?)\*\*\s*$/))){
if(cur.t){if(csub)cur.subs.push(csub);csub={t:m[1].trim(),tools:[],items:[]};}
}else if((m=line.match(/^[-*]\s+`(\w+)`\s*[-\u2013\u2014]+\s*(.+)/))){
var tgt=csub||cur;tgt.tools=tgt.tools||[];tgt.tools.push({n:m[1],d:m[2].replace(/`[^`]*`/g,'').trim()});
}else if((m=line.match(/^[-*]\s+\*\*(.+?)\*\*\s*[-\u2013\u2014:]+\s*(.+)/))){
cur.rules.push({l:m[1].trim(),d:m[2].trim()});
}else if((m=line.match(/^\d+\.\s+(.+)/))){
cur.steps.push(m[1].replace(/`([^`]*)`/g,'$1').trim());
}else{
cur.b.push(line);
if(csub&&line.match(/^[-*]\s+(.{5,})/)&&!line.match(/`\w+`/)){var bm=line.match(/^[-*]\s+(.+)/);if(bm)csub.items=csub.items||[],csub.items.push(bm[1].trim());}
}
});
if(csub)cur.subs.push(csub);
if(cur.t||cur.b.length)sections.push(cur);

function classify(s){
var tl=s.t.toLowerCase(),cl=s.b.join(' ').toLowerCase();
var hasManyTools=s.tools.length>3||(s.subs&&s.subs.filter(function(ss){return ss.tools&&ss.tools.length>0;}).length>2);
if(tl.match(/^tool|capabilit|function|available/)||hasManyTools)return 'tools';
if(tl.match(/behav|how to|personality|respond|interact|guideline/))return 'behavior';
if(tl.match(/code|change|mak.*change|writing code/)||(s.steps.length>2&&cl.match(/edit|file|code/)))return 'workflow';
if(tl.match(/safe|secur|constraint|restrict|forbidden|guard/))return 'safety';
if(tl.match(/think|reason|approach|strateg/))return 'thinking';
if(tl.match(/memory|context|todo|note|knowledge|persist/))return 'memory';
if(tl.match(/output|response|done|finish|complet|summar|when.*done/))return 'output';
if(tl.match(/environ|setup|config/)||cl.match(/you are|your role/))return 'identity';
if(!s.t&&cl.match(/you are/i))return 'identity';
if(s.steps.length>2)return 'workflow';
if(s.rules.length>2)return 'behavior';
return 'general';
}

var typed={};
sections.forEach(function(s){s._type=classify(s);if(!typed[s._type])typed[s._type]=[];typed[s._type].push(s);});

var mmd='flowchart TD\n';
mmd+='    classDef core fill:#7c3aed,stroke:#a78bfa,color:#fff,font-weight:bold\n';
mmd+='    classDef tool fill:#1e1b4b,stroke:#6366f1,color:#c7d2fe\n';
mmd+='    classDef dec fill:#312e81,stroke:#818cf8,color:#e0e7ff\n';
mmd+='    classDef io fill:#0c4a6e,stroke:#38bdf8,color:#bae6fd\n';
mmd+='    classDef safe fill:#450a0a,stroke:#ef4444,color:#fca5a5\n';
mmd+='    classDef mem fill:#064e3b,stroke:#34d399,color:#a7f3d0\n\n';

mmd+='    IN(["User Message"]):::io\n';

var idSec=(typed.identity||[])[0]||sections[0]||{b:[]};
var idLabel='Agent';
var idText=idSec.b.join(' ');
var rm=idText.match(/[Yy]ou are (.+?)[\.\,]/);
if(rm)idLabel=san(rm[1]).substring(0,50);
else if(idSec.t)idLabel=san(idSec.t);
if(idText.match(/autonom/i))idLabel+='<br/>Autonomous operation';
if(idText.match(/workspace|full access/i))idLabel+='<br/>Full tool access';
mmd+='    ID["'+idLabel+'"]:::core\n';
mmd+='    IN --> ID\n\n';

var memSec=(typed.memory||[])[0];
if(memSec){
var loadTools=memSec.tools.filter(function(t){return t.n.match(/read|search|note/i);});
if(loadTools.length>0){
mmd+='    CB{"Load context?"}:::dec\n';
mmd+='    ID --> CB\n';
mmd+='    CB -->|"unfamiliar codebase"| MLD["'+san(loadTools.map(function(t){return t.n;}).join(', '))+'"]:::mem --> INT\n';
mmd+='    CB -->|"known"| INT\n\n';
}else{mmd+='    ID --> INT\n\n';}
}else{mmd+='    ID --> INT\n\n';}

var behavSec=(typed.behavior||[])[0];
var hasIntentBranch=behavSec&&behavSec.rules.length>=2;
if(hasIntentBranch){
mmd+='    INT{"'+san(behavSec.t)+'"}:::dec\n';
var implEdge=null;
for(var ri=0;ri<behavSec.rules.length;ri++){
var r=behavSec.rules[ri],rl=r.l.toLowerCase(),rId='BR'+ri;
if(rl.match(/implement|fix|change|build|create|code/)){
implEdge=r.l;
mmd+='    INT -->|"'+san(r.l)+'"| CX\n';
}else if(rl.match(/plan/)){
mmd+='    PR["Read codebase"]:::tool\n';
mmd+='    PD["Detailed plan, specific files + steps"]:::core\n';
mmd+='    PA{"Proceed?"}:::dec\n';
mmd+='    INT -->|"'+san(r.l)+'"| PR --> PD --> PA\n';
mmd+='    PA -->|"plan only"| RESP\n';
mmd+='    PA -->|"execute"| CX\n';
}else{
mmd+='    '+rId+'["'+san(r.d).substring(0,55)+'"]:::core\n';
mmd+='    INT -->|"'+san(r.l)+'"| '+rId+' --> RESP\n';
}
}
if(!implEdge)mmd+='    INT --> CX\n';
mmd+='\n';
}else{
mmd+='    INT{"Process Request"}:::dec\n';
mmd+='    INT --> CX\n\n';
}

var thinkSec=(typed.thinking||[])[0];
if(thinkSec){
var thDesc=thinkSec.b.filter(function(l){return l.trim()&&!l.match(/^[<`]/);}).slice(0,2).map(function(l){return san(l.trim()).substring(0,40);}).join('<br/>');
if(!thDesc)thDesc='Reason before acting';
mmd+='    CX{"Complex task?"}:::dec\n';
mmd+='    CX -->|"yes"| TH["'+san(thinkSec.t)+'<br/>'+thDesc+'"]:::core --> RD\n';
mmd+='    CX -->|"simple"| RD\n';
}else{
mmd+='    CX["Begin work"]:::core\n';
}

var wfSec=(typed.workflow||[])[0];
if(wfSec&&wfSec.steps.length>0){
mmd+='    RD["'+san(wfSec.steps[0]).substring(0,55)+'"]:::tool\n';
if(!thinkSec)mmd+='    CX --> RD\n';
}else{
mmd+='    RD["Read relevant files"]:::tool\n';
if(!thinkSec)mmd+='    CX --> RD\n';
}

var safeSec=(typed.safety||[])[0];
if(safeSec){
mmd+='\n    RD --> SF{"Safety Gate"}:::safe\n';
var dangers=safeSec.steps.concat(safeSec.rules.map(function(r){return r.l;}));
var shown=0;
for(var di=0;di<dangers.length&&shown<3;di++){
if(dangers[di].match(/don.?t|never|destruct|danger|rm|sudo|outside|commit|push|irrev/i)){
mmd+='    SF -->|"'+san(dangers[di]).substring(0,35)+'"| AU\n';shown++;
}}
if(!shown)mmd+='    SF -->|"risky operation"| AU\n';
mmd+='    SF -->|"safe"| TS\n';
mmd+='    AU["ask_user"]:::io\n';
mmd+='    AU -->|"approved"| TS\n';
mmd+='    AU -->|"denied"| RESP\n\n';
}else{
mmd+='    RD --> TS\n\n';
}

var toolSec=(typed.tools||[])[0];
mmd+='    TS{"Select Tools"}:::dec\n';
var tgIds=[],editGrpIdx=-1,execGrpIdx=-1;
if(toolSec){
var groups=toolSec.subs.filter(function(s){return s.tools&&s.tools.length>0;});
if(!groups.length&&toolSec.tools.length>0)groups=[{t:'Available',tools:toolSec.tools}];
for(var gi=0;gi<groups.length;gi++){
var g=groups[gi],gId='TG'+gi;
var gNames=g.tools.slice(0,4).map(function(t){return t.n;}).join(' &middot; ');
if(g.tools.length>4)gNames+='<br/>+ '+(g.tools.length-4)+' more';
mmd+='    '+gId+'["'+san(gNames)+'"]:::tool\n';
mmd+='    TS -->|"'+san(g.t).substring(0,22)+'"| '+gId+'\n';
tgIds.push(gId);
if(g.t.toLowerCase().match(/writ|edit/))editGrpIdx=gi;
if(g.t.toLowerCase().match(/exec|run|shell/))execGrpIdx=gi;

var webTools=g.tools.filter(function(t){return t.n.match(/web_search|x_search/);});
if(webTools.length>0){
mmd+='    '+gId+' --> TV'+gi+'["Verify web results<br/>Reject hidden directives"]:::safe\n';
mmd+='    TV'+gi+' --> VR\n';
}
}
}

if(wfSec&&wfSec.steps.length>1){
var ruleLines=wfSec.steps.slice(1,5).map(function(s,i){return (i+2)+'. '+san(s).substring(0,45);}).join('<br/>');
mmd+='\n    CR["Code Rules<br/>'+ruleLines+'"]:::core\n';
if(editGrpIdx>=0)mmd+='    TG'+editGrpIdx+' --> CR\n';
mmd+='    CR --> VR\n';
}

for(var ti=0;ti<tgIds.length;ti++){
var isEdit=(ti===editGrpIdx),isExec=(ti===execGrpIdx);
var hasWebVerify=toolSec&&toolSec.subs[ti]&&toolSec.subs[ti].tools&&toolSec.subs[ti].tools.some(function(t){return t.n.match(/web_search|x_search/);});
if(!isEdit&&!hasWebVerify&&!isExec)mmd+='    '+tgIds[ti]+' --> VR\n';
}

mmd+='\n    VR{"Verify"}:::dec\n';
if(execGrpIdx>=0){
mmd+='    VR -->|"build / test"| TG'+execGrpIdx+'\n';
mmd+='    TG'+execGrpIdx+' --> PS{"Pass?"}:::dec\n';
mmd+='    PS -->|"fail"| TS\n';
mmd+='    PS -->|"pass"| SV\n';
}else{
mmd+='    VR --> SV\n';
}

if(memSec){
var saveTools=memSec.tools.filter(function(t){return t.n.match(/add|create|save|update|write/i);});
if(saveTools.length>0){
mmd+='\n    SV{"Save context?"}:::dec\n';
for(var sti=0;sti<Math.min(saveTools.length,3);sti++){
var stId='MS'+sti;
mmd+='    '+stId+'["'+san(saveTools[sti].n)+'"]:::mem\n';
mmd+='    SV -->|"'+san(saveTools[sti].d||saveTools[sti].n).substring(0,25)+'"| '+stId+' --> RESP\n';
}
mmd+='    SV -->|"done"| RESP\n';
}else{mmd+='    SV --> RESP\n';}
}else{mmd+='\n    SV --> RESP\n';}

var outSec=(typed.output||[])[0];
var respLabel='Response';
if(outSec){
var oi=outSec.steps.length?outSec.steps:outSec.rules.map(function(r){return r.l;});
if(oi.length)respLabel+='<br/>'+oi.slice(0,3).map(function(s){return san(s).substring(0,30);}).join('<br/>');
}
mmd+='    RESP(["'+respLabel+'"]):::io\n';
return mmd;
}

window.sysGenMermaid=function(){
var ta=document.getElementById('sysPromptEditor');
var prompt=ta?ta.value:sysState.prompt;
if(sysState.nodes&&sysState.nodes.length>0){
api('/api/system/prompt/mermaid',{method:'POST',body:{prompt:prompt,nodes:sysState.nodes,edges:sysState.edges}}).then(function(d){
sysState.mermaidSrc=d.mermaid||'';
var wrap=document.getElementById('sysMermaidWrap');
if(wrap){wrap.innerHTML='<div class="mermaid-render" data-mermaid-src="'+esc(sysState.mermaidSrc)+'"></div>';renderMermaidBlocks();}
}).catch(function(e){toast('Failed: '+e.message,'error');});
}else{
var mmd=generateFlowchartFromPrompt(prompt);
if(mmd){
sysState.mermaidSrc=mmd;
var wrap=document.getElementById('sysMermaidWrap');
if(wrap){wrap.innerHTML='<div class="mermaid-render" data-mermaid-src="'+esc(sysState.mermaidSrc)+'"></div>';renderMermaidBlocks();}
}else{toast('Could not generate flowchart from prompt','error');}
}
};

window.sysAddNode=function(){
var id='N'+Date.now().toString(36);
sysState.nodes.push({id:id,label:'',type:'process'});
var fb=document.getElementById('sysFlowBuilder');
if(fb){fb.innerHTML=renderFlowBuilder();}
};
window.sysRemoveNode=function(idx){
var removed=sysState.nodes.splice(idx,1)[0];
if(removed)sysState.edges=sysState.edges.filter(function(e){return e.from!==removed.id&&e.to!==removed.id;});
var fb=document.getElementById('sysFlowBuilder');if(fb)fb.innerHTML=renderFlowBuilder();
};
window.sysUpdateNode=function(idx,key,val){
if(sysState.nodes[idx])sysState.nodes[idx][key]=val;
};
window.sysAddEdge=function(){
sysState.edges.push({from:'',to:'',label:''});
var fb=document.getElementById('sysFlowBuilder');if(fb)fb.innerHTML=renderFlowBuilder();
};
window.sysRemoveEdge=function(idx){
sysState.edges.splice(idx,1);
var fb=document.getElementById('sysFlowBuilder');if(fb)fb.innerHTML=renderFlowBuilder();
};
window.sysUpdateEdge=function(idx,key,val){
if(sysState.edges[idx])sysState.edges[idx][key]=val;
};
window.sysPreviewFlow=function(){
api('/api/system/prompt/mermaid',{method:'POST',body:{prompt:'',nodes:sysState.nodes,edges:sysState.edges}}).then(function(d){
sysState.mermaidSrc=d.mermaid||'';
var wrap=document.getElementById('sysMermaidWrap');
if(wrap){wrap.innerHTML='<div class="mermaid-render" data-mermaid-src="'+esc(sysState.mermaidSrc)+'"></div>';renderMermaidBlocks();}
}).catch(function(e){toast('Failed: '+e.message,'error');});
};

window.sysSaveBlueprint=function(){
var ta=document.getElementById('sysPromptEditor');
var prompt=ta?ta.value:sysState.prompt;
avaPrompt('Blueprint name:','',function(name){
if(!name)return;
avaPrompt('Description (optional):','',function(desc){
api('/api/system/blueprints',{method:'POST',body:{name:name,description:desc,prompt:prompt,nodes:sysState.nodes,edges:sysState.edges,mermaid_src:sysState.mermaidSrc||'',tools:[],model_assignments:{}}}).then(function(bp){
sysState.blueprints.unshift(bp);
var list=document.getElementById('sysBlueprintList');
if(list){renderSystemInner();}
toast('Blueprint saved','info');
}).catch(function(e){toast('Failed: '+e.message,'error');});
},{title:'Blueprint Description',okText:'Save'});
},{title:'Save Blueprint',okText:'Next'});
};

window.sysLoadBlueprint=function(id){
api('/api/system/blueprints/'+encodeURIComponent(id),{silent:true}).then(function(bp){
sysState.prompt=bp.prompt||'';
sysState.nodes=bp.nodes||[];
sysState.edges=bp.edges||[];
sysState.mermaidSrc=bp.mermaid_src||'';
if(!sysState.mermaidSrc&&sysState.prompt){
sysState.mermaidSrc=generateFlowchartFromPrompt(sysState.prompt);
}
renderSystemInner();
toast('Blueprint loaded: '+bp.name,'info');
}).catch(function(e){toast('Failed: '+e.message,'error');});
};

window.sysDeployBlueprint=function(id){
avaConfirm('Deploy this blueprint as the active system prompt?',function(){
api('/api/system/blueprints/'+encodeURIComponent(id)+'/deploy',{method:'POST'}).then(function(){
toast('Blueprint deployed as active prompt','info');
loadSystemData();
}).catch(function(e){toast('Failed: '+e.message,'error');});
},{title:'Deploy Blueprint',okText:'Deploy',danger:false});
};

window.sysDeleteBlueprint=function(id){
avaConfirm('Delete this blueprint permanently?',function(){
api('/api/system/blueprints/'+encodeURIComponent(id),{method:'DELETE'}).then(function(){
sysState.blueprints=sysState.blueprints.filter(function(b){return b.id!==id;});
renderSystemInner();
toast('Blueprint deleted','info');
}).catch(function(e){toast('Failed: '+e.message,'error');});
},{title:'Delete Blueprint'});
};

/* ── Agent Builder AI ─────────────────────────────────── */
window.abToggleToolPicker=function(){
sysState.ab.showToolPicker=!sysState.ab.showToolPicker;
renderSystemInner();
};
window.abToggleModelPicker=function(){
sysState.ab.showModelPicker=!sysState.ab.showModelPicker;
renderSystemInner();
};
window.abToggleTool=function(name,checked){
if(checked)sysState.ab.selectedTools[name]=true;
else delete sysState.ab.selectedTools[name];
};
window.abToggleCat=function(cat){
var tools=sysState.availableTools||[];
var catTools=tools.filter(function(t){
var c='General';
if(t.name.match(/read_file|search_files|list_dir|glob/))c='Explore';
else if(t.name.match(/web_search|x_search|read_url/))c='Web';
else if(t.name.match(/edit_file|write_file|undo_edit/))c='Edit';
else if(t.name.match(/run_shell|run_tests/))c='Execute';
else if(t.name.match(/generate_image|edit_image|generate_video|edit_video|extend_video/))c='Media';
else if(t.name.match(/create_tool|modify_tool|delete_tool/))c='Forge';
else if(t.name.match(/research_api|setup_api|call_api/))c='API';
else if(t.name.match(/vault|memory|todo|note/))c='Memory';
else if(t.name.match(/db_query/))c='Database';
else if(t.name.match(/save_article|search_article/))c='Knowledge';
return c===cat;
});
var allSelected=catTools.every(function(t){return sysState.ab.selectedTools[t.name];});
catTools.forEach(function(t){
if(allSelected)delete sysState.ab.selectedTools[t.name];
else sysState.ab.selectedTools[t.name]=true;
});
renderSystemInner();
};
window.abAssignModel=function(step,modelId){
if(modelId)sysState.ab.modelAssignments[step]=modelId;
else delete sysState.ab.modelAssignments[step];
};
window.abGenerate=function(){
var desc=document.getElementById('abDescInput');
var description=desc?desc.value.trim():'';
if(!description){toast('Enter a description of the agent you want to build','error');return;}
sysState.ab.desc=description;
sysState.ab.generating=true;
sysState.ab.result=null;
renderSystemInner();
var toolNames=Object.keys(sysState.ab.selectedTools);
api('/api/system/agent-builder',{method:'POST',body:{
description:description,
tools:toolNames.length?toolNames:undefined,
models:Object.keys(sysState.ab.modelAssignments).length?sysState.ab.modelAssignments:undefined
}}).then(function(d){
sysState.ab.generating=false;
sysState.ab.result=d;
renderSystemInner();
toast('Blueprint generated','success');
}).catch(function(e){
sysState.ab.generating=false;
renderSystemInner();
toast('Generation failed: '+(e.message||'unknown error'),'error');
});
};
window.abLoadToEditor=function(){
var r=sysState.ab.result;if(!r)return;
sysState.prompt=r.prompt||'';
sysState.nodes=r.nodes||[];
sysState.edges=r.edges||[];
sysState.mermaidSrc=generateFlowchartFromPrompt(sysState.prompt);
renderSystemInner();
toast('Loaded to editor','info');
};
window.abSaveAsBlueprint=function(){
var r=sysState.ab.result;if(!r)return;
var mmd=generateFlowchartFromPrompt(r.prompt||'');
api('/api/system/blueprints',{method:'POST',body:{
name:r.name||'AI-Generated Blueprint',
description:r.description||'',
prompt:r.prompt||'',
nodes:r.nodes||[],
edges:r.edges||[],
tools:r.tools||[],
model_assignments:r.model_assignments||{},
mermaid_src:mmd
}}).then(function(bp){
sysState.blueprints.unshift(bp);
renderSystemInner();
toast('Blueprint saved: '+(bp.name||'Untitled'),'success');
}).catch(function(e){toast('Save failed: '+e.message,'error');});
};
window.abDeployDirect=function(){
var r=sysState.ab.result;if(!r||!r.prompt)return;
avaConfirm('Deploy this AI-generated blueprint as the active system prompt?',function(){
api('/api/system/prompt',{method:'POST',body:{prompt:r.prompt}}).then(function(){
sysState.prompt=r.prompt;
sysState.mermaidSrc=generateFlowchartFromPrompt(r.prompt);
renderSystemInner();
toast('Blueprint deployed as active prompt','success');
}).catch(function(e){toast('Deploy failed: '+e.message,'error');});
},{title:'Deploy Blueprint',okText:'Deploy'});
};

/* ── Settings Page ────────────────────────────────────── */
function renderSettings(){
var modelOpts=renderModelOptions();

var masterKeySection='<div class="set-section"><h3>'+ic('users',16)+' User Accounts</h3>'+
'<div class="set-row"><span class="set-label">Authentication</span><span class="set-value" id="mkStatus">Checking...</span></div>'+
'<p style="font-size:.8125rem;color:var(--text-muted);margin:0 0 .75rem">Manage user accounts. Admins can edit accounts, API keys, and servers.</p>'+
'<div id="accountsList"><div class="spinner" style="width:14px;height:14px;border-width:2px;display:inline-block"></div></div>'+
(S.user&&S.user.is_admin?
'<div style="display:flex;gap:.5rem;margin-top:.75rem;flex-wrap:wrap;align-items:center">'+
'<input class="set-input" id="newUsername" type="text" placeholder="username" style="max-width:140px">'+
'<input class="set-input" id="newPassword" type="password" placeholder="password (min 8)" style="max-width:160px">'+
'<button type="button" id="newIsAdmin" onclick="this.classList.toggle(\'active\')" style="display:inline-flex;align-items:center;gap:.25rem;font-size:.75rem;padding:.25rem .75rem;border-radius:9999px;border:1px solid rgba(255,255,255,.1);background:rgba(255,255,255,.04);color:var(--text-muted);cursor:pointer;transition:all .2s;font-weight:500" onmouseover="if(!this.classList.contains(\'active\'))this.style.borderColor=\'rgba(168,85,247,.4)\'" onmouseout="if(!this.classList.contains(\'active\'))this.style.borderColor=\'rgba(255,255,255,.1)\'">'+ic('shield',11)+' Admin</button>'+
'<style>#newIsAdmin.active{background:rgba(168,85,247,.2)!important;border-color:#a855f7!important;color:#c4b5fd!important;box-shadow:0 0 8px rgba(168,85,247,.25)}</style>'+
'<button class="btn-sm" onclick="createAccount()">'+ic('plus',12)+' Add Account</button>'+
'</div>':'<div style="font-size:.8125rem;color:var(--text-muted);margin-top:.5rem">Only admins can manage accounts.</div>')+
'</div>'+
(S.user&&S.user.has_password?'<div class="set-section"><h3>'+ic('key',16)+' Change My Password</h3>'+
'<div style="display:flex;gap:.5rem;flex-wrap:wrap;align-items:center">'+
'<input class="set-input" id="myCurrentPw" type="password" placeholder="current password" style="max-width:160px">'+
'<input class="set-input" id="myNewPw" type="password" placeholder="new password (min 8)" style="max-width:160px">'+
'<button class="btn-sm" onclick="changeMyPassword()">'+ic('key',12)+' Change</button>'+
'</div></div>':'');
api('/api/auth/status',{silent:true}).then(function(d){
var el=document.getElementById('mkStatus');
if(el){
if(!d.master_key_configured)el.innerHTML='<span style="color:#f87171">No accounts configured</span>';
else if(!d.requires_login)el.innerHTML='<span style="color:#a78bfa">Open access (no password set)</span>';
else el.innerHTML='<span style="color:#4ade80">Authentication active</span>';
}
}).catch(function(){});
setTimeout(function(){loadAccounts();},0);

var isAdm=S.user&&S.user.is_admin;
var xaiSection=isAdm?'<div class="set-section"><h3>'+ic('zap',16)+' xAI API Key</h3>'+
'<div class="set-row"><span class="set-label">Key</span><input class="set-input" id="apiKeyInput" type="password" placeholder="xai-..."></div>'+
'<div class="set-row"><span class="set-label">Status</span><span class="set-value">'+(S.hasXaiKey?'<span style="color:#4ade80">Configured</span>':'<span style="color:#f87171">Not set</span>')+'</span></div>'+
'<div style="display:flex;gap:.5rem;margin-top:.5rem"><button class="btn-sm" onclick="saveXaiKey()">'+ic('save',12)+' Save xAI Key</button>'+
'<button class="btn-sm" onclick="clearStoredXaiKey()" style="opacity:.8">'+ic('trash',12)+' Clear Key</button></div></div>':'';

var vultrSection=isAdm?'<div class="set-section"><h3>'+ic('server',16)+' Vultr API Key</h3>'+
'<div class="set-row"><span class="set-label">Key</span><input class="set-input" id="vultrKeyInput" type="password" placeholder="Vultr API key"></div>'+
'<div class="set-row"><span class="set-label">Status</span><span class="set-value" id="vultrKeyStatus">Checking...</span></div>'+
'<div style="display:flex;gap:.5rem;margin-top:.5rem"><button class="btn-sm" onclick="saveVultrKey()">Save Vultr Key</button>'+
'<button class="btn-sm" onclick="clearVultrKey()" style="opacity:.8">'+ic('trash',12)+' Clear Key</button></div></div>':'';
if(isAdm){api('/api/vultr/key/status',{silent:true}).then(function(d){
var el=document.getElementById('vultrKeyStatus');
if(el)el.innerHTML=d.configured?'<span style="color:#4ade80">Configured</span>':'<span style="color:#f87171">Not set</span>';
}).catch(function(){});}

return '<div class="settings-wrap">'+
masterKeySection+
xaiSection+
vultrSection+
'<div class="set-section"><h3>'+ic('brain',16)+' Model</h3>'+
'<div class="set-row"><span class="set-label">Active Model</span><select id="settingsModel" onchange="setModelFromSelect(this)">'+modelOpts+'</select></div>'+
renderReasoningEffortSelect(false)+
renderSettingsMediaOpts()+
'</div>'+
'<div class="set-section"><h3>'+ic('zap',16)+' Extracurricular Model</h3>'+
'<p style="font-size:.8125rem;color:var(--text-muted);margin:0 0 .5rem">Used for background tasks: asset analysis, mock data generation, semantic tagging, etc.</p>'+
'<div class="set-row"><span class="set-label">Model</span><select id="extraModelSelect" onchange="setExtraModel(this.value)">'+
'<option value=""'+((!S.extraModel)?' selected':'')+'>Same as primary</option>'+
renderExtraModelOptions()+
'</select></div></div>'+
'<div class="set-section"><h3>'+ic('folder',16)+' Workspace</h3>'+
'<div class="set-row"><span class="set-label">Directory</span>'+
'<div style="display:flex;gap:.5rem;flex:1">'+
'<input class="set-input" id="workspaceInput" value="'+esc(S.status?S.status.workspace:'')+'" style="flex:1">'+
'<button class="btn-sm" onclick="browseWorkspace()" title="Browse folder">'+ic('folder',14)+' Browse</button>'+
'</div></div>'+
'<div class="set-row"><span class="set-label">Port</span><span class="set-value">'+((S.status&&S.status.port)?S.status.port:'--')+'</span></div></div>'+
'<div class="set-section"><h3>'+ic('code',16)+' Editor</h3>'+
'<div class="set-row"><span class="set-label" title="Max undo/redo steps">History Limit</span><input type="number" class="set-input" id="editorHistoryInput" value="'+(typeof S.editorHistoryLimit==='number'?S.editorHistoryLimit:25)+'" min="5" max="100" onchange="S.editorHistoryLimit=parseInt(this.value)||25"></div></div>'+

'<div class="set-section"><h3>'+ic('chat',16)+' Session</h3>'+
'<div class="set-row"><span class="set-label">Current Session</span><span class="set-value">'+(esc(S.session)||'default')+'</span></div>'+
'<div class="set-row"><span class="set-label">Messages</span><span class="set-value">'+S.messages.length+'</span></div></div>'+
'<button class="btn-save" onclick="saveSettings()">'+ic('save',16)+' Save Settings</button>'+
'</div>';
}

window.saveXaiKey=function(){
var k=document.getElementById('apiKeyInput');if(!k||!k.value.trim()){toast('Enter an xAI API key','error');return;}
api('/api/settings',{method:'POST',body:{xai_api_key:k.value.trim()}}).then(function(r){
toast('xAI API key saved','success');
if(r&&r.settings&&typeof r.settings.has_xai_key==='boolean')S.hasXaiKey=r.settings.has_xai_key;
return api('/api/status');
}).then(function(st){if(st){S.hasXaiKey=!!st.has_xai_key;}render();}).catch(function(){});
};
window.saveVultrKey=function(){
var k=document.getElementById('vultrKeyInput');if(!k||!k.value){toast('Enter a Vultr API key','error');return;}
api('/api/vultr/key',{method:'POST',body:{api_key:k.value}}).then(function(){toast('Vultr API key saved','success');render();}).catch(function(){});
};
window.clearVultrKey=function(){
avaConfirm('Remove Vultr API key?',function(){
api('/api/vultr/key',{method:'DELETE'}).then(function(){
toast('Vultr key cleared','info');render();
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});
},{title:'Remove Vultr Key',okText:'Remove',danger:true});
};
function renderSettingsMediaOpts(){
var m=getModelMeta(S.model);
if(!m)return '';
var ms=S.mediaSettings||{};
if(m.type==='image_gen'){
return '<div style="margin-top:.75rem;padding-top:.75rem;border-top:1px solid var(--border)"><div style="font-size:.8125rem;color:var(--accent-pale);margin-bottom:.5rem;font-weight:600">'+ic('image',14)+' Image Generation Settings</div>'+
'<div class="set-row"><span class="set-label">Aspect Ratio</span><select class="set-input" onchange="updateMediaSetting(\'ratio\',this.value)"><option value="1:1"'+((ms.ratio||'1:1')==='1:1'?' selected':'')+'>1:1 (Square)</option><option value="16:9"'+(ms.ratio==='16:9'?' selected':'')+'>16:9 (Landscape)</option><option value="9:16"'+(ms.ratio==='9:16'?' selected':'')+'>9:16 (Portrait)</option><option value="4:3"'+(ms.ratio==='4:3'?' selected':'')+'>4:3</option><option value="3:4"'+(ms.ratio==='3:4'?' selected':'')+'>3:4</option><option value="3:2"'+(ms.ratio==='3:2'?' selected':'')+'>3:2</option><option value="2:3"'+(ms.ratio==='2:3'?' selected':'')+'>2:3</option><option value="1:2"'+(ms.ratio==='1:2'?' selected':'')+'>1:2</option><option value="2:1"'+(ms.ratio==='2:1'?' selected':'')+'>2:1</option><option value="auto"'+(ms.ratio==='auto'?' selected':'')+'>Auto</option></select></div>'+
'<div class="set-row"><span class="set-label">Resolution</span><select class="set-input" onchange="updateMediaSetting(\'resolution\',this.value)"><option value="1k"'+((ms.resolution||'1k')==='1k'?' selected':'')+'>1k</option><option value="2k"'+(ms.resolution==='2k'?' selected':'')+'>2k</option></select></div>'+
'<div class="set-row"><span class="set-label">Quality</span><select class="set-input" onchange="updateMediaSetting(\'quality\',this.value)"><option value=""'+(!ms.quality?' selected':'')+'>Default</option><option value="low"'+(ms.quality==='low'?' selected':'')+'>Low</option><option value="medium"'+(ms.quality==='medium'?' selected':'')+'>Medium</option><option value="high"'+(ms.quality==='high'?' selected':'')+'>High</option></select></div>'+
'<div class="set-row"><span class="set-label">Number of Images</span><select class="set-input" onchange="updateMediaSetting(\'count\',this.value)"><option value="1"'+(ms.count==='1'||!ms.count?' selected':'')+'>1</option><option value="2"'+(ms.count==='2'?' selected':'')+'>2</option><option value="3"'+(ms.count==='3'?' selected':'')+'>3</option><option value="4"'+(ms.count==='4'?' selected':'')+'>4</option></select></div></div>';
}
if(m.type==='video_gen'){
return '<div style="margin-top:.75rem;padding-top:.75rem;border-top:1px solid var(--border)"><div style="font-size:.8125rem;color:var(--accent-pale);margin-bottom:.5rem;font-weight:600">'+ic('video',14)+' Video Generation Settings</div>'+
'<div class="set-row"><span class="set-label">Duration</span><select class="set-input" onchange="updateMediaSetting(\'duration\',this.value)"><option value="5"'+((ms.duration||'5')==='5'?' selected':'')+'>5 seconds</option><option value="10"'+(ms.duration==='10'?' selected':'')+'>10 seconds</option><option value="15"'+(ms.duration==='15'?' selected':'')+'>15 seconds</option></select></div>'+
'<div class="set-row"><span class="set-label">Aspect Ratio</span><select class="set-input" onchange="updateMediaSetting(\'ratio\',this.value)"><option value="16:9"'+((ms.ratio||'16:9')==='16:9'?' selected':'')+'>16:9 (Landscape)</option><option value="9:16"'+(ms.ratio==='9:16'?' selected':'')+'>9:16 (Portrait)</option><option value="1:1"'+(ms.ratio==='1:1'?' selected':'')+'>1:1 (Square)</option></select></div>'+
'<div class="set-row"><span class="set-label">Resolution</span><select class="set-input" onchange="updateMediaSetting(\'resolution\',this.value)"><option value="480p"'+(ms.resolution==='480p'?' selected':'')+'>480p</option><option value="720p"'+((ms.resolution||'720p')==='720p'||ms.resolution==='720p'?' selected':'')+'>720p</option></select></div></div>';
}
return '';
}


window.clearStoredXaiKey=function(){
avaConfirm('Remove the xAI API key from ~/.avacli/settings.json? (Environment variable may still apply.)',function(){
api('/api/settings',{method:'POST',body:{model:S.model,xai_api_key:''}}).then(function(r){
toast('Stored xAI key cleared','info');
if(r&&r.settings&&typeof r.settings.has_xai_key==='boolean')S.hasXaiKey=r.settings.has_xai_key;
return api('/api/status');
}).then(function(st){
if(st){S.hasXaiKey=!!st.has_xai_key;}
render();
}).catch(function(){});
},{title:'Remove API Key',okText:'Remove',danger:true});
};

/* ── User Accounts ─────────────────────────────────────── */
function loadAccounts(){
api('/api/auth/accounts',{silent:true}).then(function(d){
var list=document.getElementById('accountsList');if(!list)return;
var accounts=(d&&d.accounts)?d.accounts:[];
if(!accounts.length){list.innerHTML='<div style="color:var(--text-muted);font-size:.8125rem">No accounts configured.</div>';return;}
var isAdmin=S.user&&S.user.is_admin;
var html='';
for(var i=0;i<accounts.length;i++){
var a=accounts[i];
var adminBadge=a.is_admin?'<span style="background:rgba(168,85,247,.15);color:#c4b5fd;font-size:.6875rem;padding:.125rem .5rem;border-radius:9999px;margin-left:.5rem;font-weight:600">ADMIN</span>':'<span style="background:rgba(255,255,255,.05);color:var(--text-muted);font-size:.6875rem;padding:.125rem .5rem;border-radius:9999px;margin-left:.5rem">USER</span>';
var isSelf=S.user&&S.user.username===a.username;
var actions='';
if(isAdmin||isSelf){
actions+='<button class="btn-sm" style="font-size:.75rem" onclick="renameAccount(\''+esc(a.username)+'\')">'+ic('edit',11)+' Rename</button>';
}
if(isAdmin){
actions+='<button class="btn-sm" style="font-size:.75rem" onclick="promptChangePassword(\''+esc(a.username)+'\')">'+ic('key',11)+' Password</button>';
if(!isSelf){
actions+='<button class="btn-sm" style="font-size:.75rem" onclick="toggleAdmin(\''+esc(a.username)+'\','+(a.is_admin?'false':'true')+')">'+ic('shield',11)+' '+(a.is_admin?'Demote':'Promote')+'</button>';
actions+='<button class="btn-sm" style="color:#f87171;background:rgba(248,113,113,.08);border-color:rgba(248,113,113,.25);font-size:.75rem" onclick="deleteAccount(\''+esc(a.username)+'\')">'+ic('trash',11)+' Remove</button>';
}
}
html+='<div style="display:flex;align-items:center;justify-content:space-between;padding:.5rem 0;border-bottom:1px solid var(--border);gap:.5rem;flex-wrap:wrap">'+
'<div style="display:flex;align-items:center"><span style="font-family:var(--mono);font-size:.875rem">'+esc(a.username)+'</span>'+adminBadge+'</div>'+
'<div style="display:flex;gap:.375rem;flex-wrap:wrap">'+actions+'</div>'+
'</div>';
}
list.innerHTML=html;
}).catch(function(){});}
window.createAccount=function(){
var u=(document.getElementById('newUsername')||{}).value||'';
var p=(document.getElementById('newPassword')||{}).value||'';
var adm=document.getElementById('newIsAdmin');
var isAdmin=adm?adm.classList.contains('active'):false;
if(!u.trim()||!p.trim()){toast('Username and password required','error');return;}
if(p.length<8){toast('Password must be at least 8 characters','error');return;}
api('/api/auth/accounts',{method:'POST',body:{username:u.trim(),password:p,is_admin:isAdmin}}).then(function(d){
if(d&&d.token){
localStorage.setItem('avacli_token',d.token);
S.user={username:d.username,is_admin:d.is_admin};
localStorage.setItem('avacli_user',JSON.stringify(S.user));
toast('Admin account created, logged in!','success');
render();return;
}
toast('Account created','success');
var ui=document.getElementById('newUsername'),pi=document.getElementById('newPassword');
if(ui)ui.value='';if(pi)pi.value='';
if(adm){adm.classList.remove('active');adm.style.borderColor='rgba(255,255,255,.1)';adm.style.background='rgba(255,255,255,.04)';adm.style.color='var(--text-muted)';adm.style.boxShadow='none';}
loadAccounts();
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});};
window.deleteAccount=function(username){
avaConfirm('Remove account "'+username+'"?',function(){
api('/api/auth/accounts/'+encodeURIComponent(username),{method:'DELETE'}).then(function(){
toast('Account removed','success');loadAccounts();
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});
},{title:'Remove Account',okText:'Remove'});};
window.promptChangePassword=function(username){
avaPrompt('Enter new password for "'+username+'" (min 8 characters):','',function(newPw){
if(!newPw)return;
if(newPw.length<8){toast('Password must be at least 8 characters','error');return;}
api('/api/auth/accounts/update',{method:'POST',body:{username:username,password:newPw}}).then(function(){
toast('Password updated for '+username,'success');
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});
},{title:'Change Password',okText:'Update Password',inputType:'password'});};
window.toggleAdmin=function(username,makeAdmin){
api('/api/auth/accounts/update',{method:'POST',body:{username:username,is_admin:makeAdmin}}).then(function(){
toast(username+' is now '+(makeAdmin?'an admin':'a regular user'),'success');loadAccounts();
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});};
window.renameAccount=function(username){
avaPrompt('New name for "'+username+'"',username,function(newName){
if(!newName||newName.trim().length<2){toast('Username must be at least 2 characters','error');return;}
api('/api/auth/accounts/update',{method:'POST',body:{username:username,new_username:newName.trim()}}).then(function(){
toast('Renamed "'+username+'" to "'+newName.trim()+'"','success');
if(S.user&&S.user.username===username){S.user.username=newName.trim();localStorage.setItem('avacli_user',JSON.stringify(S.user));}
loadAccounts();
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});
},{title:'Rename Account',okText:'Rename'});
};
window.changeMyPassword=function(){
var cur=(document.getElementById('myCurrentPw')||{}).value||'';
var nw=(document.getElementById('myNewPw')||{}).value||'';
if(!cur||!nw){toast('Both current and new password required','error');return;}
if(nw.length<8){toast('New password must be at least 8 characters','error');return;}
api('/api/auth/change-password',{method:'POST',body:{current_password:cur,new_password:nw}}).then(function(){
toast('Password changed successfully','success');
var c=document.getElementById('myCurrentPw'),n=document.getElementById('myNewPw');
if(c)c.value='';if(n)n.value='';
}).catch(function(e){toast('Error: '+(e.message||'failed'),'error');});};

/* ── Browse Workspace ──────────────────────────────────── */
window.browseWorkspace=function(){
if(typeof showDirectoryPicker!=='undefined'){
showDirectoryPicker().then(function(dir){
var inp=document.getElementById('workspaceInput');
// File System Access API gives us the name but not full path
// We can only use the name; user must confirm the full path
if(inp)inp.value=dir.name;
toast('Selected: '+dir.name+' — verify the full path is correct','info');
}).catch(function(){});
}else{
toast('Directory browsing is not supported in this browser — please type the path manually','info');
}};

window.saveSettings=function(){
var wsInput=document.getElementById('workspaceInput');
var newWs=wsInput?wsInput.value.trim():'';

var body={model:S.model};

var promises=[];
promises.push(api('/api/settings',{method:'POST',body:body}));
if(newWs&&S.status&&newWs!==S.status.workspace){
promises.push(api('/api/settings/workspace',{method:'POST',body:{workspace:newWs}}));
}
Promise.all(promises).then(function(results){
toast('Settings saved','success');
if(results[1]&&results[1].workspace){
S.status.workspace=results[1].workspace;
}
if(results[0]){
if(results[0].settings){
if(typeof results[0].settings.has_xai_key==='boolean')S.hasXaiKey=results[0].settings.has_xai_key;
}
}
return api('/api/status');
}).then(function(st){
if(st){
S.hasXaiKey=!!st.has_xai_key;
}
render();
}).catch(function(){});
};

/* ── Knowledge Base Page ──────────────────────────────── */
var kbState={articles:null,search:'',viewingId:null,viewingArticle:null};

function renderKnowledge(){
if(kbState.viewingId)return renderArticleView();
if(!kbState.articles){loadArticles();return '<div class="usage-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';}
var searchBar='<div style="display:flex;gap:.5rem;align-items:center;margin-bottom:1rem">'+
'<input id="kbSearchInput" type="text" placeholder="Search articles..." value="'+esc(kbState.search||'')+'" style="flex:1;background:var(--bg-elevated);border:1px solid var(--border);border-radius:var(--radius-sm);color:var(--text-primary);padding:.5rem .75rem;font-size:.875rem;font-family:var(--font);outline:none" onkeydown="if(event.key===\'Enter\')doKbSearch()">'+
'<button class="btn-sm" onclick="doKbSearch()">'+ic('search',14)+' Search</button>'+
(kbState.search?'<button class="btn-sm" onclick="clearKbSearch()">'+ic('xic',12)+' Clear</button>':'')+
'<button class="btn-sm" onclick="refreshArticles()">'+ic('refresh',14)+'</button>'+
'</div>';

var grid='';
if(!kbState.articles.length){
grid='<div class="empty" style="padding:3rem;text-align:center">'+ic('book',40)+
'<p style="margin-top:1rem;color:var(--text-secondary)">No articles yet</p>'+
'<p style="font-size:.8125rem;color:var(--text-muted)">Ask the AI to research a topic — articles will appear here.</p></div>';
}else{
grid='<div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:.75rem">';
for(var i=0;i<kbState.articles.length;i++){
var a=kbState.articles[i];
var tags='';
var aTags=a.tags;if(typeof aTags==='string'){try{aTags=JSON.parse(aTags);}catch(e){aTags=[];}}
if(aTags&&aTags.length){for(var t=0;t<Math.min(aTags.length,4);t++)tags+='<span style="font-size:.65rem;background:rgba(124,58,237,.15);color:var(--accent-pale);padding:.0625rem .375rem;border-radius:999px">'+esc(aTags[t])+'</span>';}
grid+='<div class="server-card" style="cursor:pointer" onclick="viewArticle(\''+esc(a.id)+'\')">'+
'<div class="server-card-header"><span class="server-card-name" style="font-size:.9375rem">'+esc(a.title)+'</span></div>'+
'<div class="server-card-details">'+
(a.summary?'<div style="font-size:.8125rem;color:var(--text-secondary);margin-bottom:.5rem;display:-webkit-box;-webkit-line-clamp:3;-webkit-box-orient:vertical;overflow:hidden">'+esc(a.summary)+'</div>':'')+
(tags?'<div style="display:flex;flex-wrap:wrap;gap:.25rem;margin-bottom:.5rem">'+tags+'</div>':'')+
'<div style="font-size:.75rem;color:var(--text-muted)">'+timeAgo(a.created_at||0)+(a.created_by?' &middot; '+esc(a.created_by):'')+'</div>'+
'</div></div>';
}
grid+='</div>';
}

return '<div class="usage-wrap" style="padding:1.5rem">'+
'<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1.25rem">'+
'<h2 style="font-size:1.25rem;color:var(--accent-pale);margin:0">'+ic('book',18)+' Knowledge Base</h2>'+
'<span style="font-size:.8125rem;color:var(--text-muted)">'+kbState.articles.length+' articles</span>'+
'</div>'+searchBar+grid+'</div>';
}

function renderArticleView(){
var a=kbState.viewingArticle;
if(!a){
api('/api/articles/'+encodeURIComponent(kbState.viewingId)).then(function(d){
kbState.viewingArticle=d;render();
}).catch(function(){toast('Failed to load article','error');kbState.viewingId=null;render();});
return '<div class="usage-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';
}
var tags='';
var aTags=a.tags;if(typeof aTags==='string'){try{aTags=JSON.parse(aTags);}catch(e){aTags=[];}}
if(aTags&&aTags.length){for(var t=0;t<aTags.length;t++)tags+='<span style="font-size:.75rem;background:rgba(124,58,237,.15);color:var(--accent-pale);padding:.125rem .5rem;border-radius:999px">'+esc(aTags[t])+'</span>';}

var contentHtml=esc(a.content||'').replace(/\n/g,'<br>');

return '<div class="usage-wrap" style="padding:1.5rem;max-width:800px;margin:0 auto">'+
'<div style="margin-bottom:1.25rem">'+
'<button class="btn-sm" onclick="kbState.viewingId=null;kbState.viewingArticle=null;render()">'+ic('chevLeft',14)+' Back</button>'+
'</div>'+
'<h1 style="font-size:1.5rem;margin:0 0 .5rem">'+esc(a.title)+'</h1>'+
'<div style="font-size:.8125rem;color:var(--text-muted);margin-bottom:.75rem">'+
timeAgo(a.created_at||0)+(a.created_by?' &middot; by '+esc(a.created_by):'')+
(a.source_session?' &middot; session: '+esc(a.source_session):'')+
'</div>'+
(tags?'<div style="display:flex;flex-wrap:wrap;gap:.25rem;margin-bottom:1.25rem">'+tags+'</div>':'')+
(a.summary?'<div style="padding:.75rem 1rem;background:rgba(124,58,237,.05);border:1px solid var(--border-accent);border-radius:var(--radius-sm);font-size:.875rem;color:var(--text-secondary);margin-bottom:1.5rem;line-height:1.6">'+esc(a.summary)+'</div>':'')+
'<div class="article-content" style="font-size:.9375rem;line-height:1.8;color:var(--text-primary);white-space:pre-wrap;word-break:break-word">'+contentHtml+'</div>'+
'<div style="margin-top:2rem;padding-top:1rem;border-top:1px solid var(--border);display:flex;gap:.5rem">'+
'<button class="btn-sm" style="color:#f87171" onclick="deleteArticle(\''+esc(a.id)+'\')">'+ic('trash',14)+' Delete</button>'+
'</div></div>';
}

function loadArticles(){
var url='/api/articles';
if(kbState.search)url+='?q='+encodeURIComponent(kbState.search);
api(url,{silent:true}).then(function(d){
kbState.articles=d&&d.articles?d.articles:[];render();
}).catch(function(){kbState.articles=[];render();});
}
window.viewArticle=function(id){kbState.viewingId=id;kbState.viewingArticle=null;render();};
window.doKbSearch=function(){var inp=document.getElementById('kbSearchInput');kbState.search=inp?inp.value.trim():'';kbState.articles=null;render();};
window.clearKbSearch=function(){kbState.search='';kbState.articles=null;render();};
window.refreshArticles=function(){kbState.articles=null;render();};
window.deleteArticle=function(id){
avaConfirm('Delete this article permanently?',function(){
api('/api/articles/'+encodeURIComponent(id),{method:'DELETE'}).then(function(){
toast('Article deleted','success');kbState.viewingId=null;kbState.viewingArticle=null;kbState.articles=null;render();
}).catch(function(e){toast('Delete failed: '+(e.message||'error'),'error');});
},{title:'Delete Article',okText:'Delete'});
};

/* ── Apps Page (App Drawer) ────────────────────────────── */
var appsState={apps:null,ctxApp:null,longTimer:null};
var appPalette=['#7c3aed','#2563eb','#059669','#d97706','#dc2626','#6d28d9','#0891b2','#be185d','#4f46e5','#0d9488','#ea580c','#7c2d12'];
function appColor(n){var h=0;for(var i=0;i<n.length;i++)h=n.charCodeAt(i)+((h<<5)-h);return appPalette[Math.abs(h)%appPalette.length];}

function renderApps(){
if(!appsState.apps){loadApps();return '<div class="usage-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';}

var grid='';
if(!appsState.apps.length){
grid='<div class="empty" style="padding:3rem;text-align:center">'+ic('grid',48)+
'<p style="margin-top:1rem;color:var(--text-secondary)">No apps yet</p>'+
'<p style="font-size:.8125rem;color:var(--text-muted)">Ask the AI to create an app, or tap + to create one.</p></div>';
}else{
grid='<div class="app-drawer">';
for(var i=0;i<appsState.apps.length;i++){
var a=appsState.apps[i];
var col=appColor(a.name||'');
var ini=(a.name||'A').charAt(0).toUpperCase();
var iconHtml=a.icon_url
  ?'<img class="app-drawer-icon-img" src="'+esc(a.icon_url)+'" alt="">'
  :'<div class="app-drawer-icon-letter" style="background:linear-gradient(135deg,'+col+','+col+'99)">'+ini+'</div>';
grid+='<div class="app-drawer-item" data-appid="'+esc(a.id)+'" data-slug="'+esc(a.slug)+'"'+
' oncontextmenu="appCtx(event,\''+esc(a.id)+'\')"'+
' ontouchstart="appLongStart(event,\''+esc(a.id)+'\')"'+
' ontouchend="appLongEnd()" ontouchmove="appLongEnd()"'+
' onclick="openAppLink(\''+esc(a.slug)+'\')">'+
'<div class="app-drawer-icon">'+iconHtml+'</div>'+
'<div class="app-drawer-label">'+esc(a.name)+'</div></div>';
}
grid+='</div>';
}

return '<div class="usage-wrap" style="padding:1.5rem">'+
'<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem">'+
'<h2 style="font-size:1.25rem;color:var(--accent-pale);margin:0">'+ic('grid',18)+' Apps</h2>'+
'<button class="btn-deploy" onclick="createNewApp()">'+ic('plus',14)+' New App</button>'+
'</div>'+grid+
'<div id="appCtxMenu" class="app-ctx-menu" style="display:none"></div></div>';
}

function loadApps(){
api('/api/apps',{silent:true}).then(function(d){
appsState.apps=d&&d.apps?d.apps:[];render();
}).catch(function(){appsState.apps=[];render();});
}

window.openAppLink=function(slug){window.open('/apps/'+encodeURIComponent(slug)+'/','_blank');};

/* ── Context Menu (right-click desktop / long-press mobile) ── */
window.appCtx=function(e,id){e.preventDefault();e.stopPropagation();showAppMenu(id,e.clientX,e.clientY);};
window.appLongStart=function(e,id){
appsState.longTimer=setTimeout(function(){
var t=e.touches&&e.touches[0];
if(t)showAppMenu(id,t.clientX,t.clientY);
},500);
};
window.appLongEnd=function(){if(appsState.longTimer){clearTimeout(appsState.longTimer);appsState.longTimer=null;}};

function showAppMenu(id,x,y){
var app=null;
for(var i=0;i<(appsState.apps||[]).length;i++){if(appsState.apps[i].id===id){app=appsState.apps[i];break;}}
if(!app)return;
appsState.ctxApp=app;
var m=document.getElementById('appCtxMenu');if(!m)return;

var safeName=esc(app.name).replace(/'/g,"\\'");
var items=[
{ic:'globe',l:'Open',fn:'openAppLink(\''+esc(app.slug)+'\')'},
{ic:'code',l:'Edit in Files',fn:'editAppFiles(\''+esc(app.id)+'\',\''+esc(app.slug)+'\')'},
{sep:true},
{ic:'edit',l:'Rename',fn:'renameApp(\''+esc(app.id)+'\',\''+safeName+'\')'},
{ic:'image',l:'Generate Icon',fn:'genAppIcon(\''+esc(app.id)+'\',\''+safeName+'\')'},
{sep:true},
{ic:'trash',l:'Delete',fn:'deleteApp(\''+esc(app.id)+'\')',danger:true}
];
var html='';
for(var i=0;i<items.length;i++){
if(items[i].sep){html+='<div class="app-ctx-sep"></div>';continue;}
html+='<div class="app-ctx-item'+(items[i].danger?' danger':'')+'" onclick="'+items[i].fn+';hideAppMenu()">'+ic(items[i].ic,14)+' '+items[i].l+'</div>';
}
m.innerHTML=html;m.style.display='block';
var vw=window.innerWidth,vh=window.innerHeight;
m.style.left=Math.min(x,vw-190)+'px';
m.style.top=Math.min(y,vh-m.offsetHeight-10)+'px';
setTimeout(function(){document.addEventListener('click',hideAppMenu,{once:true});document.addEventListener('touchstart',hideAppMenu,{once:true});},20);
}
window.hideAppMenu=function(){var m=document.getElementById('appCtxMenu');if(m)m.style.display='none';};

window.renameApp=function(id,cur){
avaPrompt('New name:',cur,function(n){
if(!n||!n.trim()||n.trim()===cur)return;
api('/api/apps/'+encodeURIComponent(id),{method:'PUT',body:{name:n.trim()}}).then(function(){
toast('Renamed','success');appsState.apps=null;render();
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
},{title:'Rename App',okText:'Rename'});
};

window.genAppIcon=function(id,name){
avaPrompt('Describe the icon you want:','A modern minimalist app icon for '+name,function(desc){
if(!desc||!desc.trim())return;
toast('Generating icon...','info');
api('/api/apps/'+encodeURIComponent(id)+'/generate-icon',{method:'POST',body:{prompt:desc.trim(),model:'grok-2-image',size:'1024x1024'}}).then(function(d){
if(d&&d.icon_url){toast('Icon set!','success');appsState.apps=null;render();}
else toast('No image returned','error');
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
},{title:'Generate App Icon',okText:'Generate'});
};

window.editAppFiles=function(id,slug){
toast('Exporting to workspace...','info');
api('/api/apps/'+encodeURIComponent(id)+'/export',{method:'POST'}).then(function(d){
if(d&&(d.path||d.exported)){
dirCache={};
window._pendingAppDir=d.path||('apps/'+slug);
navigate('/files');
toast('App files in '+(d.path||('apps/'+slug))+'/','success');
}else{
toast('Export returned no path','error');
}
}).catch(function(e){toast('Export failed: '+(e.message||'error'),'error');});
};

window.createNewApp=function(){
avaPrompt('App name:','',function(name){
if(!name||!name.trim())return;
api('/api/apps',{method:'POST',body:{name:name.trim()}}).then(function(d){
toast('App created','success');appsState.apps=null;render();
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
},{title:'New App',okText:'Create'});
};

window.deleteApp=function(id){
avaConfirm('Delete this app and all its files?',function(){
api('/api/apps/'+encodeURIComponent(id),{method:'DELETE'}).then(function(){
toast('App deleted','success');appsState.apps=null;render();
}).catch(function(e){toast('Delete failed: '+(e.message||'error'),'error');});
},{title:'Delete App',okText:'Delete'});
};

/* ── Services Page ────────────────────────────────────── */
var svcState={services:null,viewingLogs:null,logs:null};

function renderServices(){
if(!svcState.services){loadServices();return '<div class="usage-wrap" style="display:flex;justify-content:center;padding:3rem"><div class="spinner"></div></div>';}

var grid='';
if(!svcState.services.length){
grid='<div class="empty" style="padding:3rem;text-align:center">'+ic('activity',40)+
'<p style="margin-top:1rem;color:var(--text-secondary)">No services yet</p>'+
'<p style="font-size:.8125rem;color:var(--text-muted)">Ask the AI to create a service, or create one manually. Services run background tasks like RSS feeds, bots, and scheduled prompts.</p></div>';
}else{
grid='<div style="display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:.75rem">';
for(var i=0;i<svcState.services.length;i++){
var s=svcState.services[i];
var live=s.live_status||s.status;
var stColor=live==='running'?'#4ade80':(live==='error'?'#f87171':'var(--text-muted)');
var typeLabel={'rss_feed':'RSS Feed','scheduled_prompt':'Scheduled Prompt','custom_script':'Script','bot':'Bot','custom':'Custom'}[s.type]||s.type;
grid+='<div class="server-card">'+
'<div class="server-card-header"><span class="server-card-name">'+esc(s.name)+'</span>'+
'<div class="server-card-status"><span class="status-dot" style="background:'+stColor+'"></span> '+esc(live)+'</div></div>'+
'<div class="server-card-details">'+
'<div style="font-size:.75rem;color:var(--accent-pale);margin-bottom:.25rem">'+esc(typeLabel)+'</div>'+
(s.description?'<div style="font-size:.8125rem;color:var(--text-secondary);margin-bottom:.5rem">'+esc(s.description)+'</div>':'')+
(s.last_run?'<div style="font-size:.75rem;color:var(--text-muted)">Last run: '+timeAgo(s.last_run)+'</div>':'')+
'</div>'+
'<div class="server-card-actions">'+
(live==='running'
?'<button onclick="stopService(\''+esc(s.id)+'\')">'+ic('pause',14)+' Stop</button>'
:'<button onclick="startService(\''+esc(s.id)+'\')">'+ic('play',14)+' Start</button>')+
'<button onclick="viewServiceLogs(\''+esc(s.id)+'\')">'+ic('log',14)+' Logs</button>'+
'<button onclick="deleteService(\''+esc(s.id)+'\')" style="color:#f87171">'+ic('trash',14)+' Delete</button>'+
'</div></div>';
}
grid+='</div>';
}

var logPanel='';
if(svcState.viewingLogs){
var logEntries='';
if(!svcState.logs){logEntries='<div style="text-align:center;padding:1rem"><div class="spinner"></div></div>';}
else if(!svcState.logs.length){logEntries='<div style="text-align:center;padding:1rem;color:var(--text-muted);font-size:.8125rem">No logs yet</div>';}
else{
for(var li=0;li<svcState.logs.length;li++){
var l=svcState.logs[li];
var lc={info:'#4ade80',warn:'#fbbf24',error:'#f87171',debug:'rgba(255,255,255,0.3)'}[l.level]||'var(--text-muted)';
logEntries+='<div style="font-size:.75rem;font-family:var(--mono);padding:.25rem 0;border-bottom:1px solid var(--border)">'+
'<span style="color:var(--text-muted)">'+new Date(l.timestamp).toLocaleTimeString()+'</span> '+
'<span style="color:'+lc+';text-transform:uppercase;font-size:.625rem">'+esc(l.level)+'</span> '+
'<span style="color:var(--text-primary)">'+esc(l.message)+'</span></div>';
}}
logPanel='<div style="margin-top:1.5rem;background:var(--bg-surface);border:1px solid var(--border);border-radius:var(--radius-sm);overflow:hidden">'+
'<div style="display:flex;align-items:center;justify-content:space-between;padding:.625rem .75rem;border-bottom:1px solid var(--border)">'+
'<span style="font-size:.875rem;font-weight:600">Service Logs</span>'+
'<button class="btn-sm" onclick="svcState.viewingLogs=null;svcState.logs=null;render()">'+ic('xic',12)+' Close</button></div>'+
'<div style="max-height:300px;overflow-y:auto;padding:.5rem .75rem">'+logEntries+'</div></div>';
}

return '<div class="usage-wrap" style="padding:1.5rem">'+
'<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1.25rem">'+
'<h2 style="font-size:1.25rem;color:var(--accent-pale);margin:0">'+ic('activity',18)+' Services</h2>'+
'<div style="display:flex;gap:.5rem">'+
'<button class="btn-sm" onclick="refreshServices()">'+ic('refresh',14)+'</button>'+
'<button class="btn-deploy" onclick="createNewService()">'+ic('plus',14)+' New Service</button>'+
'</div></div>'+grid+logPanel+'</div>';
}

function loadServices(){
api('/api/services',{silent:true}).then(function(d){
svcState.services=d&&d.services?d.services:[];render();
}).catch(function(){svcState.services=[];render();});
}
window.refreshServices=function(){svcState.services=null;render();};
window.startService=function(id){
api('/api/services/'+encodeURIComponent(id)+'/start',{method:'POST'}).then(function(){
toast('Service started','success');svcState.services=null;render();
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
};
window.stopService=function(id){
api('/api/services/'+encodeURIComponent(id)+'/stop',{method:'POST'}).then(function(){
toast('Service stopped','success');svcState.services=null;render();
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
};
window.viewServiceLogs=function(id){
svcState.viewingLogs=id;svcState.logs=null;render();
api('/api/services/'+encodeURIComponent(id)+'/logs',{silent:true}).then(function(d){
svcState.logs=d&&d.logs?d.logs:[];render();
}).catch(function(){svcState.logs=[];render();});
};
window.deleteService=function(id){
avaConfirm('Delete this service?',function(){
api('/api/services/'+encodeURIComponent(id),{method:'DELETE'}).then(function(){
toast('Service deleted','success');svcState.services=null;render();
}).catch(function(e){toast('Delete failed: '+(e.message||'error'),'error');});
},{title:'Delete Service',okText:'Delete'});
};
window.createNewService=function(){
avaPrompt('Service name:','',function(name){
if(!name||!name.trim())return;
api('/api/services',{method:'POST',body:{name:name.trim(),type:'custom'}}).then(function(){
toast('Service created','success');svcState.services=null;render();
}).catch(function(e){toast('Failed: '+(e.message||'error'),'error');});
},{title:'New Service',okText:'Create'});
};

/* ── Servers Page (Vultr VPS) ─────────────────────────── */
var deployState={plans:null,regions:null,os:null,selectedPlan:null,selectedRegion:'ewr',selectedConfig:'agent-only',label:'',deploying:false};

function renderServers(){
return '<div class="servers-wrap">'+
'<div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:1.5rem;flex-wrap:wrap;gap:.75rem">'+
'<h2 style="font-size:1.25rem;color:var(--accent-pale);margin:0">'+ic('server',18)+' VPS Management</h2>'+
'<div style="display:flex;gap:.5rem;flex-wrap:wrap">'+
'<button class="btn-sm" onclick="showAddNodeModal()">'+ic('plus',14)+' Add Node</button>'+
(S.vultrKeyConfigured
? '<button class="btn-deploy" onclick="showDeployModal()">'+ic('plus',16)+' Deploy Instance</button>'
: '<button class="btn-deploy" style="opacity:.45;cursor:not-allowed" onclick="toast(\'Set your Vultr API key in Settings first\',\'error\')">'+ic('plus',16)+' Deploy Instance</button>')+
'</div>'+
'</div>'+
'<div class="deploy-section-title">Vultr Account</div>'+
'<div id="vultrAccountPanel"><div style="display:flex;justify-content:center;padding:1rem"><div class="spinner"></div></div></div>'+
'<div style="margin-bottom:1.5rem"><div class="deploy-section-title">This Node</div>'+
'<div class="server-card" style="border-color:var(--border-accent)">'+
'<div class="server-card-header"><span class="server-card-name">'+ic('dot',10)+' Local</span>'+
'<div class="server-card-status"><span class="status-dot online"></span> running</div></div>'+
'<div class="server-card-details">'+
'<div>Workspace: '+esc(S.status?S.status.workspace:'--')+'</div>'+
'<div>Model: '+esc(S.model||'--')+'</div>'+
'<div>Port: '+((S.status&&S.status.port)?S.status.port:'--')+'</div>'+
'</div></div></div>'+
'<div class="deploy-section-title">Vultr Instances</div>'+
'<div id="serversList"><div style="display:flex;justify-content:center;padding:2rem"><div class="spinner"></div></div></div>'+
'<div id="nodeModalRoot"></div>'+
'<div id="deployModalRoot"></div>'+
'<div class="deploy-section-title" style="margin-top:1.5rem">Connected Nodes</div>'+
'<div id="nodesList"><div style="display:flex;justify-content:center;padding:2rem"><div class="spinner"></div></div></div>'+
'</div>';
}

var vultrAccountCache=null;
function loadVultrAccountPanel(){
var el=document.getElementById('vultrAccountPanel');if(!el)return;
if(!S.vultrKeyConfigured){
el.innerHTML='<div style="padding:1rem;color:var(--text-muted);font-size:.875rem">Add your Vultr API key in Settings to load balance, billing, and bandwidth.</div>';
return;
}
el.innerHTML='<div style="display:flex;justify-content:center;padding:1rem"><div class="spinner"></div></div>';
api('/api/vultr/account',{silent:true}).then(function(d){
vultrAccountCache=d;
var rawAcct=d&&d.account?d.account:{};
var acct=rawAcct.account?rawAcct.account:(rawAcct.balance!=null||rawAcct.name?rawAcct:{});
var bw=d&&d.bandwidth&&d.bandwidth.bandwidth?d.bandwidth.bandwidth:null;
var pend=d&&d.pending_charges;
var name=acct.name||'\u2014';
var email=acct.email||'\u2014';
var bal=(typeof acct.balance==='number')?acct.balance:null;
var pendAmt=(typeof acct.pending_charges==='number')?acct.pending_charges:null;
var fmt=function(n){if(typeof n!=='number'||!isFinite(n))return '\u2014';return n.toFixed(2);};
var balColor=bal===null?'var(--text-primary)':(bal>=0?'#4ade80':(bal>-10?'#fbbf24':'#f87171'));
var html='<div style="display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:.75rem;margin-bottom:1rem">';
html+='<div class="server-card" style="text-align:center;padding:1.25rem"><div style="font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:var(--text-muted);margin-bottom:.25rem">Balance</div>';
html+='<div style="font-size:1.75rem;font-weight:700;font-family:var(--mono);color:'+balColor+'">$'+esc(fmt(bal))+'</div>';
if(pendAmt!==null)html+='<div style="font-size:.75rem;color:var(--text-muted);margin-top:.25rem">Pending: $'+esc(fmt(pendAmt))+'</div>';
html+='</div>';
html+='<div class="server-card" style="text-align:center;padding:1.25rem"><div style="font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:var(--text-muted);margin-bottom:.25rem">Account</div>';
html+='<div style="font-size:.9375rem;font-weight:600">'+esc(name)+'</div>';
html+='<div style="font-size:.75rem;color:var(--text-muted);margin-top:.25rem">'+esc(email)+'</div>';
html+='</div>';
if(bw&&bw.current_month_to_date){
var cm=bw.current_month_to_date;
var gbIn=cm.gb_in!=null?cm.gb_in:0;
var gbOut=cm.gb_out!=null?cm.gb_out:0;
var instances=cm.total_instance_count!=null?cm.total_instance_count:0;
html+='<div class="server-card" style="text-align:center;padding:1.25rem"><div style="font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:var(--text-muted);margin-bottom:.25rem">Bandwidth (this month)</div>';
html+='<div style="display:flex;justify-content:center;gap:1.5rem;margin-top:.375rem">';
html+='<div><div style="font-size:1.125rem;font-weight:600;font-family:var(--mono);color:#38bdf8">'+esc(String(gbIn.toFixed?gbIn.toFixed(1):gbIn))+'</div><div style="font-size:.625rem;color:var(--text-muted)">GB In</div></div>';
html+='<div><div style="font-size:1.125rem;font-weight:600;font-family:var(--mono);color:#a78bfa">'+esc(String(gbOut.toFixed?gbOut.toFixed(1):gbOut))+'</div><div style="font-size:.625rem;color:var(--text-muted)">GB Out</div></div>';
html+='<div><div style="font-size:1.125rem;font-weight:600;font-family:var(--mono);color:#4ade80">'+instances+'</div><div style="font-size:.625rem;color:var(--text-muted)">Instances</div></div>';
html+='</div></div>';
}
if(pend&&pend.pending_charges&&pend.pending_charges.length){
html+='<div class="server-card" style="text-align:center;padding:1.25rem"><div style="font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:var(--text-muted);margin-bottom:.25rem">Billing Lines</div>';
html+='<div style="font-size:1.125rem;font-weight:600;font-family:var(--mono)">'+pend.pending_charges.length+'</div>';
html+='<div style="font-size:.75rem;color:var(--text-muted);margin-top:.25rem">pending charges</div>';
html+='</div>';
}
html+='</div>';
html+='<div style="display:flex;gap:.5rem;flex-wrap:wrap;margin-bottom:1rem">';
html+='<a href="https://my.vultr.com/" target="_blank" rel="noopener" class="btn-sm" style="text-decoration:none">'+ic('globe',12)+' Vultr Console</a>';
html+='<a href="https://my.vultr.com/billing/" target="_blank" rel="noopener" class="btn-sm" style="text-decoration:none">'+ic('activity',12)+' Billing</a>';
html+='<a href="https://my.vultr.com/billing/#/addFunds" target="_blank" rel="noopener" class="btn-sm" style="text-decoration:none;color:#4ade80">'+ic('plus',12)+' Top Up</a>';
html+='</div>';
el.innerHTML=html;
}).catch(function(e){
el.innerHTML='<div style="color:#f87171;padding:1rem;font-size:.875rem">'+ic('xic',14)+' '+esc(e.message||'Could not load Vultr account')+' &mdash; check your Vultr API key in Settings.</div>';
});
}

function loadServers(){
api('/api/vultr/key/status',{silent:true}).then(function(d){
S.vultrKeyConfigured=!!(d&&d.configured);
var btn=document.querySelector('.btn-deploy');
if(btn){
if(S.vultrKeyConfigured){
btn.disabled=false;btn.style.opacity='';btn.style.cursor='';
btn.onclick=function(){showDeployModal();};
}else{
btn.disabled=false;btn.style.opacity='.45';btn.style.cursor='not-allowed';
btn.onclick=function(){toast('Set your Vultr API key in Settings first','error');};
}
}
loadVultrAccountPanel();
}).catch(function(){loadVultrAccountPanel();});
loadNodes();
api('/api/vultr/instances',{silent:true}).then(function(data){
var list=document.getElementById('serversList');if(!list)return;
var instances=(data&&data.instances)?data.instances:[];
if(!instances.length){
list.innerHTML='<div class="empty" style="padding:2rem">'+ic('server',36)+'<p>No Vultr instances</p><p style="font-size:.8125rem">Add your Vultr API key in Settings, then deploy instances here.</p></div>';
return;
}
var html='<div class="server-grid">';
for(var i=0;i<instances.length;i++){
var s=instances[i];
var stColor=s.status==='active'?'#4ade80':(s.power_status==='running'?'#4ade80':'#f87171');
var ipDisplay=s.main_ip&&s.main_ip!=='0.0.0.0'?s.main_ip:'Provisioning...';
html+='<div class="server-card">'+
'<div class="server-card-header"><span class="server-card-name">'+esc(s.label||s.hostname||'Instance')+'</span>'+
'<div class="server-card-status"><span class="status-dot" style="background:'+stColor+'"></span> '+esc(s.status||'unknown')+'</div></div>'+
'<div class="server-card-details">'+
'<div>IP: <span style="font-family:var(--mono)">'+esc(ipDisplay)+'</span></div>'+
'<div>Region: '+esc(s.region||'--')+'</div>'+
'<div>Plan: '+esc(s.plan||'--')+'</div>'+
'<div>OS: '+esc(s.os||'--')+'</div>'+
'<div>RAM: '+(s.ram?Math.round(s.ram/1024)+'GB':'--')+'</div>'+
'<div>vCPUs: '+(s.vcpu_count||'--')+'</div>'+
'</div>'+
'<div class="server-card-actions">'+
'<button onclick="window.open(\'http://'+esc(s.main_ip||'')+'\',\'_blank\')">'+ic('globe',14)+' Open</button>'+
'<button onclick="destroyInstance(\''+esc(s.id)+'\')" style="color:#f87171">'+ic('trash',14)+' Destroy</button>'+
'</div></div>';
}
html+='</div>';
list.innerHTML=html;
}).catch(function(e){
var list=document.getElementById('serversList');
if(list)list.innerHTML='<div style="color:var(--text-muted);text-align:center;padding:2rem">'+esc(e.message||'Failed to load instances. Check your Vultr API key in Settings.')+'</div>';
});
}

window.showDeployModal=function(){
deployState.plans=null;deployState.regions=null;deployState.os=null;deployState.balance=null;
deployState.selectedPlan=null;deployState.selectedRegion='ewr';deployState.selectedConfig='agent-only';deployState.label='';deployState.deploying=false;
renderDeployModal();
if(vultrAccountCache&&vultrAccountCache.account&&vultrAccountCache.account.account){
var a=vultrAccountCache.account.account;
deployState.balance={balance:a.balance,pending:a.pending_charges};renderDeployModal();
}else{
api('/api/vultr/account',{silent:true}).then(function(d){
var a=d&&d.account&&d.account.account?d.account.account:{};
deployState.balance={balance:a.balance,pending:a.pending_charges};renderDeployModal();
}).catch(function(){});
}
api('/api/vultr/plans',{silent:true}).then(function(d){deployState.plans=(d&&d.plans)?d.plans:[];renderDeployModal();}).catch(function(){deployState.plans=[];renderDeployModal();});
api('/api/vultr/regions',{silent:true}).then(function(d){deployState.regions=(d&&d.regions)?d.regions:[];renderDeployModal();}).catch(function(){deployState.regions=[];renderDeployModal();});
api('/api/vultr/os',{silent:true}).then(function(d){deployState.os=(d&&d.os)?d.os:[];renderDeployModal();}).catch(function(){deployState.os=[];renderDeployModal();});
};

window.closeDeployModal=function(){
var root=document.getElementById('deployModalRoot');
if(root)root.innerHTML='';
};

window.selectDeployPlan=function(id){deployState.selectedPlan=id;renderDeployModal();};
window.selectDeployRegion=function(id){deployState.selectedRegion=id;renderDeployModal();};
window.selectDeployConfig=function(id){deployState.selectedConfig=id;renderDeployModal();};

function renderDeployModal(){
var root=document.getElementById('deployModalRoot');if(!root)return;
var loading=!deployState.plans||!deployState.regions;

var regionFlags={ewr:'\uD83C\uDDFA\uD83C\uDDF8',lax:'\uD83C\uDDFA\uD83C\uDDF8',ord:'\uD83C\uDDFA\uD83C\uDDF8',dfw:'\uD83C\uDDFA\uD83C\uDDF8',atl:'\uD83C\uDDFA\uD83C\uDDF8',sea:'\uD83C\uDDFA\uD83C\uDDF8',mia:'\uD83C\uDDFA\uD83C\uDDF8',sjc:'\uD83C\uDDFA\uD83C\uDDF8',hnl:'\uD83C\uDDFA\uD83C\uDDF8',ams:'\uD83C\uDDF3\uD83C\uDDF1',lhr:'\uD83C\uDDEC\uD83C\uDDE7',fra:'\uD83C\uDDE9\uD83C\uDDEA',cdg:'\uD83C\uDDEB\uD83C\uDDF7',sgp:'\uD83C\uDDF8\uD83C\uDDEC',nrt:'\uD83C\uDDEF\uD83C\uDDF5',icn:'\uD83C\uDDF0\uD83C\uDDF7',syd:'\uD83C\uDDE6\uD83C\uDDFA',yto:'\uD83C\uDDE8\uD83C\uDDE6',sao:'\uD83C\uDDE7\uD83C\uDDF7',mex:'\uD83C\uDDF2\uD83C\uDDFD',waw:'\uD83C\uDDF5\uD83C\uDDF1',mad:'\uD83C\uDDEA\uD83C\uDDF8',sto:'\uD83C\uDDF8\uD83C\uDDEA',del:'\uD83C\uDDEE\uD83C\uDDF3',blr:'\uD83C\uDDEE\uD83C\uDDF3',bom:'\uD83C\uDDEE\uD83C\uDDF3',jnb:'\uD83C\uDDFF\uD83C\uDDE6',mel:'\uD83C\uDDE6\uD83C\uDDFA',tlv:'\uD83C\uDDEE\uD83C\uDDF1'};

var plansHtml='';
if(!deployState.plans){
plansHtml='<div style="display:flex;justify-content:center;padding:1.5rem"><div class="spinner"></div></div>';
}else{
var filtered=deployState.plans.filter(function(p){return p.type==='vc2'&&p.locations&&p.locations.length>0;}).sort(function(a,b){return a.monthly_cost-b.monthly_cost;});
if(!filtered.length)filtered=deployState.plans.slice(0,20).sort(function(a,b){return (a.monthly_cost||0)-(b.monthly_cost||0);});
plansHtml='<div class="plan-grid">';
for(var i=0;i<Math.min(filtered.length,12);i++){
var p=filtered[i];
var sel=deployState.selectedPlan===p.id?'selected':'';
var ram=p.ram>=1024?(Math.round(p.ram/1024))+'GB':p.ram+'MB';
plansHtml+='<div class="plan-card '+sel+'" onclick="selectDeployPlan(\''+esc(p.id)+'\')">'+
'<div class="plan-card-name">'+esc(p.id)+'</div>'+
'<div class="plan-card-price">$'+p.monthly_cost+'<span>/mo</span></div>'+
'<div class="plan-card-specs">'+
'<div><span class="spec-val">'+p.vcpu_count+'</span> vCPUs</div>'+
'<div><span class="spec-val">'+ram+'</span> RAM</div>'+
'<div><span class="spec-val">'+p.disk+'</span> GB disk</div>'+
'<div><span class="spec-val">'+p.bandwidth+'</span> TB b/w</div>'+
'</div></div>';
}
plansHtml+='</div>';
}

var regionsHtml='';
if(!deployState.regions){
regionsHtml='<div style="display:flex;justify-content:center;padding:1rem"><div class="spinner"></div></div>';
}else{
regionsHtml='<div class="region-grid">';
var selPlan=null;
if(deployState.selectedPlan&&deployState.plans){for(var pi=0;pi<deployState.plans.length;pi++){if(deployState.plans[pi].id===deployState.selectedPlan){selPlan=deployState.plans[pi];break;}}}
for(var i=0;i<deployState.regions.length;i++){
var r=deployState.regions[i];
var available=!selPlan||!selPlan.locations||(selPlan.locations.indexOf(r.id)>=0);
if(!available)continue;
var sel=deployState.selectedRegion===r.id?'selected':'';
var flag=regionFlags[r.id]||'\uD83C\uDF10';
regionsHtml+='<div class="region-option '+sel+'" onclick="selectDeployRegion(\''+esc(r.id)+'\')">'+
'<span class="region-flag">'+flag+'</span> '+esc(r.city||r.id)+
'</div>';
}
regionsHtml+='</div>';
}

var configsHtml='';
var configs=[
{id:'agent-only',title:'AvalynnAI Client Only',desc:'Minimal install with the AvalynnAI agent pre-configured.',tags:['Ubuntu 24.04','avacli']},
{id:'lamp-stack',title:'Full LAMP Stack',desc:'Web development environment with AvalynnAI client pre-installed.',tags:['Ubuntu 24.04','Apache2','MariaDB','PHP 8.3','Redis','avacli']},
{id:'blank',title:'Blank Server',desc:'Clean Ubuntu 24.04 image. Configure it yourself.',tags:['Ubuntu 24.04']}
];
for(var ci=0;ci<configs.length;ci++){
var c=configs[ci];
var sel=deployState.selectedConfig===c.id?'selected':'';
var tags='';for(var ti=0;ti<c.tags.length;ti++){tags+='<span class="config-tag">'+c.tags[ti]+'</span>';}
configsHtml+='<div class="config-option '+sel+'" onclick="selectDeployConfig(\''+esc(c.id)+'\')">'+
'<div class="config-radio"></div>'+
'<div><div class="config-title">'+c.title+'</div>'+
'<div class="config-desc">'+c.desc+'</div>'+
'<div class="config-tags">'+tags+'</div></div></div>';
}

var labelVal=deployState.label||'';
var canDeploy=deployState.selectedPlan&&deployState.selectedRegion&&!deployState.deploying;

root.innerHTML='<div class="deploy-backdrop" onclick="if(event.target===this)closeDeployModal()">'+
'<div class="deploy-modal">'+
'<div class="deploy-modal-head"><h2>'+ic('server',18)+' Deploy VPS Instance</h2>'+
'<button class="deploy-modal-close" onclick="closeDeployModal()">'+ic('xic',18)+'</button></div>'+
'<div class="deploy-modal-body">'+
(function(){
var b=deployState.balance;if(!b)return '';
var fmt=function(n){return(typeof n==='number'&&isFinite(n))?n.toFixed(2):'\u2014';};
var balColor=(typeof b.balance==='number')?(b.balance>=0?'#4ade80':(b.balance>-10?'#fbbf24':'#f87171')):'var(--text-primary)';
return '<div style="display:flex;align-items:center;justify-content:space-between;padding:.75rem 1rem;background:rgba(255,255,255,.02);border:1px solid var(--border);border-radius:var(--radius-sm);margin-bottom:1rem">'+
'<div style="display:flex;align-items:center;gap:1rem"><div><span style="font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:var(--text-muted)">Balance</span>'+
'<div style="font-size:1.25rem;font-weight:700;font-family:var(--mono);color:'+balColor+'">$'+esc(fmt(b.balance))+'</div></div>'+
(typeof b.pending==='number'?'<div><span style="font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:var(--text-muted)">Pending</span><div style="font-size:.875rem;font-family:var(--mono);color:var(--text-secondary)">$'+esc(fmt(b.pending))+'</div></div>':'')+
'</div>'+
'<a href="https://my.vultr.com/billing/#/addFunds" target="_blank" rel="noopener" class="btn-sm" style="text-decoration:none;font-size:.75rem;color:#4ade80">'+ic('plus',10)+' Top Up</a></div>';
})()+
'<div class="deploy-section"><div class="deploy-section-title">Select Plan</div>'+plansHtml+'</div>'+
'<div class="deploy-section"><div class="deploy-section-title">Deploy Region</div>'+regionsHtml+'</div>'+
'<div class="deploy-section"><div class="deploy-section-title">Server Configuration</div>'+configsHtml+'</div>'+
'<div class="deploy-section"><div class="deploy-section-title">Server Label</div>'+
'<input class="deploy-input" id="deployLabel" type="text" placeholder="my-server" value="'+esc(labelVal)+'" oninput="deployState.label=this.value">'+
'</div></div>'+
'<div class="deploy-modal-foot">'+
'<button class="btn-ghost" onclick="closeDeployModal()">Cancel</button>'+
'<button class="btn-deploy" onclick="doDeploy()"'+(canDeploy?'':' disabled style="opacity:.4;cursor:not-allowed"')+'>'+
(deployState.deploying?'<div class="spinner" style="width:14px;height:14px;border-width:2px"></div> Deploying...':ic('plus',14)+' Deploy Instance')+
'</button></div></div></div>';
}

window.doDeploy=function(){
if(!deployState.selectedPlan||!deployState.selectedRegion){toast('Select a plan and region','error');return;}
deployState.deploying=true;renderDeployModal();
var osId='2136';
if(deployState.os&&deployState.os.length){
for(var i=0;i<deployState.os.length;i++){
var o=deployState.os[i];
if(o.name&&o.name.indexOf('Ubuntu 24.04')>=0&&o.arch==='x64'&&o.family==='ubuntu'){osId=String(o.id);break;}
}
}
api('/api/vultr/instances',{method:'POST',body:{
region:deployState.selectedRegion,
plan:deployState.selectedPlan,
os_id:osId,
label:deployState.label||'avacli-'+deployState.selectedRegion,
deploy_config:deployState.selectedConfig||'blank'
}}).then(function(d){
deployState.deploying=false;
if(d&&d.error){toast(typeof d.error==='string'?d.error:'Deploy failed','error');renderDeployModal();return;}
toast('Instance deployed! It may take a few minutes to provision.','success');
closeDeployModal();loadServers();
}).catch(function(e){
deployState.deploying=false;toast('Deploy failed: '+(e.message||e),'error');renderDeployModal();
});
};

window.destroyInstance=function(id){
avaConfirm('Destroy this Vultr instance? This cannot be undone.',function(){
api('/api/vultr/instances/'+id,{method:'DELETE'}).then(function(){
toast('Instance destroyed','success');loadServers();
}).catch(function(e){toast('Failed: '+(e.message||e),'error');});
},{title:'Destroy Instance',okText:'Destroy',danger:true});
};

/* ── Add Node ───────────────────────────────────────────── */
window.showAddNodeModal=function(){
var root=document.getElementById('nodeModalRoot');if(!root)return;
root.innerHTML=
'<div class="deploy-backdrop" onclick="if(event.target===this)closeAddNodeModal()">'+
'<div class="deploy-modal" style="max-width:480px">'+
'<div class="deploy-modal-head"><h2>'+ic('server',18)+' Add Node</h2>'+
'<button class="deploy-modal-close" onclick="closeAddNodeModal()">'+ic('xic',18)+'</button></div>'+
'<div class="deploy-modal-body">'+
'<div class="deploy-section"><div class="deploy-section-title">Connection</div>'+
'<div style="display:flex;flex-direction:column;gap:.75rem">'+
'<div><label style="font-size:.75rem;color:var(--text-muted);display:block;margin-bottom:.25rem">Label</label><input class="deploy-input" id="nodeLabel" type="text" placeholder="my-server"></div>'+
'<div><label style="font-size:.75rem;color:var(--text-muted);display:block;margin-bottom:.25rem">Host *</label><input class="deploy-input" id="nodeIp" type="text" placeholder="192.168.1.1 or node.example.com"></div>'+
'<div><label style="font-size:.75rem;color:var(--text-muted);display:block;margin-bottom:.25rem">Port (default: 8080)</label><input class="deploy-input" id="nodePort" type="text" placeholder="8080" value="8080"></div>'+
'</div></div>'+
'<div class="deploy-section"><div class="deploy-section-title">Authentication (optional)</div>'+
'<div style="display:flex;flex-direction:column;gap:.75rem">'+
'<div><label style="font-size:.75rem;color:var(--text-muted);display:block;margin-bottom:.25rem">Username</label><input class="deploy-input" id="nodeUsername" type="text" placeholder="admin" autocomplete="off"></div>'+
'<div><label style="font-size:.75rem;color:var(--text-muted);display:block;margin-bottom:.25rem">Password</label><input class="deploy-input" id="nodePassword" type="password" placeholder="password" autocomplete="new-password"></div>'+
'</div></div>'+
'</div>'+
'<div class="deploy-modal-foot">'+
'<button class="btn-ghost" onclick="closeAddNodeModal()">Cancel</button>'+
'<button class="btn-deploy" onclick="submitAddNode()">'+ic('plus',14)+' Add Node</button>'+
'</div></div></div>';
};
window.closeAddNodeModal=function(){var r=document.getElementById('nodeModalRoot');if(r)r.innerHTML='';};
window.submitAddNode=function(){
var ip=(document.getElementById('nodeIp')||{}).value||'';
var port=(document.getElementById('nodePort')||{}).value||'8080';
var label=(document.getElementById('nodeLabel')||{}).value||'';
var username=(document.getElementById('nodeUsername')||{}).value||'';
var password=(document.getElementById('nodePassword')||{}).value||'';
if(!ip.trim()){toast('Host is required','error');return;}
var body={ip:ip.trim(),port:parseInt(port)||8080,label:label.trim()||ip.trim()};
if(username.trim())body.username=username.trim();
if(password)body.password=password;
var btn=document.querySelector('#nodeModalRoot .btn-deploy');
if(btn){btn.disabled=true;btn.innerHTML='<div class="spinner" style="width:14px;height:14px;border-width:2px;display:inline-block;vertical-align:middle"></div> Testing connection...';}
api('/api/nodes',{method:'POST',body:body}).then(function(d){
toast('Node added — connection test in progress','success');closeAddNodeModal();loadNodes();
S.nodesLoaded=false;loadChatNodes();
}).catch(function(e){toast('Failed: '+(e.message||e),'error');if(btn){btn.disabled=false;btn.innerHTML=ic('plus',14)+' Add Node';}});
};
function loadNodes(){
api('/api/nodes',{silent:true}).then(function(data){
var list=document.getElementById('nodesList');if(!list)return;
var nodes=(data&&data.nodes)?data.nodes:[];
if(!nodes.length){
list.innerHTML='<div class="empty" style="padding:2rem">'+ic('server',36)+'<p>No connected nodes</p><p style="font-size:.8125rem">Add nodes via host address to manage them from here.</p></div>';
return;
}
var html='<div class="server-grid">';
for(var i=0;i<nodes.length;i++){
var n=nodes[i];
var st=n.status||'unknown';
var stColor=st==='connected'?'#4ade80':(st==='testing'?'#38bdf8':(st==='auth_failed'?'#fbbf24':'#f87171'));
var stLabel=st==='connected'?'connected':(st==='testing'?'testing...':(st==='auth_failed'?'auth failed':(st==='provisioning'?'provisioning':(st==='not_avacli'?'not avacli':'unreachable'))));
html+='<div class="server-card">'+
'<div class="server-card-header"><span class="server-card-name">'+esc(n.label||n.ip)+'</span>'+
'<div class="server-card-status"><span class="status-dot" style="background:'+stColor+'"></span> '+stLabel+'</div></div>'+
'<div class="server-card-details">'+
'<div>Host: <span style="font-family:var(--mono)">'+esc(n.ip)+'</span></div>'+
'<div>Port: '+esc(String(n.port||8080))+'</div>'+
(n.latency_ms>0?'<div>Latency: <span style="font-family:var(--mono);color:'+(n.latency_ms>500?'#fbbf24':'#4ade80')+'">'+n.latency_ms+'ms</span></div>':'')+
(n.version?'<div>Version: <span style="font-family:var(--mono)">'+esc(n.version)+'</span></div>':'')+
(n.workspace?'<div>Workspace: '+esc(n.workspace)+'</div>':'')+
(n.username?'<div>User: <span style="font-family:var(--mono)">'+esc(n.username)+'</span></div>':'')+
(n.vultr_instance_id?'<div>Vultr: <span style="font-family:var(--mono);font-size:.75rem">'+esc(n.vultr_instance_id)+'</span></div>':'')+
'</div>'+
'<div class="server-card-actions">'+
'<button onclick="testNode(\''+esc(n.id)+'\',this)">'+ic('refresh',14)+' Test</button>'+
(st==='connected'?'<button onclick="openRemoteUI(\''+esc(n.id)+'\',\''+esc(n.label||n.ip)+'\')">'+ic('globe',14)+' Remote UI</button>':'')+
'<button onclick="window.open(\'http://'+esc(n.ip)+':'+esc(String(n.port||8080))+'\')" >'+ic('globe',14)+' Open</button>'+
'<button onclick="deleteNode(\''+esc(n.id)+'\',\''+esc(n.label||n.ip)+'\')" style="color:#f87171">'+ic('trash',14)+' Remove</button>'+
'</div></div>';
}
html+='</div>';
list.innerHTML=html;
}).catch(function(){var l=document.getElementById('nodesList');if(l)l.innerHTML='<div style="color:var(--text-muted);text-align:center;padding:2rem">Could not load nodes</div>';});
}

window.testNode=function(id,btn){
if(btn){btn.disabled=true;btn.innerHTML=ic('refresh',14)+' Testing...';}
api('/api/nodes/'+encodeURIComponent(id)+'/test',{method:'POST'}).then(function(d){
var st=d.status||'unknown';
toast('Node '+st+(d.latency_ms>0?' ('+d.latency_ms+'ms)':''),'info');
loadNodes();
}).catch(function(e){toast('Test failed: '+(e.message||e),'error');loadNodes();});
};
window.deleteNode=function(id,name){
avaConfirm('Remove node "'+name+'"?',function(){
api('/api/nodes/'+encodeURIComponent(id),{method:'DELETE'}).then(function(){toast('Node removed','success');loadNodes();}).catch(function(e){toast('Failed: '+(e.message||e),'error');});
},{title:'Remove Node',okText:'Remove'});
};

/* ── Auth ─────────────────────────────────────────────── */
function checkAuth(){
var authH={};var tk=localStorage.getItem('avacli_token');if(tk)authH['Authorization']='Bearer '+tk;
fetch('/api/auth/status',{headers:authH}).then(function(r){return r.json();}).then(function(d){
if(!d.master_key_configured){
document.getElementById('setupOverlay').style.display='flex';
document.getElementById('loginOverlay').style.display='none';
document.getElementById('app').style.display='none';
var sInp=document.getElementById('setupUserInput');
var sPw=document.getElementById('setupPwInput');
if(sPw)sPw.addEventListener('keydown',function(e){if(e.key==='Enter')doSetup();});
if(sInp){sInp.addEventListener('keydown',function(e){if(e.key==='Enter'){if(sPw)sPw.focus();}});sInp.focus();}
}else if(d.requires_login&&!d.authenticated){
document.getElementById('setupOverlay').style.display='none';
document.getElementById('loginOverlay').style.display='flex';
document.getElementById('app').style.display='none';
var uInp=document.getElementById('loginUserInput');
var pInp=document.getElementById('loginKeyInput');
if(pInp)pInp.addEventListener('keydown',function(e){if(e.key==='Enter')doLogin();});
if(uInp){uInp.addEventListener('keydown',function(e){if(e.key==='Enter'){if(pInp)pInp.focus();}});uInp.focus();}
else if(pInp)pInp.focus();
}else{
document.getElementById('setupOverlay').style.display='none';
document.getElementById('loginOverlay').style.display='none';
document.getElementById('app').style.display='';
initApp();
}
}).catch(function(){initApp();});
}

window.doSetup=function(){
var uInp=document.getElementById('setupUserInput');
var pInp=document.getElementById('setupPwInput');
var errEl=document.getElementById('setupError');
var username=uInp?uInp.value.trim():'';
var password=pInp?pInp.value:'';
if(!username){if(errEl)errEl.textContent='Username is required';return;}
if(password&&password.length<8){if(errEl)errEl.textContent='Password must be at least 8 characters';return;}
if(errEl)errEl.textContent='';
fetch('/api/auth/accounts',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:username,password:password})}).then(function(r){return r.json();}).then(function(d){
if(d.error){if(errEl)errEl.textContent=d.error;return;}
if(d.token){
localStorage.setItem('avacli_token',d.token);
if(d.username)localStorage.setItem('avacli_user',d.username);
if(typeof d.is_admin==='boolean')localStorage.setItem('avacli_is_admin',d.is_admin?'1':'0');
S.user={username:d.username||username,is_admin:true};
}
document.getElementById('setupOverlay').style.display='none';
document.getElementById('app').style.display='';
initApp();
}).catch(function(e){if(errEl)errEl.textContent='Setup failed: '+(e.message||e);});
};

window.doLogin=function(){
var uInp=document.getElementById('loginUserInput');
var pInp=document.getElementById('loginKeyInput');
var username=uInp?uInp.value:'';
var password=pInp?pInp.value:'';
if(!password)return;
fetch('/api/auth/login',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({username:username,password:password})}).then(function(r){return r.json();}).then(function(d){
if(d.token){
localStorage.setItem('avacli_token',d.token);
if(d.username)localStorage.setItem('avacli_user',d.username);
if(typeof d.is_admin==='boolean')localStorage.setItem('avacli_is_admin',d.is_admin?'1':'0');
S.user={username:d.username||'admin',is_admin:d.is_admin!==false};
document.getElementById('loginOverlay').style.display='none';
document.getElementById('app').style.display='';
initApp();
}else if(d.error){
document.getElementById('loginError').textContent=d.error;
}
}).catch(function(e){
document.getElementById('loginError').textContent='Login failed: '+e.message;
});
};

window.doLogout=function(){
var t=localStorage.getItem('avacli_token');
fetch('/api/auth/logout',{method:'POST',headers:{'Content-Type':'application/json','Authorization':'Bearer '+(t||'')}}).then(function(){
localStorage.removeItem('avacli_token');localStorage.removeItem('avacli_user');localStorage.removeItem('avacli_is_admin');
location.reload();
}).catch(function(){
localStorage.removeItem('avacli_token');localStorage.removeItem('avacli_user');localStorage.removeItem('avacli_is_admin');
location.reload();
});
};

/* ── Init ─────────────────────────────────────────────── */
function initApp(){
S.route=getRoute();
S.token=localStorage.getItem('avacli_token')||'';
S.user={username:localStorage.getItem('avacli_user')||'admin',is_admin:localStorage.getItem('avacli_is_admin')!=='0'};
if(typeof mermaid!=='undefined'){mermaid.initialize({startOnLoad:false,theme:'dark',flowchart:{useMaxWidth:false,htmlLabels:true,padding:25,nodeSpacing:50,rankSpacing:60,wrappingWidth:350},themeVariables:{primaryColor:'#7c3aed',primaryTextColor:'#e2e2e8',primaryBorderColor:'rgba(255,255,255,0.08)',lineColor:'#a855f7',secondaryColor:'#12121e',tertiaryColor:'#0a0a14',background:'#0c0c18',mainBkg:'#12121e',nodeBorder:'rgba(255,255,255,0.08)',clusterBkg:'rgba(124,58,237,0.08)',titleColor:'#c4b5fd',edgeLabelBackground:'#0a0a14',textColor:'#e2e2e8'}});}

api('/api/auth/me',{silent:true}).then(function(u){
if(u&&u.username){S.user={username:u.username,is_admin:!!u.is_admin,has_password:!!u.has_password};localStorage.setItem('avacli_user',u.username);localStorage.setItem('avacli_is_admin',u.is_admin?'1':'0');}
}).catch(function(){});

api('/api/status',{silent:true}).then(function(d){
if(d){
S.status=d;S.model=S.model||d.model||'';S.session=localStorage.getItem('ava_last_session')||S.session||d.session||'';
S.hasXaiKey=!!d.has_xai_key;
if(d.extra_model)S.extraModel=d.extra_model;
}
return api('/api/models',{silent:true});
}).then(function(m){
S.models=Array.isArray(m)?m:[];
if(!S.model&&S.models.length)S.model=S.models[0].id;
}).catch(function(){}).then(function(){
render();startStatusPoll();
if(S.session){
var local=loadMessagesFromLocal(S.session);
if(local&&local.length){
S.messages=local;
autoScroll=true;render();
setTimeout(function(){scrollToBottom(true);},50);
}else{
api('/api/sessions/'+encodeURIComponent(S.session),{silent:true}).then(function(data){
if(!data||!data.messages)return;
S.messages=restoreMessagesFromApi(data);
saveMessagesToLocal();
autoScroll=true;
render();
setTimeout(function(){scrollToBottom(true);},50);
}).catch(function(){});
}
}
});
}

var statusPollTimer=null;
function startStatusPoll(){
if(statusPollTimer)clearInterval(statusPollTimer);
statusPollTimer=setInterval(function(){
api('/api/status',{silent:true}).then(function(d){
if(!d)return;
S.status=d;
var changed=false;
if(d.model&&d.model!==S.model){
S.model=d.model;
changed=true;
var sel=document.getElementById('modelSelect');
if(sel&&sel.value!==S.model)sel.value=S.model;
var ssel=document.getElementById('settingsModel');
if(ssel&&ssel.value!==S.model)ssel.value=S.model;
}
if(typeof d.has_xai_key==='boolean'&&d.has_xai_key!==S.hasXaiKey){S.hasXaiKey=d.has_xai_key;changed=true;}
if(changed){
var sbFoot=document.querySelector('.sb-foot');
if(sbFoot){
var modelShort=S.model?(S.model.length>25?S.model.slice(0,25)+'...':S.model):'--';
var connDot='<span class="status-dot online" style="display:inline-block;width:6px;height:6px;border-radius:50%;background:#4ade80;margin-right:4px"></span>Local';
var stColor=S.status?'#4ade80':'#f87171';
var stText=S.status?'Connected':'Unknown';
sbFoot.innerHTML=
'<div>Model: <span title="'+esc(S.model)+'">'+esc(modelShort)+'</span></div>'+
'<div>'+connDot+'</div>'+
'<div>Status: <span style="color:'+stColor+'">'+ic('dot',8)+' '+stText+'</span></div>';
}
updateMediaSettings();
}
}).catch(function(){});
},5000);
}

function setAppHeight(){
var h=window.visualViewport?window.visualViewport.height:window.innerHeight;
document.documentElement.style.setProperty('--app-height',h+'px');
}
setAppHeight();
if(window.visualViewport){
window.visualViewport.addEventListener('resize',function(){
setAppHeight();
scrollToBottom();
});
}else{
window.addEventListener('resize',setAppHeight);
}

document.addEventListener('DOMContentLoaded',checkAuth);
window.navigate=navigate;
})();
