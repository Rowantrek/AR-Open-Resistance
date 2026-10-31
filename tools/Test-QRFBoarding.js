// Actual deployment seating/recovery methods with paused native actions mocked.
const fs=require('fs'),path=require('path');
const source=fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations/AROR_GroundOperationForce.c'),'utf8');
let assertions=0,server=true;
const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length;},configurable:true});
for(const [name,fn]of Object.entries({Count(){return this.length;},Insert(x){this.push(x);},Contains(x){return this.includes(x);},IsEmpty(){return !this.length;},Clear(){this.length=0;},Copy(x){this.splice(0,this.length,...x);}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
const env={Math:{Max:Math.max},Replication:{IsServer:()=>server},GetGame:()=>({GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:x=>x?.player?1:0})}),
  AROR_EForceType:{AIRBORNE:3,MOTORIZED:0},ECloseDoorAfterActions:{CLOSE_DOOR:1,INVALID:-1},
  string:{Format:(s,...a)=>s.replace(/%(\d+)/g,(_,i)=>a[i-1])}};
for(const name of ['BaseCompartmentManagerComponent','SCR_AIVehicleUsageComponent','CompartmentAccessComponent'])env[name]={Cast:x=>x};
env.ChimeraCharacter={Cast:x=>x?.character?x:null};
let deleted=[];env.AIControlComponent={Cast:x=>x};env.SCR_EntityHelper={DeleteEntityAndChildren:x=>deleted.push(x)};
env.TurretCompartmentSlot={Cast:x=>x?.kind==='turret'?x:null};env.CargoCompartmentSlot={Cast:x=>x?.kind==='cargo'?x:null};
const methods=['ReplaceUnseated_S','ReplaceIntoSeat','ReplaceIntoSeatEntity','Seat','PreparePassengerRoster_S','Mount_S','IsRequiredGunnerSeat','IsMountComplete','ReseatMissing_S','ReseatMember','RecoverStaleBoarding_S','IsSeatedInOwnVehicle'];
function compile(name){
  const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!m)throw Error('Missing '+name);
  const args=m[1].replace(/array<\w+>\s+|\b(?:IEntity|bool|BaseCompartmentSlot|CompartmentAccessComponent|SCR_AIGroup)\s+/g,'');
  let body=m[2].replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g,'for(const [$1, $2] of $3.entries())')
    .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
    .replace(/array<\w+> (\w+) = \{([^;]*)\};/g,'let $1 = [$2];')
    .replace(/int (\w+), (\w+), (\w+);/g,'let $1=0,$2=0,$3=0;')
    .replace(/int (\w+), (\w+);/g,'let $1=0,$2=0;')
    .replace(/\b(?:IEntity|bool|int|float|string|Physics|ChimeraCharacter|AIAgent|AIGroup|AIControlComponent|SCR_AIGroupUtilityComponent|BaseCompartmentSlot|BaseCompartmentManagerComponent|SCR_AIVehicleUsageComponent|CompartmentAccessComponent)\s+(\w+)/g,'let $1')
    .replace(/let (\w+);/g,'let $1=0;').replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
  for(const n of methods)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
}
const compiled=Object.fromEntries(methods.map(n=>[n,compile(n)]));
function fixture(crewCount=1,passengerCount=4){
  const all=[],calls=[],resets=[];
  const vehicle={speed:0,FindComponent(type){return type===env.BaseCompartmentManagerComponent?{GetCompartments:a=>a.push(...slots)}:usage;},GetPhysics(){return {GetVelocity:()=>({Length:()=>this.speed})};}};
  function seat(kind){return {kind,occupant:null,reserved:null,accessible:true,GetOccupant(){return this.occupant;},IsOccupied(){return !!this.occupant;},
    GetAttachedTurret(){return this.attachedTurret||null;},GetOwner(){return this.owner||null;},IsCompartmentAccessible(){return this.accessible;},IsReserved(){return !!this.reserved;},IsReservedBy(x){return this.reserved===x;},SetReserved(x){this.reserved=x;},GetVehicle:()=>vehicle};}
  const slots=[seat('driver'),...Array.from({length:crewCount-1},()=>seat('turret')),...Array.from({length:passengerCount},()=>seat('cargo'))];
  const usage={CanBePiloted:()=>true,GetPilotCompartmentSlot:()=>slots[0]};
  function actor(){
    const a={character:true,dead:false,GetCharacterController(){return {IsDead:()=>this.dead};},FindComponent:()=>a.access};
    a.access={entering:true,exiting:false,inside:false,current:null,allow:true,
      CanGetInVehicle(){return this.allow;},IsGettingIn(){return this.entering;},IsGettingOut(){return this.exiting;},IsInCompartment(){return this.inside;},GetCompartment(){return this.current;},
      InterruptVehicleActionQueue(...args){resets.push({actor:a,args});this.entering=false;},
      GetInVehicle(v,slot,teleport,door,close,paused){calls.push({actor:a,slot,paused,target:v,close});if(!this.allow&&!(this.allowUnpaused&&!paused))return false;
        // Outside the simulation bubble, an ordinary queued action cannot execute.
        if(!paused){this.entering=true;return true;}this.current=slot;this.inside=true;this.entering=false;slot.occupant=a;return true;}};
    all.push(a);return a;
  }
  const crew=Array.from({length:crewCount},actor),passengers=Array.from({length:passengerCount},actor);
  const group=members=>({players:0,m_aUnitPrefabSlots:['rifle','support'],GetGroupUtilityComponent:()=>({AddUsableVehicle:()=>{}}),GetPlayerCount(){return this.players;},GetAgents:out=>out.push(...members.map(x=>({GetControlledEntity:()=>x})))});
  all.forEach((a,i)=>slots[i].reserved=a);
  const force={...compiled,m_Vehicle:vehicle,m_Profile:{m_eType:0},m_CrewGroup:group(crew),m_Group:group(passengers),
    m_aMembers:all,m_aPassengers:passengers,m_bMountRequested:true,m_bMounted:false,m_bDismountRequested:false,m_bDismounted:false,
    m_iSeatAttempts:0,m_iSeatQueueResets:0,m_iSeatBusy:0,m_sSeatRejection:''};
  return {force,vehicle,crew,passengers,slots,all,calls,resets};
}
let f=fixture();
check(!f.force.IsMountComplete(),'missing native occupancy cannot activate deployment');
let report=f.force.ReseatMissing_S(false);
check(!f.calls.length&&!f.resets.length&&report.includes('busy=5'),'normal grace does not interrupt pending actions');
report=f.force.ReseatMissing_S(true);
check(f.resets.length===5&&f.calls.length===5,'stuck driver and all passengers receive recovery and a fresh seat request');
check(f.calls.every(x=>x.paused===true),'initial teleports execute when simulation is paused');
check(f.resets.every(x=>x.args.every(v=>v===true)),'interrupt current action, force and replicate native queue reset');
check(f.force.IsMountComplete(),'activation becomes possible only after actual driver/passenger occupancy');
check(report.includes('accepted=5 attempted=5 queueResets=5 busy=0'),'retry diagnostics distinguish requests from acceptance');
f=fixture(3,4);f.force.ReseatMissing_S(true);
check(f.force.IsMountComplete()&&f.calls.length===7,'IFV crew and infantry all recover to distinct seats');
check(new Set(f.calls.map(x=>x.slot)).size===7,'one occupant per seat');
f=fixture();f.crew[0].access.allow=false;report=f.force.ReseatMissing_S(true);
check(report.includes('accepted=4 attempted=5'),'native rejection distinguishable from retry being skipped');
check(report.includes('canEnter=false')&&report.includes('paused=true'),'paced rejection diagnostic records native entry eligibility and execution mode');
check(!f.force.IsMountComplete(),'accepted passengers cannot hide an absent driver');
f=fixture();f.crew[0].access.entering=false;check(f.force.Seat(f.crew[0],f.slots[0])&&f.calls[0].paused,'first placement also uses paused execution');
f=fixture();f.force.m_bMounted=true;f.force.Seat(f.crew[0],f.slots[0]);check(f.calls[0].paused===false,'live-operation reseating does not force paused execution');
f=fixture();const turretEntity={turret:true};f.slots[0].owner=turretEntity;f.crew[0].access.entering=false;f.force.Seat(f.crew[0],f.slots[0]);
check(f.calls[0].target===turretEntity&&f.calls[0].close===env.ECloseDoorAfterActions.INVALID,'first placement targets the seat on its own (child) entity with no door action, as the base game does');
f=fixture();f.crew[0].access.entering=false;f.crew[0].access.allow=false;f.crew[0].access.allowUnpaused=true;
check(f.force.Seat(f.crew[0],f.slots[0])&&f.calls.length===2&&f.calls[0].paused&&!f.calls[1].paused,'a refused paused placement retries once the way the base game calls it');
for(const [label,mutate]of [
  ['client',f=>server=false],['already mounted',f=>f.force.m_bMounted=true],['dismounting',f=>f.force.m_bDismountRequested=true],
  ['already dismounted',f=>f.force.m_bDismounted=true],['no initial request',f=>f.force.m_bMountRequested=false],
  ['aircraft',f=>f.force.m_Profile.m_eType=3],['player',f=>f.crew[0].player=true],['player-controlled crew group',f=>f.force.m_CrewGroup.players=1],
  ['player-controlled infantry group',f=>f.force.m_Group.players=1],['dead actor',f=>f.crew[0].dead=true],
  ['foreign actor',f=>f.force.m_aMembers=[]],['moving vehicle',f=>f.vehicle.speed=2],['already seated',f=>f.crew[0].access.inside=true],
  ['exiting',f=>f.crew[0].access.exiting=true],['idle actor',f=>f.crew[0].access.entering=false],
  ['other vehicle',f=>f.crew[0].access.current={GetVehicle:()=>({})}]]){
  server=true;f=fixture();mutate(f);
  check(!f.force.RecoverStaleBoarding_S(f.crew[0],f.crew[0].access),'do not interrupt '+label);
  check(f.resets.length===0,'native queue untouched for '+label);
}
server=true;f=fixture();const other={};f.slots[1].reserved=other;
f.force.RecoverStaleBoarding_S(f.crew[0],f.crew[0].access);
check(f.slots[0].reserved===null&&f.slots[1].reserved===other,'only actor-owned empty claims cleared');
f=fixture();f.slots[0].occupant=other;
check(!f.force.Seat(f.crew[0],f.slots[0])&&f.slots[0].occupant===other,'real occupants never displaced');
f=fixture();f.crew[0].access.entering=false;f.slots[0].reserved=other;
check(!f.force.ReseatMember(f.crew[0],[f.slots[0]],null),'another actor reservation is not stolen');
f=fixture();f.crew[0].player=true;
check(!f.force.Seat(f.crew[0],f.slots[0])&&!f.calls.length,'player never teleported into deployment seat');
// A nonempty driver seat is insufficient when its occupant is not the assigned crew.
f=fixture();f.force.ReseatMissing_S(true);
const spare={...f.slots[1],occupant:f.crew[0]};f.crew[0].access.current=spare;
f.slots[0].occupant={character:true,GetCharacterController:()=>({IsDead:()=>false})};
check(!f.force.IsMountComplete(),'foreign driver cannot satisfy readiness while assigned crew sit elsewhere');
f=fixture();f.force.ReseatMissing_S(true);f.force.m_CrewGroup.GetAgents=()=>{};
check(!f.force.IsMountComplete(),'empty crew cannot activate merely because the driver compartment is occupied');
f=fixture();f.force.ReseatMissing_S(true);f.crew[0].dead=true;
check(!f.force.IsMountComplete(),'a dead assigned driver cannot be hidden by other living crew');
f=fixture();f.force.ReseatMissing_S(true);f.crew[0].player=true;
check(!f.force.IsMountComplete(),'player-controlled driver cannot pass automated deployment readiness');
f=fixture();f.force.ReseatMissing_S(true);f.force.m_CrewGroup.players=1;
check(!f.force.IsMountComplete(),'player takeover of the crew group revokes deployment readiness');
// An attached mounted gun can use a cargo-style compartment, rather than TurretCompartmentSlot.
f=fixture(2,2);f.slots[1].kind='cargo';f.slots[1].attachedTurret={};
// Native enumeration need not list the weapon ahead of ordinary passenger seats.
const mountedWeaponSeat=f.slots.splice(1,1)[0];f.slots.push(mountedWeaponSeat);
check(f.force.Mount_S(),'armed cargo-style compartment accepts the authored Humvee crew');
check(mountedWeaponSeat.occupant===f.crew[1]&&f.force.IsMountComplete(),'assigned gunner occupies attached weapon before dispatch');
for(const [label,change]of [
 ['empty gunner',f=>{f.slots[1].occupant=null;const cargo={...f.slots[2],occupant:f.crew[1]};f.slots.push(cargo);f.crew[1].access.current=cargo;}],
 ['dead gunner',f=>f.crew[1].dead=true],['player gunner',f=>f.crew[1].player=true],
 ['inaccessible gunner seat',f=>f.slots[1].accessible=false],
 ['foreign gunner',f=>{const foreign={...f.crew[1],access:{...f.crew[1].access}};f.slots[1].occupant=foreign;const cargo={...f.slots[2],occupant:f.crew[1]};f.slots.push(cargo);f.crew[1].access.current=cargo;}]
]){
 f=fixture(2,2);f.force.ReseatMissing_S(true);change(f);
 check(!f.force.IsMountComplete(),label+' cannot satisfy dispatch readiness');
}
for(const [label,change]of [
 ['occupied turret',f=>f.slots[1].occupant={player:true}],['blocked turret',f=>f.slots[1].accessible=false],
 ['reserved turret',f=>f.slots[1].reserved={}]
]){
 f=fixture(2,2);change(f);check(!f.force.Mount_S(),label+' must not be skipped in favour of cargo');
}
f=fixture(3,2);f.force.ReseatMissing_S(true);f.slots[2].occupant=null;
check(!f.force.IsMountComplete(),'every turret in a multi-gunner vehicle is required');
f=fixture(2,2);f.slots[1].kind='cargo';f.slots[1].attachedTurret={};f.force.ReseatMissing_S(true);
check(f.slots[1].occupant===f.crew[1]&&f.force.IsMountComplete(),'boarding recovery also prioritizes attached weapon seats');
check(f.force.ReseatMissing_S().includes('occupied gunner seats=1/1'),'retry diagnostics expose gunner occupancy');
f=fixture(2,2);f.force.m_CrewGroup.GetAgents=out=>out.push({GetControlledEntity:()=>f.crew[0]});
check(!f.force.Mount_S(),'insufficient authored crew must not launch a driver-only armed vehicle');
for(const [crewCount,cargoCount]of [[1,13],[2,3],[3,7]]){
 f=fixture(crewCount,cargoCount);Object.assign(f.force.m_Profile,{m_bFillPassengerSeats:true,m_iCrewCount:crewCount});f.force.m_iReservedMembers=22;
 check(f.force.PreparePassengerRoster_S(),'native full load fits reserved capacity');
 check(f.force.m_Group.m_aUnitPrefabSlots.length===cargoCount,'every passenger seat receives an infantry slot');
 check(f.force.m_Group.m_aUnitPrefabSlots.every((x,i)=>x===['rifle','support'][i%2]),'full load repeats the authored infantry mix');
}
f=fixture(2,3);Object.assign(f.force.m_Profile,{m_bFillPassengerSeats:true,m_iCrewCount:2});f.force.m_iReservedMembers=4;
check(!f.force.PreparePassengerRoster_S()&&f.force.m_Group.m_aUnitPrefabSlots.length===2,'under-reserved full load is rejected before changing spawn queues');
f=fixture(2,3);Object.assign(f.force.m_Profile,{m_bFillPassengerSeats:true,m_iCrewCount:1});f.force.m_iReservedMembers=22;
check(!f.force.PreparePassengerRoster_S(),'missing turret crew cannot be replaced by dismount infantry');
f=fixture(2,3);Object.assign(f.force.m_Profile,{m_bFillPassengerSeats:true,m_iCrewCount:2});f.force.m_iReservedMembers=22;f.slots[1].kind='cargo';f.slots[1].attachedTurret={};
check(f.force.PreparePassengerRoster_S()&&f.force.m_Group.m_aUnitPrefabSlots.length===3,'mounted weapon is excluded from passenger capacity');
for(const escorts of [0,1]){
 f=fixture(1,24);Object.assign(f.force.m_Profile,{m_eType:3,m_bFillPassengerSeats:true,m_iCrewCount:1+escorts,m_iEscortCount:escorts});f.force.m_iReservedMembers=32;
 check(f.force.PreparePassengerRoster_S()&&f.force.m_Group.m_aUnitPrefabSlots.length===24,'helicopter fills cabin independently of the separate escort pilot');
 f.force.m_iReservedMembers=24+escorts;
 check(!f.force.PreparePassengerRoster_S(),'helicopter cabin must leave reserved room for transport and escort pilots');
}
f=fixture(2,4);f.slots.forEach(s=>s.reserved=null);f.all.forEach(a=>{a.access.entering=false;a.access.GetInVehicle=(v,s)=>{f.calls.push({actor:a,slot:s});return true;}});
f.force.ReseatMissing_S();
check(f.calls.length===6&&new Set(f.calls.map(c=>c.slot)).size===6,'ground retry assigns distinct destinations without synchronous native reservation updates');
// Last resort: refused seating is replaced by the seat's default occupant spawned straight into the seat.
{
  const r=fixture(2,3);r.all.forEach(a=>{a.access.entering=false;a.access.allow=false;});deleted=[];
  const given=[],foreign={removed:0,RemoveAgent(){this.removed++;}};
  for(const g of [r.force.m_CrewGroup,r.force.m_Group]){g.added=[];g.AddAgent=function(a){this.added.push(a);};}
  r.slots.forEach(s=>{s.GetDefaultOccupantPrefab=()=>'{0000000000000000}US_Soldier.et';
    s.SpawnDefaultCharacterInCompartment=g=>{given.push(g);const agent={GetParentGroup:()=>foreign};
      const c={character:true,spawned:true,GetCharacterController:()=>({IsDead:()=>false}),access:{IsInCompartment:()=>true,IsGettingIn:()=>false,IsGettingOut:()=>false,GetCompartment:()=>s},
        FindComponent(t){return t===env.AIControlComponent?{GetControlAIAgent:()=>agent}:this.access;}};
      s.occupant=c;return c;};});
  const originals=[...r.all];
  check(r.force.ReplaceUnseated_S()===5,'every refused crew member and passenger is replaced in its seat');
  check(r.slots.every(s=>s.occupant&&s.occupant.spawned),'driver, gunner and cargo seats are all occupied by the spawned replacements');
  check(given[0]===r.force.m_CrewGroup&&given[1]===r.force.m_CrewGroup&&given.slice(2).every(g=>g===r.force.m_Group),'crew seats spawn into the crew group, cargo seats into the infantry group');
  check(r.force.m_CrewGroup.added.length===2&&r.force.m_Group.added.length===3&&foreign.removed===5,'replacements spawned into another group are moved into the package groups');
  check(deleted.length===5&&originals.every(a=>deleted.includes(a)),'the characters left standing are removed');
  check(r.force.m_aPassengers.every(p=>p.spawned),'the passenger roster follows the replacements for delivery and dismount');
  check(r.force.ReplaceUnseated_S()===0&&deleted.length===5,'occupied seats are never replaced twice');
  const none=fixture(1,2);none.all.forEach(a=>{a.access.entering=false;});none.slots.forEach(s=>{s.GetDefaultOccupantPrefab=()=>'';});deleted=[];
  check(none.force.ReplaceUnseated_S()===0&&!deleted.length,'a seat without a default occupant keeps its member and the normal retries');
  const moving=fixture(1,2);moving.force.m_bMounted=true;check(moving.force.ReplaceUnseated_S()===0,'never used once the package has mounted');
}
console.log(JSON.stringify({assertions,note:'Native paused simulation/queue cancellation must be verified in Workbench Play.'}));
