/* Actual garrison seating/order methods with mocked native services. No native acceptance claim. */
function runGarrisonCrewTests(source) {
 Object.defineProperty(String.prototype,'IsEmpty',{value:function(){return this.length===0;},configurable:true});
 let assertions=0,clock=0;
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 const deleted=[];
 const env={Replication:{IsServer:()=>true},Math:{Max:Math.max,Min:Math.min,Mod:(a,b)=>a%b,Clamp:(v,a,b)=>Math.max(a,Math.min(b,v))},
  vector:{Zero:[0,0,0],DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
  string:{Format:(s,...a)=>s.replace(/%(\d+)/g,(_,n)=>String(a[n-1]))},Print:()=>{},LogLevel:{NORMAL:0},
  GetGame:()=>({GetWorld:()=>({GetWorldTime:()=>clock}),GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:e=>e.player?1:0})}),
  ECloseDoorAfterActions:{CLOSE_DOOR:0},SENTRY_VARIANT:1000,Math3D:{AnglesToMatrix:()=>{}},SCR_EntityHelper:{DeleteEntityAndChildren:e=>deleted.push(e)}};
 for(const name of ['BaseGameEntity','BaseCompartmentManagerComponent','SCR_AIVehicleUsageComponent','DamageManagerComponent','CompartmentAccessComponent','FactionAffiliationComponent','AIWaypoint'])env[name]={Cast:x=>x};
 env.ChimeraCharacter={Cast:e=>e&&e.character?e:null};
 const names=['IsPlayerOrForeignAsset','MaintainCrewSeats_S','HasMountedDefence','CrewAndOrder','IsSentry','AssignMembers','HasDormantReserve','InfantryDefenceGoal'];
 function compile(name){
  const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!m)throw Error('Missing actual method '+name);
  const args=m[1].replace(/\b(?:AROR_\w+|IEntity|bool)\s+/g,'');
  let body=m[2].replace(/array<\w+> (\w+) = \{\};/g,'let $1=[];').replace(/vector (\w+)\[4\];/g,'let $1=[];')
   .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
   .replace(/\b(?:bool|int|float|vector|IEntity|NavmeshWorldComponent|ChimeraAIWorld|ChimeraCharacter|DamageManagerComponent|CompartmentAccessComponent|FactionAffiliationComponent|Base\w+|AIWaypoint|SCR_\w+|AROR_\w+) (\w+);/g,'let $1=0;')
   .replace(/\b(?:bool|int|float|vector|IEntity|NavmeshWorldComponent|ChimeraAIWorld|ChimeraCharacter|DamageManagerComponent|CompartmentAccessComponent|FactionAffiliationComponent|Base\w+|AIWaypoint|SCR_\w+|AROR_\w+) (\w+) =/g,'let $1 =')
   .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x).replace(/\.Count\(\)/g,'.length');
  for(const n of [...names,'Spawn','GetOwner','GetDefenceCenter','GetDefenceHalfExtent'])body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(', 'g'),'this.'+n+'(');
  body=body.replace('lead.m_vPosition - this.GetDefenceCenter()','lead.m_vPosition.map((v,i)=>v-this.GetDefenceCenter()[i])')
   .replace('contact.m_vPosition - this.GetDefenceCenter()','contact.m_vPosition.map((v,i)=>v-this.GetDefenceCenter()[i])')
   .replace('this.GetDefenceCenter() + this.GetOwner().VectorToParent(local)','this.GetDefenceCenter().map((v,i)=>v+this.GetOwner().VectorToParent(local)[i])')
   .replace('let reachable=0;','let reachable=[0,0,0];');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
 }
 const compiled=Object.fromEntries(names.slice(0,5).map(n=>[n,compile(n)]));
 function fixture(kind=3){
  clock=0;
  const requests=[],events=[],waypoints=[];
  const seats=Array.from({length:kind===1?1:2},()=>({occupant:null,GetOccupant(){return this.occupant;},IsOccupied(){return !!this.occupant;}}));
  const usage={},damage={IsDestroyed:()=>vehicle.destroyed,GetHealthScaled:()=>1};
  const vehicle={destroyed:false,GetOrigin:()=>[0,0,0],FindComponent:type=>type===env.BaseCompartmentManagerComponent?{GetCompartments:out=>out.push(...seats)}:type===env.SCR_AIVehicleUsageComponent?usage:type===env.DamageManagerComponent?damage:type===env.FactionAffiliationComponent?{GetAffiliatedFactionKey:()=>vehicle.foreign?'FOREIGN':'OWN'}:null};
  function crew(name){
   const actor={character:true,name,position:[2,0,0],accept:true,GetOrigin(){return this.position;},GetCharacterController:()=>({IsDead:()=>!!actor.dead})};
   const access={IsGettingIn:()=>!!actor.gettingIn,IsGettingOut:()=>!!actor.gettingOut,IsSwitchingSeatsAnim:()=>!!actor.switching,
    IsInCompartment:()=>!!actor.otherVehicle,GetVehicleCompartmentManagerOwner:()=>actor.otherVehicle,
    GetInVehicle:(hull,seat,force)=>{requests.push({actor,hull,seat,force,registered:events.includes('registered')});return actor.accept;}};
   actor.FindComponent=type=>type===env.CompartmentAccessComponent?access:type===env.FactionAffiliationComponent?{GetAffiliatedFactionKey:()=>actor.foreign?'FOREIGN':'OWN'}:null;
   return actor;
  }
  const actors=seats.map((_,i)=>crew(i===0?'driver':'gunner'));
  const slots=[{m_iKind:kind,m_Entity:vehicle,m_bSpawned:true},...actors.map((a,i)=>({m_iKind:4,m_Entity:a,m_iVehicle:0,m_iSeat:i,m_bSpawned:true,m_vPosition:[2,0,0]}))];
  const group={GetGroupUtilityComponent:()=>({AddUsableVehicle:()=>events.push('registered')}),
   AddWaypoint:wp=>waypoints.push(wp),RemoveWaypoint:wp=>events.push('removed'),IsDormant:()=>false};
  const pending={m_Group:group,m_iId:9,m_aSlots:actors.map((_,i)=>i+1),m_iNextSeatAttempt:0,m_iSeatAttempts:0,m_bOrdered:false,m_bMountedDefence:false};
  const state={...compiled,m_aSlots:slots,m_aGroups:[pending],m_sFactionKey:'OWN',m_eRosterRole:1,
   m_Location:{GetId:()=>'crew-fixture',GetControllingRole:()=>1},m_fCrewSeatRecoveryDistance:100,m_iCrewSeatRetrySeconds:2,m_fDefenceHoldRadius:6,
   Spawn:()=>({SetCompletionRadius:()=>{}}),AssignMembers:()=>{},HasDormantReserve:()=>false};
  return {state,pending,vehicle,seats,slots,actors,requests,events,waypoints,at:t=>clock=t,
   tick(){state.CrewAndOrder(pending);},occupy(){seats.forEach((s,i)=>s.occupant=actors[i]);}};
 }
 const f=fixture();f.tick();
 check(f.requests.length===2&&f.requests.every(r=>r.force&&r.hull===f.vehicle),'initial driver and gunner are placed directly into assigned seats');
 check(f.requests.every(r=>r.registered),'native usable-vehicle registration precedes compartment entry');
 check(!f.pending.m_bOrdered,'accepted boarding requests do not count as observed occupancy');
 f.occupy();f.at(1000);f.tick();
 check(f.pending.m_bOrdered&&f.pending.m_bMountedDefence,'actual driver/gunner occupancy establishes mounted readiness');
 check(f.waypoints.length===0,'mounted vehicle crews receive no infantry defend waypoint');
 f.tick();check(f.waypoints.length===0&&f.requests.length===2,'stable mounted readiness neither restarts orders nor reseats crew');
 f.seats[0].occupant=null;f.at(2000);f.tick();
 check(f.requests.length===3&&f.requests[2].actor===f.actors[0],'driver occupancy is maintained after the original ready latch');
 f.at(2500);f.tick();check(f.requests.length===3,'seating requests are throttled between audits');
 f.actors[0].accept=false;f.at(4000);f.tick();
 check(f.requests.length===4&&!f.state.MaintainCrewSeats_S(f.pending),'a rejected request cannot manufacture current seat readiness');

 const partial=fixture();partial.slots[2].m_Entity=null;partial.slots[2].m_bSpawned=false;partial.tick();
 check(partial.requests.length===1&&partial.requests[0].actor===partial.actors[0],'available driver seats before a second crew member leaves the queue');
 partial.seats[0].occupant=partial.actors[0];partial.at(2000);partial.slots[2].m_bPlacementUnavailable=true;partial.tick();
 check(partial.pending.m_bOrdered&&partial.pending.m_bMountedDefence,'omitted unproduced crew cannot block an existing driver from taking its defensive role');

 const swapped=fixture();swapped.seats[0].occupant=swapped.actors[1];swapped.seats[1].occupant=swapped.actors[0];swapped.tick();
 check(swapped.pending.m_bOrdered&&swapped.requests.length===0,'native seat swaps between assigned surviving crew are accepted without interference');
 const lost=fixture();lost.occupy();lost.tick();lost.slots[0].m_bLost=true;lost.vehicle.destroyed=true;lost.tick();
 check(lost.requests.length===0&&!lost.pending.m_bMountedDefence&&lost.waypoints.length===1,'hull loss preserves on-foot orders rather than forcing crew into a wreck');
 lost.tick();check(lost.waypoints.length===1,'on-foot transition emits one defend order');

 const dead=fixture();dead.actors.forEach(a=>a.dead=true);dead.tick();
 check(dead.requests.length===0&&!dead.pending.m_bOrdered,'dead crew are neither reseated nor treated as mounted readiness');
 const casualties=fixture();casualties.slots.slice(1).forEach(s=>{s.m_bLost=true;s.m_Entity=null;});const count=casualties.slots.length;casualties.tick();
 check(casualties.requests.length===0&&casualties.slots.length===count,'seat maintenance never generates replacement crew for casualties');
 const far=fixture();far.actors.forEach(a=>a.position=[101,0,0]);far.tick();
 check(far.requests.length===0,'seat recovery does not pull distant AI into the vehicle');
 const transition=fixture();transition.actors[0].gettingOut=true;transition.actors[1].switching=true;transition.tick();
 check(transition.requests.length===0,'active exit/seat-switch transitions are not interrupted');
 const elsewhere=fixture();elsewhere.actors.forEach(a=>a.otherVehicle={});elsewhere.tick();
 check(elsewhere.requests.length===0,'crew aboard another vehicle are not remotely transferred');

 const captured=fixture();captured.seats[1].occupant={player:true};captured.tick();
 check(captured.requests.length===0,'a player occupant protects the whole captured vehicle from reseating');
 const controlled=fixture();controlled.actors[0].player=true;controlled.tick();
 check(!controlled.requests.some(r=>r.actor===controlled.actors[0]),'player-controlled crew are never teleported');
 const foreignSeat=fixture();foreignSeat.seats[0].occupant={};foreignSeat.tick();
 check(foreignSeat.requests.length===0,'untracked occupied seats are never displaced');
 const foreignHull=fixture();foreignHull.vehicle.foreign=true;foreignHull.tick();
 check(foreignHull.requests.length===0,'explicit foreign affiliation prevents vehicle commandeering');
 const foreignCrew=fixture();foreignCrew.actors.forEach(a=>a.foreign=true);foreignCrew.tick();
 check(foreignCrew.requests.length===0,'foreign crew are never reseated');

 const parked=fixture(2);parked.tick();
 check(parked.requests.length===0&&!parked.pending.m_bMountedDefence,'intentionally parked armour remains unmanned');
 const mg=fixture(1);mg.tick();mg.occupy();mg.at(1000);mg.tick();
 check(mg.requests.length===1&&mg.pending.m_bMountedDefence&&mg.waypoints.length===0,'static MG gunners use the same verified mounted posture');
 const infantry=fixture();infantry.slots.slice(1).forEach(s=>{s.m_iKind=0;s.m_iVehicle=-1;});infantry.tick();infantry.tick();
 check(infantry.requests.length===0&&infantry.waypoints.length===1&&!infantry.pending.m_bMountedDefence,'ordinary infantry retain one on-foot defend order');
 const missing=fixture();missing.slots[2].m_Entity=null;missing.slots[2].m_bSpawned=false;missing.tick();
 check(!missing.pending.m_bOrdered,'unresolved live queue members still delay group readiness after seating the available driver');
 const save=fixture();compile('Observe_S').call(save.state,false);
 check(save.requests.length===0&&save.waypoints.length===0,'saving observes state without seating or issuing orders');
 const client=fixture();env.Replication.IsServer=()=>false;client.tick();
 check(client.requests.length===0&&client.waypoints.length===0,'clients cannot seat garrison crew or issue orders');
 env.Replication.IsServer=()=>true;
 let contact=null,walkable=true,queries=0;
 const originalGame=env.GetGame;
 env.ChimeraAIWorld={Cast:x=>x};
 const soldiers={GetReachablePoint(point,radius,out){queries++;out.splice(0,3,...point);return walkable;}};
 env.GetGame=()=>({...originalGame(),GetWorld:()=>({...originalGame().GetWorld(),GetSurfaceY:()=>0}),GetAIWorld:()=>({GetNavmeshWorldComponent:()=>soldiers})});
 env.AROR_OperationManagerComponent={GetInstance:()=>({GetRecentCombatContact:()=>contact,ReportAttack_S(){throw Error('Patrol must not dispatch a QRF');}})};
 const goal=compile('InfantryDefenceGoal');
 const patrol=fixture();patrol.slots.slice(1).forEach(s=>{s.m_iKind=0;s.m_iVehicle=-1;});
 Object.assign(patrol.slots[1],{m_vPosition:[7,0,7]});
 Object.assign(patrol.state,{m_bLocalDefencePatrols:true,m_iInfantryOrderInterval:45,InfantryDefenceGoal:goal,
  GetDefenceHalfExtent:()=>18,GetDefenceCenter:()=>[0,0,0],GetOwner:()=>({VectorToLocal:v=>v.slice(),VectorToParent:v=>v.slice()})});
 Object.assign(patrol.state.m_Location,{GetRadius:()=>150,IsPositionInside:p=>Math.hypot(p[0],p[2])<=150});
 Object.assign(patrol.pending,{m_iNextFootOrder:0,m_iPatrolLeg:0,m_iContactSequence:0});
 const firstPatrol=goal.call(patrol.state,patrol.pending,patrol.slots[1]);
 const secondPatrol=goal.call(patrol.state,patrol.pending,patrol.slots[1]);
 check(firstPatrol[0]===3.5&&secondPatrol[0]===7,'mobile fireteams alternate bounded interior and home-sector tasks');
 contact={m_sLocationId:'crew-fixture',m_vPosition:[140,0,0],m_iSequence:3};
 const reaction=goal.call(patrol.state,patrol.pending,patrol.slots[1]);
 check(reaction[0]===8&&reaction[2]===0&&patrol.pending.m_iContactSequence===3,'recent reported contact directs a mobile team to its own defensive edge');
 contact={...contact,m_sLocationId:'other-site'};
 goal.call(patrol.state,patrol.pending,patrol.slots[1]);
 check(patrol.pending.m_iContactSequence===0,'a neighbouring location report does not redirect this site\'s patrol');
 walkable=false;
 check(goal.call(patrol.state,patrol.pending,patrol.slots[1])===patrol.slots[1].m_vPosition,'unwalkable patrol target falls back to the established defence position');walkable=true;contact=null;
 patrol.tick();const firstOrders=patrol.waypoints.length,firstQueries=queries;
 patrol.at(1000);patrol.tick();
 check(patrol.waypoints.length===firstOrders&&queries===firstQueries,'patrol path queries and waypoints are throttled between tactical orders');
 patrol.at(60000);patrol.tick();
 check(patrol.waypoints.length===firstOrders+1,'patrol can update its sector after the configured interval');
 const mountedPatrol=fixture(1);mountedPatrol.state.m_bLocalDefencePatrols=true;mountedPatrol.tick();mountedPatrol.occupy();mountedPatrol.at(1000);mountedPatrol.tick();mountedPatrol.at(60000);mountedPatrol.tick();
 check(mountedPatrol.waypoints.length===0&&mountedPatrol.pending.m_bMountedDefence,'local patrols never issue infantry orders to a manned static');
 return {assertions,note:'Native compartments and AI mocked; Workbench compile and sustained mounted-defence acceptance still required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path');
 console.log(runGarrisonCrewTests(fs.readFileSync(path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Garrisons/AROR_GarrisonComponent.c'),'utf8')));
}
