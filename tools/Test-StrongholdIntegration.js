// Actual-source reconciliation checks. Native physics, placement, subscriptions and replication are mocked.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
const source=Object.fromEntries(Object.entries({script:'Operations/AROR_ScriptedResponses.c',waves:'Operations/AROR_QRFTacticalCommander.c',stronghold:'Strongholds/AROR_StrongholdComponent.c',save:'Strongholds/AROR_StrongholdComponentSerializer.c'}).map(([k,p])=>[k,fs.readFileSync(path.join(root,p),'utf8')]));
let assertions=0,server=true,operations=null;
const check=(ok,message)=>{assertions++;if(!ok)throw Error(message)};
for(const [name,fn]of Object.entries({Count(){return this.length},IsEmpty(){return !this.length},Insert(x){this.push(x)},Clear(){this.length=0},Remove(i){this.splice(i,1)},ToString(){return this.join(' ')}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const env={Replication:{IsServer:()=>server},AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},AROR_EOperationType:{QRF:1},AROR_EOperationState:{ACTIVE:2},
 Math:{Min:Math.min,Max:Math.max,Clamp:(v,a,b)=>Math.min(b,Math.max(a,v)),Mod:(a,b)=>a%b,Sin:Math.sin,Cos:Math.cos,DEG2RAD:Math.PI/180},
 Print(){},LogLevel:{NORMAL:0,WARNING:1},string:{Empty:'',Format:(s,...a)=>s.replace(/%(\d+)/g,(_,i)=>a[i-1])},
 AROR_OperationManagerComponent:{GetInstance:()=>operations},ChimeraCharacter:{Cast:x=>x},ESerializeResult:{OK:1}};
env.AROR_Operation=class{constructor(id,role,type,target){Object.assign(this,{id,role,type,target,state:0})}GetRole(){return this.role}GetTargetLocationId(){return this.target}IsTerminal(){return this.state>=3}};
const names=new Set(Object.values(source).flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])));
for(const name of ['IsReady','IsEnemyRole','HasLocation','NotifyChanged','TacticalRoadPoint','IsLaunchPositionSafe'])names.add(name);
function compile(file,name){
 const m=source[file].match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
 if(!m)throw Error('Missing actual method '+name);
 const args=m[1].replace(/\b(?:notnull |out )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
 let body=m[2].replace(/array<[^>]+>\s+(\w+)\s*=\s*\{([^;]*)\};/g,'let $1=[$2];')
 .replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/\b(?:IEntity|AROR_(?!E[A-Z])\w+)\s+(\w+);/g,'let $1=null;')
 .replace(/\bint version, elapsed, kills;/g,'let version=0,elapsed=0,kills=0;')
 .replace(/\b(?:int|bool|float|string|vector|BaseWorld|ChimeraCharacter|AROR_\w+)\s+(\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1=0;')
 .replace(/super\.(\w+)\(/g,'this.Base$1(')
 .replace(/context.ReadValue\(([^,]+), ([^)]+)\)/g,'context.ReadValue($1,v=>$2=v)')
 .replace('anchor + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius','anchor.map((v,i)=>v + [Math.Sin(angle),0,Math.Cos(angle)][i] * radius)')
 .replace('(m_iPackages + 1) / 2','Math.Min(m_iPackages, Math.trunc((m_iPackages + 1) / 2))')
 .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
 .replace(/(?<![.\w])(\w+)\(/g,(s,n)=>names.has(n)?'this.'+s:s)
 .replace(/(?<![.\w])(RETRY_SECONDS|KILL_CREDIT_SECONDS)\b/g,'this.$1');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
env.Math.trunc=Math.trunc;
const request=compile('script','RequestScriptedQRF_S');
const chickenLayer=fs.readFileSync(path.join(root,'../../Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/MilitaryInstallations.layer'),'utf8');
// Workbench may save the port flat or as a named child of a $grp block sharing its prefab
check(/^\s*(?:GenericEntity\s+)?AROR_Location_everon_saint_pierre_port\b/m.test(chickenLayer)&&chickenLayer.includes('m_sId "everon_saint_pierre_port"'),'relocated port preserves its stable ID and entity name');
check(chickenLayer.includes('m_sName "Morton Port"')&&!chickenLayer.includes('m_sId "everon_morton_port"'),'display relocation does not introduce a replacement persistence identity');
for(const role of [1,2]){
 const ctx={m_aOperations:[],m_iNextId:0,m_iClock:42,m_bInvaderOperationsEnabled:true,IsReady:()=>server,IsEnemyRole:r=>r===1||r===2,HasLocation:id=>id==='fort',NotifyChanged(){}};
 server=false;check(request.call(ctx,role,'fort','recipe',5,true,[1,2,3],true)==='','clients cannot request scripted responses');server=true;
 for(const args of [[0,'fort','recipe',5],[role,'missing','recipe',5],[role,'fort','',5],[role,'fort','recipe',0],[role,'fort','recipe',6]])check(request.call(ctx,...args,false,[0,0,0],true)==='','invalid scripted request does not allocate an ID');
 check(ctx.m_iNextId===0,'invalid requests consume no IDs or operation slots');
 if(role===2){ctx.m_bInvaderOperationsEnabled=false;check(request.call(ctx,role,'fort','recipe',5,false,[0,0,0],true)==='','Invader admission gate remains authoritative');ctx.m_bInvaderOperationsEnabled=true}
 const id=request.call(ctx,role,'fort','recipe',5,true,[1,2,3],true),op=ctx.m_aOperations[0];
 check(id&&op.m_bFundingWaived&&op.m_bHasOriginOverride&&op.m_bForceTwoWaves&&op.m_iPoolReadinessLevel===5,'scripted flags and metrics are frozen on the admitted record');
 check(!op.m_bDebugRequest&&!op.m_bCaptureThreatRecorded,'scripted defence is neither MARS nor capture-QRF eligibility evidence');
 check(request.call(ctx,role,'fort','recipe',5,true,[1,2,3],true)==='','duplicate scripted response rejected');
 op.m_bFundingWaived=false;check(request.call(ctx,role,'fort','recipe',5,true,[1,2,3],true)==='','scripted admission also preserves ordinary operation target conflicts');
 op.state=4;check(!!request.call(ctx,role,'fort','recipe',5,false,[0,0,0],false),'terminal history does not block later defence');
}
// Scenario funding bypass does not grant a bypass to ordinary QRFs.
{
 let ordinary=0;
 const reserve=compile('script','ReserveQRFFunding'),ctx={BaseReserveQRFFunding(){ordinary++;return false}};
 check(reserve.call(ctx,{m_bFundingWaived:true},[],5)&&ordinary===0,'scenario-funded defence does not reserve faction money');
 check(!reserve.call(ctx,{},[],5)&&ordinary===1,'ordinary response retains its finance gate');
}
// Fixed coast launch points are not safe recovery destinations; the normal constructor still runs.
{
 let base=0,forced=0;
 const create=compile('script','CreateQRFTacticalCommander');
 for(const fixed of [false,true]){
  const op={m_bHasOriginOverride:fixed,m_bForceTwoWaves:true};
  create.call({BaseCreateQRFTacticalCommander(o){base++;o.m_TacticalCommander={m_sOriginLocationId:'site',m_bSafeWithdrawalOrigin:true,ForceTwoWaves(){forced++}}}},op,5,[1,2,3]);
  check(op.m_TacticalCommander.m_bSafeWithdrawalOrigin===!fixed,'only an owned base can qualify as a safe return');
  check(op.m_TacticalCommander.m_sOriginLocationId===(fixed?'':'site'),'coast override cannot inherit a coincident installation ID');
 }
 check(base===2&&forced===2,'normal tactical construction and requested two-wave policy remain connected');
 const available=compile('script','TacticalOriginAvailable');
 check(available.call({}, {m_bHasOriginOverride:true,m_TacticalCommander:{m_sOriginLocationId:''}}),'fixed coast launch remains eligible for construction');
 check(!available.call({BaseTacticalOriginAvailable:()=>false},{m_TacticalCommander:{}}),'ordinary origin checks are not bypassed');
}
const forceWaves=compile('waves','ForceTwoWaves');
// Coast fallback retains hard terrain/road budgets even when every candidate fails.
{
 env.GetGame=()=>({GetWorld:()=>({})});
 const find=compile('script','FindCoastalLanding_S');
 let terrain=0,roads=0;
 const ctx={IsCoastalLand(){terrain++;return false},TacticalRoadPoint(){roads++;return false}};
 check(!find.call(ctx,[0,0,0],300,8000,0,360,null)&&terrain===884&&roads===0,'coastal terrain search has a finite worst-case candidate count');
 terrain=0;ctx.IsCoastalLand=()=>{terrain++;return true};
 check(!find.call(ctx,[0,0,0],300,8000,0,360,null)&&roads===48&&terrain===49,'coastal fallback never exceeds its road-query budget');
 for(const [min,max,arc]of [[299,5000,360],[300,8001,360],[5000,500,360],[300,5000,0]])
  check(!find.call(ctx,[0,0,0],min,max,0,arc,null),'out-of-range coastal search is rejected');
 const before=roads;server=false;check(!find.call(ctx,[0,0,0],300,8000,0,360,null)&&roads===before,'clients cannot run native coastal probes');server=true;
}
for(let count=1;count<=8;count++){
 const commander={m_iPackages:count,m_iFirstWaveCount:count,m_aWave:Array(count).fill(0)};
 forceWaves.call(commander);
 check(commander.m_iFirstWaveCount===Math.ceil(count/2),'forced first wave uses a bounded half of the packages');
 check(commander.m_aWave.filter(x=>x===0).length===Math.ceil(count/2),'lead count matches actual wave assignments');
 const before=commander.m_aWave.join(',');forceWaves.call(commander);check(commander.m_aWave.join(',')===before,'forcing waves twice is idempotent');
}
// Response completion between one-second stronghold polls must retain the actual dispatch receipt.
{
 const update=compile('stronghold','UpdateResponse');
 for(const deployed of [false,true])for(const final of [false,true]){
  let banners=0;
  operations={GetOperation:()=>({GetState:()=>4,IsTerminal:()=>true,m_TacticalCommander:{m_iFirstDispatch:deployed?10:-1}})};
  const ctx={m_sResponseId:'op',m_bResponseDeployed:false,m_bFinalDefenceActive:final,m_iClock:100,m_iPlayerKills:0,m_iRetaliationKills:4,m_iFinalDefenceRearm:1800,RETRY_SECONDS:30,m_Location:{GetId:()=> 'fort'},Announce(){banners++}};
  update.call(ctx);
  check(ctx.m_sResponseId===''&&!ctx.m_bFinalDefenceActive,'terminal response releases its stronghold slot');
  check(deployed?(final?ctx.m_iNextResponseAt===1900:ctx.m_iPlayerKills===0):ctx.m_iNextResponseAt===130,'deployment receipt distinguishes normal cooldown from construction retry');
  check(banners===(deployed?1:0),'failed preparation cannot announce inbound forces');
  if(!deployed&&!final)check(ctx.m_iPlayerKills===4,'failed retaliation retains its kill threshold for a retry');
 }
 let banners=0,state=1;
 const live={GetState:()=>state,IsTerminal:()=>false};
 operations={GetOperation:()=>live};
 const ctx={m_sResponseId:'live',m_bResponseDeployed:false,Announce(){banners++}};
 update.call(ctx);update.call(ctx);check(banners===0,'accepted but unspawned requests remain silent');
 state=2;update.call(ctx);update.call(ctx);check(banners===1,'confirmed ACTIVE deployment announces once across repeated polls');
}
// Deleted living defenders are not fabricated kills, and stale subscriptions are detached.
{
 const refresh=compile('stronghold','RefreshWatches');let detach=0,kills=0;
 const missing={m_Entity:null,m_iPlayerId:7,m_iHitAt:10,IsDead:()=>true,Detach_S(){detach++}};
 const ctx={m_aWatches:[missing],m_iClock:15,KILL_CREDIT_SECONDS:30,OnDefenderKilledByPlayer_S(){kills++}};
 check(!refresh.call(ctx)&&kills===0&&detach===1&&ctx.m_aWatches.length===0,'retired defender cannot trigger a retaliation');
 const death=compile('stronghold','IsDead');
 check(!death.call({m_Entity:null,m_DamageManager:{IsDestroyed:()=>true}}),'null alone is not confirmed defender death');
 check(death.call({m_Entity:{GetCharacterController:()=>({IsDead:()=>true})},m_DamageManager:{IsDestroyed:()=>false}}),'actual character death is confirmed independently of hull damage state');
}
// Capture policy and persistence use the same home-role authority.
{
 const allowed=compile('stronghold','AllowsCaptureBy'),restore=compile('stronghold','Restore_S');
 for(const home of [1,2])for(const role of [0,1,2])check(allowed.call({m_eHomeRole:home},role)===(role===0||role===home),'only the home role and Resistance may capture');
 const ctx={m_iProtectionSeconds:3600};restore.call(ctx,9000,-5);check(ctx.m_iProtectedElapsed===3600&&ctx.m_iPlayerKills===0,'restore clamps malformed counters');
 server=false;restore.call(ctx,0,99);check(ctx.m_iProtectedElapsed===3600&&ctx.m_iPlayerKills===0,'clients cannot restore campaign counters');server=true;
 env.AROR_StrongholdComponent={Cast:x=>x};
 const serialize=compile('save','Serialize'),deserialize=compile('save','Deserialize');
 const record={};serialize(null,{GetProtectedElapsed:()=>80,GetPlayerKills:()=>3},{WriteValue:(k,v)=>record[k]=v});
 let restored=null;const component={Restore_S:(...v)=>restored=v};
 check(deserialize(null,component,{ReadValue:(k,set)=>{if(!(k in record))return false;set(record[k]);return true}})&&restored.join(',')==='80,3','serializer restores elapsed time and retaliation count');
 restored=null;record.version=99;check(deserialize(null,component,{ReadValue:(k,set)=>{set(record[k]);return true}})&&!restored,'unknown save schema keeps fresh state');
}
console.log(JSON.stringify({assertions,note:'Actual-source scripted admission, funding, wave, stronghold lifecycle and save policy. Native placement, flight, garrison queues and multiplayer remain unverified.'}));
