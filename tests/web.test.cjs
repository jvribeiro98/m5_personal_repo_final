const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require('node:path').join(__dirname, '../firmware/m5_personal.ino'), 'utf8');
function pageScript(name) {
  const start = source.indexOf('void ' + name + '() {');
  const body = source.slice(start, source.indexOf('\n}', start)).replace('String(savedNetworkCount)', '"0"');
  const scripts = decodeStrings(body);
  return scripts.match(/<script>([\s\S]*?)<\/script>/)[1];
}
function decodeStrings(body) {
  return [...body.matchAll(/R"(\w*)\(([\s\S]*?)\)\1"|"(?:\\.|[^"\\])*"/g)]
    .map(m => m[2] === undefined ? JSON.parse(m[0]) : m[2]).join('');
}
function shellScript(beforeBody = false) {
  const start = source.indexOf('String webShell(');
  let body = source.slice(start, source.indexOf('\n}', start));
  if (beforeBody) body = body.slice(0, body.indexOf('\n  body +'));
  return [...decodeStrings(body).matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m => m[1]).join('\n');
}
function element() {
  const classes = new Set();
  return { textContent: '', innerHTML: '', value: '0', hidden: false, disabled: false,
    style: {}, dataset: {}, children: [], selectedIndex: 0,
    options: [{text:'Samsung'},{text:'Midea'}],
    classList: {add:c=>classes.add(c),remove:c=>classes.delete(c),contains:c=>classes.has(c),toggle:(c,v)=>v?classes.add(c):classes.delete(c)},
    appendChild(e){this.children.push(e);return e;}, querySelectorAll(){return [];},
    setAttribute(k,v){this[k]=v;},removeAttribute(k){delete this[k];},hasAttribute(k){return k in this;},focus(){},addEventListener(){} };
}
function screen(name, reply) {
  const nodes = {};
  for (const id of ['remoteBox','remoteName','device','temp','modeLabel','stateLabel','remoteStatus','remoteRetry',
    'bigNumber','limit','remaining','cattleCard','cattleStatus','cattleActions','setup','active','horseName',
    'passNumber','history','trainCount','horseSelects','trainStatus','trainingActions','setupStatus',
    'nets','scanButton','connectButton','saveStatus','netModal','netIndex','netSsid','netPassword','netTest',
    'modalTitle','saveButton','webStatus','webToggle','connectionStatus','connectionMessage','toast','irLed',
    'systemStatus','systemWifi','systemBattery','systemTime','systemCity','systemTemperature','brightnessControls','syncButton']) nodes[id]=element();
  nodes.trainCount.value='1';
  nodes.netTest.checked=true;
  const timers = new Map(); let nextTimer=0;
  const context = vm.createContext({...nodes,AbortController,URL,Date,
    document:{getElementById:id=>nodes[id]||(nodes[id]=element()),querySelectorAll:()=>[],createElement:element,activeElement:null,addEventListener(){}},
    window:{addEventListener(){}}, location:{reload(){context.reloads++;},pathname:'/'}, reloads:0,
    api:async(url,options)=>reply(url,options),flashIr(){},toastMessage(){},confirm:()=>true,prompt:()=>'',
    setInterval(fn){const id=++nextTimer;timers.set(id,fn);return id;},clearInterval:id=>timers.delete(id),
    setTimeout(fn){const id=++nextTimer;timers.set(id,fn);return id;},clearTimeout:id=>timers.delete(id)});
  context.timers=timers;
  if(name) vm.runInContext(pageScript(name),context);
  return context;
}
const settle = () => new Promise(resolve => setImmediate(resolve));
function setup(reply) {
  return screen('handleWebAcPage',reply);
}
test('opening the AC remote displays the firmware state, including mode', async()=>{
  const c=setup(()=>({ok:true,device:0,temp:21,mode:'SECO',fan:'AUTO',power:true,swing:false,turbo:false,sleep:'OFF'}));
  await c.openRemote();
  assert.equal(String(c.temp.textContent),'21');
  assert.equal(c.modeLabel.textContent,'SECO');
});
test('AC commands use the returned state instead of a browser-side temperature counter', async()=>{
  const c=setup(()=>({ok:true,device:0,temp:19,mode:'FRIO',fan:'AUTO',power:true,swing:false,turbo:false,sleep:'OFF'}));
  await c.ac(0);
  assert.equal(String(c.temp.textContent),'19');
});
test('a rejected AC command leaves the displayed state untouched',async()=>{
  const c=setup(()=>({ok:false}));c.temp.textContent='22';
  await c.ac(0);
  assert.equal(c.temp.textContent,'22');
});
test('setup polling ends after a failed connection, without an endless timer',async()=>{
 const context=screen('handleWebRoot',()=>({ok:true,connected:false,connecting:false,message:'Falha'}));
 context.poll();await [...context.timers.values()][0]();assert.equal(context.timers.size,0);
});

test('shared API is available before page startup scripts execute',()=>{
 const c=screen();delete c.api;vm.runInContext(shellScript(true),c);
 assert.equal(vm.runInContext('typeof api',c),'function');
});
test('HTTP failure cannot be interpreted as a successful command',async()=>{
 const c=screen();c.fetch=async()=>({ok:false,status:503,json:async()=>({ok:true,message:'Unavailable'})});
 vm.runInContext(shellScript(),c);
 const result=await c.api('/api/team/training/add',{method:'POST'});
 assert.equal(result.ok,false);
 assert.equal(c.connectionStatus.hidden,false);
});
test('timed-out commands report uncertain delivery without automatic replay',async()=>{
 const c=screen();let calls=0;
 c.fetch=(url,options)=>{calls++;return new Promise((resolve,reject)=>options?.signal?.addEventListener('abort',()=>reject(Object.assign(new Error('timeout'),{name:'AbortError'}))));};
 vm.runInContext(shellScript(),c);
 const pending=c.api('/api/team/training/add',{method:'POST'});
 assert.ok(c.timers.size>0,'a stalled request must have a deadline');
 await [...c.timers.values()][0]();
 const result=await pending;
 assert.equal(result.ok,false);assert.equal(result.uncertain,true);assert.equal(calls,1);
});
test('lost cattle connection preserves the last confirmed selection',async()=>{
 let reply={ok:true,selected:4,max:9,remainingCount:2,remaining:[4,5]};
 const c=screen('handleWebCattlePage',()=>reply);await settle();
 reply={ok:false,message:'Offline'};await c.refresh();
 assert.equal(String(c.bigNumber.textContent),'4');
 assert.equal(String(c.limit.value),'9');
 assert.ok(c.cattleStatus.textContent.length>0);
});
test('polling does not overwrite a cattle limit being edited and cancellation restores it',async()=>{
 const c=screen('handleWebCattlePage',()=>({ok:true,selected:4,max:9,remainingCount:2,remaining:[4,5]}));await settle();
 c.limit.value='6';c.document.activeElement=c.limit;await c.refresh();assert.equal(String(c.limit.value),'6');
 c.confirm=()=>false;await c.changeLimit();assert.equal(String(c.limit.value),'9');
});
test('a double cattle mark cannot dispatch two increments before confirmation',async()=>{
 let resolve;let writes=0;
 const c=screen('handleWebCattlePage',(url)=>url.endsWith('/mark')?(writes++,new Promise(r=>resolve=r)):{ok:true,selected:4,max:9,remainingCount:2,remaining:[4,5]});await settle();
 const first=c.mark();const second=c.mark();await settle();assert.equal(writes,1);
 resolve({ok:true});await Promise.all([first,second]);
});
test('lost training connection does not offer to replace a running session',async()=>{
 let reply={ok:true,active:true,current:{index:0,name:'Linda',passes:7},history:[]};
 const c=screen('handleWebTrainingPage',()=>reply);await settle();
 reply={ok:false,message:'Offline'};await c.refreshTrain();
 assert.equal(c.active.style.display,'block');assert.equal(c.setup.style.display,'none');
 assert.equal(String(c.passNumber.textContent),'7');assert.ok(c.trainStatus.textContent.length>0);
});
test('rapid training clicks cannot add the same pass twice',async()=>{
 let resolve;let writes=0;
 const c=screen('handleWebTrainingPage',url=>url.endsWith('/add')?(writes++,new Promise(r=>resolve=r)):{ok:true,active:true,current:{index:0,name:'Linda',passes:7},history:[]});await settle();
 const first=c.addPass();const second=c.addPass();await settle();assert.equal(writes,1);
 resolve({ok:true});await Promise.all([first,second]);
});
test('AC poll failures identify stale readings while preserving the temperature',async()=>{
 let reply={ok:true,device:0,temp:21,mode:'SECO',fan:'AUTO',power:true,swing:false,turbo:false,sleep:'OFF'};
 const c=screen('handleWebAcPage',()=>reply);await c.openRemote();
 reply={ok:false,message:'Offline'};await c.refreshState();
 assert.equal(String(c.temp.textContent),'21');assert.match(c.remoteStatus.textContent,/atualiz|conex|confirm/i);
});
test('changing TV brand cannot leave the previous remote title on screen',()=>{
 const c=screen('handleWebTvPage',()=>({ok:true}));c.openRemote();
 c.device.value='2';c.device.selectedIndex=1;
 if(c.device.onchange)c.device.onchange();
 assert.equal(c.remoteName.textContent,'Midea');
});
test('a new password for a saved network is not silently replaced by its old password',async()=>{
 let body='';const c=screen('handleWebRoot',(url,options)=>{body=options.body;return {ok:false};});
 c.prompt=()=> 'corrected-password';await c.choose({ssid:'Home',saved:true});await settle();
 const params=new URLSearchParams(body);assert.equal(params.get('password'),'corrected-password');assert.equal(params.get('reuse'),'0');
});
test('failed Web UI toggles keep the page available without reloading',async()=>{
 const c=screen('handleWebWifiPage',()=>({ok:false,message:'Conecte ao Wi-Fi primeiro'}));
 await c.toggleWeb();for(const tick of [...c.timers.values()])await tick();
 assert.equal(c.reloads,0);
});
test('training history renders into the element rather than the browser history object',async()=>{
 const c=screen('handleWebTrainingPage',()=>({ok:true,active:false,history:[{date:'2026-09-15',horses:[{name:'Linda',passes:5}]}]}));
 c.history={back(){}};await settle();
 assert.equal(c.document.getElementById('history').children.length,1);
});
test('changing a saved network does not reload the old IP after a successful test',async()=>{
 const c=screen('handleWebWifiPage',()=>({ok:true,connected:true,connecting:false,ip:'192.168.1.25',message:'Conectado'}));
 c.pollSave();await [...c.timers.values()][0]();
 for(const tick of [...c.timers.values()])await tick();
 assert.equal(c.reloads,0);assert.match(c.saveStatus.textContent,/192\.168\.1\.25/);
});
test('Wi-Fi status polling never overlaps a slow request',async()=>{
 let calls=0,resolve;
 const c=screen('handleWebRoot',()=>{calls++;return new Promise(r=>resolve=r);});
 c.poll();const tick=[...c.timers.values()][0];const a=tick(),b=tick();
 assert.equal(calls,1);resolve({ok:true,connected:false,connecting:false});await Promise.all([a,b]);
});
test('brightness changes refresh confirmed system data without reloading',async()=>{
 let reply={ok:true,message:'OK',wifi:true,webMode:'lan',battery:70,time:'20:45',date:'2026-09-15',city:'Uberaba',temperature:26,brightness:4};
 const c=screen('handleWebSystemPage',()=>reply);await settle();await c.brightness(4);
 assert.equal(c.reloads,0);assert.equal(c.systemBattery.textContent,'70%');
});

function installer(reply,{secure=true,serial=true,library=true}={}) {
 const path=require('node:path');const js=path.join(__dirname,'../web/installer.js');
 const code=fs.existsSync(js)?fs.readFileSync(js,'utf8'):[...fs.readFileSync(path.join(__dirname,'../web/index.html'),'utf8').matchAll(/<script>([\s\S]*?)<\/script>/g)].map(m=>m[1]).join('\n');
 const nodes={};for(const id of ['version','installButton','installer','installStatus','retryButton'])nodes[id]=element();
 nodes.installButton.disabled=false;nodes.retryButton.hidden=true;
 const timers=new Map();let next=0;
 const c=vm.createContext({...nodes,URL,AbortController,isSecureContext:secure,navigator:serial?{serial:{}}:{},
   location:{href:'https://example.test/m5/index.html'},document:{getElementById:id=>nodes[id]},
   customElements:{get:()=>library?function(){}:undefined,whenDefined:()=>library?Promise.resolve():new Promise(()=>{})},
   fetch:reply,setTimeout(fn){const id=++next;timers.set(id,fn);return id;},clearTimeout:id=>timers.delete(id)});
 c.timers=timers;vm.runInContext(code,c);return c;
}
const manifest={name:'M5 Personal',version:'a1b2c3d',builds:[{chipFamily:'ESP32',parts:[{path:'firmware.bin',offset:0}]}]};
function releaseReply(url,options){
 if(String(url).includes('manifest.json'))return Promise.resolve({ok:true,json:async()=>manifest});
 if(String(url).includes('firmware.bin')&&options.method==='HEAD')return Promise.resolve({ok:true,headers:{get:()=> '4194304'}});
 return Promise.resolve({ok:false,status:404,json:async()=>({})});
}
test('installer remains unavailable when the published binary is missing',async()=>{
 const c=installer((url,options)=>String(url).includes('firmware.bin')?Promise.resolve({ok:false,status:404}):releaseReply(url,options));await settle();
 assert.equal(c.installButton.disabled,true);assert.equal(c.retryButton.hidden,false);assert.match(c.installStatus.textContent,/firmware|publica/i);
});
test('installer uses the version from the verified manifest',async()=>{
 const c=installer(releaseReply);await settle();
 assert.equal(c.version.textContent,'a1b2c3d');assert.equal(c.installButton.disabled,false);assert.match(c.installStatus.textContent,/pront|dispon/i);
});
test('installer identifies an unsupported browser before offering a USB connection',async()=>{
 const c=installer(releaseReply,{serial:false});await settle();
 assert.equal(c.installButton.disabled,true);assert.match(c.installStatus.textContent,/Chrome|Edge/);
});
test('installer reports a failed network check and allows a deliberate retry',async()=>{
 let fail=true;const c=installer((url,options)=>fail?Promise.reject(new Error('offline')):releaseReply(url,options));await settle();
 assert.equal(c.installButton.disabled,true);assert.equal(c.retryButton.hidden,false);
 fail=false;assert.equal(typeof c.checkRelease,'function');await c.checkRelease();
 assert.equal(c.installButton.disabled,false);assert.equal(c.retryButton.hidden,true);
});
test('installer cannot enable installation while the USB library has not loaded',async()=>{
 const c=installer(releaseReply,{library:false});await settle();
 assert.equal(c.installButton.disabled,true);assert.ok(c.timers.size>0);
 await [...c.timers.values()][0]();await settle();
 assert.equal(c.retryButton.hidden,false);assert.match(c.installStatus.textContent,/instalador|carreg/i);
});
