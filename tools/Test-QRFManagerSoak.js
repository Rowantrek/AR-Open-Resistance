// Deterministic scheduler soak against actual-source manager/operation bodies.
// Native actors, finance and registration are mocked; this is not an engine soak.
const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
const managerSource=fs.readFileSync(path.join(root,'AROR_OperationManagerComponent.c'),'utf8');
const operationSource=fs.readFileSync(path.join(root,'AROR_Operation.c'),'utf8');
let assertions=0,server=true,seed=0x19790507,callbackDepth=0,maxDepth=0,notifications=0;
const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg)};
const random=()=>{seed=(Math.imul(seed,1664525)+1013904223)>>>0;return seed>>>8};
for(const[n,f]of Object.entries({Clear(){this.length=0},Insert(x){this.push(x)},Count(){return this.length},Remove(i){this.splice(i,1)}}))
 Object.defineProperty(Array.prototype,n,{value:f,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const states={PLANNED:0,SPAWNING:1,ACTIVE:2,SUCCEEDED:3,FAILED:4,CANCELLED:5};
const sites=Array.from({length:12},()=>({registered:true,role:0,GetControllingRole(){return this.role}}));
const env={Replication:{IsServer:()=>server},AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},
 AROR_EOperationType:{ATTACK:0,QRF:1},AROR_EOperationState:states,AROR_QRFSearchPlan:class{},
 AROR_LocationManagerComponent:{GetInstance:()=>({GetLocationById:id=>{const s=sites[Number(id)];return s?.registered?s:null}})},
 Math:{Max:Math.max},Print(){},LogLevel:{NORMAL:0},string:{Empty:'',Format:(s,...a)=>s.replace(/%(\d+)/g,(_,n)=>a[n-1])}};
const names=new Set([...managerSource.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm),
 ...operationSource.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1]).filter(n=>!n.startsWith('AROR_')));
function compile(source,name){
 const pattern=/^\t[^\r\n]*\bMETHOD\(([^\r\n]*)\)\r?\n\t\{([\s\S]*?)^\t\}/m;
 const m=source.match(new RegExp(pattern.source.replace('METHOD',name),'m'));
 if(!m)throw Error('Missing actual method '+name);
 const args=m[1].replace(/\b(?:notnull |const )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
 let body=m[2].replace(/array<[^>]+> (\w+) = \{\};/g,'let $1=[];')
 .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/\bAROR_(?!E[A-Z])\w+\s+(\w+);/g,'let $1=null;')
 .replace(/\b(?:bool|int|float|string|AROR_\w+)\s+(\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1=0;').replace(/(?<![.\w])[ms]_\w+/g,x=>'this.'+x)
 .replace(/(?<![.\w])(\w+)\(/g,(s,n)=>names.has(n)?'this.'+s:s);
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const init=compile(operationSource,'AROR_Operation');
env.AROR_Operation=function(...args){this.m_bDebugRequest=false;this.m_iSpawnPreparationTimeout=0;this.m_TacticalCommander=null;this.m_Funding=null;init.apply(this,args)};
for(const n of ['GetId','GetRole','GetType','GetTargetLocationId','GetState','GetDueTime','GetStateSince','GetResult','IsTerminal','Transition_S','UsesTacticalResponse'])
 env.AROR_Operation.prototype[n]=compile(operationSource,n);
const m={m_iClock:0,m_iNextId:0,m_aOperations:[],m_aPendingNotifications:[],m_bNotifying:false,m_bInvaderOperationsEnabled:true,
 m_iMaxAttacks:1,m_iMaxQRFs:1,m_iAttackInterval:5,m_iQRFCooldown:3,m_iOccupiersNextAttack:0,m_iInvadersNextAttack:0,
 m_iOccupiersNextQRF:0,m_iInvadersNextQRF:0,m_iSpawnTimeout:2,m_iActiveTimeout:6,m_iHistoryLimit:4,
 m_iNextStrategyTick:1000000,m_bTownCaptureQRFs:false,m_bOutpostCaptureQRFs:false,m_bQRFSpotting:false,m_bQRFDebugMarkers:false,
 m_Commanders:{IsCampaignActive:()=>true},m_QRFFinance:{Activate_S(){},Observe_S(){}}};
m.s_Instance=m;
for(const n of ['GetOperation','GetOperations','IsReady','IsEnemyRole','HasLocation','GetNextRequestTime','RequestOperation_S',
 'ChangeState_S','MarkActive_S','ResolveOperation_S','NotifyChanged','ActiveTimeoutFor','ProcessOperations','Tick'])
 m[n]=compile(managerSource,n);
m.m_OnOperationChanged={Invoke(op){
 callbackDepth++;maxDepth=Math.max(maxDepth,callbackDepth);notifications++;
 if(op.GetState()===1){
  if(random()%3)m.MarkActive_S(op.GetId());
  if(random()%5===0)m.ResolveOperation_S(op.GetId(),4,'synchronous failure');
  if(random()%7===0)m.RequestOperation_S(op.GetRole()===1?2:1,1,String(random()%12),0);
 }
 callbackDepth--;
}};
let admitted=0,rejected=0;
for(let tick=0;tick<10000;tick++){
 if(tick%97===0){const site=sites[random()%sites.length];site.registered=!site.registered;site.role=random()%3}
 for(let request=0;request<3;request++){
  const role=[1,2,0,3][random()%4],type=[0,1,1,99][random()%4],id=String(random()%12),delay=[0,1,2,-1][random()%4];
  const accepted=m.RequestOperation_S(role,type,id,delay,random()%5===0);if(accepted)admitted++;else rejected++;
 }
 if(tick%13===0)for(const op of [...m.m_aOperations])if(!op.IsTerminal()&&random()%2===0)
  m.ResolveOperation_S(op.GetId(),[3,4,5][random()%3],'external outcome');
 m.Tick();
 const live=m.m_aOperations.filter(op=>!op.IsTerminal());
 check(live.filter(op=>op.GetType()===0).length<=1,'one global attack even during synchronous admission');
 for(const role of [1,2])check(live.filter(op=>op.GetRole()===role&&op.GetType()===1).length<=1,'per-role QRF capacity survives reordered callbacks');
 check(new Set(live.map(op=>op.GetRole()+'/'+op.GetTargetLocationId())).size===live.length,'no conflicting live role/target records');
 check(m.m_aOperations.length<=7&&m.m_aOperations.filter(op=>op.IsTerminal()).length<=4,'terminal history remains bounded during a long session');
 check(!m.m_bNotifying&&m.m_aPendingNotifications.length===0,'nested lifecycle notifications drain without leaking work');
 if(tick%997===0){server=false;const clock=m.m_iClock;
  check(!m.RequestOperation_S(1,1,'1',0),'client cannot inject scheduler work');m.Tick();check(m.m_iClock===clock,'client ticks cannot advance campaign time');server=true}
}
check(maxDepth===1,'ScriptInvoker is never called recursively');
check(notifications>1000&&admitted>1000&&rejected>1000,'soak exercises admission, rejection and callbacks at scale');
console.log(JSON.stringify({assertions,simulatedTicks:10000,admittedOrDeduplicated:admitted,rejected,notifications,maxCallbackDepth:maxDepth,
 note:'Deterministic mocked scheduler soak; no native driving, combat, network or real-time performance evidence.'}));
