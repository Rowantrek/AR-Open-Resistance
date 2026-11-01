// Actual-source interruption, optional-escort, retention and funding regressions.
// Native factories, compartments, geometry and Economy are mocked.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
const src=Object.fromEntries(['AROR_OperationExecutorComponent','AROR_ReaperQRFExecutor','AROR_ReaperAirForce','AROR_ReaperRetirement','AROR_QRFFinance','AROR_QRFFunding','AROR_QRFAssetAccount','AROR_ForceProfile','AROR_GroundQRFRecipe','AROR_QRFTacticalCommander','AROR_OperationForce','AROR_GroundOperationForce'].map(n=>[n,fs.readFileSync(path.join(root,n+'.c'),'utf8')]));
let assertions=0,server=true;
const settingsEvidence=[];
const check=(ok,label)=>{assertions++;if(!ok)throw Error(label)};
for(const[n,fn]of Object.entries({Count(){return this.length},Contains(x){return this.includes(x)},IsEmpty(){return !this.length},Clear(){this.length=0},Insert(x){this.push(x)},Remove(i){this.splice(i,1)},RemoveItem(x){const i=this.indexOf(x);if(i>=0)this.splice(i,1)},Copy(x){this.splice(0,this.length,...x)}}))
 Object.defineProperty(Array.prototype,n,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const env={Replication:{IsServer:()=>server},AROR_EOperationType:{ATTACK:0,QRF:1},AROR_EOperationState:{SPAWNING:1,ACTIVE:2,FAILED:4},AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},
 AROR_EForceType:{MOTORIZED:0,MECHANIZED:1,ARMOUR:2,AIRBORNE:3,INFANTRY:4},Print(){},LogLevel:{NORMAL:0,WARNING:1},Math:{Max:Math.max,Min:Math.min,Mod:(a,b)=>a%b,Clamp:(n,a,b)=>Math.max(a,Math.min(b,n))},string:{Format:()=>''},Vector:(...x)=>x,
 GetGame:()=>({GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:e=>e?.player?7:0}),GetCallqueue:()=>({Remove(){},CallLater(){}})})};
for(const n of ['DamageManagerComponent','BaseCompartmentManagerComponent','ChimeraCharacter','FactionAffiliationComponent'])env[n]={Cast:x=>x};
env.AROR_GroundOperationForce=class {static Cast(x){return x?.ground?x:null} InitializeSpawnFaction_S(entity){return entity?.allowFaction!==false}TrackDamage_S(){}};
env.AROR_ReaperAirForce=class {static Cast(x){return x?.air?x:null}static AircraftAlive(x){return !!x?.alive}static HasPlayerOccupant(x){return !!x?.player}constructor(){this.air=true}};
env.AROR_QRFDeferredForce={Cast:x=>x?.deferred?x:null};
env.AROR_OperationForce=class {};
const names=new Set(Object.values(src).flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])));
function compile(file,name){
 const m=src[file].match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
 if(!m)throw Error('Missing actual method '+name);
 const args=m[1].replace(/\b(?:out |notnull |const )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
 let body=m[2].replace(/#ifdef WORKBENCH[\s\S]*?#endif/g,'').replace(/array<[^>]+>\s+(\w+)\s*=\s*\{([^;]*)\};/g,'let $1=[$2];')
 .replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/\b(?:IEntity|Resource|SCR_\w+|REAPER_\w+|DamageManagerComponent|BaseCompartmentManagerComponent|AROR_(?!E[A-Z])\w+)\s+(\w+);/g,'let $1=null;')
 .replace(/\b(?:bool|int|float|string|vector|ResourceName|Resource|Physics|IEntity|PlayerManager|SCR_\w+|REAPER_\w+|DamageManagerComponent|BaseCompartmentManagerComponent|BaseCompartmentSlot|ChimeraCharacter|FactionAffiliationComponent|AROR_\w+)\s+(\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1=0;')
 .replace(/super\.(\w+)\(/g,'this.Base$1(')
 .replace(/(?<![.\w])Tick_S\b/g,'this.Tick_S')
 .replace(/(?<![.\w])OnAgentAdded\b/g,'this.OnAgentAdded')
 .replace(/(?<![.\w])[ms]_\w+/g,x=>'this.'+x)
 .replace(/(?<![.\w])(\w+)\(/g,(s,n)=>names.has(n)?'this.'+s:s);
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const executor='AROR_OperationExecutorComponent',reaper='AROR_ReaperQRFExecutor',air='AROR_ReaperAirForce';
// Initial admission must leave enough time after finite pad search for the same frozen
// per-package preparation deadline enforced on active reserves.
{
 const previousSearch=env.AROR_ReaperLandingSearch;
 env.AROR_ReaperLandingSearch=class {Initialize(){}GetCandidateLimit(){return 121}GetDiagnostics(){return 'mock'}Advance(){}IsComplete(){return false}};
 const prepare=compile(reaper,'PrepareReaperLanding');
 const op={GetId:()=> 'air-planning'},ctx={m_sReaperPlanningOperation:'',m_Operations:{GetOperation:()=>null},m_iReaperPreparationTimeout:120,m_iExecutorClock:0,
  m_vReaperLZOffset:0,ReaperLandingRadius:()=>100,ReportSpawnWait(){}};
 check(!prepare.call(ctx,op,{GetCenter:()=>0,GetRadius:()=>100},5),'unfinished landing search cannot dispatch');
 check(op.m_iSpawnPreparationTimeout>=186,'landing search and five paced constructions retain the full 120-second boarding allowance');
 const deadline=op.m_iSpawnPreparationTimeout;ctx.m_iExecutorClock=20;prepare.call(ctx,op,{GetCenter:()=>0,GetRadius:()=>100},5);
 check(op.m_iSpawnPreparationTimeout===deadline,'repeated planning ticks cannot slide the initial timeout indefinitely');
 env.AROR_ReaperLandingSearch=previousSearch;
}
// Native Workbench diagnosed declaration initializers overwriting loaded resource values.
// Keep defaults in Attribute metadata; only explicit script factories may assign them.
{
 env.AROR_ForceProfile=class {};
 env.AROR_GroundQRFRecipe=class {};
 env.AROR_EResponseType={COMBINED_ARMS:5};
 for(const [file,fields]of [['AROR_ForceProfile',['m_iMinimumReadinessLevel','m_iPersonnelLossValue','m_iVehicleLossValue','m_iEscortLossValue']],['AROR_GroundQRFRecipe',['m_eResponseType','m_iMinimumReadinessLevel','m_iMaximumReadinessLevel']]]){
  const declarations=[...src[file].matchAll(/\[Attribute\([^\r\n]*\)\]\s*\w+\s+(m_\w+)([^;]*);/g)];
  check(declarations.length>0&&declarations.every(m=>!m[2].includes('=')),'attributed '+file+' fields cannot overwrite authored values with script initializers');
  const instance=compile(file,'CreateForScript')();
  for(const field of fields){
   const attribute=src[file].match(new RegExp('\\[Attribute\\("(-?[\\d.]+)"[^\\r\\n]*\\)\\]\\s*\\w+\\s+'+field+';'));
   check(attribute&&instance[field]===Number(attribute[1]),'script factory preserves the attribute default for '+file+'.'+field);
  }
 }
}
// Execute both validators against source defaults and every serialized executor in shipped resources.
{
 const defaults=Object.fromEntries([src[executor],src[reaper]].flatMap(s=>[...s.matchAll(/\[Attribute\("([-\d.]+)"[^\r\n]*\)\]\s*(?:protected )?(?:int|float|bool) (m_\w+)/g)].map(m=>[m[2],Number(m[1])])));
 const makeVector=value=>Object.defineProperty(value.trim().split(/\s+/).map(Number),'Length',{value(){return Math.hypot(...this)}});
 for(const s of [src[executor],src[reaper]])for(const m of s.matchAll(/\[Attribute\("([-\d. ]+)"[^\r\n]*\)\]\s*(?:protected )?vector (m_\w+)/g))defaults[m[2]]=makeVector(m[1]);
 const methods={TacticalSettingsValid:compile(executor,'TacticalSettingsValid'),ReaperSettingsValid:compile(reaper,'ReaperSettingsValid')};
 const fields=[...new Set(Object.keys(methods).flatMap(n=>{
  const source=n==='TacticalSettingsValid'?src[executor]:src[reaper];
  return [...source.match(new RegExp('\\b'+n+'\\(\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'))[1].matchAll(/\bm_\w+/g)].map(m=>m[0]);
 }))];
 for(const name of fields)check(Number.isFinite(defaults[name])||Array.isArray(defaults[name]),'every validated field has an explicit parsed default: '+name);
 const fixtures=[{file:'source defaults',settings:defaults}];
 const addon=path.resolve(root,'../../..');
 function visit(dir){for(const entry of fs.readdirSync(dir,{withFileTypes:true})){
  const file=path.join(dir,entry.name);
  if(entry.isDirectory())visit(file);
  else if(/\.(et|ent|layer)$/.test(file)){
   const source=fs.readFileSync(file,'utf8');
   for(const match of source.matchAll(/\bAROR_OperationExecutorComponent\b[^{}\r\n]*\{/g)){
    let end=match.index+match[0].length,depth=1;
    for(;end<source.length&&depth;end++){if(source[end]==='{')depth++;if(source[end]==='}')depth--}
    const body=source.slice(match.index,end),settings={...defaults};
    for(const m of body.matchAll(/^\s*(m_\w+)\s+(-?\d+(?:\.\d+)?)\s*$/gm))if(fields.includes(m[1]))settings[m[1]]=Number(m[2]);
    for(const m of body.matchAll(/^\s*(m_v\w+)\s+"?([-\d.]+ [-\d.]+ [-\d.]+)"?\s*$/gm))if(fields.includes(m[1]))settings[m[1]]=makeVector(m[2]);
    fixtures.push({file:path.relative(addon,file).split(path.sep).join('/'),settings});
   }
  }
 }}
 visit(addon);check(fixtures.length>1,'shipped executor instances are included, not just class defaults');
 for(const fixture of fixtures){
  for(const [name,valid]of Object.entries(methods))check(valid.call(fixture.settings),name+' accepts '+fixture.file);
  settingsEvidence.push({fixture:fixture.file,values:Object.fromEntries(fields.map(n=>[n,fixture.settings[n]]))});
 }
 for(const [field,value]of Object.entries({m_fAttackStagingDistance:199,m_iAttackStagingTimeout:601,m_iOriginLaunchCooldown:3601,m_fMinOriginDistance:3001,m_fOriginWaterPenalty:0.99}))
  check(!methods.TacticalSettingsValid.call({...defaults,[field]:value}),'invalid tactical bound still rejects '+field);
 check(!methods.ReaperSettingsValid.call({...defaults,m_iReaperPreparationTimeout:0}),'air preparation timeout cannot be disabled accidentally');
}
// Optional escorts: execute actual construction, with native creation and resource failure injection.
{
 let missing='',spawned=[],groups=[],loaded=[];
 env.Resource={Load:n=>{loaded.push(n);return n===missing?null:{name:n,IsValid:()=>true}}};
 env.AROR_ReaperFlightGeometry={Direction:()=>[0,0,1],FormationPoint:()=>[75,0,0],IsAirspaceClear:()=>missing!=='airspace'};
 env.AROR_InfantryRoster={Apply_S(){}};
 const spawn=compile(reaper,'SpawnReaperPackage');
 for(const escorted of [false,true])for(const failure of ['none','transport','gunship','airspace','clearance','transportPhysics','escortPhysics','transportFaction','escortFaction','crew','infantry','escortCrew','slots','waypoint']){
  missing=failure;spawned=[];groups=[];loaded=[];
  const profile={m_sVehicle:'transport',m_sEscortVehicle:'gunship',m_sInfantryGroup:'infantry',m_iEscortCount:escorted?1:0,m_iEscortLossValue:2000,
   PickVehicle(){return this.m_sVehicle},PickEscortVehicle(){return this.m_sEscortVehicle}};
  const force={ground:true,air:true,m_Profile:profile,m_bUnescorted:!escorted,m_iPackageIndex:0,m_iPackageCount:1,m_iReservedMembers:8,
   PreparePassengerRoster_S:()=>true,InitializeSpawnFaction_S:entity=>entity?.allowFaction!==false,TrackDamage_S(){},HoldAircraft_S(){this.held=(this.held||0)+1}};
  const ctx={m_iExecutorClock:10,m_iReaperPreparationTimeout:120,m_fReaperInsertionHeight:100,IsLaunchPositionSafe:()=>failure!=='clearance',ReaperGunshipHeight:()=>200,
   ConfigureForceFunding(f){f.m_bOwnsPackageQuote=true},SpawnWaypoint:()=>failure==='waypoint'?null:{},QRFPoolReadinessLevel:()=>3,
   // Authored aircraft only here (the profile picks no preset hull); preset fitting is covered by its own review
   SpawnPresetAircraft(owner,picked,authored,transport,launch,height){const r=env.Resource.Load(authored);return r&&r.IsValid()?this.SpawnReaperAircraft(r,launch,height):null},
   SpawnReaperAircraft(resource){const v={name:resource.name,allowFaction:failure!==(resource.name==='gunship'?'escortFaction':'transportFaction'),GetPhysics:()=>failure!==(resource.name==='gunship'?'escortPhysics':'transportPhysics')};spawned.push(v);return v},
   ReaperGroup(owner,prefab,pilot){
    const kind=owner===force?(pilot?'crew':'infantry'):'escortCrew';
    if(failure===kind)return null;
    const g={m_aUnitPrefabSlots:Array(failure==='slots'&&kind==='infantry'?10:4).fill('unit'),requests:0,RequestSpawn(n){this.requests+=n}};
    if(pilot)owner.m_CrewGroup=g;else owner.m_Group=g;groups.push(g);return g;
   }};
  const ok=spawn.call(ctx,{GetId:()=> 'test',GetRole:()=>1},force,'pilot');
  check(force.m_iPreparationDeadline===130,'construction freezes a finite per-package preparation deadline');
  const irrelevant=!escorted&&['gunship','escortPhysics','escortFaction','escortCrew'].includes(failure);
  check(ok===(failure==='none'||irrelevant),'optional-escort construction/failure outcome '+[escorted,failure]);
  check(spawned.every(v=>v===force.m_Vehicle||v===force.m_Escort?.m_Vehicle),'every partially spawned hull remains owned on failure');
  if(spawned.length&&!ok)check(force.m_bAircraftHeld&&force.held>=1,'partial construction failures hold owned hulls before validation can abort');
  if(!escorted)check(!force.m_Escort&&!loaded.includes('gunship'),'light packages never allocate or load a phantom escort');
  if(force.m_Escort)check(!force.m_Escort.m_bOwnsPackageQuote,'escort child cannot finalize the shared package before transport queues are cancelled');
  if(ok)check(groups.reduce((n,g)=>n+g.requests,0)===(escorted?6:5),'queued personnel match actual infantry and pilot counts');
  if(['airspace','clearance'].includes(failure))check(spawned.length===0,'stale launch safety fails before any actor is created');
 }
}
// Automatic air selection must inspect the entire pool, not stop at a locked heavy profile.
{
 env.AROR_ReaperRetiredForce=class {};
 env.AROR_ReaperAirForce.Controller=()=>null;
 let orders=0;env.AROR_ReaperAirForce.SetFlightOrder_S=()=>{orders++};
 const enqueue=compile('AROR_ReaperRetirement','Enqueue_S');
 for(const dispatched of [false,true])for(const escorted of [false,true]){
  orders=0;
  const group={GetPlayerCount:()=>0};
  const force={m_bOrdersIssued:dispatched,m_bDismounted:dispatched,m_CrewGroup:group,
   m_Escort:escorted?{m_CrewGroup:group}:null};
  enqueue.call({s_aForces:[],MovePrefab:()=> 'move',Tick_S(){}},force);
  check(orders===(dispatched?(escorted?2:1):0),'retirement issues egress only for already-dispatched aircraft');
 }
}
{
 const cleanup=compile(air,'Cleanup_S');
 const force={m_bImmediateCleanup:true,ReleaseAircraft_S(){},
  m_Escort:{m_bPackageQueuesPreserved:true,Cleanup_S(){}},
  BaseCleanup_S(){check(this.m_bPackageQueuesPreserved,'transport finalization inherits preserved escort spawn queues')}};
 cleanup.call(force);
}
{
 const savedLocations=env.AROR_LocationManagerComponent;
 env.AROR_CommanderManagerComponent={GetInstance:()=>({GetCommander:r=>({GetFactionKey:()=>r===1?'AROR_OCCUPIERS':'AROR_INVADERS'})})};
 env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocationById:()=>({GetCenter:()=>[1000,0,0],GetRadius:()=>100})})};
 env.AROR_ResponsePolicy={IsProfileUnlocked:(p,r)=>p.m_iMinimumReadinessLevel<=r};
 const start=compile(reaper,'StartReaperQRF');
 for(const role of [1,2])for(const readiness of [1,2,3,4,5])for(const funds of ['none','light','both'])for(const debug of [false,true]){
  const make=(heavy)=>({m_sId:heavy?'heavy':'light',m_eType:3,m_eRole:role,m_sFactionKey:role===1?'AROR_OCCUPIERS':'AROR_INVADERS',
   m_bEnabled:true,m_iMinimumReadinessLevel:heavy?5:1,m_iEscortCount:heavy?1:0,m_iPersonnelBudget:heavy?20:8,
   IsValid:()=>true,GetWeight:()=>1,GetGroupBudget:compile('AROR_ForceProfile','GetGroupBudget'),GetVehicleBudget:compile('AROR_ForceProfile','GetVehicleBudget')});
  const heavy=make(true),light=make(false);let selected='',quoted=0,failed=0,waiting=0,reason='';
  const op={m_bDebugRequest:debug,m_bUseResponseMetrics:true,m_iPoolReadinessLevel:readiness,m_iRequestedTier:2,m_sRequestedProfileId:'',
   GetId:()=> 'air',GetRole:()=>role,GetType:()=>1,GetTargetLocationId:()=> 'target',
   BindForceProfile_S(id,count){selected=id;check(count===2,'air intensity is frozen independently from equipment readiness');return true}};
  const ctx={m_aForces:[],m_aForceProfiles:[heavy,light],m_iMaxGroups:30,m_iMaxMembers:128,m_iMaxVehicles:10,m_iQRFMaxLiveAircraft:6,
   QRFPoolReadinessLevel:()=>readiness,FindForce:()=>null,HasMarsOrigin_S:()=>true,ReaperSettingsValid:()=>true,
   AffordableReaperTier:(profile,tier)=>funds==='both'||(funds==='light'&&profile===light)?tier:0,
   GetReservedGroups:()=>0,GetReservedMembers:()=>0,GetReservedVehicles:()=>0,
   PrepareReaperLanding:()=>true,m_ReaperLandingSearch:{GetResultCount:()=>1,GetResult:()=>[1000,0,0]},
   ResolveQRFOrigin:()=>[0,0,0],IsLaunchPositionSafe:()=>true,m_fReaperHoldingDistance:700,
   ReserveQRFFunding(op,elements,tier){quoted++;check(elements.length===2&&tier===2,'complete light/heavy response funds all packages once');return true},
   CreateQRFTacticalCommander(op){op.m_TacticalCommander={m_iFirstWaveCount:2}},
   ConfigureForceFunding(){},ReleaseForce(force){check(force.m_iReservedGroups===0&&force.m_iReservedMembers===0&&force.m_iReservedVehicles===0,'air reservation converts completely');this.m_aForces.RemoveItem(force)},
   FailOperation(op,message){failed++;reason=message},ReportSpawnWait(){waiting++},m_Operations:{IsQRFStrategyEnabled:()=>true}};
  start.call(ctx,op);
  const available=debug||funds!=='none',expected=readiness===5&&(debug||funds==='both')?'heavy':'light';
  check(failed===0,'funding shortages wait instead of becoming configuration failures: '+[role,readiness,funds,debug,reason]);
  check(selected===(available?expected:''),'strongest unlocked affordable air profile selected with heavy profile first');
  check(ctx.m_aForces.length===(available?2:0)&&quoted===(available?1:0),'no unfunded air construction can leak reservations');
  if(available)check(ctx.m_aForces.every(f=>f.m_bUnescorted===(selected==='light')),'light/escorted execution flag follows the frozen profile');
  if(available)check(op.m_bResponseMixAirborne===true,'both standalone air profiles mark the operation for actual-dispatch mix accounting');
  else check(waiting===1,'missing collateral uses bounded scheduler spawn wait');
 }
 env.AROR_LocationManagerComponent=savedLocations;
}
// Funding admission must continue past an earlier locked/disabled air profile.
{
 env.AROR_FactionRolesComponent={GetInstance:()=>({GetFactionKey:()=> 'army'})};
 const canFund=compile(reaper,'CanFundQRFThreat_S');
 for(const mode of ['funded','none','disabled','zeroWeight','foreign']){
  let calls=0;
  const first={m_eType:3,m_eRole:1,m_sFactionKey:'army',m_bEnabled:true,IsValid:()=>true,GetWeight:()=>1};
  const light={m_eType:3,m_eRole:1,m_sFactionKey:mode==='foreign'?'other':'army',m_bEnabled:mode!=='disabled',IsValid:()=>true,GetWeight:()=>mode==='zeroWeight'?0:1};
  const ctx={m_aForceProfiles:[first,light],m_bReaperQRFTest:true,m_Operations:{},GetGroundQRFRecipeId:()=> '',
   AffordableReaperTier(profile){calls++;return profile===light&&mode!=='none'?1:0}};
  check(canFund.call(ctx,1,1)===(mode==='funded'),'air admission respects all eligible profiles and QRF weights: '+mode);
  if(['disabled','zeroWeight','foreign'].includes(mode))check(calls===1,'ineligible light profile is never quoted: '+mode);
 }
}
// Never release aircraft physics or reposition a player-occupied package.
{
 const dispatch=compile(reaper,'DispatchReaperPackage');let effects=0;
 const force={m_Vehicle:{alive:true,player:true},m_bUnescorted:true,CountCrew:()=>1,CountInfantry:()=>4,BoardPackage_S:()=>true,ReleaseAircraft_S(){effects++}};
 check(!dispatch.call({},force)&&effects===0,'player takeover blocks flight dispatch before any physics effect');
}
// Materialized ground actors survive cancellation, while purely virtual reservations do not.
const retain=compile(executor,'ShouldRetainTacticalForce');
for(const state of ['empty','ordered','vehicle','group','crew','casualties','reservation','air']){
 const f={ground:true,m_bTacticalControlled:true,m_bReservation:state==='reservation',m_bOrdersIssued:state==='ordered',
  m_Vehicle:state==='vehicle'?{}:null,m_Group:state==='group'?{}:null,m_CrewGroup:state==='crew'?{}:null,
  air:state==='air',GetOwnedMembers:a=>{if(state==='casualties')a.push({})}};
 check(!!retain(f)===!['empty','reservation','air'].includes(state),'retention follows physical ownership, not only issued orders: '+state);
}
// Failed active reserve construction retains its partial hull and leaves healthy lead packages running.
{
 let terminal=0,lost=0;
 const partial={ground:true,m_bTacticalControlled:true,m_bOrdersIssued:false,m_Vehicle:{GetOrigin:()=>[0,0,0]},
  m_iPackageIndex:2,m_sOperationId:'test'};
 const ctx={m_sQRFConstructionOperation:'test',m_iQRFConstructionIndex:2,m_iExecutorClock:30,
  FindPackage:()=>partial,ShouldRetainTacticalForce:retain,RetainTacticalForce:compile(executor,'RetainTacticalForce'),
  ReleaseForce(){throw Error('materialized reserve hull cannot be deleted beside players')},
  m_Operations:{ResolveOperation_S(){terminal++}}};
 const op={GetState:()=>2,GetId:()=> 'test',m_TacticalCommander:{MarkLost(index){check(index===2,'only failed reserve index leaves the roster');lost++}}};
 compile(executor,'FailOperation').call(ctx,op,'injected crew resource failure');
 check(partial.m_bTacticalRetained&&lost===1&&terminal===0,'partial reserve failure retains ownership without terminating the healthy lead wave');
}
// A failed factory with no actors or queues must release its quote while healthy packages continue.
{
 const release=compile(executor,'ReleaseForce');
 for(const domain of ['ground','air','pendingAir']){
  const funding={m_iQuote:5000};
  const force={ground:true,air:domain!=='ground',m_bConstructionPending:domain==='pendingAir',m_Funding:funding,
   m_Profile:{ReplacementQuote:()=>400},m_iReservedMembers:8,GetOwnedMembers(){},Cleanup_S(){}};
  const ctx={m_aForces:[force]};
  release.call(ctx,force);release.call(ctx,force);
  check(funding.m_iQuote===4600&&force.m_iReservedMembers===0&&ctx.m_aForces.length===0,'failed empty '+domain+' releases quote once even after its construction flag was consumed');
 }
 const funding={m_iQuote:5000};
 const corpse={ground:true,m_Funding:funding,m_Profile:{ReplacementQuote:()=>400},m_iReservedMembers:8,
  GetOwnedMembers:a=>a.push({}),Cleanup_S(){}};
 release.call({m_aForces:[corpse]},corpse);
 check(funding.m_iQuote===5000,'cached physical members prevent empty-construction collateral release');
}
// Ground dispatch rechecks health, driver/seat readiness, group control and required waypoints.
{
 const dispatch=compile(executor,'DispatchReadyPackage');
 for(const failure of ['none','vehicle','crew','group','driver','destroyed','playerCrew','playerGroup','waypoint','crewWaypoint']){
  let orders=0;
  const group={GetPlayerCount:()=>failure==='playerGroup'?1:0,AddWaypoint(){orders++}};
  const crew={GetPlayerCount:()=>failure==='playerCrew'?1:0,AddWaypoint(){orders++}};
  const f={ground:true,m_Vehicle:failure==='vehicle'?null:{FindComponent:()=>({IsDestroyed:()=>failure==='destroyed'})},
   m_CrewGroup:failure==='crew'?null:crew,m_Group:failure==='group'?null:group,m_Profile:{m_eType:0},
   m_Waypoint:failure==='waypoint'?null:{},m_CrewWaypoint:failure==='crewWaypoint'?null:{},
   CountCrew:()=>1,IsMountComplete:()=>failure!=='driver'};
  const ok=dispatch.call({m_iExecutorClock:0,m_iQRFIngressTimeout:100},f,{});
  check(ok===(failure==='none')&&orders===(failure==='none'?1:0),'ground dispatch checks before ordering '+failure);
 }
}
// A withdrawal cannot dispatch newly ready reserves.
{
 const activate=compile(executor,'TryActivateOperation'),fail=compile(executor,'FailPackage');
 for(const failedIndex of [0,1,2]){
  let terminal=0,lost=[],orders=[];
  const forces=Array.from({length:3},(_,i)=>({m_sOperationId:'active',m_iPackageIndex:i,m_bReady:true}));
  const op={IsTerminal:()=>false,GetState:()=>2,GetId:()=> 'active',GetPackageCount:()=>3,
   m_TacticalCommander:{IsReserve:()=>false,MarkLost:i=>lost.push(i)}};
  const ctx={m_aForces:forces,TryActivateOperation:activate,FailPackage:fail,
   ShouldRetainTacticalForce:()=>true,RetainTacticalForce:f=>{f.m_bTacticalRetained=true},
   FailOperation(){terminal++},DispatchReadyPackage(f){if(f.m_iPackageIndex===failedIndex)return false;f.m_bOrdersIssued=true;orders.push(f.m_iPackageIndex);return true}};
  activate.call(ctx,op);
  check(terminal===0&&lost.length===1&&lost[0]===failedIndex,'one active dispatch failure degrades only its package');
  check(orders.length===2&&new Set(orders).size===2,'earlier and later healthy packages receive exactly one order despite nested activation');
 }
 let failed=0;
 const lead={m_bReady:true,m_sOperationId:'lead',m_iPackageIndex:0};
 const op={IsTerminal:()=>false,GetState:()=>1,GetId:()=> 'lead',GetRecipeId:()=> 'joint',GetPackageCount:()=>1,
  m_TacticalCommander:{m_iFirstWaveCount:1,IsReserve:()=>false}};
 activate.call({m_aForces:[lead],DispatchReadyPackage:()=>false,FailPackage:fail,FailOperation(){failed++}},op);
 check(failed===1,'a required composite lead element still aborts preparation instead of fabricating activation');
}
// Boarding waits during ACTIVE reserve construction have their own deadline, not just the parent timeout.
{
 const monitor=compile(reaper,'MonitorReaperQRF');
 for(const state of [1,2])for(const unescorted of [false,true]){
  let failed=0,waits=0;
  const group={IsDormant:()=>false,IsExpandComplete:()=>true};
  const force={m_sOperationId:'boarding',m_iPreparationDeadline:130,m_bUnescorted:unescorted,
   m_Group:group,m_CrewGroup:group,m_Vehicle:{alive:true},m_iWakeRetryTicks:0,
   m_Escort:unescorted?null:{m_Vehicle:{alive:true},m_CrewGroup:group,CountLivingMembers:()=>1},
   CountInfantry:()=>4,CountCrew:()=>1,HoldAircraft_S(){},PrepareBoarding_S:()=>true,BoardPackage_S:()=>false};
  const op={IsTerminal:()=>false,GetState:()=>state};
  const ctx={m_iExecutorClock:10,m_Operations:{GetOperation:()=>op},ReaperFactionsValid:()=>true,ReaperGroupFaction:()=>true,
   FailReaperPackage(f,o,reason){check(reason.includes('timed out'),'timeout gives an explicit boarding diagnostic');failed++},ReportSpawnWait(){waits++}};
  monitor.call(ctx,force);ctx.m_iExecutorClock=129;monitor.call(ctx,force);
  check(waits===2&&failed===0&&force.m_iPreparationDeadline===130,'seat requests cannot refresh the frozen deadline');
  ctx.m_iExecutorClock=130;monitor.call(ctx,force);
  check(failed===1&&waits===2,'initial or reserve boarding expires exactly at its own deadline');
 }
}
// A withdrawal cannot dispatch newly ready reserves.
{
 let sent=0;
 const op={IsTerminal:()=>false,GetState:()=>2,GetId:()=> 'test',GetPackageCount:()=>1,m_TacticalCommander:{m_bWithdraw:true,IsReserve:()=>false,m_iFirstWaveCount:1}};
 compile(executor,'TryActivateOperation').call({m_aForces:[{m_sOperationId:'test',m_bReady:true}],DispatchReadyPackage(){sent++;return true}},op);
 check(sent===0,'withdrawal suppresses ready-package activation');
}
// Recheck committed origins rather than trusting a cached launch coordinate.
{
 let location=null,contested=false;
 env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocationById:()=>location})};
 env.AROR_QRFOriginPolicy={IsOriginType:type=>type===8};
 env.AROR_RecaptureComponent={IsContested:()=>contested};
 const available=compile(executor,'TacticalOriginAvailable');
 const op={m_TacticalCommander:{m_sOriginLocationId:'origin'},GetRole:()=>1,GetType:()=>1};
 const ctx={m_bQRFRequireFriendlyOrigin:true};
 for(const state of ['missing','friendly','enemy','wrongType','contested']){
  location=state==='missing'?null:{GetId:()=> 'origin',GetControllingRole:()=>state==='enemy'?2:1,GetLocationType:()=>state==='wrongType'?0:8};
  contested=state==='contested';check(!!available.call(ctx,op)===(state==='friendly'),'construction origin state '+state);
 }
 op.m_TacticalCommander.m_sOriginLocationId='';
 check(!available.call(ctx,op),'required-origin mode rejects anonymous coordinates');
 ctx.m_bQRFRequireFriendlyOrigin=false;check(available.call(ctx,op),'legacy ordinary QRF fixture retains explicit coordinate opt-out');
 op.m_bDebugRequest=true;check(!available.call(ctx,op),'debug requests cannot borrow the coordinate opt-out');
}
// Aborted uncrewed aircraft remain held during retention; repeated cleanup cannot duplicate ownership.
{
 let enqueued=0,deleted=0,released=0,player=false;
 env.AROR_ReaperRetirement={Enqueue_S(){enqueued++}};
 const cleanup=compile(air,'Cleanup_S');
 const f={m_Vehicle:{},m_bOrdersIssued:false,HasPlayerOccupant:()=>player,ReleaseAircraft_S(){released++},BaseCleanup_S(){deleted++}};
 cleanup.call(f);cleanup.call(f);
 check(enqueued===1&&deleted===0&&released===0,'pre-dispatch abort retains held hull without dropping an uncrewed aircraft');
 player=true;cleanup.call(f);check(released===1&&enqueued===1,'a player taking the retained aircraft releases held simulation');
 f.m_bImmediateCleanup=true;cleanup.call(f);check(deleted===1,'safe final removal follows the normal owned cleanup path');
}
// Actual compartment occupancy is checked independently of group membership.
{
 const occupied=compile(air,'HasPlayerOccupant');
 let occupants=[];
 const vehicle={FindComponent:()=>({GetCompartments:a=>a.push(...occupants.map(e=>({GetOccupant:()=>e})))})};
 check(!occupied(null)&&!occupied(vehicle),'missing aircraft and empty seats have no player occupant');
 occupants=[{player:false}];check(!occupied(vehicle),'AI pilot is not a player takeover');
 occupants.push({player:true});check(occupied(vehicle),'player in any compartment is detected even without crew-group ownership');
}
// Retained held aircraft release physics on player takeover; distance/time gates still prevent deletion.
{
 const tick=compile('AROR_ReaperRetirement','Tick_S');let removed=0,released=0,near=true;
 const force={m_bAircraftHeld:true,m_Vehicle:{player:false},m_iRetainSeconds:10,
  ReleaseAircraft_S(){this.m_bAircraftHeld=false;released++},Cleanup_S(){removed++}};
 const ctx={s_aForces:[{m_Force:force,m_iAge:0}],ForceNearPlayers:()=>near};
 tick.call(ctx);check(released===0&&removed===0,'uncrewed aborted aircraft remain held during retention');
 force.m_Vehicle.player=true;tick.call(ctx);check(released===1&&removed===0,'takeover releases simulation without deleting the nearby aircraft');
 near=false;tick.call(ctx);check(removed===1&&ctx.s_aForces.length===0,'safe final removal drains retirement ownership');
}
// Both deferred constructors refuse a lost origin before native actor creation.
{
 const groundMonitor=compile(executor,'MonitorDeferredQRF'),airMonitor=compile(reaper,'MonitorReaperQRF');
 for(const state of [1,2])for(const domain of ['ground','air']){
  let failed=0,constructed=0;
  const tactical={m_bWithdraw:false,ReserveExpired:()=>false};
  const op={m_TacticalCommander:tactical,IsTerminal:()=>false,GetState:()=>state,GetId:()=> 'test'};
  const force={m_sOperationId:'test',m_bOrdersIssued:false,m_bConstructionPending:true};
  const ctx={m_Operations:{GetOperation:()=>op},TacticalOriginAvailable:()=>false,
   FailPackage(){failed++},FailReaperPackage(){failed++},
   StartGroundForce(){constructed++},SpawnReaperPackage(){constructed++}};
  (domain==='ground'?groundMonitor:airMonitor).call(ctx,force);
  check(failed===1&&constructed===0&&tactical.m_bWithdraw,'lost origin cancels queued '+domain+' construction before spawning in state '+state);
 }
}
// Cancellation after preparation also stops already-materialized, unissued aircraft.
{
 let failed=0;
 const op={m_TacticalCommander:{m_bWithdraw:true},IsTerminal:()=>false,GetState:()=>2};
 compile(reaper,'MonitorReaperQRF').call({m_Operations:{GetOperation:()=>op},FailReaperPackage(){failed++}},
  {m_sOperationId:'test',m_bOrdersIssued:false,m_bConstructionPending:false});
 check(failed===1,'ready-but-unissued air is cancelled during withdrawal instead of waiting forever');
}
// Fixed security tasks cannot extend a failed passenger exit deadline forever.
{
 env.vector={DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])};
 const update=compile(executor,'UpdateTacticalGround');
 for(const limit of [0,1,2,5]){
  let withdrawals=0,requests=0;
  const commander={m_bWithdraw:false,m_iTownHoldSeconds:900,m_fTownRadius:100,m_fMargin:50,m_vTown:[0,0,0]};
  const force={m_bTacticalControlled:true,m_Group:{GetPlayerCount:()=>0},m_CrewGroup:{GetPlayerCount:()=>0},
   m_Profile:{m_eType:0},m_bTacticalDropStarted:true,m_iTacticalExitStarted:0,m_iTacticalExitDeadline:30,m_iTacticalExitRetries:0,
   m_bDismountRequested:true,m_vApproachPosition:[0,0,0],m_Vehicle:{GetOrigin:()=>[0,0,0],GetPhysics:()=>({GetVelocity:()=>({Length:()=>0})})},
   HasDismountedInfantry:()=>false,ArePassengersOut:()=>false,RequestDismount_S(){requests++}};
  const op={m_TacticalCommander:commander,GetId:()=> 'test'};
  const ctx={m_iExecutorClock:0,m_iQRFExitTimeout:30,m_iQRFExitRetryLimit:limit,m_iQRFExitRecoveryDelay:10,
   RecordTacticalStrength(){},IsQRFDeliveryReached:()=>true,
   TacticalWithdrawal(f){withdrawals++;f.m_bTacticalWithdrawing=true;return true}};
  for(let window=0;window<=limit;window++){ctx.m_iExecutorClock=30*(window+1);update.call(ctx,force,op,4,1)}
  check(withdrawals===1&&force.m_iTacticalExitRetries===limit,'exit retries reach finite package withdrawal at configured limit '+limit);
  const attempts=requests;ctx.m_iExecutorClock+=30;update.call(ctx,force,op,4,1);
  check(requests===attempts&&withdrawals===2,'withdrawal never resumes blocked exit retries');
 }
}
// The heavy fifth package needs a reinforcement window after initial gunship support/egress.
{
 const commander='AROR_QRFTacticalCommander';
 const layer=fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'),'utf8');
 const reserveWait=Number(layer.match(/m_iQRFReserveWait\s+(\d+)/)[1]);
 const defaults=Object.fromEntries([...src[reaper].matchAll(/\[Attribute\("([-\d.]+)",[^\r\n]*\)\]\s*(?:protected )?(?:int|float|bool) (m_\w+)/g)].map(m=>[m[2],Number(m[1])]));
 const support=Number(layer.match(/m_iReaperSupportSeconds\s+(\d+)/)?.[1]??defaults.m_iReaperSupportSeconds);
 const c={m_iPackages:5,m_iFirstWaveCount:3,m_aWave:[0,0,0,1,1],m_iFirstDispatch:0,m_iReserveDeadline:reserveWait,m_iWaveDelay:60,m_iLosses:0,m_bWithdraw:false,
  IsReserve:compile(commander,'IsReserve'),ReserveExpired:compile(commander,'ReserveExpired')};
 const can=compile(commander,'CanConstruct');
 check(can.call(c,3,120,4,6,2),'first heavy reserve pair may construct when two live-frame slots open');
 check(!can.call(c,4,180,5,6,2),'second heavy reserve cannot exceed the live-aircraft limit');
 check(can.call(c,4,support+30,2,6,2),'authored reserve window permits the fifth package after configured support and modeled egress');
 check(!can.call(c,4,reserveWait,0,6,2),'extended Chicken reserve window remains finite');
 c.m_iReserveDeadline=support;
 check(!can.call(c,4,support+30,2,6,2),'deadline equal to support duration expires before modeled egress frees capacity');
 check(reserveWait>support,'authored reserve window leaves finite headroom beyond the support cycle');
}
// Economy remains the only bank: healthy cleanup has no refund/income and confirmed losses charge once.
{
 let bank=null;
 env.AROR_EconomyComponent={GetInstance:()=>bank};
 env.AROR_FactionRoles={GetAIMilitary:a=>a.push(1,2)};
 const asset='AROR_QRFAssetAccount',funding='AROR_QRFFunding',finance='AROR_QRFFinance';
 env.AROR_QRFAssetAccount=class {constructor(){this.m_bSettled=false}IsLost(){return compile(asset,'IsLost').call(this)}};
 env.AROR_QRFFunding=class {constructor(){Object.assign(this,{m_aAssets:[],m_aReleasedPackages:[],m_iLosses:0,m_iRecovered:0,m_bClosed:false,m_bActivated:false,m_bUnsecured:false})}};
 for(const n of ['Register_S','RegisterPackageAsset_S','ReleaseUnusedPackageQuote_S','ObserveLosses_S','Retire_S','Reserved','HasUnsettledAssets'])env.AROR_QRFFunding.prototype[n]=compile(funding,n);
 const methods=Object.fromEntries(['Debt','SetDebt_S','Available','Reserve_S','Charge_S','Activate_S','Observe_S','Tick_S'].map(n=>[n,compile(finance,n)]));
 for(const role of [1,2])for(const activated of [false,true])for(const lost of [false,true])for(const externalSpend of [0,9900]){
  let funds=10000,spent=0;
  bank={GetFactionFunds:()=>funds,SpendFactionFunds_S(r,n){check(r===role&&n<=funds,'bank never overdrafts or charges the other role');funds-=n;spent+=n;return true}};
  const manager={...methods,m_aDeployments:[],m_iOccupierDebt:0,m_iInvaderDebt:0,m_iOccupierLossValue:0,m_iInvaderLossValue:0};
  const operation={GetId:()=> 'mixed',GetRole:()=>role};
  const account=manager.Reserve_S(operation,2000,60);
  check(account&&funds===10000&&manager.Available(role)===7940,'funding reservation is collateral, never an up-front duplicate payment');
  const entity={dead:false,GetCharacterController(){return {IsDead:()=>this.dead}},FindComponent:()=>null};
  account.Register_S(entity,400,false,'army','native');account.Register_S(entity,400,false,'army','native');
  check(account.m_aAssets.length===1,'repeated registration cannot duplicate liabilities');
  funds-=externalSpend;
  if(activated){manager.Activate_S(account);manager.Activate_S(account)}
  entity.dead=lost;manager.Observe_S(account);manager.Observe_S(account);
  account.m_bClosed=true;account.Retire_S(entity);account.Retire_S(entity);
  manager.Observe_S(account);
  const bill=(activated?60:0)+(lost?400:0);
  check(spent+manager.Debt(role)===bill,'overhead and losses reconcile exactly once across external spending');
  check(account.Reserved()===0&&!account.HasUnsettledAssets(),'closed, settled deployment releases all collateral');
  check(account.m_iRecovered===(lost?0:400),'healthy cleanup releases collateral without treating it as income or combat loss');
  manager.Tick_S();check(manager.m_aDeployments.length===0,'settled deployment history is removed');
 }
 // Final owned cleanup releases only the unused package margin; future waves retain their full quote.
 {
  env.ChimeraCharacter={Cast:e=>e?.GetCharacterController?e:null};
  env.AROR_OperationDamageObserver=class {Attach_S(){return false}};
  env.SCR_EntityHelper={DeleteEntityAndChildren:e=>{e.deleted=true}};
  let currentFinance=null;
  env.AROR_OperationManagerComponent={GetInstance:()=>({GetQRFFinance:()=>currentFinance})};
  const configure=compile(executor,'ConfigureForceFunding'),convert=compile(executor,'ConvertReservation');
  const groundProfile={m_eType:0,m_iVehicleLossValue:200,m_iPersonnelLossValue:25,m_iPersonnelBudget:8,
   VehicleLossValue:compile('AROR_ForceProfile','VehicleLossValue'),ReplacementQuote:compile('AROR_ForceProfile','ReplacementQuote'),GetGroupBudget:compile('AROR_ForceProfile','GetGroupBudget')};
  const fundingStub={};const pending={m_iReservedGroups:2,m_iReservedMembers:8,m_iReservedVehicles:1},rootForce={};
  configure(pending,{m_Funding:fundingStub},groundProfile);configure(rootForce,{m_Funding:fundingStub},groundProfile);
  convert(pending,groundProfile);
  check(!pending.m_bOwnsPackageQuote&&rootForce.m_bOwnsPackageQuote&&rootForce.m_iPackageQuote===400,'conversion transfers quote finalization from virtual to physical owner');
  for(const role of [1,2])for(const state of ['healthy','lostPilot','missingPilot','missingHull','playerCapture','playerGroup']){
   let funds=10000,key='army';
   bank={GetFactionFunds:()=>funds,SpendFactionFunds_S(r,n){funds-=n;return true}};
   const manager={...methods,m_aDeployments:[],m_iOccupierDebt:0,m_iInvaderDebt:0,m_iOccupierLossValue:0,m_iInvaderLossValue:0};
   currentFinance=manager;
   const account=manager.Reserve_S({GetId:()=> 'partial',GetRole:()=>role},3600,60);
   manager.Activate_S(account);
   const pilot={player:state==='playerCapture'||state==='playerGroup',dead:state==='lostPilot',GetCharacterController(){return {IsDead:()=>this.dead}},
    FindComponent:type=>type===env.FactionAffiliationComponent?{GetAffiliatedFactionKey:()=>key}:null};
   const hull={FindComponent:type=>type===env.BaseCompartmentManagerComponent?{GetCompartments:a=>{if(pilot.player)a.push({GetOccupant:()=>pilot})}}:
    (type===env.FactionAffiliationComponent?{GetAffiliatedFactionKey:()=>key}:null)};
   const profile={m_eType:3,m_iVehicleLossValue:1600,m_iPersonnelLossValue:25,m_iPersonnelBudget:8,m_iEscortCount:0,m_iEscortLossValue:2000,
    VehicleLossValue:compile('AROR_ForceProfile','VehicleLossValue'),ReplacementQuote:compile('AROR_ForceProfile','ReplacementQuote')};
   const force={m_sOperationId:'partial',m_iPackageIndex:0,m_sFactionKey:'army',m_sAssetFactionKey:'native',
    m_aMembers:[pilot],m_aPassengers:[],m_aDamageObservers:[],m_Vehicle:hull,CountLivingMembers(){},OnAgentAdded(){},
    m_CrewGroup:{GetOnAgentAdded:()=>({Remove(){}}),GetPlayerCount:()=>state==='playerGroup'?1:0},
    BaseCleanup_S:compile('AROR_OperationForce','Cleanup_S')};
   configure(force,{m_Funding:account},profile);
   const track=compile('AROR_OperationForce','TrackDamage_S');
   track.call(force,pilot);track.call(force,hull);
   check(account.m_aAssets.length===2&&account.m_aAssets.every(a=>a.m_iPackageIndex===0),'real tracking attributes pilots and hulls to one package without duplicate registration');
   if(state==='missingPilot'){account.m_aAssets[0].m_Entity=null;force.m_aMembers=[null]}
   if(state==='missingHull'){account.m_aAssets[1].m_Entity=null;force.m_Vehicle=null}
   const cleanup=compile('AROR_GroundOperationForce','Cleanup_S');
   cleanup.call(force);
   const physical=pilot.player?1625:0;
   const unbuilt=state==='playerGroup'?175:0;
   check(account.Reserved()===1800+physical+unbuilt,'partial cleanup preserves later waves and player-owned spawn queues: '+state);
   check(funds===9940-(state==='lostPilot'?25:0),'cleanup charges only the confirmed pilot death and initial overhead');
   if(state==='missingPilot')check(account.m_iRecovered===1600,'an unobserved missing pilot is not labelled recovered');
   if(state==='missingHull')check(account.m_iRecovered===25,'an unobserved missing hull is not labelled recovered');
   const quote=account.m_iQuote,recovered=account.m_iRecovered;
   cleanup.call(force);if(!force.m_bPackageQueuesPreserved)account.ReleaseUnusedPackageQuote_S(0,1800);
   check(account.m_iQuote===quote&&account.m_iRecovered===recovered,'duplicate finalization cannot release a package margin twice');
   account.ReleaseUnusedPackageQuote_S(1,1800);
   check(account.Reserved()===physical+unbuilt,'cancelling an entirely unbuilt later wave releases only its own allocation');
   if(pilot.player){
    check(!pilot.deleted&&!hull.deleted,'actual owned cleanup preserves the player and occupied hull');
    key='captured';manager.Observe_S(account);manager.Observe_S(account);
    check(account.Reserved()===unbuilt&&funds===8315,'subsequent confirmed faction capture settles preserved assets exactly once');
   }
   account.m_bClosed=true;manager.Tick_S();
   check(manager.m_aDeployments.length===0,'package-aware settlement leaves no closed-account leak');
  }
  const legacy=new env.AROR_QRFFunding();legacy.m_iQuote=1000;
  const entity={GetCharacterController:()=>({IsDead:()=>false}),FindComponent:()=>null};
  legacy.Register_S(entity,25,false,'army','native');
  legacy.RegisterPackageAsset_S(entity,25,false,'army','native',0);
  legacy.RegisterPackageAsset_S(entity,25,false,'army','native',1);
  check(legacy.m_aAssets.length===1&&legacy.m_aAssets[0].m_iPackageIndex===0,'legacy registration upgrades attribution without duplication or reassignment to another package');
 }
 // Real Economy events can invoke reconciliation while a closed account's loss is being charged.
 for(const role of [1,2]){
  let funds=10000,nested=0;
  const manager={...methods,m_aDeployments:[],m_iOccupierDebt:0,m_iInvaderDebt:0,m_iOccupierLossValue:0,m_iInvaderLossValue:0};
  Object.defineProperty(manager.m_aDeployments,'Remove',{value(index){check(index>=0&&index<this.length,'nested reconciliation cannot remove a closed account twice');this.splice(index,1)}});
  bank={GetFactionFunds:()=>funds,SpendFactionFunds_S(r,n){funds-=n;nested++;manager.Tick_S();return true}};
  const account=manager.Reserve_S({GetId:()=> 'nested',GetRole:()=>role},2000,60);
  account.Register_S({GetCharacterController:()=>({IsDead:()=>true}),FindComponent:()=>null},400,false,'army','native');
  account.m_bClosed=true;
  manager.Tick_S();
  check(funds===9600&&nested===1&&manager.Debt(role)===0,'nested Economy callback cannot double-charge or fabricate debt');
  check(manager.m_aDeployments.length===0&&!manager.m_bReconciling,'reconciliation drains the closed account and clears its guard');
 }
 server=false;
 const manager={...methods,m_aDeployments:[],m_iOccupierDebt:0,m_iInvaderDebt:0};
 check(!manager.Reserve_S({GetRole:()=>1},100,10),'clients cannot create funding allocations');
 server=true;
}
// A valid but rejected ground seat plan keeps its actors and expires at its own bound.
{
 const monitor=compile(executor,'MonitorGroundForce');
 let failed=0,retries=0,refreshes=0;
 const group={IsDormant:()=>false,IsExpandComplete:()=>true,GetFaction:()=>({GetFactionKey:()=>'army'})};
 const force={m_sOperationId:'ground-boarding',m_sFactionKey:'army',m_Group:group,m_CrewGroup:group,
  m_Vehicle:{FindComponent:()=>null},m_bMountRequested:true,m_iMountWaitTicks:0,m_iWakeRetryTicks:0,
  CountInfantry:()=>4,CountCrew:()=>2,HasCorrectMemberFactions:()=>true,IsMountComplete:()=>false,
  GetMountRejection:()=>'gunner seat blocked',PrepareBoarding_S(refresh){if(refresh)refreshes++;return true},
  ReseatMissing_S(){retries++;return 'blocked'},ReplaceUnseated_S(){throw Error('must not spawn replacements')}};
 const operation={IsTerminal:()=>false,GetId:()=>'ground-boarding'};
 const ctx={m_Operations:{GetOperation:()=>operation},m_iQRFBoardingTimeout:3,m_iWakeRetryInterval:1,m_iQRFSeatRecoveryDelay:1,
  ValidateGroundVehicleFaction:()=>true,ReportPackageProgress(){},FailPackage(f,o,reason){
   check(f===force&&o===operation&&reason.includes('gunner seat blocked'),'timeout retains package identity and audit reason');failed++}};
 monitor.call(ctx,force);monitor.call(ctx,force);
 check(failed===0&&retries===2&&refreshes===2,'bounded retries renew activation without replacement churn');
 monitor.call(ctx,force);check(failed===1&&force.m_iMountWaitTicks===3&&retries===2,'ground boarding expires exactly at configured limit');
}
console.log(JSON.stringify({assertions,settingsEvidence,note:'Actual-source interruption, optional-escort, retention and Economy checks; native physics, spawn queues and multiplayer still require Workbench.'}));
