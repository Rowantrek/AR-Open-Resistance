// Actual-source policy and widget lifecycle tests. Native visibility, flight and UI rendering are mocked.
// Run: node tools/Test-SpottingAndAirResponse.js
const fs=require('fs'),path=require('path');
const addon=path.resolve(__dirname,'../AROpenResistance/Open Resistance');
const read=p=>fs.readFileSync(path.join(addon,p),'utf8');
const spotting=read('Scripts/Game/Operations/AROR_QRFSpotting.c');
const intel=read('Scripts/Game/Locations/AROR_QRFMapIntel.c');
const symbols=read('Scripts/Game/Locations/AROR_MapFrameIcon.c');
const town=read('Scripts/Game/Towns/AROR_TownWavesComponent.c');
const air=read('Scripts/Game/Operations/AROR_ReaperQRFExecutor.c');
const executor=read('Scripts/Game/Operations/AROR_OperationExecutorComponent.c');
let assertions=0,server=true,dedicated=false,clock=0,occluded=false,traceCalls=0,lastTraceEnd=null,draws=[],packets=[],manager=null;
const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
const extension=(type,name,value)=>Object.defineProperty(type.prototype,name,{value,configurable:true});
for(const [name,fn]of Object.entries({Count(){return this.length;},IsEmpty(){return !this.length;},
 Insert(v){this.push(v);},Clear(){this.length=0;},Contains(v){return this.includes(v);},Remove(i){this.splice(i,1);},
 RemoveItem(v){const i=this.indexOf(v);if(i>=0)this.splice(i,1);},Copy(v){this.splice(0,this.length,...v);},
 Length(){return Math.hypot(...this);},Normalize(){const l=this.Length();if(l)this.forEach((v,i)=>this[i]=v/l);}}))extension(Array,name,fn);
extension(String,'IsEmpty',function(){return !this.length;});
extension(String,'Split',function(sep,out){out.push(...this.split(sep).filter(Boolean));});
extension(String,'ToFloat',function(){return Number(this);});
const vadd=(a,b)=>a.map((n,i)=>n+b[i]),vsub=(a,b)=>a.map((n,i)=>n-b[i]);
const factions=[{key:'R'},{key:'US'},{key:'USSR'}];factions.forEach((f,role)=>{f.role=role;f.IsFactionEnemy=other=>!!other&&other!==f;});
const component={};
const affiliation=entity=>({GetAffiliatedFaction:()=>factions[entity.role],GetAffiliatedFactionKey:()=>factions[entity.role]?.key});
function entity(role=1,position=[0,0,200],character=false){return{role,position,character,dead:false,claimed:false,
 GetOrigin(){return [...this.position];},EyePosition(){return [this.position[0],this.position[1]+(this.eyeHeight??1.7),this.position[2]];},
 GetCharacterController(){return {IsDead:()=>this.dead,GetHeadAimingComponent:()=>({GetAimingDirectionWorld:()=>[0,0,1]})};},
 FindComponent(type){if(type===component.faction)return affiliation(this);if(type===component.damage)return{IsDestroyed:()=>this.dead};
 if(type===component.access)return {IsInCompartment:()=>!!this.seated,GetCompartment:()=>({GetVehicle:()=>this.aircraft||null})};return null;}};}
const viewer=entity(0,[0,0,0],true),enemy=entity(1),gunship=entity(2,[0,100,300]);
let players=[1],assets=[enemy],kinds=[0],sides=[1],created=0,failLayout=false;
const world={GetWorldTime:()=>clock*1000,TraceMove:trace=>{traceCalls++;lastTraceEnd=[...trace.End];return occluded?0.2:1;}};
function widget(){const children={};return{visible:true,removed:false,
 FindAnyWidget(name){return children[name]||(children[name]=widget());},SetVisible(v){this.visible=v;},
 SetZOrder(v){this.z=v;},SetOpacity(v){this.opacity=v;},SetColor(v){this.color=v;},
 SetText(v){this.text=v;},SetExactFontSize(v){this.font=v;},RemoveFromHierarchy(){this.removed=true;}};}
const workspace={DPIScale:n=>n,DPIUnscale:n=>n,CreateWidgets:()=>{created++;if(failLayout){failLayout=false;return null;}return widget();}};
const game={GetWorld:()=>world,GetWorkspace:()=>workspace,GetPlayerController:()=>({GetControlledEntity:()=>viewer}),
 GetPlayerManager:()=>({GetPlayers:out=>out.push(...players),GetPlayerControlledEntity:id=>id===1?viewer:entity(2,[0,0,0],true),
 GetPlayerController:()=>({AROR_SendQRFIntel_S:(...args)=>packets.push(args.map(x=>Array.isArray(x)?[...x]:x))})})};
const env={GetGame:()=>game,Math:{Min:Math.min,Max:Math.max,Clamp:(v,a,b)=>Math.max(a,Math.min(b,v)),
 Floor:Math.floor,Mod:(a,b)=>a%b,Cos:Math.cos,Sin:Math.sin,DEG2RAD:Math.PI/180,PI2:Math.PI*2},
 vector:{Dot:(a,b)=>a.reduce((n,v,i)=>n+v*b[i],0),DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2]),Zero:[0,0,0]},
 Vector:(...v)=>v,vadd,vsub,Replication:{IsServer:()=>server},RplSession:{Mode:()=>dedicated?1:0},RplMode:{Dedicated:1},
 AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},
 AROR_ResistanceFleet:{IsClaimed:e=>!!e?.claimed},
 AROR_FactionRolesComponent:{GetInstance:()=>({GetFaction:r=>factions[r],GetFactionKey:r=>factions[r].key,GetRoleOfFaction:()=>true})},
 AROR_EOperationType:{ATTACK:0,QRF:1},AROR_EOperationState:{PLANNED:0,SPAWNING:1,ACTIVE:2,SUCCEEDED:3,FAILED:4,CANCELLED:5},
 HelicopterControllerComponent:{},
 ChimeraCharacter:{Cast:e=>e?.character?e:null},
 FactionAffiliationComponent:component.faction={Cast:x=>x},DamageManagerComponent:component.damage={Cast:x=>x},
 CompartmentAccessComponent:component.access={Cast:x=>x},
 SCR_PlayerController:{Cast:x=>x},TraceParam:class{},TraceFlags:{WORLD:1,ENTS:2,VISIBILITY:4},
 Print:()=>{},LogLevel:{NORMAL:0},string:{Empty:'',Format:(s,...xs)=>s.replace(/%(\d+)/g,(_,i)=>xs[i-1])},
 AROR_QRFSighting:class{constructor(){Object.assign(this,{m_iFirstLook:0,m_iLastLook:0,m_iLastSeen:0,m_bConfirmed:false});}},
 ImageWidget:{Cast:x=>x},CanvasWidget:{Cast:x=>x},RichTextWidget:{Cast:x=>x},
 AROR_UIColors:{FACTION_US:1,FACTION_USSR:2,Get:x=>x},
 FrameSlot:{SetSize:(w,x,y)=>w.size=[x,y],SetAlignment:()=>{},SetPos:(w,x,y)=>w.pos=[x,y]},
 AROR_MapFrameIcon:{DrawContact:(canvas,commands,size,pixel,kind,side)=>draws.push({kind,side})},
 AROR_OperationManagerComponent:{GetInstance:()=>manager},
 AROR_GroundOperationForce:{Cast:f=>f?.ground?f:null},AROR_ReaperAirForce:{Cast:f=>f?.air?f:null,AircraftAlive:e=>!!e&&!e.dead}
};
const methodNames=['Find','RecordLook','Clear','ClearWidgets','Label','Executor','PostPosition','TrackOperation','AddSpottingAsset','Points','Arc','SetQRFObserverMapOpen',
 'DispatchReaperPackage','Watch','GetOperations','IsHostileAttackTarget','ChangeState_S','HasLocation','ActiveTimeoutFor'];
function compile(source,name){
 const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
 if(!m)throw Error('Missing source method '+name);
 let body=m[2]
 .replace(/super\.(\w+)\(/g,'this.Base$1(')
 .replace(/foreach \([^:\n]+ (\w+) : ([^)]+)\)/g,'for (const $1 of $2)')
 .replace(/(?:ref )?array<ref array<float>> (\w+) = \{\};/g,'let $1 = [];')
 .replace(/(?:ref )?array<[^>]+> (\w+) = \{\};/g,'let $1 = [];')
 .replace(/array<ref CanvasWidgetCommand> (\w+);/g,'let $1;')
 .replace(/\b(?:AROR_\w+|FactionAffiliationComponent|DamageManagerComponent|CompartmentAccessComponent|BaseCompartmentSlot|CharacterHeadAimingComponent|ChimeraCharacter|SCR_PlayerController|SCR_MapEntity|RichTextWidget|CanvasWidget|ImageWidget|TraceParam|WorkspaceWidget|Widget|IEntity|Faction|Color|float|bool|int|vector|string) (\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1 = 0;').replace(/let x, y;/g,'let x=0,y=0;')
 .replace(/target = vehicle.GetOrigin\(\) \+ Vector\(0, 1.5, 0\);/g,'target = vadd(vehicle.GetOrigin(), Vector(0, 1.5, 0));')
 .replace(/delta = target - eye;/g,'delta = vsub(target,eye);')
 .replace(/vehicleIndex = m_iPairCursor \/ players.Count\(\);/g,'vehicleIndex = MathNative.floor(m_iPairCursor / players.Count());')
 .replace(/if \(!m_mContactCommands.Find\(root, commands\)\)/g,'commands = m_mContactCommands.Get(root); if (!commands)')
 .replace(/commands = \{\};/g,'commands = [];')
 .replace(/m_ScanRoles.GetRoleOfFaction\(affiliation.GetAffiliatedFaction\(\), role\)/g,'((role=affiliation.GetAffiliatedFaction().role), m_ScanRoles.GetRoleOfFaction(affiliation.GetAffiliatedFaction()))')
 .replace(/(?<![.\w])(IsResistance|InView|IsAircraftKind)\(/g,'AROR_QRFSpotting.$1(')
 .replace(/mapEntity.WorldToScreen\(position\[0\], position\[2\], x, y, true\);/g,'[x,y] = mapEntity.WorldToScreen(position[0], position[2]);')
 .replace(/(?<![.\w])(?:m_|s_)\w+/g,x=>'this.'+x)
 .replace(/(?<![.\w])(?:PHASE_ALERT|PRESSING_CHECK_SECONDS|AIR_ASSAULT_WAVE)\b/g,x=>'this.'+x)
 .replace(new RegExp('(?<![.\\w])('+methodNames.join('|')+')\\(', 'g'),'this.$1(');
 const args=m[1].replace(/(?:notnull )?(?:array<ref array<float>>|array<[^>]+>|[A-Z]\w*|int|float|bool|string|vector) /g,'');
 return new Function(...Object.keys(env),'MathNative','return function('+args+'){'+body+'}')(...Object.values(env),Math);
}
env.AROR_QRFSpotting={IsResistance:compile(spotting,'IsResistance'),InView:compile(spotting,'InView'),IsAircraftKind:compile(spotting,'IsAircraftKind')};
const spot={m_aSightings:[],m_aMapObserversBlocked:[],m_iNextId:0,m_iNextBroadcast:0,m_iPairCursor:0,m_bWasDebug:false};
for(const name of ['Find','RecordLook','SetMapOpen_S','Tick_S'])spot[name]=compile(spotting,name);
const assetSource={GetQRFSpottingAssets_S:(a,k,s)=>{a.push(...assets);k.push(...kinds);s.push(...sides);}};
function tick(time){clock=time;spot.Tick_S(assetSource,time,800,1500,0.9,8,90,false);}
tick(1);check(packets.length===1&&packets[0][0].length===0,'First glance is not a public sighting');
tick(2);check(spot.m_aSightings[0].m_bConfirmed,'Second unobstructed observation confirms');
tick(6);check(packets.at(-1)[0].length===1&&packets.at(-1)[2][0]===0,'Confirmed contact broadcasts correct class');
occluded=true;enemy.position=[0,0,600];tick(11);
check(packets.at(-1)[1][0][2]===200&&packets.at(-1)[3][0]===5,'Occluded target freezes at last-seen position');
occluded=false;spot.SetMapOpen_S(1,true);const tracesBefore=traceCalls;tick(16);
check(traceCalls===tracesBefore&&packets.at(-1)[1][0][2]===200,'Map readers cannot refresh sightings');
spot.SetMapOpen_S(1,false);enemy.role=0;tick(21);
check(spot.m_aSightings[0].m_iLastSeen===6,'Friendly affiliation cannot refresh a hostile contact');
enemy.role=1;enemy.claimed=true;tick(26);check(!spot.m_aSightings.length,'Claimed vehicle removes its contact');enemy.claimed=false;
enemy.position=[0,0,-200];tick(31);check(!spot.m_aSightings.length,'Behind-view target is not spotted');
enemy.position=[0,0,2000];tick(36);check(!spot.m_aSightings.length,'Ground target outside range is not spotted');
enemy.position=[0,0,200];enemy.dead=true;tick(41);check(!spot.m_aSightings.length,'Destroyed target cannot be newly spotted');enemy.dead=false;
assets=[gunship];kinds=[4];sides=[2];tick(46);tick(47);tick(51);
check(packets.at(-1)[2][0]===4&&packets.at(-1)[5][0]===2,'Gunship is classified independently from transports');
players=[1,2];tick(56);check(packets.at(-1)[0].length===1,'Resistance receives shared report');
const countBefore=packets.length;tick(61);check(packets.length===countBefore+1,'Non-Resistance player receives no snapshot');
spot.SetMapOpen_S(1,true);tick(151);check(packets.at(-1)[0].length===0,'Unobserved confirmed contact expires from published intelligence');
check(env.AROR_QRFSpotting.IsAircraftKind(3)&&env.AROR_QRFSpotting.IsAircraftKind(4)&&!env.AROR_QRFSpotting.IsAircraftKind(6),'Only helicopters receive air spotting range');
const prone=entity(1,[0,0,200],true);prone.eyeHeight=0.3;assets=[prone];kinds=[6];sides=[1];players=[1];spot.SetMapOpen_S(1,false);tick(160);
check(lastTraceEnd[1]===0.3,'Infantry visibility uses stance-aware eye height rather than air above low cover');
const traceStart=traceCalls;assets=Array.from({length:20},()=>entity(1));kinds=assets.map(()=>0);sides=assets.map(()=>1);tick(161);
check(traceCalls-traceStart===8,'Native visibility trace work stays within the global tick budget');
const mapState={s_aIds:[],s_aPositions:[],s_aKinds:[],s_aSides:[],s_aAges:[],s_iRevision:0,s_iLifetime:90,
 m_aWidgets:[],m_mContactCommands:{data:new Map(),Get(k){return this.data.get(k);},Set(k,v){this.data.set(k,v);},Remove(k){this.data.delete(k);},Clear(){this.data.clear();}},
 m_iDrawnRevision:-1,m_fNextLabelsAt:0,m_fDrawnPixelScale:0};
for(const name of ['Clear','Receive','ClearWidgets','Label','Update'])mapState[name]=compile(intel,name);
function receive(ids=['a','b'],classes=[3,4],roles=[1,2],ages=null,life=90){mapState.Receive(ids,ids.map((_,i)=>[i*100,0,300]),classes,ages||ids.map(()=>0),life,roles);}
const map={WorldToScreen:(x,z)=>[x+10,z+20]},parent={};clock=200;receive();mapState.Update(parent,'layout',map);
check(draws.length===2&&draws[0].kind===3&&draws[1].kind===4,'Transport and gunship draw different symbols');
check(mapState.m_aWidgets[0].FindAnyWidget('QRFLabel').text.includes('TRANSPORT HELICOPTER'),'Transport label accompanies symbol');
check(mapState.m_aWidgets[0].pos[0]===10&&mapState.m_aWidgets[1].pos[0]===110,'Each marker uses its own map position');
const reused=mapState.m_aWidgets[0];receive(['b','a'],[2,6],[2,1]);mapState.Update(parent,'layout',map);
check(mapState.m_aWidgets[0]===reused&&draws.at(-2).kind===2&&draws.at(-2).side===2,'Reused widgets refresh class and faction on reordered snapshot');
check(reused.FindAnyWidget('QRFLabel').text.includes('ARMOURED VEHICLE'),'Reordered label refreshes immediately');
receive(['dup','dup'],[3,4],[1,2]);check(mapState.s_aIds.length===1,'Duplicate IDs do not create duplicate markers');
receive(['bad'],[7],[1]);check(!mapState.s_aIds.length,'Unsupported class cannot enter client cache');
receive(['old'],[3],[1],[90]);check(!mapState.s_aIds.length,'Expired report cannot enter cache');
receive(['safe'],[3],[1]);const revision=mapState.s_iRevision;receive(['invalid'],[4],[1],[0],999);
check(mapState.s_iRevision===revision&&mapState.s_aIds[0]==='safe','Malformed lifetime cannot overwrite cache');
mapState.Update(parent,'layout',map);clock=290;mapState.Update(parent,'layout',map);
check(!mapState.m_aWidgets[0].visible,'Marker expires locally without waiting for another packet');
mapState.ClearWidgets();check(!mapState.m_aWidgets.length&&mapState.m_mContactCommands.data.size===0,'Map close releases widgets and canvas command owners');
clock=300;receive(['retry'],[3],[1]);failLayout=true;mapState.Update(parent,'layout',map);
check(!!mapState.m_aWidgets[0],'Transient layout failure retries instead of permanently losing marker');
viewer.dead=true;mapState.Update(parent,'layout',map);check(!mapState.s_aIds.length&&!mapState.m_aWidgets.length,'Dead viewer clears cached intelligence');viewer.dead=false;
dedicated=true;const createdBefore=created;mapState.Update(parent,'layout',map);check(created===createdBefore,'Dedicated server creates no map widgets');dedicated=false;
const mapLayer=read('Scripts/Game/Locations/AROR_LocationMapIconsComponent.c'),suppressed={s_bSuppressed:true,m_bHandlingMap:false,SetQRFObserverMapOpen(open){this.blocked=open;}};
compile(mapLayer,'OnMapOpen').call(suppressed,null);check(suppressed.blocked===true,'Suppressed HICOMM map still blocks world observation');
compile(mapLayer,'OnMapClose').call(suppressed,null);check(suppressed.blocked===false,'Closing suppressed map restores observation');
const glyph={Points:compile(symbols,'Points'),Arc:compile(symbols,'Arc')},contactGlyph=compile(symbols,'ContactGlyph');
const shapes=[];
for(let kind=0;kind<=6;kind++){
 const strokes=[];contactGlyph.call(glyph,kind,strokes);shapes.push(JSON.stringify(strokes));
 check(strokes.length>0&&strokes.every(points=>points.length>=4&&points.length%2===0&&points.every(Number.isFinite)),'Valid vector strokes for class '+kind);
}
check(new Set(shapes).size===7,'Every spotting class has distinct glyph geometry');
const addAsset=compile(executor,'AddSpottingAsset'),list=[],types=[],army=[];
addAsset.call({},enemy,0,1,list,types,army);addAsset.call({},enemy,0,1,list,types,army);
check(list.length===1&&types.length===1&&army.length===1,'Candidate arrays stay aligned without duplicates');
enemy.dead=true;addAsset.call({},enemy,0,1,[],[],[]);enemy.dead=false;
check(executor.includes('AddSpottingAsset(leader, 6'),'Dismounted squad leaders are included in visual candidates');
const transport=entity(2,[0,100,600]),foot=entity(1,[50,0,200],true),seated=entity(2,[0,100,600],true);seated.seated=true;
const nativeForces=[
 {ground:true,m_sOperationId:'ground',m_Vehicle:enemy,m_Profile:{GetSpottingKind:()=>0},m_bOrdersIssued:true,m_Group:{GetLeaderEntity:()=>foot}},
 {ground:true,air:true,m_sOperationId:'air',m_Vehicle:transport,m_Profile:{GetSpottingKind:()=>3},m_Escort:{m_Vehicle:gunship},m_bOrdersIssued:true,m_Group:{GetLeaderEntity:()=>seated}},
 {m_sOperationId:'foot',m_bOrdersIssued:true,m_Group:{GetLeaderEntity:()=>entity(1,[90,0,200],true)}},
 {m_sOperationId:'reservation',m_bReservation:true}
];
const adapter={m_aForces:nativeForces,AddSpottingAsset:addAsset,m_Operations:{GetOperation:id=>({UsesTacticalResponse:()=>true,GetRole:()=>id==='air'?2:1})}};
const candidates=[],candidateKinds=[],candidateSides=[];
compile(executor,'GetQRFSpottingAssets_S').call(adapter,candidates,candidateKinds,candidateSides);
check(candidateKinds.join(',')==='0,6,3,4,6','Actual adapter separates vehicle, squad, transport and gunship candidates');
check(candidateSides.join(',')==='1,1,2,2,1','Candidate markers preserve campaign role');
check(!candidates.includes(seated),'Seated passengers do not generate dismounted squad markers');
const tryAir=compile(town,'TryAirAssault');let airCalls=0,pressing=0,reject=false;
manager={GetReadinessLevel:()=>1,GetCrackdownLevel:()=>2,RequestTownWave_S:()=>{airCalls++;return reject?'':'air_operation';}};
const t={PHASE_ALERT:1,PRESSING_CHECK_SECONDS:5,AIR_ASSAULT_WAVE:9,m_iPhase:1,m_iClock:100,m_iNextAirAttempt:0,
 m_bAirRequested:false,m_iAirAssaultReadiness:1,m_bAirOnFirstWave:true,m_iWavesSent:1,m_eRole:1,m_aOperationIds:['ground'],
 m_Location:{GetId:()=> 'town'},PostPosition:()=>[0,0,0],Executor:()=>({FindTownWaveRecipeId_S:()=> 'townair',CountPressingPackages_S:()=>pressing}),
 TrackOperation(id,wave){this.tracked=[id,wave];}};
tryAir.call(t);check(airCalls===0,'Alert does not dispatch insertion prematurely');t.m_iPhase=2;
tryAir.call(t);check(airCalls===1&&t.tracked[1]===9,'First ground wave requests tracked helicopter set at Readiness one');
t.m_iClock=200;tryAir.call(t);check(airCalls===1,'Accepted insertion requested once per response');
t.m_bAirRequested=false;t.m_bAirOnFirstWave=false;t.m_iNextAirAttempt=0;tryAir.call(t);
check(airCalls===1,'Legacy ground-pressure option waits for actual pressure');pressing=1;t.m_iClock=205;tryAir.call(t);check(airCalls===2,'Legacy option launches after ground pressure');
t.m_bAirRequested=false;t.m_iNextAirAttempt=0;reject=true;t.m_iClock=210;tryAir.call(t);tryAir.call(t);
check(airCalls===3&&!t.m_bAirRequested,'Rejected request remains retryable without per-frame spam');reject=false;t.m_iClock=215;tryAir.call(t);
check(airCalls===4&&t.m_bAirRequested,'Rejected request can succeed on next scheduled attempt');
const dispatchAir=compile(air,'DispatchReadyPackage');
const distant={air:true,m_iFlightDeadline:1800,m_vTown:[0,0,0]};
check(dispatchAir.call({DispatchReaperPackage:()=>true,m_iReaperSupportSeconds:300},distant,{})&&distant.m_iSupportEndTime===2100,'Gunship ingress allowance includes calculated distant-origin travel time');
const begin=compile(air,'BeginGunshipSupport'),airCtx={m_iExecutorClock:100,m_iReaperSupportSeconds:300,m_fReaperGunshipRunRadius:450};
const f={m_bSupportStarted:false,m_iSupportEndTime:480,m_Escort:{m_Vehicle:gunship},m_iEscortStage:0,m_fTownRadius:250,m_vTown:[0,0,0],m_sOperationId:'air'};
gunship.position=[0,100,2000];begin.call(airCtx,f);check(!f.m_bSupportStarted,'Ingress does not spend gunship station timer');
gunship.position=[0,100,500];begin.call(airCtx,f);check(f.m_bSupportStarted&&f.m_iSupportEndTime===400,'Gunship gets full support time near town');
airCtx.m_iExecutorClock=200;begin.call(airCtx,f);check(f.m_iSupportEndTime===400,'Repeated near-town checks cannot extend support forever');
f.m_bSupportStarted=false;f.m_iEscortStage=2;begin.call(airCtx,f);check(!f.m_bSupportStarted,'Departing gunship cannot restart support');
f.m_iEscortStage=0;f.m_iSupportEndTime=200;begin.call(airCtx,f);check(!f.m_bSupportStarted,'Withdrawal or expired support cannot restart a station timer');
const traffic=compile(air,'HasReaperAirTraffic');
for(const [force,blocked,label]of [
 [{air:true,m_bConstructionPending:true},true,'pending aircraft'],
 [{air:true,m_Vehicle:transport},true,'live transport'],
 [{air:true,m_bTransportRetired:true,m_Escort:{m_Vehicle:gunship}},true,'live escort after insertion'],
 [{air:true,m_bTransportRetired:true,m_bEscortLost:true},false,'delivered infantry only'],
 [{air:true,m_bTacticalRetained:true,m_bConstructionPending:true},false,'empty terminal reservation'],
 [{air:true,m_Vehicle:{dead:true},m_Escort:{m_Vehicle:{dead:true}}},false,'retained wrecks']
])check(traffic.call({m_aForces:[force]})===blocked,'Airspace admission: '+label);
const releasePlan=compile(air,'ReleaseReaperLandingPlan'),planner={m_sReaperPlanningOperation:'new',m_ReaperLandingSearch:{}};
releasePlan.call(planner,'old');check(planner.m_ReaperLandingSearch!==null,'Another operation cannot release an active landing plan');
releasePlan.call(planner,'new');check(planner.m_ReaperLandingSearch===null&&planner.m_sReaperPlanningOperation==='','Committed package releases planning workspace for later insertions');
for(const scenario of ['AROR_Everon_Chicken_Layers','AROR_Everon_Layers']){
 const layer=read('Worlds/AROR_Everon/'+scenario+'/Managers.layer');
 check(layer.includes('m_bQRFDebugMarkers 0'),'Real spotting enabled instead of debug reveal: '+scenario);
 check(layer.includes('m_bAllowAirborneResponses 1')&&layer.includes('m_bAllowJointResponses 1'),'Both airborne backends enabled: '+scenario);
 check(layer.includes('m_iJointTransportPercent 100'),'Transport-heavy joint responses: '+scenario);
}
const chicken=read('Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer');
for(const role of ['occupier','invader'])check(new RegExp('m_sId "'+role+'_townair_r4"[\\s\\S]*?m_iMinimumReadinessLevel 1').test(chicken),'Town air recipe covers early readiness: '+role);
for(const name of ['Frame','Icon','QRFLabel'])check(read('UI/Layouts/Map/AROR_MapIcon.layout').includes('Name "'+name+'"'),'Shared layout contains '+name);
env.AROR_LocationForces={IsInAircraft:compile(read('Scripts/Game/Capture/AROR_LocationForces.c'),'IsInAircraft')};
const contest=compile(town,'OnContestEntity');let watched=0;
const scan={m_aScanReinforcements:[],m_ScanRoles:env.AROR_FactionRolesComponent.GetInstance(),m_eRole:1,m_iScanEnemies:0,m_iScanFriendlies:0,Watch:()=>watched++};
const seatedEnemy=entity(1,[0,0,0],true);seatedEnemy.seated=true;
seatedEnemy.aircraft={GetRootParent:()=>null,FindComponent:t=>t===env.HelicopterControllerComponent?{}:null};
contest.call(scan,seatedEnemy);check(scan.m_iScanEnemies===0&&watched===0,'Seated helicopter squad cannot progress town lockdown');
seatedEnemy.seated=false;scan.m_aScanReinforcements.push(seatedEnemy);contest.call(scan,seatedEnemy);check(scan.m_iScanEnemies===1&&watched===1,'The landed reinforcement counts only after disembarkation');
const hostile=compile(read('Scripts/Game/Operations/AROR_OperationManagerComponent.c'),'IsHostileAttackTarget');
for(const [attacker,defender,debug,expected]of [[2,1,false,true],[1,2,false,true],[2,0,false,true],[1,1,false,false],[2,1,true,false],[2,0,true,true],[2,3,false,false]]){
 check(!!hostile({GetRole:()=>attacker,m_bDebugRequest:debug},{GetControllingRole:()=>defender})===expected,'Attack admission respects diplomacy and debug target policy '+[attacker,defender,debug]);
}
let targetRole=1,transitions=[];
env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocationById:()=>({GetControllingRole:()=>targetRole})})};
const activeAttack={GetRole:()=>2,GetType:()=>0,m_bDebugRequest:false,IsTerminal:()=>false,GetState:()=>2,GetTargetLocationId:()=> 'town',GetStateSince:()=>0};
const scheduler={GetOperations:out=>out.push(activeAttack),IsHostileAttackTarget:hostile,HasLocation:()=>true,
 m_iClock:100,ActiveTimeoutFor:()=>3600,ChangeState_S:(op,state)=>transitions.push(state)};
compile(read('Scripts/Game/Operations/AROR_OperationManagerComponent.c'),'ProcessOperations').call(scheduler,0);
check(!transitions.length,'Physical Invader attack on Occupiers is not cancelled by the scheduler');
targetRole=2;compile(read('Scripts/Game/Operations/AROR_OperationManagerComponent.c'),'ProcessOperations').call(scheduler,0);
check(transitions.join(',')==='3','Attack succeeds after canonical ownership changes to the attacker');
console.log(JSON.stringify({assertions,nativeVerified:false,note:'Actual-source spotting, map widget, insertion and gunship policy checks. Native traces, renderer, REAPER flight and dedicated replication remain unverified.'}));
