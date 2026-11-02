// Actual-source audit scheduler checks. Native road/terrain queries are mocked.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
const source=fs.readFileSync(path.join(root,'AROR_QRFRouteAudit.c'),'utf8');
let assertions=0,server=true,scheduled=0,removed=0,busy=false,queries=[];
const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg)};
for(const [n,fn]of Object.entries({Clear(){this.length=0},Insert(x){this.push(x)},Count(){return this.length},IsEmpty(){return !this.length}}))
 Object.defineProperty(Array.prototype,n,{value:fn,configurable:true});
const sites=Array.from({length:45},(_,i)=>({GetId:()=>String(i),GetLocationType:()=>i<21?8:0,IsControlFixed:()=>false}));
sites.push({GetId:()=> 'hq',GetLocationType:()=>1,IsControlFixed:()=>true});
let airQueries=0,landingTargets=0;
const executor={ProbeGroundRoute_S(o,t){queries.push([o.GetId(),t.GetId()]);return (+o.GetId()+ +t.GetId())%2===0},
 ProbeAirLaunch_S(){airQueries++;return true},CreateLandingAudit_S(){landingTargets++;let count=0,last=-1;return {
 Advance(tick){check(tick>last,'landing search advances at most once per audit tick');last=tick;count++},IsComplete:()=>count>=3,GetDiagnostics:()=> 'mocked',GetResultCount:()=>1,GetResult:()=>[0,0,0]}}};
const operations={GetOwner:()=>({FindComponent:()=>executor}),GetOperations:out=>{if(busy)out.push({IsTerminal:()=>false})}};
const env={Replication:{IsServer:()=>server},AROR_ELocationType:{HQ:1},
 AROR_OperationManagerComponent:{GetInstance:()=>operations},AROR_LocationManagerComponent:{GetInstance:()=>({GetLocations:out=>out.push(...sites)})},
 AROR_OperationExecutorComponent:{Cast:x=>x},AROR_QRFOriginPolicy:{IsOriginType:t=>t===8},
 Print(){},LogLevel:{NORMAL:0},string:{Format:()=>''},
 GetGame:()=>({GetCallqueue:()=>({CallLater(){scheduled++},Remove(){removed++}})})};
function compile(name){
 const m=source.match(new RegExp(String.raw`^\t[^\r\n]*\b${name}\(([^\r\n]*)\)\r?\n\t\{([\s\S]*?)^\t\}`,'m'));
 const args=m[1].replace(/\b\w+\s+(\w+)/g,'$1');
 let body=m[2].replace(/array<[^>]+> (\w+) = \{\};/g,'let $1=[];')
 .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/\b(?:array<\w+>|int|bool|string|vector|AROR_\w+) (\w+)/g,'let $1').replace(/let (\w+);/g,'let $1=0;')
 .replace(/(?<![.\w])s_\w+/g,x=>'this.'+x).replace(/(?<![.\w])(Tick_S|TickLanding_S|IsIdle|Stop_S)\b/g,'this.$1');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const audit={s_aOrigins:[],s_aTargets:[],s_Executor:null,s_mLandingPads:{data:new Map(),Clear(){this.data.clear()},Set(k,v){this.data.set(k,v)},Get(k){return this.data.get(k)}}};
for(const n of ['Start_S','Tick_S','Stop_S','IsIdle','TickLanding_S'])audit[n]=compile(n);
server=false;check(!audit.Start_S()&&scheduled===0,'client cannot start a route sweep');server=true;
busy=true;check(!audit.Start_S()&&scheduled===0,'active operations block the planning audit');busy=false;
check(audit.Start_S()&&scheduled===1,'idle server starts one incremental sweep');
check(!audit.Start_S()&&scheduled===1,'duplicate start cannot duplicate timers');
for(let i=0;i<4000&&audit.s_Executor;i++)audit.Tick_S();
check(queries.length===924&&airQueries===1848,'all 21 by 45 non-self pairs receive ground, escorted-air and light-air probes exactly once');
check(landingTargets===45&&audit.s_iAirTarget===45,'every target receives one bounded five-pad landing search');
check(new Set(queries.map(p=>p.join('/'))).size===924,'no duplicate directed pair');
check(queries.every(([a,b])=>a!==b&&b!=='hq'),'HQ and self-target routes excluded');
check(audit.s_iChecked===924&&audit.s_iPassed===452,'native planning outcomes counted without converting them into driving results');
check(audit.s_Executor===null&&audit.s_aOrigins.length===0&&removed===1,'completion releases references and cancels the timer');
check(audit.Start_S()&&scheduled===2,'another disposable run can start after completion');
busy=true;audit.Tick_S();check(audit.s_Executor===null&&removed===2,'new gameplay operations stop the audit cleanly');
console.log(JSON.stringify({assertions,note:'Audit scheduling and coverage only. Run the sweep in Workbench for native planning evidence.'}));
