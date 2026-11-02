// Actual-source policy checks. Native spawning, REAPER, widgets and replication remain mocked.
// Run: node tools/Test-MarsAndAttacks.js
const fs = require('fs'), path = require('path');
const root = path.join(__dirname, '../AROpenResistance/Open Resistance');
const read = name => fs.readFileSync(path.join(root, 'Scripts/Game/Operations', name + '.c'), 'utf8');
const sources = Object.fromEntries(['AROR_MarsAccess','AROR_MarsTargets','AROR_MarsOperationManager',
  'AROR_OperationManagerComponent','AROR_OperationExecutorComponent','AROR_TerritoryAttackState',
  'AROR_TerritoryAttackManager','AROR_ForceProfile','AROR_GroundQRFRecipe','AROR_ForcePool',
  'AROR_ReaperQRFExecutor','AROR_ResponsePolicy','AROR_ResponseSelection'].map(name => [name, read(name)]));
let assertions = 0;
const responseCoverage=[];
function check(ok, label) { assertions++; if (!ok) throw Error(label); }
function extension(proto, name, value) { Object.defineProperty(proto, name, {value, configurable:true}); }
extension(Array.prototype,'Count',function(){return this.length;});
extension(Array.prototype,'IsEmpty',function(){return !this.length;});
extension(Array.prototype,'Clear',function(){this.length=0;});
extension(Array.prototype,'Insert',function(x){this.push(x);});
extension(Array.prototype,'Contains',function(x){return this.includes(x);});
extension(Array.prototype,'Find',function(x){return this.indexOf(x);});
extension(Array.prototype,'Remove',function(i){this.splice(i,1);});
extension(Array.prototype,'RemoveItem',function(x){const i=this.indexOf(x);if(i>=0)this.splice(i,1);});
extension(String.prototype,'IsEmpty',function(){return !this.length;});
extension(String.prototype,'Length',function(){return this.length;});
extension(String.prototype,'IndexOf',function(x){return this.indexOf(x);});
extension(String.prototype,'Substring',function(start,len){return this.substring(start,start+len);});
extension(String.prototype,'ToInt',function(){return parseInt(this,10)||0;});
extension(String.prototype,'Contains',function(x){return this.includes(x);});
extension(String.prototype,'Compare',function(x){return this<x?-1:this>x?1:0;});
extension(Number.prototype,'ToString',function(){return this.toString();});
const roles={RESISTANCE:0,OCCUPIERS:1,INVADERS:2,CIVILIANS:3};
const types={MOTORIZED:0,MECHANIZED:1,ARMOUR:2,AIRBORNE:3,INFANTRY:4};
const opTypes={ATTACK:0,QRF:1};
const states={PLANNED:0,SPAWNING:1,ACTIVE:2,SUCCEEDED:3,FAILED:4,CANCELLED:5};
let printed=[];
let server=true, currentLocations=[], player={GetOrigin:()=>[0,0,0]}, hints=[];
const locations={GetLocations:out=>out.push(...currentLocations),GetLocationById:id=>currentLocations.find(x=>x.GetId()===id)};
const env={
	AROR_EResponseType:{...types,COMBINED_ARMS:5,LIGHT_SUPPORT:6,AIR_GROUND:7},
	AROR_ResponseSelection:class {constructor(){Object.assign(this,{m_aElements:[],m_sProfileId:'',m_sRecipeId:'',m_sComposition:'',m_iGroups:0,m_iPersonnel:0,m_iVehicles:0});}},
	float:{MAX:Number.MAX_VALUE},
  AROR_EFactionRole:roles,AROR_EForceType:types,AROR_EOperationType:opTypes,AROR_EOperationState:states,
  AROR_ELocationType:{HQ:1,OUTPOST:8}, AROR_MarsEntry:class {},
  Replication:{IsServer:()=>server},
  Math:Object.assign(Object.create(Math),{Max:Math.max,Min:Math.min,Round:Math.round,Clamp:(v,a,b)=>Math.max(a,Math.min(b,v))}),
  string:{Empty:'',Format:(s,...args)=>s.replace(/%(\d+)/g,(_,i)=>args[i-1])},
  Print:message=>printed.push(message),LogLevel:{NORMAL:0,WARNING:1},
  GetGame:()=>({GetPlayerManager:()=>({GetPlayerControlledEntity:()=>player})}),
  AROR_NotificationManagerComponent:{SendHint_S:(...args)=>hints.push(args)},
  AROR_LocationManagerComponent:{GetInstance:()=>locations},
  AROR_FactionRolesComponent:{GetInstance:()=>({GetFactionKey:r=>r===1?'AROR_OCCUPIERS':'AROR_INVADERS'})},
  AROR_QRFSpotting:{IsResistance:x=>!!x},
  AROR_OperationExecutorComponent:{Cast:x=>x},
  AROR_QRFOriginPolicy:{IsOriginType:t=>t===8},
  AROR_RecaptureComponent:{IsContested:()=>false},AROR_StrongholdComponent:{Cast:x=>x},
  AROR_CaptureLocationAction:{IsCapturableType:t=>t===8},
  vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])}
};
const allMethods=new Set(Object.values(sources).flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])).filter(x=>!x.startsWith('AROR_')));
allMethods.add('GetOwner');
let workbenchBuild=true;
function compile(sourceName,name) {
  const source=sources[sourceName].replace(/^#ifdef WORKBENCH\r?\n([\s\S]*?)^#else\r?\n([\s\S]*?)^#endif\r?\n/gm,(_,workbench,packaged)=>workbenchBuild?workbench:packaged), match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!match)throw Error('Missing '+sourceName+'.'+name);
  const removeTypes=s=>s.replace(/\b(?:notnull\s+)?array<(?:ref\s+)?\w+>\s+/g,'').replace(/\b(?:int|float|bool|string|vector|IEntity|Faction|AROR_\w+)\s+(\w+)/g,'$1');
  const args=removeTypes(match[1]);
  let body=match[2]
    .replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g,'for (const [$1, $2] of $3.entries())')
    .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for (const $1 of $2)')
    .replace(/array<(?:ref\s+)?\w+> (\w+) = \{\};/g,'let $1 = [];')
    .replace(/\b(?:int|float|bool|string|vector|IEntity|Faction|AROR_\w+)\s+(\w+)/g,'let $1')
    .replace(/let (\w+);/g,'let $1 = 0;')
    .replace(/let (best|close) = 0;/g,'let $1 = null;')
    .replace(/super\.(\w+)\(/g,'this.Base$1(')
    .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
    .replace(/(?<![.\w])(\w+)\(/g,(s,x)=>allMethods.has(x)?'this.'+s:s)
    .replace(/([\w.]+)\.Find\(([^,]+), (\w+)\);/g,'$3 = $1.Find($2);');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
}
env.AROR_MarsAccess={Accepts:compile('AROR_MarsAccess','Accepts'),CanRequest_S:()=>true};
env.AROR_ResponseSelection.prototype.IsAvailable=compile('AROR_ResponseSelection','IsAvailable');
env.AROR_ResponsePolicy={};
env.AROR_ResponseSelection.prototype.GetComposition=compile('AROR_ResponseSelection','GetComposition');
for(const name of ['IsProfileUnlocked','IsRecipeEligible','MatchesProfileType','TypeName','Select'])env.AROR_ResponsePolicy[name]=compile('AROR_ResponsePolicy',name);
env.AROR_MarsTargets={NearestRebelTerritory:compile('AROR_MarsTargets','NearestRebelTerritory')};
const credential=sources.AROR_MarsAccess.match(/credential == "([^"]+)"/)[1];
check(env.AROR_MarsAccess.Accepts(credential),'entry accepts configured credential');
check(!env.AROR_MarsAccess.Accepts('')&&!env.AROR_MarsAccess.Accepts(credential.toLowerCase()),'entry rejects absent/incorrect credential');
function site(id,role,x,type=8,stronghold=null){return {GetId:()=>id,GetName:()=>id,GetControllingRole:()=>role,GetCenter:()=>[x,0,0],GetLocationType:()=>type,IsControlFixed:()=>false,IsFlipLocked:()=>false,GetOwner:()=>({FindComponent:()=>stronghold})};}
currentLocations=[site('hq',0,0,1),site('far-rebel',0,1000),site('enemy',1,10),site('near-rebel',0,100)];
check(env.AROR_MarsTargets.NearestRebelTerritory(locations,[0,0,0]).GetId()==='near-rebel','nearest Rebel territory selected; HQ/enemy excluded');
currentLocations=[site('hq',0,0,1),site('enemy',1,10)];
check(!env.AROR_MarsTargets.NearestRebelTerritory(locations,[0,0,0]),'no real Rebel holding means no target');

const layer=fs.readFileSync(path.join(root,'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'),'utf8');
function fields(body){const obj={};for(const m of body.matchAll(/\b(m_\w+)\s+(?:"([^"]*)"|(-?\d+(?:\.\d+)?))/g))obj[m[1]]=m[2]??Number(m[3]);return obj;}
const profiles=[...layer.matchAll(/^ {4}AROR_ForceProfile \{([\s\S]*?)^ {4}\}/gm)].map(m=>fields(m[1]));
for(const p of profiles){
  for(const name of ['IsValid','HasNativeExecutor','GetGroupBudget','GetVehicleBudget','GetWeight'])p[name]=compile('AROR_ForceProfile',name);
  Object.assign(p,{m_bLightVehicle:!!p.m_bLightVehicle});
}
const recipes=[...layer.matchAll(/^ {4}AROR_GroundQRFRecipe \{([\s\S]*?)^ {4}\}/gm)].map(m=>{
  const r=fields(m[1]);r.m_eResponseType??=5;r.m_aProfileIds=[...m[1].match(/m_aProfileIds \{([\s\S]*?)\}/)[1].matchAll(/"([^"]+)"/g)].map(x=>x[1]);
  for(const name of ['IsValid','AllowsReadinessLevel','Resolve'])r[name]=compile('AROR_GroundQRFRecipe',name);return r;
});
const executor={m_aForceProfiles:profiles,m_aGroundQRFRecipes:recipes,m_bAllowAirborneResponses:true,m_bAllowJointResponses:true,m_bQRFTacticalCommander:true,m_bExecuteOperations:true,m_bQRFRequireFriendlyOrigin:true,
  BestOrigin:compile('AROR_OperationExecutorComponent','BestOrigin'),IsUsableOrigin:compile('AROR_OperationExecutorComponent','IsUsableOrigin'),
  OriginCost:(a,b)=>env.vector.DistanceXZ(a,b),IsGarrisonDepleted:()=>false,m_mOriginLaunchedAt:{Contains:()=>false},m_aRoadlessOrigins:[],m_fMinOriginDistance:1500,
  GetResponsePreview:compile('AROR_OperationExecutorComponent','GetResponsePreview'),
  IsLaunchPositionSafe:()=>true,GetMarsEntries:compile('AROR_OperationExecutorComponent','GetMarsEntries'),
  HasFriendlyQRFOrigin:compile('AROR_OperationExecutorComponent','HasFriendlyQRFOrigin'),HasMarsOrigin_S:compile('AROR_OperationExecutorComponent','HasMarsOrigin_S')};
for(const role of [1,2])for(let tier=1;tier<=5;tier++){
  const entries=[];executor.GetMarsEntries(role,tier,entries);
  check(entries.some(x=>x.m_bAirborne),'airborne present for army/tier '+role+'/'+tier);
  check(entries.some(x=>x.m_sRecipeId==='combined_arms_l1'),'combined arms present for army/tier '+role+'/'+tier);
  check(entries.every(x=>x.m_sRecipeId||profiles.find(p=>p.m_sId===x.m_sProfileId).m_eRole===role),'no cross-army profiles');
  check(new Set(entries.map(x=>x.m_sKey)).size===entries.length,'unique grid selection keys');
}
// Every type/army/Readiness/Crackdown cell resolves through the same actual policy.
for(const role of [1,2])for(let readiness=1;readiness<=5;readiness++)for(const kind of [0,1,2,3,5,6,7]){
  let previous=0;
  for(let crackdown=1;crackdown<=5;crackdown++){
    const choice=executor.GetResponsePreview(role,kind,readiness,crackdown);
    responseCoverage.push({Role:role===1?'OCCUPIERS':'INVADERS',Family:env.AROR_ResponsePolicy.TypeName(kind),Readiness:readiness,Crackdown:crackdown,Available:choice.IsAvailable(),Recipe:choice.m_sRecipeId||'',Profile:choice.m_sProfileId||'',Packages:choice.m_aElements.length,ReservedGroups:choice.m_iGroups,ReservedPersonnel:choice.m_iPersonnel,ReservedVehicles:choice.m_iVehicles,Reason:choice.m_sReason,NativeExecution:'NOT_RUN'});
    const minimum=kind===1?3:(kind===7?2:(kind===2?5:1));
    check(choice.IsAvailable()===(readiness>=minimum),'equipment gate for army/type/Readiness/Crackdown '+[role,kind,readiness,crackdown]);
    if(!choice.IsAvailable())continue;
    if(kind===3)check(choice.m_aElements.every(p=>p.m_iEscortCount===(readiness<5?0:1)),'Readiness selects light transport-only or heavy escorted air without changing intensity');
    if(kind===7&&readiness<5)check(choice.m_aElements[0].m_iEscortCount===0&&choice.m_aElements.slice(1).every(p=>p.m_eType!==types.ARMOUR||p.m_bLightVehicle),'early joint responses exclude gunships and heavy armour');
    const physicalGroups=choice.m_aElements.reduce((n,p)=>n+(p.m_eType===3?2+p.m_iEscortCount:((p.m_eType===4||(p.m_eType===2&&!p.m_bFillPassengerSeats))?1:2)),0);
    const physicalVehicles=choice.m_aElements.reduce((n,p)=>n+(p.m_eType===3?1+p.m_iEscortCount:(p.m_eType===4?0:1)),0);
    check(choice.m_iGroups===physicalGroups&&choice.m_iVehicles===physicalVehicles&&choice.m_iPersonnel===choice.m_aElements.reduce((n,p)=>n+p.m_iPersonnelBudget,0),'preview reserves actual infantry/pilot/escort groups, vehicles and personnel');
    check(choice.m_aElements.every(p=>p.m_eRole===role&&p.m_iMinimumReadinessLevel<=readiness),'no borrowed or locked profile in preview');
    check(choice.m_aElements.length>=previous,'Crackdown never shrinks the authored element count');
    previous=choice.m_aElements.length;
    if(kind!==5&&kind!==7)check(choice.m_aElements.length===crackdown,'homogeneous intensity follows Crackdown');
    else check(choice.m_aElements.length===recipes.find(r=>r.m_sId===choice.m_sRecipeId&&r.m_eRole===role).m_aProfileIds.length,'recipe intensity is authored, never multiplied twice');
  }
}
check(!executor.GetResponsePreview(1,3,0,5).IsAvailable()&&!executor.GetResponsePreview(1,3,5,6).IsAvailable(),'invalid metric ranges rejected');
check(executor.GetResponsePreview(1,0,1,3).GetComposition()==='3 MOTORIZED','preview composition describes resolved elements rather than multiplying the recipe');
executor.m_bAllowAirborneResponses=false;
check(!executor.GetResponsePreview(1,3,5,5).IsAvailable(),'air preview respects adapter switch');executor.m_bAllowAirborneResponses=true;
const custom={...recipes.find(r=>r.m_eRole===1&&r.m_iMinimumReadinessLevel===1),m_sId:'custom_response',m_eResponseType:100,m_iLevel:2};
recipes.push(custom);check(executor.GetResponsePreview(1,100,1,2).m_sRecipeId==='custom_response','new response families can be supplied by authored recipe');recipes.pop();
currentLocations=[site('target',0,100),site('origin',1,2000)];
check(executor.HasMarsOrigin_S(1,'target')&&!executor.HasMarsOrigin_S(2,'target'),'launch requires selected army ownership');
executor.m_bQRFRequireFriendlyOrigin=false;
check(!executor.HasMarsOrigin_S(2,'target'),'debug origin check cannot fall back to coordinates');
executor.m_bQRFRequireFriendlyOrigin=true;
executor.MarsOriginFailure_S=compile('AROR_OperationExecutorComponent','MarsOriginFailure_S');
check(executor.MarsOriginFailure_S(2,'target').includes('owns no eligible'),'origin diagnostic distinguishes missing USSR holdings from a missing Rebel target');
currentLocations=[site('target',0,100),site('vostok',2,2000)];
const originalClearance=executor.IsLaunchPositionSafe;
executor.IsLaunchPositionSafe=()=>false;
check(executor.MarsOriginFailure_S(2,'target').includes('clearance'),'owned USSR origin reports its failed safety gate');
executor.IsLaunchPositionSafe=originalClearance;
currentLocations=[site('target',0,100),site('origin',1,2000)];
// The old ground-only fixture and legacy blanket air override cannot change an explicit selection.
const route=compile('AROR_ReaperQRFExecutor','StartForce');
let airDispatch=0,groundDispatch=0,rejectedDispatch=0;
const router={m_aForceProfiles:profiles,m_bAllowAirborneResponses:true,m_bReaperQRFTest:true,
  GetGroundQRFRecipeId:()=> 'combined_arms_l1',StartReaperQRF:()=>airDispatch++,
  BaseStartForce:()=>groundDispatch++,FailOperation:()=>rejectedDispatch++};
const selectedAir={GetState:()=>states.SPAWNING,m_sRequestedProfileId:'us_airborne',m_sRequestedRecipeId:''};
route.call(router,selectedAir);check(airDispatch===1&&groundDispatch===0,'explicit US aircraft bypasses ground-only fixture');
selectedAir.m_sRequestedProfileId='ussr_airborne';route.call(router,selectedAir);
check(airDispatch===2,'explicit USSR aircraft reaches flight adapter');
selectedAir.m_sRequestedProfileId='us_motorized';route.call(router,selectedAir);
check(groundDispatch===1&&airDispatch===2,'explicit ground profile bypasses blanket air override');
selectedAir.m_sRequestedProfileId='';selectedAir.m_sRequestedRecipeId='combined_arms_l1';route.call(router,selectedAir);
check(groundDispatch===2&&airDispatch===2,'explicit combined arms reaches native recipe executor');
selectedAir.m_sRequestedProfileId='us_airborne';router.m_bAllowAirborneResponses=false;route.call(router,selectedAir);
check(rejectedDispatch===1&&airDispatch===2,'disabled adapter cannot silently dispatch selected aircraft as ground');
env.AROR_Operation=class {
  constructor(id,role,type,target){Object.assign(this,{id,role,type,target,state:states.PLANNED,m_sRequestedProfileId:'',m_sRequestedRecipeId:'',m_iSpawnPreparationTimeout:0});}
  GetId(){return this.id;}GetRole(){return this.role;}GetType(){return this.type;}GetTargetLocationId(){return this.target;}GetState(){return this.state;}
  GetDueTime(){return 0;}GetStateSince(){return 0;}
  IsTerminal(){return this.state>=states.SUCCEEDED;}
};
function manager(){return {
  m_bMarsEnabled:true,m_bTerritoryAttacksEnabled:true,m_iClock:1000,m_iMaxAttacks:1,m_iMaxQRFs:1,m_iNextId:0,m_iQRFCooldown:120,m_iAttackInterval:300,
  m_bInvaderOperationsEnabled:true,m_aOperations:[],m_mMarsNextRequest:{items:new Map(),Find(id){return this.items.get(id)||0;},Set(id,v){this.items.set(id,v);}},
  IsReady:()=>true,IsEnemyRole:r=>r===1||r===2,HasLocation:id=>!!locations.GetLocationById(id),GetNextRequestTime:()=>9999,
  GetOwner:()=>({FindComponent:()=>executor}),GetOperation(id){return this.m_aOperations.find(x=>x.id===id);},
  NotifyChanged:()=>{},MarsReply:(...x)=>hints.push(x),RequestOperation_S:compile('AROR_OperationManagerComponent','RequestOperation_S')
};}
let m=manager();const request=compile('AROR_MarsOperationManager','RequestMars_S');
{
 const denied=manager();const initialHints=hints.length;
 env.AROR_MarsAccess.CanRequest_S=()=>false;
 request.call(denied,7,credential,1,'type:3',5,5);
 request.call(denied,7,credential,1,'type:3',5,5);
 check(!denied.m_aOperations.length&&hints.length===initialHints+1,'unauthorized requests never dispatch and denial replies share the request throttle');
 denied.m_iClock+=5;request.call(denied,7,credential,1,'type:3',5,5);
 check(hints.length===initialHints+2,'denial may be retried at the configured throttle boundary');
 env.AROR_MarsAccess.CanRequest_S=()=>true;
}

request.call(m,1,credential,1,'type:2',5,1);check(!m.m_aOperations.length,'server rechecks selected Readiness and rejects locked armour');
m.m_iClock+=5;request.call(m,1,credential,1,'type:3',5,5);
check(m.m_aOperations.length===1&&m.m_aOperations[0].m_iPoolReadinessLevel===5&&m.m_aOperations[0].m_bUseResponseMetrics,'server freezes the preview metrics for explicit air dispatch');
check(m.m_aOperations[0].m_iRequestedTier===5,'server retains Crackdown separately from Readiness');
m=manager();
request.call(m,1,'bad',1,'profile:us_airborne',5);check(m.m_aOperations.length===0,'server rejects bypassed entry check');
m.m_iClock+=5;request.call(m,1,credential,1,'profile:ussr_airborne',5);check(m.m_aOperations.length===0,'server rejects cross-army selection');
m.m_iClock+=5;request.call(m,1,credential,2,'profile:ussr_airborne',5);check(m.m_aOperations.length===0,'server rejects absent launch territory');
currentLocations=[site('hq',0,0,1),site('origin',1,2000)];m.m_iClock+=5;
request.call(m,1,credential,1,'profile:us_airborne',5);check(m.m_aOperations.length===0,'server rejects HQ-only Rebel ownership');
currentLocations=[site('target',0,100),site('origin',1,2000)];m.m_iClock+=5;
request.call(m,1,credential,1,'profile:us_airborne',5);
check(m.m_aOperations.length===1&&m.m_aOperations[0].m_sRequestedProfileId==='us_airborne','explicit airborne request admitted despite ordinary cooldown');
check(m.m_aOperations[0].m_bDebugRequest&&m.m_aOperations[0].m_iRequestedTier===5,'debug intent and selected package count retained');
check(m.m_iOccupiersNextQRF===undefined,'debug request leaves normal QRF cooldown untouched');
m.m_iClock+=5;request.call(m,1,credential,1,'recipe:combined_arms_l1',1);
check(m.m_aOperations.length===1&&m.m_aOperations[0].m_sRequestedProfileId==='us_airborne','repeat request never mutates an existing force');
server=false;m=manager();request.call(m,1,credential,1,'profile:us_airborne',5);check(!m.m_aOperations.length,'client cannot admit a force');server=true;
m=manager();m.m_bMarsEnabled=false;request.call(m,1,credential,1,'profile:us_airborne',5);check(!m.m_aOperations.length,'scenario debug switch enforced');

m=manager();
m.GetNextRequestTime=()=>0;
for(const invalidType of [-1,2,99])check(!m.RequestOperation_S(1,invalidType,'target',0)&&m.m_aOperations.length===0,'unsupported operation types cannot create immortal scheduler records');
// One global attack slot, including the preparation and spawning stages and both roles.
m=manager();m.GetNextRequestTime=()=>0;
let id=m.RequestOperation_S(1,opTypes.ATTACK,'target',60);
check(!!id,'first attack admitted');
currentLocations.push(site('second-target',0,500));
check(!m.RequestOperation_S(2,opTypes.ATTACK,'second-target',60),'second army cannot create another attack');
check(m.RequestOperation_S(1,opTypes.ATTACK,'target',60)===id,'matching attack deduplicates');
m.m_aOperations[0].state=states.SPAWNING;
check(!m.RequestOperation_S(2,opTypes.ATTACK,'second-target',60),'spawning attack holds global slot');
m.m_aOperations[0].state=states.ACTIVE;
check(!m.RequestOperation_S(2,opTypes.ATTACK,'second-target',60),'active attack holds global slot');
m.m_aOperations[0].state=states.FAILED;
check(!!m.RequestOperation_S(2,opTypes.ATTACK,'second-target',60),'terminal attack releases global slot');

const processOperations=compile('AROR_OperationManagerComponent','ProcessOperations');
let outcome;
const completion={m_iClock:10,m_iSpawnTimeout:90,ActiveTimeoutFor:()=>900,HasLocation:()=>true,
  GetOperations:out=>out.push(attackForCompletion),
  ChangeState_S:(op,state,reason)=>outcome={state,reason}};
let attackForCompletion=new env.AROR_Operation('capture',1,opTypes.ATTACK,'target');
attackForCompletion.state=states.ACTIVE;
currentLocations=[site('target',0,100)];processOperations.call(completion,opTypes.ATTACK);
check(!outcome,'arrival/activity without canonical capture cannot succeed');
currentLocations=[site('target',1,100)];processOperations.call(completion,opTypes.ATTACK);
check(outcome.state===states.SUCCEEDED,'canonical capture resolves an active attack');
attackForCompletion.state=states.PLANNED;processOperations.call(completion,opTypes.ATTACK);
check(outcome.state===states.CANCELLED,'target lost during preparation cancels without false success');
const idle=compile('AROR_OperationExecutorComponent','FinishGroundQRFIfIdle');
const idleContext={HasOperatingQRF:()=>false,HasPendingQRF:()=>false,
  m_Operations:{ResolveOperation_S:(id,state)=>outcome={state}},FailOperation:()=>outcome={state:states.FAILED}};
attackForCompletion.state=states.ACTIVE;currentLocations=[site('target',0,100)];idle.call(idleContext,attackForCompletion);
check(outcome.state===states.FAILED,'withdrawal without capture fails the attack');
currentLocations=[site('target',1,100)];idle.call(idleContext,attackForCompletion);
check(outcome.state===states.SUCCEEDED,'return tick also honors an already confirmed capture');
currentLocations=[site('target',0,100),site('origin',1,2000)];

attackForCompletion=new env.AROR_Operation('air-plan',1,opTypes.QRF,'target');attackForCompletion.state=states.SPAWNING;
attackForCompletion.m_iSpawnPreparationTimeout=121;completion.m_iClock=90;outcome=null;
processOperations.call(completion,opTypes.QRF);check(!outcome,'bounded outer landing search is not cut off by normal 90-second timeout');
completion.m_iClock=121;processOperations.call(completion,opTypes.QRF);check(outcome.state===states.FAILED,'air preparation allowance still expires');
attackForCompletion.m_iSpawnPreparationTimeout=0;completion.m_iClock=90;outcome=null;
processOperations.call(completion,opTypes.QRF);check(outcome.state===states.FAILED,'ordinary ground spawn timeout unchanged');

const TAM='AROR_TerritoryAttackManager';
env.AROR_TerritoryAttackState=class {constructor(){this.m_iQRFCompletedAt=-1;this.m_sActiveQRF='';} };
env.AROR_TerritoryAttackState.prototype.IsDue=compile('AROR_TerritoryAttackState','IsDue');
let state=new env.AROR_TerritoryAttackState();Object.assign(state,{m_sLocationId:'target',m_iCapturedAt:1000,m_iNextAttempt:1000});
check(!state.IsDue(1299,300,300)&&state.IsDue(1300,300,300),'a held site is due once its capture grace ends, with no capture QRF required');
state.m_sActiveQRF='qrf';check(!state.IsDue(5000,300,300),'a capture QRF still deployed on the site holds the attack');state.m_sActiveQRF='';
state.m_iQRFCompletedAt=1800;
check(!state.IsDue(2099,300,300)&&state.IsDue(2100,300,300),'post-QRF recovery independently enforced');
state.m_iQRFCompletedAt=-1;
const changed=compile(TAM,'NotifyChanged');
function attackPolicy(target){return Object.assign(target,{BaseTick:()=>{},BaseNotifyChanged:()=>{},m_AttackLocations:locations,m_iNextTerritoryAttackTick:0,m_iAttackRestUntil:0,m_aQRFThreats:[],m_aAttackTerritories:[],
  m_iTerritoryAttackGrace:300,m_iTerritoryAttackAfterQRF:300,m_iTerritoryAttackRest:900,m_bQRFStrategicEconomy:true,m_sAttackHoldKey:'',m_sAttackFailure:'',
  OutstandingAttack:compile(TAM,'OutstandingAttack'),QueuedThreats:compile(TAM,'QueuedThreats'),FrontDistance:compile(TAM,'FrontDistance'),
  LaunchTerritoryAttack_S:compile(TAM,'LaunchTerritoryAttack_S'),GetAttackFailure:compile(TAM,'GetAttackFailure'),HoldAttacks:compile(TAM,'HoldAttacks'),
  AttackWaitReason:compile(TAM,'AttackWaitReason'),IsAttackTerritory:compile(TAM,'IsAttackTerritory'),
  OnAttackTerritoryRemoved:compile(TAM,'OnAttackTerritoryRemoved'),OnAttackTerritoryChanged:compile(TAM,'OnAttackTerritoryChanged'),
  RecordTerritoryOperationChange:compile(TAM,'RecordTerritoryOperationChange'),
  GetCrackdownLevel:()=>3,GetReadinessLevel:()=>2,ScheduleAttack_S(role,target){return this.RequestOperation_S(role,opTypes.ATTACK,target,60);},GetNextRequestTime:()=>0});}
m=attackPolicy(manager());m.m_aAttackTerritories=[state];
let qrf=new env.AROR_Operation('qrf',1,opTypes.QRF,'target');qrf.m_bCaptureThreatRecorded=true;
qrf.state=states.FAILED;changed.call(m,qrf);check(state.m_iQRFCompletedAt===-1&&!state.m_sActiveQRF,'a capture QRF that never deployed leaves no recovery wait');
qrf.state=states.ACTIVE;qrf.m_bDebugRequest=true;changed.call(m,qrf);check(!state.m_sActiveQRF,'debug QRF never holds automatic attacks');
qrf.m_bDebugRequest=false;changed.call(m,qrf);check(state.m_sActiveQRF==='qrf','physically activated capture QRF tracked');
qrf.state=states.FAILED;m.m_iClock=1200;changed.call(m,qrf);check(state.m_iQRFCompletedAt===1200&&!state.m_sActiveQRF,'its end starts the recovery wait');

// The Occupiers attack as soon as the Resistance holds territory: grace, then the front site nearest their base.
const tick=compile(TAM,'Tick');
executor.CanFundQRFThreat_S=()=>true;
{
 currentLocations=[site('deep',0,100),site('front',0,3000),site('origin',1,5000)];printed=[];
 const occupiers=attackPolicy(manager());
 for(const s of currentLocations)occupiers.OnAttackTerritoryChanged(s);
 check(occupiers.m_aAttackTerritories.length===2,'every Resistance-held military site (captured or loaded) is tracked at once');
 occupiers.m_iClock=1299;tick.call(occupiers);
 check(!occupiers.m_aOperations.length&&printed.some(x=>x.includes('capture grace')),'no attack inside the capture grace, and the log says so');
 occupiers.m_iClock=1315;tick.call(occupiers);
 const first=occupiers.m_aOperations[0];
 check(occupiers.m_aOperations.length===1&&first.type===opTypes.ATTACK&&first.role===1,'grace over: the Occupiers attack without waiting for a capture QRF');
 check(first.target==='front','the front site nearest an Occupier base is attacked first');
 check(first.m_iRequestedTier===3&&first.m_iPoolReadinessLevel===2,'attack freezes Crackdown and Readiness separately');
 occupiers.m_iClock+=15;tick.call(occupiers);check(occupiers.m_aOperations.length===1,'campaign never duplicates an outstanding attack');
 first.state=states.FAILED;changed.call(occupiers,first);
 check(occupiers.m_iAttackRestUntil===occupiers.m_iClock+900,'Occupier rest starts when their attack ends');
 occupiers.m_iClock+=30;tick.call(occupiers);check(occupiers.m_aOperations.length===1,'rest blocks the next attack');
 occupiers.m_iClock=occupiers.m_iAttackRestUntil;
 const invaderAttack=new env.AROR_Operation('operation_7',2,opTypes.ATTACK,'deep');occupiers.m_aOperations.push(invaderAttack);
 tick.call(occupiers);check(occupiers.m_aOperations.length===2&&printed.some(x=>x.includes('waiting for attack operation_7')),'an Invader attack holds the single global attack slot, and the log names it');
 invaderAttack.state=states.SUCCEEDED;const rest=occupiers.m_iAttackRestUntil;changed.call(occupiers,invaderAttack);
 check(occupiers.m_iAttackRestUntil===rest,'an Invader attack ending never starts the Occupier rest');
 occupiers.m_aOperations.push(new env.AROR_Operation('operation_8',1,opTypes.QRF,'elsewhere'));
 occupiers.m_aQRFThreats=[{m_eRole:2}];occupiers.m_iClock+=15;printed=[];tick.call(occupiers);
 check(occupiers.m_aOperations.filter(op=>op.type===opTypes.ATTACK&&!op.IsTerminal()).length===1,'a QRF elsewhere and Invader threats no longer starve Occupier attacks');
 const second=occupiers.m_aOperations.find(op=>op.type===opTypes.ATTACK&&!op.IsTerminal());
 second.state=states.CANCELLED;occupiers.m_iAttackRestUntil=0;occupiers.m_aQRFThreats=[{m_eRole:1}];occupiers.m_iClock+=60;printed=[];tick.call(occupiers);
 check(!occupiers.m_aOperations.some(op=>op.type===opTypes.ATTACK&&!op.IsTerminal())&&printed.some(x=>x.includes('queued Occupier capture response')),'their own queued capture response takes priority');
 occupiers.m_aQRFThreats=[];executor.CanFundQRFThreat_S=()=>false;occupiers.m_iClock+=15;printed=[];
 for(const s of occupiers.m_aAttackTerritories)s.m_iNextAttempt=0;tick.call(occupiers);
 check(!occupiers.m_aOperations.some(op=>op.type===opTypes.ATTACK&&!op.IsTerminal())&&printed.some(x=>x.includes('cannot fund')),'an unaffordable attack takes no slot and is logged');
 executor.CanFundQRFThreat_S=()=>true;currentLocations=[site('deep',0,100),site('front',0,3000)];occupiers.m_iClock+=30;printed=[];
 for(const s of occupiers.m_aAttackTerritories)s.m_iNextAttempt=0;tick.call(occupiers);
 check(!occupiers.m_aOperations.some(op=>op.type===opTypes.ATTACK&&!op.IsTerminal())&&printed.some(x=>x.includes('no safe Occupier launch site')),'no Occupier base means no attack, and the log says why');
 occupiers.OnAttackTerritoryChanged(site('front',1,3000));
 check(occupiers.m_aAttackTerritories.length===1,'a site the enemy retakes leaves the attack list');
 occupiers.OnAttackTerritoryChanged(site('front',0,3000));
 check(occupiers.m_aAttackTerritories.find(s=>s.m_sLocationId==='front').m_iCapturedAt===occupiers.m_iClock,'a recapture restarts that site grace');
}
// Strongholds and test attacks.
{
 const policy=attackPolicy(manager());
 const vostok=site('vostok',0,100,8,{AllowsCaptureBy:r=>r===2||r===0});
 const delaware=site('delaware',0,100,8,{AllowsCaptureBy:r=>r===1||r===0});
 check(!policy.IsAttackTerritory(vostok,1)&&policy.IsAttackTerritory(vostok,2),'a Resistance-held Vostok is only attackable by the Invaders');
 check(policy.IsAttackTerritory(delaware,1),'their own stronghold remains a valid Occupier target');
 policy.OnAttackTerritoryChanged(vostok);check(!policy.m_aAttackTerritories.length,'the Occupier list skips a stronghold they cannot take');
 const debugAttack=new env.AROR_Operation('mars',1,opTypes.ATTACK,'delaware');debugAttack.m_bDebugRequest=true;debugAttack.state=states.FAILED;
 changed.call(policy,debugAttack);check(policy.m_iAttackRestUntil===0,'a finished test attack never starts the campaign attack rest');
 const waitingSite=new env.AROR_TerritoryAttackState();Object.assign(waitingSite,{m_sLocationId:'delaware',m_iCapturedAt:0,m_iNextAttempt:0});policy.m_iClock=5000;
 policy.m_aOperations=[new env.AROR_Operation('operation_9',1,opTypes.QRF,'delaware')];
 check(policy.AttackWaitReason(waitingSite)==='Occupier QRF operation_9 still responding','an attack waits for their own QRF on the site instead of being refused by the scheduler');
 policy.m_aOperations[0].state=states.FAILED;check(policy.AttackWaitReason(waitingSite)==='','once that QRF ends the site is due');
}
// The shared launch path for both armies.
{
 currentLocations=[site('target',0,100),site('origin',1,5000),site('vostok',2,-5000)];
 const launcher=attackPolicy(manager());
 const invaderId=launcher.LaunchTerritoryAttack_S(2,'target');
 check(!!invaderId&&launcher.m_aOperations[0].role===2&&launcher.m_aOperations[0].type===opTypes.ATTACK,'the Invader push launches a real attack through the shared path');
 check(launcher.LaunchTerritoryAttack_S(2,'target')===invaderId&&launcher.m_aOperations.length===1,'asking again returns the outstanding attack instead of a second one');
 check(!launcher.LaunchTerritoryAttack_S(1,'target')&&launcher.GetAttackFailure().includes('scheduler refused'),'the other army is refused while that attack runs, with a reason');
 currentLocations=[site('target',0,100),site('origin',1,5000)];launcher.m_aOperations[0].state=states.FAILED;
 check(!launcher.LaunchTerritoryAttack_S(2,'target')&&launcher.GetAttackFailure().includes('no safe launch site'),'no Invader base means no Invader attack');
 currentLocations=[site('target',0,100),site('origin',1,2000)];
}
// Workbench MARS test attacks: the territory-attack response without campaign timers, still one attack at a time.
{
 currentLocations=[site('target',0,100),site('origin',1,2000)];
 const tester=manager();tester.m_iClock+=5;const before=hints.length;
 request.call(tester,1,credential,1,'attack:0',3,1);
 const launched=tester.m_aOperations[0];
 check(tester.m_aOperations.length===1&&launched.type===opTypes.ATTACK&&launched.m_bDebugRequest,'Workbench MARS launches a debug territory attack');
 check(launched.m_bUseResponseMetrics&&launched.m_iPoolReadinessLevel===1&&launched.m_iRequestedTier===3,'test attack freezes the previewed Readiness and Crackdown');
 check(tester.m_iOccupiersNextAttack===undefined&&tester.m_iOccupiersNextQRF===undefined,'test attack leaves both campaign timers untouched');
 check(hints.slice(before).some(h=>String(h[1]).includes('attack launched')),'MARS reply names the attack');
 currentLocations.push(site('second-target',0,150));tester.m_iClock+=5;
 request.call(tester,1,credential,2,'attack:0',3,1);
 check(tester.m_aOperations.length===1,'a test attack still holds the single global attack slot');
 workbenchBuild=false;
 const packagedRequest=compile('AROR_MarsOperationManager','RequestMars_S');const packaged=manager();packaged.m_iClock+=5;
 packagedRequest.call(packaged,1,credential,1,'attack:0',3,1);
 check(!packaged.m_aOperations.length&&hints.some(h=>String(h[1]).includes('only available in Workbench')),'packaged servers refuse test attacks even from a modified client');
 workbenchBuild=true;
 const real=manager();real.GetNextRequestTime=()=>0;currentLocations=[site('target',0,100),site('origin',1,2000)];
 real.RequestOperation_S(1,opTypes.ATTACK,'target',60);
 check(real.m_iOccupiersNextAttack===real.m_iClock+300,'automatic attacks still start the normal attack interval');
}
// Hold logging: once per change of reason.
{
 currentLocations=[site('target',0,100),site('origin',1,2000)];
 const quiet=attackPolicy(manager());printed=[];
 const waiting=new env.AROR_TerritoryAttackState();Object.assign(waiting,{m_sLocationId:'target',m_iCapturedAt:1000,m_iNextAttempt:1000,m_sActiveQRF:'operation_3'});
 quiet.m_aAttackTerritories=[waiting];
 tick.call(quiet);check(printed.some(x=>x.includes('capture QRF operation_3 still deployed')),'log names the capture QRF holding the site');
 const count=printed.length;quiet.m_iClock+=15;tick.call(quiet);
 check(printed.length===count,'an unchanged reason is not repeated every evaluation');
 waiting.m_sActiveQRF='';waiting.m_iQRFCompletedAt=1030;quiet.m_iClock+=15;tick.call(quiet);
 check(printed.some(x=>x.includes('capture grace')),'log reports the remaining capture grace');
 const grace=printed.length;quiet.m_iClock+=15;tick.call(quiet);
 check(printed.length===grace,'a countdown alone does not repeat the log');
 quiet.m_iClock=1310;tick.call(quiet);check(printed.some(x=>x.includes('recovery after capture QRF')),'log reports the recovery after a capture QRF');
 quiet.m_iClock=1345;tick.call(quiet);
 check(quiet.m_aOperations.some(op=>op.type===opTypes.ATTACK)&&quiet.m_sAttackHoldKey==='','a scheduled attack clears the reason');
 currentLocations=[site('target',0,100),site('origin',1,2000)];
}

// Validate the separate UI resource/pool contract without pretending native rendering is tested.
const layout=fs.readFileSync(path.join(root,'UI/Layouts/Phone/AROR_Mars.layout'),'utf8');
const names=[...layout.matchAll(/\bName "([^"]+)"/g)].map(x=>x[1]);
check(new Set(names).size===names.length,'MARS widget names unique');
for(const [prefix,count]of [['MarsPanel',12],['MarsText',40],['MarsBtnBg',40],['MarsBtnLabel',40],['MarsBtn',40]])
  check(Array.from({length:count},(_,i)=>prefix+i).every(x=>names.includes(x)),'complete '+prefix+' pool');
check(names.includes('MarsCredential')&&names.includes('MarsApp')&&names.includes('MarsBackground'),'MARS entry/root widgets present');
check(names.includes('MarsReadinessSlider')&&names.includes('MarsCrackdownSlider'),'both native metric sliders declared');
check(names.includes('MarsBackdrop'),'circular background canvas declared');
if(typeof process!=='undefined'&&process.argv.includes('--export-coverage')){
  const columns=Object.keys(responseCoverage[0]),quote=x=>'"'+String(x??'').replace(/"/g,'""')+'"';
  const csv=[columns.map(quote).join(','),...responseCoverage.map(row=>columns.map(k=>quote(row[k])).join(','))].join('\n')+'\n';
  const output=path.join(__dirname,'../docs/validation/CHICKEN_RESPONSE_COVERAGE.csv');
  fs.mkdirSync(path.join(__dirname,'../docs/validation'),{recursive:true});fs.writeFileSync(output,csv);
}
if(typeof module!=='undefined')module.exports={responseCoverage};
console.log(JSON.stringify({assertions,note:'Native AI, REAPER flight, UI and replication require Workbench compile and Play.'}));
