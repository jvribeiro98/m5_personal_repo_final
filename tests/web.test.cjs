const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const source = fs.readFileSync(require('node:path').join(__dirname, '../firmware/m5_personal.ino'), 'utf8');
function pageScript(name) {
  const start = source.indexOf('void ' + name + '() {');
  const body = source.slice(start, source.indexOf('\n}', start));
  const scripts = [...body.matchAll(/"(?:\\.|[^"\\])*"/g)].map(m => JSON.parse(m[0])).join('');
  return scripts.match(/<script>([\s\S]*?)<\/script>/)[1];
}
function setup(reply) {
  const nodes = {};
  for (const id of ['remoteBox','remoteName','device','temp','modeLabel','stateLabel']) nodes[id] = {textContent:'',style:{},value:'0',selectedIndex:0,options:[{text:'Samsung'},{text:'Midea'}],querySelectorAll:()=>[]};
  nodes.temp.textContent='24';
  nodes.modeLabel.textContent='Frio';
  const context = vm.createContext({...nodes,document:{getElementById:id=>nodes[id],querySelectorAll:()=>[]},api:async()=>reply(),flashIr(){},toastMessage(){},setInterval(){},clearInterval(){}});
  vm.runInContext(pageScript('handleWebAcPage'), context);
  return context;
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
 let cleared=false;
 const context=vm.createContext({setupStatus:{textContent:''},api:async()=>({connected:false,connecting:false,message:'Falha'}),setInterval:fn=>{context.tick=fn;return 1;},clearInterval:()=>{cleared=true;}});
 vm.runInContext(pageScript('handleWebRoot'),context);context.poll();await context.tick();assert.equal(cleared,true);
});
