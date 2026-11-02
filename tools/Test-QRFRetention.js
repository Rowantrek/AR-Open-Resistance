/* Actual-source checks for retained QRF packages and the launch-wait diagnostics: idle packages that never boarded
 * must not hold the live ground cap, and are cleared once no player is close; dispatched ones keep the normal rules.
 * Native AI, distance culling and deletion are mocked. Run: node tools/Test-QRFRetention.js
 */
const fs=require('fs'),path=require('path');
const source=fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_OperationExecutorComponent.c'),'utf8');
let assertions=0;
const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
for(const [name,fn]of Object.entries({Count(){return this.length;},Insert(x){this.push(x);},Contains(x){return this.includes(x);},IsEmpty(){return !this.length;}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
let players=[],released=[],clock=1000;
const env={Math:{Max:Math.max,Min:Math.min},vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},Replication:{IsServer:()=>true},
 string:{Format:(s,...a)=>s.replace(/%(\d+)/g,(_,n)=>String(a[n-1]))},UNDISPATCHED_CLEANUP_DISTANCE:300,
 GetGame:()=>({GetPlayerManager:()=>({GetPlayers:out=>out.push(...players.map((p,i)=>i)),GetPlayerControlledEntity:i=>({GetOrigin:()=>players[i]})}),
  GetWorld:()=>({GetSurfaceY:()=>10})}),
 AROR_ReaperAirForce:{Cast:f=>f&&f.air?f:null},AROR_GroundOperationForce:{Cast:f=>f&&f.ground?f:null},
 AROR_EconomyComponent:{GetInstance:()=>({})},AROR_RecaptureComponent:{IsContested:id=>id==='contested'},
 AROR_QRFOriginPolicy:{IsOriginType:t=>t===8},AROR_EForceType:{ARMOUR:2}};
const methods=['LiveGroundQRFCount','MonitorTacticalRetention','TacticalEntitiesFar','ThreatWaitReason_S','HasFriendlyQRFOrigin','IsLaunchPositionSafe','IsGarrisonDepleted','ReleaseForce','ReplaceTacticalWaypoint','BestOrigin','RequestDismount_S'];
function compile(name){
 const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
 if(!m)throw Error('Missing '+name);
 const args=m[1].replace(/\b(?:AROR_\w+|float|int|bool|string|vector)\s+(\w+)(\s*=\s*[^,]+)?/g,'$1$2');
 let body=m[2].replace(/array<[^>]+> (\w+) = \{\};/g,'let $1=[];')
  .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
  .replace(/int (\w+), (\w+), (\w+), (\w+), (\w+), (\w+);/g,'let $1=0,$2=0,$3=0,$4=0,$5=0,$6=0;')
  .replace(/\b(?:int|float|bool) (\w+);/g,'let $1=0;').replace(/\bstring (\w+);/g,"let $1='';")
  .replace(/\b(?:AROR_\w+|IEntity|Physics|vector) (\w+);/g,'let $1=null;')
  .replace(/\b(?:int|float|bool|string|vector|IEntity|Physics|AROR_\w+) (\w+) =/g,'let $1 =')
  .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
 for(const n of methods)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const at=(x,z)=>({GetOrigin:()=>[x,0,z]});
function pkg(extra){return Object.assign({ground:true,m_bTacticalRetained:true,m_bOrdersIssued:false,m_bDismounted:true,m_iTacticalNextOrder:0,m_iTacticalRetainedAt:clock,
 m_Group:null,m_Profile:{m_eType:0},m_Vehicle:at(0,0),CountLivingMembers:()=>5,CountCrew:()=>1,GetOwnedMembers:out=>out.push(at(0,0))},extra);}
const executor={m_aForces:[],m_iExecutorClock:clock,m_iQRFGroundRetention:120,m_fQRFGroundCleanupDistance:1200,m_aRoadlessOrigins:[],
 m_mOriginLaunchedAt:{Contains:id=>id==='busy',Get:()=>clock-10},m_iOriginLaunchCooldown:600,m_fMinTargetDistance:150,m_fMinPlayerDistance:100,
 m_bQRFRequireFriendlyOrigin:true,m_Operations:{GetOperation:()=>null},
 ReleaseForce(f){released.push(f);this.m_aForces.splice(this.m_aForces.indexOf(f),1);},IsGarrisonDepleted:site=>site.depleted,
 BestOrigin:()=>null,ReplaceTacticalWaypoint:()=>true};
for(const n of ['LiveGroundQRFCount','MonitorTacticalRetention','TacticalEntitiesFar','ThreatWaitReason_S','HasFriendlyQRFOrigin','IsLaunchPositionSafe'])executor[n]=compile(n);

// The live ground cap
const idle=pkg({}),fighting=pkg({m_bOrdersIssued:true}),fresh=pkg({m_bTacticalRetained:false}),air=pkg({air:true,m_bTacticalRetained:false});
executor.m_aForces=[idle,idle,idle,fighting,fresh,air];
check(executor.LiveGroundQRFCount()===2,'packages that never boarded do not hold the live ground cap; dispatched and live ones still do, aircraft never');
fighting.CountLivingMembers=()=>0;fighting.CountCrew=()=>0;
check(executor.LiveGroundQRFCount()===1,'a dispatched package with nobody left alive frees its place');

// Cleanup of idle packages
executor.m_aForces=[idle];players=[[250,0,0]];executor.MonitorTacticalRetention(idle);
check(!released.length,'an idle package is kept while a player is close enough to watch it vanish');
players=[[400,0,0]];idle.m_iTacticalNextOrder=0;executor.MonitorTacticalRetention(idle);
check(released.includes(idle),'an idle package is cleared once no player is within 300 m, with no minimum age');
const dispatched=pkg({m_bOrdersIssued:true});executor.m_aForces=[dispatched];released=[];players=[[2000,0,0]];executor.MonitorTacticalRetention(dispatched);
check(!released.length,'a package that fought keeps its minimum retention age');
executor.m_iExecutorClock=clock+200;dispatched.m_iTacticalNextOrder=0;players=[[900,0,0]];executor.MonitorTacticalRetention(dispatched);
check(!released.length,'a package that fought still waits for every player to be beyond the cleanup distance');
dispatched.m_iTacticalNextOrder=0;players=[[1300,0,0]];executor.MonitorTacticalRetention(dispatched);
check(released.includes(dispatched),'then it is cleared as before');

// The capture-response wait names the blocker
const site=(id,role,x,extra={})=>Object.assign({GetId:()=>id,GetName:()=>id,GetControllingRole:()=>role,GetLocationType:()=>8,GetCenter:()=>[x,0,0]},extra);
const sites=[site('target',0,0),site('near-player',1,1000),site('contested',1,2000),site('busy',1,3000),site('tired',1,4000,{depleted:true}),site('pinewood',1,5000),site('other-army',2,6000)];
env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocations:out=>out.push(...sites),GetLocationById:id=>sites.find(s=>s.GetId()===id)})};
env.AROR_FactionRolesComponent={GetInstance:()=>({GetFactionKey:()=>'AROR_OCCUPIERS'})};
executor.ThreatWaitReason_S=compile('ThreatWaitReason_S');executor.HasFriendlyQRFOrigin=compile('HasFriendlyQRFOrigin');
executor.m_aRoadlessOrigins=['pinewood'];players=[[1000,0,50]];
const why=executor.ThreatWaitReason_S(1,2,true,'target');
check(why==='no usable launch base: owned=5 playersNearOrUnsafe=1 contested=1 launchCooldown=1 garrisonDepleted=1 noRoad=1','the wait log counts the army\'s launch bases by the reason each is unusable: '+why);
executor.BestOrigin=()=>sites[1];
check(executor.ThreatWaitReason_S(1,4,false,'target').includes('no eligible ground recipe at tier 4'),'with a usable base the wait is blamed on the recipe gate');
check(executor.ThreatWaitReason_S(1,2,false,'missing')==='target unavailable','an unknown target is reported as such');
console.log(JSON.stringify({assertions,note:'Native distance culling, AI dormancy and deletion require Workbench Play.'}));
