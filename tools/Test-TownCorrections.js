// Execute current Enforce method bodies against explicit native-service contracts. This is not an engine test.
const fs=require('fs'),path=require('path'),assert=require('assert/strict');
const root=path.resolve(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
const read=p=>fs.readFileSync(path.join(root,p),'utf8');
const source={militia:read('Towns/AROR_TownMilitiaComponent.c'),team:read('Towns/AROR_MilitiaTeam.c'),site:read('Locations/AROR_SiteLifeComponent.c'),
 force:read('Common/AROR_ForceSpawner.c'),population:read('Common/AROR_PopulationBudget.c'),high:read('HighCommand/AROR_HighCommandComponent.c'),
 spotting:read('Operations/AROR_QRFSpotting.c'),executor:read('Operations/AROR_OperationExecutorComponent.c'),town:read('Towns/AROR_TownWavesComponent.c')};
let assertions=0,server=true,deleted=[],requests=[],near=0,positions=[],militiaDefenders=0,clock=0,mode,groupSerial=0;
const check=(ok,msg)=>{assertions++;assert(ok,msg);};
const equal=(a,b,msg)=>{assertions++;assert.deepEqual(a,b,msg);};
for(const [n,fn]of Object.entries({Count(){return this.length},Find(x){return this.indexOf(x)},Contains(x){return this.includes(x)},IsEmpty(){return !this.length},Insert(x){this.push(x)},InsertAll(xs){this.push(...xs)},Clear(){this.length=0},Copy(xs){this.splice(0,this.length,...xs)},Remove(i){this.splice(i,1)},RemoveItem(x){const i=this.indexOf(x);if(i>=0)this.splice(i,1)}}))Object.defineProperty(Array.prototype,n,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const methods=new Set(Object.values(source).flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])));
const type='(?:bool|int|float|string|vector|IEntity|Faction|Instigator|ResourceName|Base\\w+|CompartmentAccessComponent|ChimeraCharacter|CharacterControllerComponent|AIAgent|SCR_\\w+|AROR_\\w+)';
const components={access:{Cast:x=>x},compartments:{Cast:x=>x},usage:{Cast:x=>x},damage:{Cast:x=>x}};
const env={Replication:{IsServer:()=>server},Math:{Max:Math.max,Min:Math.min,Clamp:(v,a,b)=>Math.max(a,Math.min(b,v)),Floor:Math.floor,Mod:(a,b)=>a%b},
 vector:{Zero:[0,0,0],DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},GetGame:()=>({GetGameMode:()=>mode,GetFactionManager:()=>({GetFactionByKey:()=>({})}),GetCallqueue:()=>({CallLater(){},Remove(){}}),GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:e=>e?.player||0,GetPlayers(){}})}),
 Print(){},LogLevel:{NORMAL:0},string:{Empty:'',Format:()=>''},FORM_UP_SECONDS:30,
 AROR_EFactionRole:{RESISTANCE:0},AROR_ELocationType:{TOWN:0,CITY:3},AROR_EMilitiaKind:{INFANTRY:0,APC:1,ARMOUR:2},
 AROR_EHighCommandEmbarkPhase:{TO_SITE:1},AROR_EHighCommandOrder:{HOLD:0},ECloseDoorAfterActions:{CLOSE_DOOR:1,INVALID:0},EGetOutType:{TELEPORT:0},
 CompartmentAccessComponent:components.access,BaseCompartmentManagerComponent:components.compartments,SCR_AIVehicleUsageComponent:components.usage,
 PilotCompartmentSlot:{Cast:s=>s?.driver?s:null},TurretCompartmentSlot:{Cast:s=>s?.turret?s:null},CargoCompartmentSlot:{Cast:s=>s?.cargo?s:null},
 ChimeraCharacter:{Cast:x=>x},SCR_DamageManagerComponent:components.damage,SCR_BaseGameMode:{Cast:x=>x},
 SCR_EntityHelper:{DeleteEntityAndChildren:e=>{deleted.push(e);e.removed=true}},AROR_ResistanceFleet:{IsClaimed:e=>!!e?.claimed},
 AROR_ResistancePresence:{Nearest:()=>near,GetPositions:out=>out.push(...positions)},
 AROR_HostilesQuery:{HasHostilesInRadius:()=>false},AROR_TownMilitiaComponent:{GetInstance:()=>({HasDefenders_S:()=>militiaDefenders>0,CountDefenders_S:()=>militiaDefenders})},
 AROR_GarrisonManagerComponent:{GetInstance:()=>({GetCommittedPersonnel_S:()=>10})},AROR_SquadManagerComponent:{GetInstance:()=>({GetCommittedPersonnel_S:()=>2})},
 AROR_OperationExecutorComponent:{Cast:x=>x},AROR_GroundOperationForce:{Cast:f=>f?.ground||f?.air?f:null},AROR_ReaperAirForce:{Cast:f=>f?.air?f:null},
 AROR_PopulationGroup:class{},AROR_ForceSpawner:{IsAlive:e=>!!e&&!e.dead,IsDestroyed:e=>!e||e.destroyed,IsVehicleInUse:e=>!!e&&(e.claimed||e.playerAboard)},
 AROR_MilitiaMember:class{constructor(){this.m_bDead=false}Attach(e){this.m_Character=e}CheckDeath(){if(this.m_Character?.dead)this.m_bDead=true}Detach(){}}};
function compile(s,name,{outputs=false,member=false}={}){
 if(member)s=s.slice(s.indexOf('class AROR_MilitiaMember'));
 const m=s.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));assert(m,'actual method '+name);
 const args=m[1].replace(/\b(?:out |notnull )?(?:array<[^>]+>|\w+)\s+(\w+)/g,'$1');
 let b=m[2].replace(/array<(?:ref )?\w+> (\w+) = \{\};/g,'let $1=[];')
  .replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g,'for(const [$1,$2] of $3.entries())')
  .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
  .replace(/\bvector (\w+);/g,'let $1=[0,0,0];')
  .replace(new RegExp('\\b'+type+' (\\w+);','g'),'let $1=0;')
  .replace(new RegExp('\\b'+type+' (\\w+) =','g'),'let $1 =')
  .replace(/(?<![.\w])(?:m_|s_)\w+/g,n=>'this.'+n)
  .replace(/(?<![.\w])(\w+)\(/g,(x,n)=>methods.has(n)?'this.'+x:x)
  .replace(/\.Insert\((On\w+)\)/g,'.Insert(this.$1.bind(this))').replace(/\.Remove\((On\w+)\)/g,'.Remove(this.$1)');
 if(name==='DefenderElementLimit')b=b.replace('(fighters + perElement - 1) / perElement','Math.Floor((fighters + perElement - 1) / perElement)');
 if(name==='WaveElements')b=b.replace('(crackdown + 1) / 2','Math.Floor((crackdown + 1) / 2)');
 if(outputs)b=b.replace('return best >= 0;','return {found:best>=0,lead,air,landing};').replace('return true;','return {found:true,position};');
 return new Function(...Object.keys(env),'return function('+args+'){'+b+'}')(...Object.values(env));
}
const group=(actors=[],dormant=0)=>({id:++groupSerial,GetAgents:out=>out.push(...actors.map(e=>({GetControlledEntity:()=>e}))),IsDormant:()=>dormant>0,GetDormantAliveCount:()=>dormant,GetPlayerCount:()=>0,GetOnAgentAdded:()=>({Insert(){},Remove(){}}),GetOrigin:()=>[0,0,0],GetFaction:()=>null});
env.AROR_ForceSpawner.CountLiving=compile(source.force,'CountLiving');
env.AROR_ForceSpawner.CountStrength=compile(source.force,'CountStrength');
env.AROR_ForceSpawner.RemoveGroup_S=g=>{if(g?.inUse)return false;if(g)deleted.push(g);return true};
const losses=compile(source.militia,'CountLosses_S');
const book=()=>({m_aInfantry:[12],m_aApcs:[1],m_aArmour:[0]});
const post=()=>({m_sTownId:'town',m_aGroups:[],m_aVehicles:[],m_aCrews:[],m_aCrewWaypoints:[],m_aVehicleKinds:[],m_aSeated:[],m_aDismountedCrews:[],m_aDismountedWaypoints:[],m_aWrecks:[],m_iRaisedAt:0});
function team(size=4){const t={m_iSize:size,m_iRaisedAt:0,m_iLosses:0,m_iReportedLosses:0,m_aMembers:[]};for(const n of ['OnAgentAdded','Attach','TakeLosses','GetStrength'])t[n]=compile(source.team,n);return t}
let t=team();t.Attach(group(),4,0);equal(t.TakeLosses(),0,'pending spawn is not a loss');
const a={dead:false};t.OnAgentAdded({GetControlledEntity:()=>a});equal(t.TakeLosses(),0,'partial materialization preserves pending soldiers');
a.dead=true;equal(t.TakeLosses(),1,'confirmed death counted');equal(t.TakeLosses(),0,'death counted once');equal(t.GetStrength(),3,'only actual death lowers strength');
t=team(12);t.Attach(group([],12),12,0);let p=post(),b=book();p.m_aGroups=[t];losses.call(b,p,0);equal(b.m_aInfantry[0],12,'dormant militia survives save accounting');
const materialized={dead:false};t.OnAgentAdded({GetControlledEntity:()=>materialized});materialized.dead=true;losses.call(b,p,0);equal(b.m_aInfantry[0],11,'casualty lowers saved book without erasing pending members');
const d=compile(source.team,'CheckDeath',{member:true});const unknown={m_Character:{GetCharacterController:()=>null},m_Damage:null,m_bDead:false};d.call(unknown);check(!unknown.m_bDead,'missing controller during virtualisation is not death');
unknown.m_Character={GetCharacterController:()=>({IsDead:()=>true})};d.call(unknown);check(unknown.m_bDead,'explicit native death is recorded');
p=post();b=book();const crew=group();p.m_aVehicles=[{destroyed:true}];p.m_aVehicleKinds=[1];p.m_aCrews=[crew];p.m_aSeated=[true];losses.call(b,p,0);
equal([p.m_aCrews.length,p.m_aDismountedCrews[0],b.m_aApcs[0]],[0,crew,0],'destroyed hull transfers surviving crew ownership');
losses.call(b,p,0);equal(p.m_aDismountedCrews.length,1,'repeated counting does not duplicate crew');
const claimed={claimed:true,playerAboard:true};p=post();p.m_aVehicles=[claimed];p.m_aVehicleKinds=[1];p.m_aCrews=[null];p.m_aSeated=[true];b=book();deleted=[];
const stand=compile(source.militia,'StandDown_S');let owner={...b,m_aTownIds:['town'],m_aPosts:[p],m_iClock:100,CountLosses_S:losses};stand.call(owner,p);
check(!deleted.includes(claimed),'claimed player hull is not deleted');equal(owner.m_aApcs[0],0,'claimed hull transfers out of saved militia book');equal(owner.m_aPosts.length,0,'completed cleanup owner retires');
p=post();crew.inUse=true;p.m_aDismountedCrews=[crew];owner={...book(),m_aTownIds:['town'],m_aPosts:[p],CountLosses_S:losses};stand.call(owner,p);equal(owner.m_aPosts.length,1,'protected crew retains cleanup owner');crew.inUse=false;stand.call(owner,p);equal(owner.m_aPosts.length,0,'crew retires after use ends');
const forget=compile(source.site,'Forget'),reap=compile(source.site,'ReapFormerForces');
function site(){return{m_aPatrols:[],m_aWorkers:[],m_aRetiringGroups:[],m_aRetiringHelicopters:[],m_aPatrolLostAt:[],m_aWorkerLostAt:[],m_aHelicopters:[],m_aHelicopterLostAt:[],m_Truck:null,m_iClock:200,m_fRetireDistance:1600,CanRetireGroup:()=>true,NearestPlayer:()=>10000}}
for(let cycle=0;cycle<500;cycle++){
 const s=site(),old={m_Group:group([{}]),m_bSeen:true,Remove_S(){deleted.push(this);return true}};s.m_aPatrols=[old];s.m_aWorkers=[{...old}];s.m_Truck={...old};s.m_aHelicopters=[{claimed:true},{claimed:false,GetOrigin:()=>[0,0,0]}];
 forget.call(s);equal(s.m_aRetiringGroups.length,3,'handover retains every former force');forget.call(s);equal(s.m_aRetiringGroups.length,3,'handover is idempotent');
 reap.call(s,false);equal(s.m_aRetiringGroups.length,3,'live former defenders remain during battle');equal(s.m_aRetiringHelicopters.length,1,'claimed aircraft transfer to fleet');
 reap.call(s,true);equal([s.m_aRetiringGroups.length,s.m_aRetiringHelicopters.length],[0,0],'repeated capture/retirement returns owned collections to baseline');
}
let s=site();s.m_aRetiringGroups=[{m_Group:group([],4),m_bSeen:true,Remove_S(){throw Error('dormant survivors removed')}}];reap.call(s,false);equal(s.m_aRetiringGroups.length,1,'native dormant site survivors remain tracked');
const slot=(id,kind)=>({id,driver:kind==='driver',cargo:kind==='cargo',occupant:null,IsCompartmentAccessible:()=>true,IsOccupied(){return !!this.occupant},GetOccupant(){return this.occupant}});
const driver=slot('driver','driver'),cargo=slot('cargo','cargo');
const vehicle={FindComponent:type=>type===components.compartments?{GetCompartments:out=>out.push(driver,cargo)}:{GetPilotCompartmentSlot:()=>driver}};
const actors=Array.from({length:3},(_,id)=>({id,FindComponent:()=>({GetCompartment:()=>null,IsInCompartment:()=>false,IsGettingIn:()=>false,IsGettingOut:()=>false,GetInVehicle:(v,seat)=>{requests.push([id,seat.id]);return true}})}));
let squad={m_Vehicle:vehicle,m_Group:group(actors),m_aBoardingSeats:[],m_aBoardingMembers:[]};const seat=compile(source.high,'SeatSquad_S');seat(squad);
equal(requests,[[0,'driver'],[1,'cargo']],'asynchronous seat requests have exclusive assignments');requests=[];seat(squad);equal(requests,[[0,'driver'],[1,'cargo']],'retry preserves assignments');
driver.occupant=actors[0];const hasDriver=compile(source.high,'HasDriver_S');check(hasDriver(squad),'confirmed living squad driver is ready');driver.occupant.dead=true;check(!hasDriver(squad),'dead driver is not ready');driver.occupant=null;check(!hasDriver(squad),'cargo-only squad is not ready');
let success;compile(source.high,'UpdateEmbark_S').call({m_iBoardTimeout:60,SeatSquad_S:()=>false,CountMounted_S:()=>1,HasDriver_S:()=>false,VehicleName:()=>'',FinishEmbark_S:(s,m,ok)=>success=ok},{m_eEmbarkPhase:2,m_Vehicle:{},m_bFormed:true,m_iEmbarkStarted:1000},62001);equal(success,false,'timeout cannot turn one passenger into driving success');
let dismounts=0;const failedBoarding={m_aBoardingMembers:[],m_aBoardingSeats:[],m_bMounted:true,m_vPosition:[0,0,0],m_iOwnerId:0};compile(source.high,'FinishEmbark_S').call({ClearWaypoint_S(){},DismountMembers_S(){dismounts++},IssueWaypoint_S(){}},failedBoarding,'',false);equal([dismounts,failedBoarding.m_bMounted],[1,false],'failed readiness releases stranded cargo instead of leaving a driverless vehicle claimed as mounted');
const confirmed=compile(source.spotting,'GetConfirmedPosition_S',{outputs:true});const entity={};let report={m_bConfirmed:false,m_iLastSeen:1,m_vPosition:[9,0,9]};const intel={Find:()=>report};
check(!confirmed.call(intel,entity,2,90,[]),'one unconfirmed look does not expose a track');report.m_bConfirmed=true;equal(confirmed.call(intel,entity,2,90,[]).position,[9,0,9],'track uses stored last sighting');check(!confirmed.call(intel,entity,91,90,[]),'expired observation is hidden');entity.claimed=true;check(!confirmed.call(intel,entity,2,90,[]),'claimed vehicle stops being enemy intel');entity.claimed=false;
const airAsset={seen:[1000,0,0]},groundAsset={seen:[100,0,0]};const forces=[{air:true,m_sOperationId:'joint',m_vLanding:[500,0,0],m_Vehicle:airAsset},{ground:true,m_sOperationId:'joint',m_Vehicle:groundAsset}];
const track=compile(source.executor,'GetOperationTrack_S',{outputs:true}),op={m_aForces:forces,m_Operations:{GetSightedPosition_S:(e,out)=>{if(!e.seen)return false;out.Copy(e.seen);return true}}};
let found=track.call(op,'joint',[0,0,0],[],false,[]);equal([found.lead,found.air,found.landing],[[100,0,0],false,[0,0,0]],'ground head retains ground type and no secret landing plan');
groundAsset.seen=null;found=track.call(op,'joint',[0,0,0],[],false,[]);check(found.air&&found.lead[0]===1000,'only observed air element yields air track');airAsset.seen=null;check(!track.call(op,'joint',[0,0,0],[],false,[]).found,'unobserved operations expose no precise track');
const presence=compile(source.town,'IsResistanceNear');near=500;check(presence.call({m_Location:{GetCenter:()=>[],GetId:()=>''},m_fAbandonDistance:1500}),'remote commanded forces prevent abandonment');near=10000;militiaDefenders=4;check(presence.call({m_Location:{GetCenter:()=>[],GetId:()=>''},m_fAbandonDistance:1500}),'militia defenders prevent abandonment');militiaDefenders=0;
const limit=compile(source.town,'DefenderElementLimit'),wave=compile(source.town,'WaveElements');const town={m_iFightersPerElement:8,m_iWaves:3,m_iExtraElements:0,m_fDefenderScaleDistance:1000,m_bScaleToDefenders:true,m_Location:{GetCenter:()=>[0,0,0],GetId:()=>'',GetLocationType:()=>0},DefenderElementLimit:limit};
for(const [fighters,expected]of [[2,1],[8,1],[9,2],[20,3],[62,8]]){positions=Array.from({length:fighters},()=>[0,0,0]);equal(limit.call(town,1),expected,'nearby force scales admission');equal(limit.call(town,3),Math.min(expected+1,8),'final wave has a bounded extra element');}
positions=[[0,0,0],[0,0,0]];equal([wave.call(town,1,5),wave.call(town,2,5),wave.call(town,3,5)],[1,1,2],'high campaign heat cannot multiply ground waves against a small force');
const limitForPopulation=compile(source.executor,'GetPopulationLimit');equal(limitForPopulation.call({m_iPopulationLimit:320,m_iCombatPersonnelReserve:72},true),248,'configured ambient headroom applies');equal(limitForPopulation.call({m_iPopulationLimit:0,m_iCombatPersonnelReserve:-1},false),96,'malformed configuration cannot silently disable every producer');
const population={s_aGroups:[],s_Mode:null};for(const n of ['Mode','RegisterGroup_S','GetAvailable_S'])population[n]=compile(source.population,n);let own=100;const executor={GetCommittedPersonnel_S:()=>own,GetPopulationLimit:ambient=>ambient?248:320};mode={FindComponent:()=>executor};
equal(population.GetAvailable_S(),208,'shared budget counts operations, garrisons and native recruits once');population.RegisterGroup_S(group(),6);equal(population.GetAvailable_S(),202,'pending group reserves immediately');equal(population.GetAvailable_S(true),130,'ambient leaves combat headroom');
population.RegisterGroup_S(population.s_aGroups[0].m_Group,6);equal(population.GetAvailable_S(),202,'re-registering one group cannot charge capacity twice');
population.s_aGroups[0].m_Group=null;equal(population.GetAvailable_S(),208,'deleted group releases capacity');own=310;equal(population.GetAvailable_S(),0,'shared population never admits negative free slots');
env.AROR_PopulationBudget={GetAvailable_S:()=>0};
// compile captures the environment at construction; now bind the real accounting wrapper with the supplied free budget.
equal(compile(source.executor,'GetReservedMembers').call({m_iMaxMembers:220,GetCommittedPersonnel_S:()=>100}),220,'operations stop admitting when other systems fill shared capacity');
mode={FindComponent:()=>executor};own=100;equal(population.GetAvailable_S(),208,'new world clears old group reservations');
let seed=17;
for(let tick=0;tick<10000;tick++){
 seed=(seed*1664525+1013904223)>>>0;
 const size=1+seed%8,ambient=(seed&8)!==0;
 if(size<=population.GetAvailable_S(ambient))population.RegisterGroup_S(group(),size);
 if(tick%3===0&&own+12<=220&&population.GetAvailable_S()>=12)own+=12;
 if(tick%7===0)own=Math.max(0,own-17);
 if(tick%2===0&&population.s_aGroups.length)population.s_aGroups[seed%population.s_aGroups.length].m_Group=null;
 const groups=population.s_aGroups.filter(e=>e.m_Group).reduce((n,e)=>n+e.m_iSize,0);
 check(own+12+groups<=320,'mixed queued admissions stay inside shared capacity');
}
let leases=0;const agentFor=e=>({GetControlledEntity:()=>e,PreventMaxLOD:n=>{equal(n,15,'simulation lease stays finite');leases++},ActivateAI:()=>{}});
const renew=compile(source.force,'RenewSimulation_S');const active={GetAgents:out=>out.push(agentFor({}),agentFor({player:1}),agentFor({dead:true}),null)};
renew.call(env.AROR_ForceSpawner,active);equal(leases,1,'only living AI receives activation lease');
const occupiedHull={claimed:true},protectedActor={FindComponent:()=>({IsInCompartment:()=>true,GetCompartment:()=>({GetVehicle:()=>occupiedHull})})};
const remove=compile(source.force,'RemoveGroup_S');check(!remove.call(env.AROR_ForceSpawner,group([protectedActor])),'real cleanup helper preserves crew in a claimed hull');
const waypoint={};t=team();t.Attach(group(),4,0);t.m_Waypoint=waypoint;t.Detach=compile(source.team,'Detach');compile(source.team,'Remove_S').call(t);check(deleted.includes(waypoint),'team cleanup removes its independently spawned waypoint');
env.AROR_FactionPresetsComponent={GetInstance:()=>({})};equal(compile(source.force,'SpawnGroup').call({},'group',0,[0],[0,0,0]),null,'native group creation is rejected before allocation when shared capacity is full');
const admissionOrder=[],queuedGroup={GetFaction:()=>null,InitFactionKey:()=>true,RequestSpawn:()=>admissionOrder.push('native queue')};for(const n of ['SetCanDeleteIfNoPlayer','SetPrivate','SetPrivacyChangeable','SetLifecyclePolicy'])queuedGroup[n]=()=>{};
env.SCR_AIGroup={IgnoreSpawning(){},Cast:x=>x};env.SCR_EAIGroupLifecyclePolicy={Manual:0};env.AROR_FactionPresetsComponent={GetInstance:()=>({FillGroup_S:()=>1})};env.AROR_PopulationBudget={GetAvailable_S:()=>10,RegisterGroup_S:()=>admissionOrder.push('reservation')};
equal(compile(source.force,'SpawnGroup').call({FactionKey:()=> 'R',SpawnPrefab:()=>queuedGroup},'group',0,[0],[0,0,0],true),queuedGroup,'admitted group returns its native owner');equal(admissionOrder,['reservation','native queue'],'reservation exists before a native spawn callback can re-enter admission');
server=false;equal(population.GetAvailable_S(),0,'clients cannot admit AI');check(!confirmed.call(intel,entity,2,90,[]),'clients cannot query server sighting state');
renew.call(env.AROR_ForceSpawner,active);equal(leases,1,'clients never renew server AI leases');
console.log(JSON.stringify({assertions,nativeVerified:false,soakCycles:500,admissionTicks:10000,note:'Actual current methods with mocked native services; compile, flight and dedicated performance remain Workbench acceptance.'}));
