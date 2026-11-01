// Actual-source lifecycle, joint activation and airborne boarding regressions.
// Run: node tools/Test-QRFReleaseLifecycle.js. Engine services are mocked.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
const read=n=>fs.readFileSync(path.join(root,n+'.c'),'utf8');
const src={observer:read('AROR_OperationDamageObserver'),op:read('AROR_Operation'),adminAccess:read('../Admin/AROR_AdminAccess'),admin:read('../Admin/AROR_AdminMenuComponent'),access:read('AROR_MarsAccess'),executor:read('AROR_OperationExecutorComponent'),
 air:read('AROR_ReaperAirForce'),reaper:read('AROR_ReaperQRFExecutor'),
 attacks:read('AROR_TerritoryAttackManager'),state:read('AROR_TerritoryAttackState'),
 profile:read('AROR_ForceProfile'),ground:read('AROR_GroundOperationForce'),recipe:read('AROR_GroundQRFRecipe')};
let assertions=0,server=true,seatRequests=0,playerId=0;
const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg)};
for(const [name,fn]of Object.entries({Count(){return this.length},IsEmpty(){return !this.length},
 Insert(x){this.push(x)},Contains(x){return this.includes(x)},RemoveItem(x){const i=this.indexOf(x);if(i>=0)this.splice(i,1)},Remove(i){this.splice(i,1)},Clear(){this.length=0}}))
 Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const states={PLANNED:0,SPAWNING:1,ACTIVE:2,SUCCEEDED:3,FAILED:4,CANCELLED:5};
const env={Replication:{IsServer:()=>server},AROR_EOperationState:states,AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},
 AROR_EOperationType:{ATTACK:0,QRF:1},AROR_EForceType:{MOTORIZED:0,MECHANIZED:1,ARMOUR:2,AIRBORNE:3,INFANTRY:4},
 AROR_EResponseType:{AIR_GROUND:7},Math:{Max:Math.max,Min:Math.min},Print(){},LogLevel:{NORMAL:0},
 string:{Empty:'',Format:()=>''},PilotCompartmentSlot:{Cast:x=>x?.pilot?x:null},
 CargoCompartmentSlot:{Cast:x=>x?.cargo?x:null},BaseCompartmentManagerComponent:{Cast:x=>x},
 CompartmentAccessComponent:{Cast:x=>x},ECloseDoorAfterActions:{CLOSE_DOOR:1,INVALID:-1},AROR_ForceSpawner:{IsAlive:x=>!!x&&!x.dead},
 AROR_GroundOperationForce:{Cast:x=>x?.ground?x:null},AROR_ReaperAirForce:{Cast:x=>x?.air?x:null},
 GetGame:()=>({GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:()=>playerId})})};
const names=new Set(Object.values(src).flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])));
function compile(source,name){
 const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
 if(!m)throw Error('Missing actual method '+name);
 const args=m[1].replace(/\b(?:notnull |const )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
 let body=m[2].replace(/\barray<[^>]+>\s+(\w+)\s*=\s*\{\s*\};/g,'let $1=[];')
 .replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/\b(?:IEntity|BaseCompartmentSlot|BaseCompartmentManagerComponent|CompartmentAccessComponent|AROR_(?!E[A-Z])\w+)\s+(\w+);/g,'let $1=null;')
 .replace(/\b(?:bool|int|float|string|vector|ResourceName|Resource|PlayerManager|PlayerController|Faction|FactionAffiliationComponent|Color|SCR_\w+|Physics|DamageManagerComponent|IEntity|AIAgent|BaseCompartmentSlot|BaseCompartmentManagerComponent|CompartmentAccessComponent|AROR_\w+)\s+(\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1=0;').replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
 .replace(/super\.(\w+)\(/g,'this.Base$1(')
 .replace(/(?<![.\w])(\w+)\(/g,(s,n)=>names.has(n)?'this.'+s:s);
 if(name==='ProbeAirLaunch_S')body=body.replace('home + right * ((index - (level - 1) * 0.5) * 200)','home.map((v,i)=>v+right[i]*((index-(level-1)*0.5)*200))');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const transition=compile(src.op,'Transition_S'),terminal=compile(src.op,'IsTerminal');
const operation=state=>({m_eState:state,m_iStateSince:7,m_sResult:'old',IsTerminal:terminal,Transition_S:transition});
for(server of [false,true])for(let from=0;from<6;from++)for(let to=-1;to<=6;to++){
 const o=operation(from),allowed=server&&from<3&&([4,5].includes(to)||(from===0&&to===1)||(from===1&&to===2)||(from===2&&to===3));
 check(o.Transition_S(to,100,'next')===allowed,'complete lifecycle transition matrix '+[server,from,to]);
 check(allowed?(o.m_eState===to&&o.m_iStateSince===100&&o.m_sResult==='next'):(o.m_eState===from&&o.m_iStateSince===7&&o.m_sResult==='old'),'rejected transitions leave state unchanged');
}
server=true;
let seed=0x12345678;
for(let trial=0;trial<1000;trial++){
 const o=operation(0);let ended=false;
 for(let step=0;step<20;step++){
  seed=(Math.imul(seed,1664525)+1013904223)>>>0;
  const next=seed%8-1,before=o.m_eState,accepted=o.Transition_S(next,step,'random');
  check(!ended||(!accepted&&o.m_eState===before),'terminal outcomes are irreversible under reordered callbacks');
  if(o.IsTerminal())ended=true;
 }
}
// Recipe/profile binding is frozen, role-independent and server-only.
for(const type of [0,1])for(const state of [0,1,2,3,4,5])for(const enabled of [false,true]){
 server=enabled;const o={m_eType:type,m_eState:state,m_sRecipeId:'',m_sForceProfileId:''};
 const bind=compile(src.op,'BindGroundRecipe_S');
 check(bind.call(o,'joint',3,4)===(enabled&&state===1),'recipe binding accepts spawning authority only');
 if(enabled&&state===1)check(!bind.call(o,'other',5,8),'a bound response cannot reroll its recipe');
}
server=true;
// A reserve aircraft is not a ground reservation: never dispatch it before construction/readiness.
const activate=compile(src.executor,'TryActivateOperation');
for(const airReady of [false,true])for(const groundReady of [false,true])for(const reserveReady of [false,true]){
 let dispatched=[],active=0;
 const forces=[{air:true,m_iPackageIndex:0,m_bReady:airReady},{ground:true,m_iPackageIndex:1,m_bReady:groundReady},
 {air:true,m_iPackageIndex:2,m_bReady:reserveReady}].map(f=>({m_sOperationId:'joint',m_bReservation:false,m_bOrdersIssued:false,m_bTacticalRetained:false,...f}));
 const tactical={IsReserve:i=>i===2,m_iFirstWaveCount:2,MarkDispatched(){}};
 const o={m_TacticalCommander:tactical,IsTerminal:()=>false,GetState:()=>active?2:1,GetPackageCount:()=>3,GetRecipeId:()=> 'joint',GetId:()=> 'joint'};
 const ctx={m_aForces:forces,m_iExecutorClock:10,EstimateArrival:()=>120,DispatchReadyPackage:f=>{dispatched.push(f);f.m_bOrdersIssued=true;return true},
 m_Operations:{MarkActive_S(){active++},GetClockSeconds:()=>10},FailOperation(){throw Error('unexpected dispatch failure')}};
 activate.call(ctx,o);
 const expected=airReady&&groundReady&&!reserveReady;
 check(active===(expected?1:0),'whole lead wave must be ready; unexpected extra ready reserves cannot skip the barrier');
 check(dispatched.every(f=>f.m_bReady),'unready reserve aircraft never dispatched');
 if(expected){check(dispatched.length===2,'both domains dispatch together');activate.call(ctx,o);check(dispatched.length===2&&active===1,'repeated callbacks do not dispatch twice');}
}
// Ready later waves can deploy while a different reserve is still constructing.
{
 const lead={m_sOperationId:'joint',m_iPackageIndex:0,m_bReady:true,m_bOrdersIssued:true};
 const reserve={m_sOperationId:'joint',m_iPackageIndex:1,m_bReady:false,m_bOrdersIssued:false};
 const ready={m_sOperationId:'joint',m_iPackageIndex:2,m_bReady:true,m_bOrdersIssued:false};
 let sent=0;
 const o={m_TacticalCommander:{IsReserve:i=>i>0,m_iFirstWaveCount:1},IsTerminal:()=>false,GetState:()=>2,GetId:()=> 'joint',GetPackageCount:()=>3};
 activate.call({m_aForces:[lead,reserve,ready],DispatchReadyPackage:f=>{sent++;f.m_bOrdersIssued=true;return true}},o);
 check(sent===1&&!reserve.m_bOrdersIssued,'ready reserve proceeds without launching an unbuilt aircraft');
}
// Packaged MARS authorization uses server roles, independently of its visible entry credential.
const savedGame=env.GetGame;
let admin=false,sessionAdmin=false;
env.EPlayerRole={ADMINISTRATOR:1,SESSION_ADMINISTRATOR:2};
env.RplMode={Dedicated:0,Listen:1,Client:2};env.RplSession={Mode:()=>env.RplMode.Dedicated};
env.GetGame=()=>({GetPlayerManager:()=>({HasPlayerRole:(id,role)=>role===1?admin:sessionAdmin})});
const packagedAccess=src.adminAccess.replace(/#ifdef WORKBENCH[\s\S]*?#else/,'').replace(/#endif/g,'');
env.AROR_AdminAccess={IsAdministrator:compile(packagedAccess,'IsAdministrator')};
const authorize=compile(src.access,'CanRequest_S');
for(server of [false,true])for(admin of [false,true])for(sessionAdmin of [false,true])
 check(authorize(7)===(server&&(admin||sessionAdmin)),'packaged debugger requires an authoritative administrator role');
server=true;admin=true;check(!authorize(0),'invalid player identity cannot request debug forces');
env.GetGame=savedGame;
// Every packaged admin mutation rejects forged direct requests before reaching engine services.
{
 const actions=['GiveMoney_S','PayIncomeNow_S','FillResistanceStock_S','GrantGameMaster_S','SendTestNotifications_S','AdjustNearestSupport_S'];
 const ctx={m_bMenuAvailable:true,GetMoneyTargetName:()=> 'test'};
 admin=false;sessionAdmin=false;server=true;let unauthorizedEconomyCalls=0;
 env.AROR_EconomyComponent={GetInstance(){unauthorizedEconomyCalls++;return null}};
 for(const name of actions){compile(src.admin,name).call(ctx,7,1,true,999999);check(unauthorizedEconomyCalls===0,'ordinary player cannot invoke '+name)}
 admin=true;ctx.m_bMenuAvailable=false;
 for(const name of actions){compile(src.admin,name).call(ctx,7,1,true,999999);check(true,'menu opt-out blocks '+name)}
 ctx.m_bMenuAvailable=true;server=false;
 for(const name of actions){compile(src.admin,name).call(ctx,7,1,true,999999);check(true,'client execution cannot invoke '+name)}
 server=true;
 let paid=0;
 env.AROR_EconomyComponent={GetInstance:()=>({AddPlayerMoney_S(id,amount){check(id===7&&amount===1000,'authorized money request retains server-derived player identity');paid++}})};
 compile(src.admin,'GiveMoney_S').call(ctx,7,1,true,1000);check(paid===1,'authorized admin tools remain usable');
 const saved=env.GetGame;let controller={GetPlayerId:()=>7};
 env.SCR_PlayerController={Cast:x=>x};env.GetGame=()=>({GetPlayerController:()=>controller});
 const local=compile(src.admin,'CanUseLocalTools');
 server=false;admin=false;check(!local.call(ctx),'ordinary packaged clients cannot open the debug menu');
 admin=true;check(local.call(ctx),'local admin UI follows replicated native roles');
 controller=null;check(!local.call(ctx),'no local player controller cannot grant debug access');
 env.GetGame=saved;server=true;
}
// Run the actual full joint preparation path with geometry/native spawning mocked.
env.AROR_ResponsePolicy={IsProfileUnlocked:(p,r)=>r>=p.m_iMinimumReadinessLevel};
env.AROR_CommanderManagerComponent={GetInstance:()=>({GetCommander:()=>({GetFactionKey:()=> 'AROR_OCCUPIERS'})})};
env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocationById:()=>({GetName:()=> 'site',GetControllingRole:()=>0,GetCenter:()=>[1000,0,1000],GetRadius:()=>150})})};
env.AROR_ReaperFlightGeometry={Direction:()=>[0,0,1],FormationPoint:()=>[100,0,100],IsAirspaceClear:()=>true};
env.Vector=(...v)=>v;
env.AROR_ReaperAirForce=class {static Cast(x){return x?.air?x:null} static HasPlayerOccupant(x){return !!x?.player} constructor(){this.air=true}};
env.AROR_QRFDeferredForce=class {};
env.AROR_OperationForce=class {};
const jointSource=src.reaper.replace(/#ifdef WORKBENCH[\s\S]*?#endif/g,'');
const jointStart=compile(jointSource,'StartReaperQRF');
const makeProfile=(type,personnel)=>({m_sId:'p'+type,m_eType:type,m_eRole:1,m_sFactionKey:'AROR_OCCUPIERS',
 m_bEnabled:true,m_iEscortCount:1,m_iMinimumReadinessLevel:5,m_iPersonnelBudget:personnel,GetWeight:()=>1,
 IsValid:()=>true,GetGroupBudget:compile(src.profile,'GetGroupBudget'),GetVehicleBudget:compile(src.profile,'GetVehicleBudget')});
const airProfile=makeProfile(3,20),truckProfile=makeProfile(0,8),apcProfile=makeProfile(1,10);
for(const escortCount of [0,1])for(const opType of [0,1])for(const debug of [false,true])for(const blocking of ['none','groups','members','vehicles','landing','funding','origin']){
 airProfile.m_iEscortCount=escortCount;
 let reserved=0,bound=0,failed=0;
 const elements=[airProfile,truckProfile,apcProfile],recipe={m_sId:'joint',m_iLevel:2};
 const o={m_bDebugRequest:debug,m_bUseResponseMetrics:true,m_iPoolReadinessLevel:5,m_iRequestedTier:2,
 m_sRequestedProfileId:'',GetId:()=> 'joint',GetTargetLocationId:()=> 'site',GetRole:()=>1,GetType:()=>opType,
 BindGroundRecipe_S(id,tier,count){check(id==='joint'&&tier===2&&count===3,'joint binds authored tier and total count once');bound++;return true}};
 const ctx={m_aForces:[],m_aForceProfiles:elements,m_bAllowAirborneResponses:true,
 m_iMaxGroups:20,m_iMaxMembers:128,m_iMaxVehicles:10,m_iQRFMaxLiveGround:6,m_iQRFMaxLiveAircraft:6,
 m_fReaperHoldingDistance:700,m_fReaperInsertionHeight:80,m_fReaperGunshipHeight:300,m_fReaperGunshipBand:20,
 FindForce:()=>null,HasMarsOrigin_S:()=>blocking!=='origin',ReaperSettingsValid:()=>true,QRFPoolReadinessLevel:()=>5,
 GetReservedGroups:()=>blocking==='groups'?20:0,GetReservedMembers:()=>blocking==='members'?128:0,
 GetReservedVehicles:()=>blocking==='vehicles'?10:0,
 PrepareReaperLanding:()=>blocking!=='landing',
 m_ReaperLandingSearch:{GetResultCount:()=>1,GetResult:()=>[1000,0,1000]},
 ResolveQRFOrigin:()=>[0,0,0],IsLaunchPositionSafe:()=>true,
 ReserveQRFFunding(op,profiles,tier){reserved++;check(profiles===elements&&tier===2,'one funding reservation covers air and ground');return blocking!=='funding'},
 ConfigureForceFunding(force){force.m_Funding='shared'},
 CreateQRFTacticalCommander(op,total){check(total===3,'commander owns all air/ground packages');op.m_TacticalCommander={m_iFirstWaveCount:3}},
 ReleaseForce(force){check(force.m_iReservedGroups===0&&force.m_iReservedMembers===0&&force.m_iReservedVehicles===0,'atomic reservation converts without resource loss');this.m_aForces.splice(this.m_aForces.indexOf(force),1)},
 ReportSpawnWait(){},FailOperation(){failed++},
 m_Operations:{IsQRFStrategyEnabled:()=>true}};
 jointStart.call(ctx,o,recipe,elements);
 // Ordinary QRFs already passed HasFriendlyQRFOrigin before reaching this adapter.
 const succeeds=blocking==='none'||(blocking==='origin'&&!debug&&opType===1);
 check(ctx.m_aForces.length===(succeeds?3:0),'joint preparation never leaves a partial force on failed preflight '+[opType,debug,blocking]);
 check(bound===(succeeds?1:0),'preflight failure cannot bind a partially deployable response');
 if(succeeds){
  check(ctx.m_aForces.reduce((n,p)=>n+p.m_iReservedGroups,0)===6+escortCount,'joint reserves infantry/pilot groups and only the configured escort');
  check(ctx.m_aForces.reduce((n,p)=>n+p.m_iReservedMembers,0)===38,'joint reserves every pilot/crew/passenger slot');
  check(ctx.m_aForces.reduce((n,p)=>n+p.m_iReservedVehicles,0)===3+escortCount,'joint reserves transport, configured gunship, truck and APC');
  check(new Set(ctx.m_aForces.map(p=>p.m_iPackageIndex)).size===3,'joint package identities never collide');
  check(ctx.m_aForces.every(p=>p.m_Funding==='shared'),'all packages use one funding account');
 }
}
// Joint assaults hold aircraft at their approach until the shared ground rally releases.
{
 const hold=compile(src.reaper,'HoldJointAssault');
 let orders=0,failures=0,release=false;
 const tactical={m_bJointResponse:true,m_bAssault:true,m_bAssaultReleased:false,m_iStagingDeadline:100};
 const op={m_TacticalCommander:tactical,GetTargetLocationId:()=> 'site'};
 const force={m_bWaitingForAssault:false,m_iFlightDeadline:30,m_vAirApproach:[0,100,0]};
 const ctx={m_iExecutorClock:10,m_iReaperFlightTimeout:60,m_fReaperTransportSpeed:160,m_fReaperTransportHeight:60,
  UpdateAssaultRelease(o,c){if(release)c.m_bAssaultReleased=true},
  ReaperHoverPrefab:()=> 'hover',ReaperOrder(){orders++;return true},FailReaperPackage(){failures++}};
 check(hold.call(ctx,force,op)&&orders===1&&force.m_iFlightDeadline===160,'joint aircraft receives a bounded holding order while ground rallies');
 check(hold.call(ctx,force,op)&&orders===1,'holding does not recreate flight waypoints every tick');
 release=true;check(!hold.call(ctx,force,op)&&!force.m_bWaitingForAssault,'ground release allows landing to proceed');
 tactical.m_bAssaultReleased=false;force.m_bWaitingForAssault=false;release=false;
 ctx.m_iExecutorClock=10;hold.call(ctx,force,op);ctx.m_iExecutorClock=161;
 check(hold.call(ctx,force,op)&&failures===1,'air rally cannot wait beyond its bounded flight deadline');
 tactical.m_bAssault=false;check(!hold.call(ctx,force,op),'ordinary joint QRFs retain their immediate insertion path');
}
// Native audit launch checks use actual selected pads for escort headings at every package count.
{
 let volumes=[],formations=[],unsafePad=false,unsafeLaunch=false,blockedEscort=false;
 const direction=(a,b)=>{const dx=b[0]-a[0],dz=b[2]-a[2],length=Math.hypot(dx,dz);return [dx/length,0,dz/length]};
 env.AROR_ReaperFlightGeometry={LANDING_CLEAR:0,Direction:direction,
 LandingRejectionReason:()=>unsafePad?1:0,
 FormationPoint(launch,heading){formations.push({launch,heading});return launch.map((v,i)=>v-heading[i]*75)},
 IsAirspaceClear(point,height,radius){volumes.push({point,height,radius});return !blockedEscort||height<200}};
 const probe=compile(src.reaper,'ProbeAirLaunch_S');
 const origin={GetCenter:()=>[0,0,0]},target={GetCenter:()=>[1000,0,1000]};
 const pads=[[1500,0,1400],[1200,0,800]];
 const ctx={ReaperSettingsValid:()=>true,IsLaunchPositionSafe:()=>!unsafeLaunch,
 m_fReaperFormationSpacing:75,m_fReaperInsertionHeight:80,m_fReaperGunshipHeight:300,m_fReaperGunshipBand:20,m_fReaperPadClearance:20};
 check(probe.call(ctx,origin,target,pads)&&volumes.length===30&&formations.length===15,'air audit checks transport and escort volumes for all sizes one through five');
 let ordinal=0;
 for(let level=1;level<=5;level++)for(let index=0;index<level;index++){
  const formation=formations[ordinal++],expected=direction(formation.launch,pads[index%pads.length]);
  check(formation.heading.every((v,i)=>Math.abs(v-expected[i])<1e-9),'escort heading follows the pad assigned to this package');
 }
 const expectedFirst=direction([0,0,0],pads[0]);check(Math.abs(formations[0].heading[0]-expectedFirst[0])<1e-9,'off-axis LZ does not reuse the target-centre heading');
 volumes=[];unsafePad=true;check(!probe.call(ctx,origin,target,pads)&&volumes.length===0,'stale unsafe cached pad cannot receive a launch-planning PASS');
 unsafePad=false;unsafeLaunch=true;check(!probe.call(ctx,origin,target,pads),'player/target launch safety remains enforced by the audit');
 unsafeLaunch=false;blockedEscort=true;volumes=[];
 check(!probe.call(ctx,origin,target,pads),'obstructed escort airspace blocks escorted launch planning');
 volumes=[];check(probe.call(ctx,origin,target,pads,false)&&volumes.length===15,'light-air planning checks all transports without requiring an unused escort corridor');
 blockedEscort=false;
 unsafeLaunch=false;check(!probe.call(ctx,origin,target,[]),'missing safe pads cannot produce an air-launch PASS');
 server=false;check(!probe.call(ctx,origin,target,pads),'clients cannot probe native air launch plans');server=true;
}
const boardPilot=compile(src.air,'BoardPilot_S');
let slots=[];
const vehicle={FindComponent:()=>({GetCompartments:out=>out.push(...slots)})};
const access={in:false,busy:false,out:false,seat:null,GetCompartment(){return this.seat},
 IsInCompartment(){return this.in},IsGettingIn(){return this.busy},IsGettingOut(){return this.out},
 GetInVehicle(v,s,teleport,door,close,paused){seatRequests++;check(paused,'pilot boarding works outside player simulation bubble');return true}};
const pilot={FindComponent:()=>access},group={GetPlayerCount:()=>0,GetAgents:out=>out.push({GetControlledEntity:()=>pilot})};
function slot(extra={}){return {pilot:true,GetOwner:()=>null,IsCompartmentAccessible:()=>true,IsOccupied:()=>false,GetOccupant:()=>null,
 IsReserved:()=>false,IsReservedBy:()=>false,GetVehicle:()=>vehicle,...extra}}
slots=[slot()];
check(!boardPilot(group,vehicle)&&seatRequests===1,'accepted pilot request is not an occupancy acknowledgement');
access.in=true;access.seat=slot({GetOccupant:()=>pilot,IsOccupied:()=>true});
check(boardPilot(group,vehicle)&&seatRequests===1,'confirmed pilot occupancy is idempotent');
access.seat=slot({pilot:false,GetOccupant:()=>pilot});check(!boardPilot(group,vehicle),'cargo occupancy cannot count as a pilot');
access.in=false;access.seat=null;slots=[slot({IsReserved:()=>true})];
check(!boardPilot(group,vehicle)&&seatRequests===1,'another actors reservation cannot be stolen');
slots=[slot({GetOccupant:()=>({})})];check(!boardPilot(group,vehicle)&&seatRequests===1,'occupied pilot seat is preserved');
slots=[slot()];playerId=7;check(!boardPilot(group,vehicle)&&seatRequests===1,'player-controlled pilot cannot be teleported');playerId=0;
access.busy=true;check(!boardPilot(group,vehicle)&&seatRequests===1,'pending native boarding is not interrupted');access.busy=false;
server=false;check(!boardPilot(group,vehicle)&&seatRequests===1,'clients cannot seat pilots');server=true;
// Full boarding acknowledgement: both pilots and every passenger must be physically seated.
{
 env.AROR_ReaperAirForce.BoardPilot_S=boardPilot;
 const makeSeat=(pilot,cargo)=>({pilot,cargo,occupant:null,reserved:null,vehicle:null,
  GetAttachedTurret:()=>null,GetOwner:()=>null,IsCompartmentAccessible:()=>true,IsOccupied(){return !!this.occupant},GetOccupant(){return this.occupant},
  IsReserved(){return !!this.reserved},IsReservedBy(who){return this.reserved===who},GetVehicle(){return this.vehicle}});
 const transportSeats=[makeSeat(true,false),makeSeat(false,true),makeSeat(false,true)],escortSeats=[makeSeat(true,false)];
 const makeVehicle=seats=>{const v={FindComponent:()=>({GetCompartments:out=>out.push(...seats)})};seats.forEach(s=>s.vehicle=v);return v};
 const transport=makeVehicle(transportSeats),escort=makeVehicle(escortSeats);
 const requests=[];
 const actor=()=>{const a={seat:null,busy:false,GetCompartment(){return this.seat},IsInCompartment(){return !!this.seat},
  IsGettingIn(){return this.busy},IsGettingOut:()=>false,
  GetInVehicle(v,s,t,d,c,paused){check(paused,'every aircraft boarding teleport permits paused simulation');requests.push([e,s]);return true}};
  const e={access:a,FindComponent:()=>a};return e};
 const tp=actor(),ep=actor(),p1=actor(),p2=actor();
 const makeGroup=actors=>({GetPlayerCount:()=>0,GetAgents:out=>out.push(...actors.map(a=>({GetControlledEntity:()=>a})))});
 const ctx={m_Escort:{m_CrewGroup:makeGroup([ep]),m_Vehicle:escort},m_CrewGroup:makeGroup([tp]),
  m_Group:makeGroup([p1,p2]),m_Vehicle:transport,m_aPassengers:[],m_iSeatAttempts:0,m_bMounted:false,m_bDismounted:false,
  BoardPilot_S:boardPilot,IsSeatedInOwnVehicle:compile(src.ground,'IsSeatedInOwnVehicle'),Seat:compile(src.ground,'Seat')};
 const board=compile(src.air,'BoardPackage_S');
 check(!board.call(ctx)&&requests.length===4,'accepted requests for two pilots and two passengers cannot activate the aircraft');
 check(new Set(requests.map(r=>r[1])).size===4,'asynchronous boarding assigns distinct seats even before native reservations update');
 for(const [entity,seat]of requests){entity.access.seat=seat;seat.occupant=entity}
 check(board.call(ctx)&&ctx.m_bMounted,'full physical occupancy completes boarding');
 check(board.call(ctx)&&ctx.m_aPassengers.length===2&&requests.length===4,'repeated acknowledgement neither duplicates passenger identities nor reseats them');
 const light={...ctx,m_Escort:null,m_bUnescorted:true};
 check(board.call(light)&&light.m_bMounted,'transport-only packages complete boarding without an escort or second pilot');
 light.m_bUnescorted=false;check(!board.call(light),'an escorted package cannot silently become unescorted after missing construction');
 p2.access.seat=null;transportSeats[2].occupant=null;transportSeats[2].reserved={};
 ctx.m_bMounted=false;check(!board.call(ctx)&&requests.length===4,'one missing passenger and a foreign reserved seat keep the force unready');
 p2.access.seat=escortSeats[0];check(!board.call(ctx),'a passenger in the wrong aircraft cannot satisfy transport occupancy');
 server=false;check(!board.call(ctx),'clients cannot board an air package');server=true;
}
// A ready aircraft can lose personnel/occupancy while another lead package is still spawning.
{
 let effects=0,occupancy=true,infantry=4,crew=1,escortCrew=1;
 env.AROR_ReaperAirForce.AircraftAlive=v=>!!v?.alive;
 const dispatch=compile(src.reaper,'DispatchReaperPackage');
 const ctx={m_iExecutorClock:10,m_iReaperFlightTimeout:30,m_fReaperTransportHeight:100,m_fReaperTransportSpeed:100,
  ReaperMovePrefab:()=> 'move',ReaperGunshipHeight:()=>100,
  ReaperOrder(){effects++;return true},InsertReaperAircraft(){effects++;return true}};
 const make=()=>({m_Vehicle:{alive:true},m_Escort:{m_Vehicle:{alive:true},CountCrew:()=>escortCrew},
  CountInfantry:()=>infantry,CountCrew:()=>crew,BoardPackage_S:()=>occupancy,
  ReleaseAircraft_S(){effects++},m_iPackageIndex:0,m_iPackageCount:1});
 for(const state of ['transportLost','escortLost','infantryLost','pilotLost','escortPilotLost','occupancyLost']){
  const force=make();effects=0;infantry=4;crew=1;escortCrew=1;occupancy=true;
  if(state==='transportLost')force.m_Vehicle.alive=false;
  if(state==='escortLost')force.m_Escort.m_Vehicle.alive=false;
  if(state==='infantryLost')infantry=0;
  if(state==='pilotLost')crew=0;
  if(state==='escortPilotLost')escortCrew=0;
  if(state==='occupancyLost')occupancy=false;
  check(!dispatch.call(ctx,force)&&effects===0&&!force.m_bOrdersIssued,'stale readiness cannot move aircraft or issue orders: '+state);
 }
 infantry=4;crew=1;escortCrew=1;occupancy=true;effects=0;
 const force=make();
 check(dispatch.call(ctx,force)&&effects===5&&force.m_bOrdersIssued&&force.m_iFlightDeadline===40,'healthy occupied aircraft retain normal dispatch');
 const light={...make(),m_bUnescorted:true,m_Escort:null};effects=0;
 check(dispatch.call(ctx,light)&&effects===3&&light.m_bOrdersIssued,'unescorted dispatch touches only transport physics, orders and insertion');
}
// Physical staging reaches the release path; route recovery cannot bypass the rally.
{
 let alerts=0,releases=0,withdrawals=0;
 env.AROR_UIColors={DANGER:1,Get:x=>x};env.AROR_ENotificationType={HEADLINE:0};
 env.AROR_Notification=class {SetDetail(){}};
 env.AROR_NotificationManagerComponent={Send_S(){alerts++}};
 env.vector={DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])};
 const commander={m_bAssault:true,m_bAssaultReleased:false,m_iStagingDeadline:180,m_iTownHoldSeconds:900,
 m_bWithdraw:false,m_vTown:[1000,0,0],m_fTownRadius:100,m_fMargin:100};
 const op={m_TacticalCommander:commander,GetId:()=> 'attack',GetTargetLocationId:()=> 'site'};
 const makeForce=()=>({ground:true,m_sOperationId:'attack',m_bTacticalControlled:true,m_bTacticalStaging:true,
 m_bTacticalStaged:false,m_bDismounted:true,m_iTacticalNextSupport:999,m_vStagingPosition:[0,0,0],
 m_Group:{GetPlayerCount:()=>0},m_CrewGroup:{GetPlayerCount:()=>0},m_Profile:{m_eType:0},
 m_Vehicle:{GetOrigin:()=>[0,0,0],GetPhysics:()=>({GetVelocity:()=>({Length:()=>0})})}});
 const a=makeForce(),b=makeForce();
 const ctx={m_aForces:[a,b],m_iExecutorClock:10,RecordTacticalStrength(){},ReportDefenders:()=>false,
 UpdateAssaultRelease:compile(src.executor,'UpdateAssaultRelease'),TacticalInfantryOrders(){},
 ReleaseFromStaging(f){releases++;f.m_bTacticalStaging=false},
 TacticalWithdrawal(){withdrawals++;return true}};
 const update=compile(src.executor,'UpdateTacticalGround');
 check(update.call(ctx,a,op,4,0)&&a.m_bTacticalStaged&&!commander.m_bAssaultReleased&&releases===0,'first physical rally arrival waits for the other package');
 update.call(ctx,b,op,4,0);check(commander.m_bAssaultReleased&&releases===1&&alerts===1,'final physical arrival releases one shared assault');
 update.call(ctx,a,op,4,0);check(releases===2&&alerts===1,'earlier rally package receives its assault orders without a duplicate headline');
 update.call(ctx,a,op,4,0);check(releases===2,'subsequent tactical ticks cannot release the same package twice');
 commander.m_bAssaultReleased=false;a.m_bTacticalStaging=true;a.m_bTacticalStaged=false;b.m_bTacticalStaging=true;b.m_bTacticalStaged=false;
 a.m_Vehicle.GetPhysics=()=>({GetVelocity:()=>({Length:()=>20})});
 update.call(ctx,a,op,4,0);check(!a.m_bTacticalStaged&&!commander.m_bAssaultReleased,'passing through a rally at speed is not staged readiness');
 ctx.m_iExecutorClock=180;update.call(ctx,a,op,4,0);check(commander.m_bAssaultReleased&&alerts===2,'staging deadline releases the attack even when an approach stalls');
 commander.m_bWithdraw=true;a.m_bTacticalStaging=true;update.call(ctx,a,op,4,0);
 check(withdrawals===1,'withdrawal takes priority over staging and assault');
}
// Town support changes through the existing settlement authority, with all hostile armies counted.
{
 let support=[],counts={0:2,1:4,2:2};
 const settlement={AddSupport_S:(role,amount)=>support.push([role,amount])};
 env.AROR_SettlementComponent={Cast:x=>x};
 env.AROR_LocationForces={Count:()=>({GetAI:r=>counts[r],GetTotal:r=>counts[r]})};
 env.TACTICAL_CHECK_SECONDS=5;
 const target={GetControllingRole:()=>0,GetOwner:()=>({FindComponent:()=>settlement})};
 const op={GetRole:()=>1},ctx={m_fQRFTownSupportPerMinute:6};
 const pacify=compile(src.executor,'PacifyTown');
 pacify.call(ctx,op,target);check(support.length===0,'ties against the combined hostile force do not pacify a town');
 counts[1]=5;pacify.call(ctx,op,target);check(support.length===1&&support[0][0]===1&&support[0][1]===0.5,'one five-second step shifts the configured six points per minute');
 ctx.m_fQRFTownSupportPerMinute=0;pacify.call(ctx,op,target);check(support.length===1,'scenario opt-out prevents support changes');
 ctx.m_fQRFTownSupportPerMinute=6;target.GetControllingRole=()=>1;pacify.call(ctx,op,target);
 check(support.length===1,'already friendly towns are not repeatedly pacified');
}
// The shared commander timer paces pacification once per operation, independently of package count.
{
 let pacifications=0;
 const c={m_iNextContactCheck:0,m_bWithdraw:false,m_iTownHoldStarted:10,m_sOriginLocationId:'',
 m_vTown:[0,0,0],m_fTownRadius:100,m_fMargin:100,RecordTargetControl:()=>false,UpdateContact_S(){},
 RefreshRoster(){},ShouldWithdraw:()=>false};
 const op={m_TacticalCommander:c,GetId:()=> 'pacify',IsTerminal:()=>false,GetTargetLocationId:()=> 'site',GetRole:()=>1};
 const ctx={m_iExecutorClock:100,m_Operations:{GetOperations:out=>out.push(op),GetRecentCombatContact:()=>null},
 HasPendingQRF:()=>false,PacifyTown(){pacifications++}};
 const tick=compile(src.executor,'TickQRFTacticalCommanders');
 tick.call(ctx);tick.call(ctx);ctx.m_iExecutorClock=104;tick.call(ctx);check(pacifications===1,'multiple ticks and packages cannot multiply town support gain');
 ctx.m_iExecutorClock=105;tick.call(ctx);check(pacifications===2,'next five-second step resumes pacification');
 c.m_bWithdraw=true;ctx.m_iExecutorClock=110;tick.call(ctx);check(pacifications===2,'withdrawing troops stop pacification');
}
// Terminal mixed-operation cleanup preserves active ground actors and releases all virtual reservations once.
{
 env.AROR_QRFDeferredForce.Cast=x=>x?.deferred?x:null;
 const funding={m_iQuote:5000};
 const make=(extra={})=>({m_sOperationId:'joint',m_iReservedGroups:2,m_iReservedMembers:8,m_iReservedVehicles:1,
  m_iPackageIndex:0,m_bTacticalControlled:true,m_bOrdersIssued:false,m_bTacticalRetained:false,Cleanup_S(){this.cleaned=(this.cleaned||0)+1},...extra});
 const ground=make({ground:true,m_bOrdersIssued:true}),air=make({air:true,m_bOrdersIssued:true,m_iReservedGroups:3,m_iReservedVehicles:2});
 const preparedGround=make({ground:true,m_Vehicle:{GetOrigin:()=>[0,0,0]}});
 const pending=make({deferred:true,m_bReservation:true,m_Funding:funding,m_Profile:{ReplacementQuote:()=>400}});
 const pendingAir=make({air:true,m_bConstructionPending:true,m_Funding:funding,m_Profile:{ReplacementQuote:()=>2000}});
 const other=make({m_sOperationId:'unrelated'});
 const ctx={m_aForces:[ground,preparedGround,air,pending,pendingAir,other],m_iExecutorClock:20,
  ReleaseForce:compile(src.executor,'ReleaseForce'),RetainTacticalForce:compile(src.executor,'RetainTacticalForce'),ShouldRetainTacticalForce:compile(src.executor,'ShouldRetainTacticalForce')};
 const release=compile(src.executor,'ReleaseOperationForces');
 release.call(ctx,'joint');
 check(ctx.m_aForces.length===3&&ctx.m_aForces.includes(ground)&&ctx.m_aForces.includes(preparedGround)&&ctx.m_aForces.includes(other),'terminal event retains ordered and materialized unissued ground actors, excluding unrelated operations');
 check(preparedGround.m_bTacticalRetained&&!preparedGround.cleaned,'pre-dispatch materialized hull follows actual terminal retention handling');
 check(ground.m_bTacticalRetained&&!ground.cleaned&&air.cleaned===1,'ground retention and airborne retirement use their separate cleanup paths');
 check(funding.m_iQuote===2600&&pending.m_iReservedMembers===0&&!pendingAir.m_bConstructionPending,'cancelled unbuilt ground and air packages release their quotes exactly once');
 const reserved=compile(src.executor,'GetReservedGroups');check(reserved.call(ctx)===6,'ordered and pre-dispatch retained actors both keep consuming the shared group budget');
 release.call(ctx,'joint');check(funding.m_iQuote===2600&&ground.m_iTacticalRetainedAt===20,'duplicate terminal notification cannot refund or reset retention twice');
 ctx.ReleaseForce(pending);ctx.ReleaseForce(pendingAir);check(funding.m_iQuote===2600,'stale monitor snapshots cannot release virtual collateral twice');
}
// Withdrawal avoids contested bases; assault intel never queries hidden defender positions.
{
 env.float={MAX:Number.MAX_VALUE};env.AROR_QRFOriginPolicy={IsOriginType:t=>t===8};
 env.AROR_RecaptureComponent={IsContested:id=>id==='contested'};
 const site=(id,x)=>({GetId:()=>id,GetCenter:()=>[x,0,0],GetControllingRole:()=>1,GetLocationType:()=>8});
 const bases=[site('contested',100),site('safe',500)],commander={m_vTown:[0,0,0]};
 const op={GetRole:()=>1},locations={GetLocations:out=>out.push(...bases)};
 const retarget=compile(src.executor,'RetargetWithdrawalOrigin');
 retarget.call({IsLaunchPositionSafe:()=>true},op,commander,locations);
 check(commander.m_bSafeWithdrawalOrigin&&commander.m_sOriginLocationId==='safe','closer contested base cannot become the safe withdrawal destination');
 bases.pop();retarget.call({IsLaunchPositionSafe:()=>true},op,commander,locations);
 check(!commander.m_bSafeWithdrawalOrigin,'no safe uncontentested base leaves the response holding rather than pretending it recovered');
 let contact=null,updates=0;
 const target={GetCenter:()=>[0,0,0],GetRadius:()=>100};
 const report=compile(src.executor,'ReportDefenders');
 const ctx={m_fQRFSearchMargin:50,m_iQRFContactMaxAge:90,m_iExecutorClock:100,
 m_Operations:{GetRecentCombatContact:(role,center,radius,age)=>{check(role===1&&radius===150&&age===90,'report respects role, vicinity and freshness');return contact}}};
 op.m_TacticalCommander={UpdateContact_S(){updates++}};
 check(!report.call(ctx,op,target)&&updates===0,'no fresh report leaves hidden defenders unknown');
 contact={m_vPosition:[10,0,10]};check(report.call(ctx,op,target)&&updates===1,'fresh attributed report may inform the assault');
}
// Config validation includes the newly active staging/support policy and origin costs.
{
 const defaults=Object.fromEntries([...src.executor.matchAll(/\[Attribute\("([-\d.]+)",[^\r\n]*\)\]\s*(?:protected )?(?:int|float|bool) (m_\w+)/g)].map(m=>[m[2],Number(m[1])]));
 const valid=compile(src.executor,'TacticalSettingsValid');
 const body=src.executor.match(/\tprotected bool TacticalSettingsValid\(\)\r?\n\t\{([\s\S]*?)^\t\}/m)[1];
 check([...new Set(body.match(/m_\w+/g))].every(n=>n in defaults),'every validated numeric setting has a real source default');
 check(valid.call(defaults),'source defaults remain a valid tactical configuration');
 for(const [field,low,high]of [['m_fAttackStagingDistance',200,1500],['m_iAttackStagingTimeout',30,600],
 ['m_fQRFTownSupportPerMinute',0,30],['m_iOriginLaunchCooldown',0,3600],['m_fOriginMaxGarrisonLoss',0,1],
 ['m_fMinOriginDistance',0,3000],['m_fOriginWaterPenalty',1,10],['m_iQRFExitRetryLimit',0,5]]){
  check(!valid.call({...defaults,[field]:low-0.01})&&!valid.call({...defaults,[field]:high+0.01}),'out-of-range configuration is rejected: '+field);
  check(valid.call({...defaults,[field]:low})&&valid.call({...defaults,[field]:high}),'documented configuration boundaries remain valid: '+field);
 }
}
// Debug force damage may supply a contact snapshot, but cannot escalate campaign Crackdown.
{
 env.EDamageType={HEALING:1,REGENERATION:2};env.InstigatorType={INSTIGATOR_GM:99};
 env.FactionAffiliationComponent={Cast:x=>x};
 let debug=true,reports=[];
 const theirs={},ours={IsFactionEnemy:f=>f===theirs};
 const affiliation=faction=>({GetAffiliatedFaction:()=>faction,GetAffiliatedFactionKey:()=>faction===ours?'AROR_OCCUPIERS':'AROR_RESISTANCE'});
 const victim={FindComponent:()=>affiliation(ours)},attacker={FindComponent:()=>affiliation(theirs),GetOrigin:()=>[20,0,30]};
 env.AROR_FactionRolesComponent={GetInstance:()=>({GetFactionKey:()=> 'AROR_OCCUPIERS'})};
 env.AROR_OperationManagerComponent={GetInstance:()=>({GetOperation:()=>({m_bDebugRequest:debug,IsTerminal:()=>false,GetRole:()=>1,GetTargetLocationId:()=> 'site'}),
 ReportCombatContact_S:(...args)=>reports.push(args)})};
 const ctx={m_Entity:victim,m_sOperationId:'debug',m_bLogFriendlyDamage:false};
 const hit={damageValue:10,damageType:0,instigator:{GetInstigatorType:()=>0,GetInstigatorEntity:()=>attacker}};
 const damage=compile(src.observer,'OnDamage');
 damage.call(ctx,hit);check(reports.length===1&&reports[0][3]===false,'debug damage contact explicitly suppresses strategic evidence');
 debug=false;damage.call(ctx,hit);check(reports.length===2&&reports[1][3]===true,'ordinary campaign damage still contributes evidence');
 hit.damageType=1;damage.call(ctx,hit);check(reports.length===2,'healing never becomes hostile evidence');
 server=false;hit.damageType=0;damage.call(ctx,hit);check(reports.length===2,'clients cannot publish damage evidence');server=true;
}
// ACTIVE can synchronously become terminal inside an operation subscriber.
const notify=compile(src.attacks,'NotifyChanged'),record=compile(src.attacks,'RecordTerritoryOperationChange');
const evidence={m_sLocationId:'site',m_sActiveQRF:'',m_iQRFCompletedAt:-1};
const qrf={m_bCaptureThreatRecorded:true,m_bDebugRequest:false,state:2,GetType:()=>1,GetTargetLocationId:()=> 'site',
 GetState(){return this.state},GetId:()=> 'capture',IsTerminal(){return this.state>=3}};
const manager={m_bTerritoryAttacksEnabled:true,m_aAttackTerritories:[evidence],m_iClock:100,m_iTerritoryAttackRest:900,
 RecordTerritoryOperationChange:record,BaseNotifyChanged(op){if(op.state===2){op.state=4;notify.call(this,op)}}};
notify.call(manager,qrf);
check(evidence.m_iQRFCompletedAt===100&&evidence.m_sActiveQRF==='','nested terminal callback retains physical activation evidence');
manager.m_iClock=200;notify.call(manager,qrf);check(evidence.m_iQRFCompletedAt===100,'duplicate terminal notification does not restart post-QRF recovery');
console.log(JSON.stringify({assertions,note:'Actual-source state transitions and dispatch/boarding policy; native AI, route reachability and replication remain unverified.'}));
