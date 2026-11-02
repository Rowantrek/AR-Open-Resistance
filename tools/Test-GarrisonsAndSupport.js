/* Actual Enforce method bodies with mocked native services. No engine compile/gameplay claim.
 * Run: node tools/Test-GarrisonsAndSupport.js
 */
function runGarrisonsAndSupportTests(sources) {
  Object.defineProperty(String.prototype,'IsEmpty',{value:function(){return this.length===0;},configurable:true});
  Object.defineProperty(Array.prototype,'IsEmpty',{value:function(){return this.length===0;},configurable:true});
  let assertions=0;
  const check=(value,message)=>{assertions++;if(!value)throw Error(message);};
  let server=true;
  const types={TOWN:0,HQ:1,AIRPORT:2,CITY:3,FACTORY:4,MILITARY_BASE:5,PORT:6,RESOURCES:7,OUTPOST:8};
  const roles={RESISTANCE:0,OCCUPIERS:1,INVADERS:2};
  const env={Replication:{IsServer:()=>server},AROR_ELocationType:types,AROR_EFactionRole:roles,
    Math:{Clamp:(n,a,b)=>Math.max(a,Math.min(b,n)),Max:Math.max,Min:Math.min,AbsFloat:Math.abs,RandomInt:(a,b)=>a,Mod:(a,b)=>a%b},
    vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
    Print:()=>{},LogLevel:{NORMAL:0,WARNING:1,ERROR:2},string:{Empty:'',Format:(text,...args)=>text.replace(/%(\d+)/g,(_,i)=>String(args[i-1]))},
    AROR_GarrisonSlot:class{constructor(){Object.assign(this,{m_bLost:false,m_fHealth:1,m_bSpawned:false,m_Entity:null,m_iGroup:-1,m_iVehicle:-1,m_iSeat:-1,m_bPlacementUnavailable:false,m_iPlacementFailureSince:-1,m_iVariant:0,m_bCrewRecorded:false});}},
    AROR_GarrisonPolicy:{},SENTRY_VARIANT:1000,AROR_DefencePositionComponent:{Cast:x=>x},AROR_SentryPostComponent:{Cast:x=>x},AROR_GarrisonManagerComponent:{GetInstance:()=>({CanDefendRole:()=>true})}};
  const methods=['SentryPosts','SetValues','UpdateControl_S','IsEligible','AddSlot','ReservedPersonnel','ReportFailure','AssignMembers','CrewAndOrder','IsPlayerOrForeignAsset','Observe_S','ClearAssignments_S','HasDormantReserve','IsDefencePending','GetDefenceCaptureBlockReason','GetLocation','GetOwner','PlayerNear','FactionFor'];
  for(const source of Object.values(sources))for(const m of source.matchAll(/^\t(?:override )?(?:protected )?(?:static )?\w+ (\w+)\(/gm))if(!methods.includes(m[1]))methods.push(m[1]);
  for(const [name,fn]of Object.entries({Clear(){this.length=0;},Copy(a){this.length=0;this.push(...a);},Contains(x){return this.includes(x);},Find(x){return this.indexOf(x);},RemoveItem(x){const i=this.indexOf(x);if(i<0)return false;this.splice(i,1);return true;},Remove(i){this.splice(i,1);}}))Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
  function compile(source,name){
    const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!m)throw Error('Missing '+name);
    let args=m[1].replace(/\bnotnull /g,'').replace(/array<[^>]+>\s+/g,'').replace(/\b(?:vector|bool|float|int|string|Faction|ResourceName|IEntity|GenericComponent|SaveContext|LoadContext|AROR_\w+)\s+/g,'');
    let body=m[2].replace(/\bfloat\.MAX/g,'Number.MAX_VALUE')
	  .replace(/const AROR_\w+ /g,'let ')
	  .replace(/array<ref AROR_GarrisonSlot> (\w+) = \{\};/g,'let $1=[];')
	  .replace(/array<(?:ref )?\w+> (\w+) = \{\};/g,'let $1=[];')
	  .replace(/\b(?:int|bool) (\w+(?:, \w+)+);/g,(_,names)=>'let '+names.split(', ').map(n=>n+'=0').join(',')+';')
	  .replace(/context.ReadValue\(([^,]+), ([^)]+)\)/g,'context.ReadValue($1,v=>$2=v)')
	  .replace(/\bstring (\w+);/g,"let $1='';")
      .replace(/foreach \((?:int (\w+), )?\w+ (\w+) : ([^)]+)\)/g,(_,index,value,list)=>index?`for(const [${index},${value}] of ${list}.entries())`:`for(const ${value} of ${list})`)
      .replace(/\b(?:bool|float|int|string|vector|IEntity|Faction|Base\w+|CompartmentAccessComponent|ResourceName|AIWaypoint|AIAgent|DamageManagerComponent|ChimeraCharacter|SCR_\w+|AROR_\w+) (\w+);/g,'let $1=0;')
      .replace(/\b(?:bool|float|int|string|vector|IEntity|Faction|Base\w+|CompartmentAccessComponent|ResourceName|AIWaypoint|AIAgent|DamageManagerComponent|ChimeraCharacter|SCR_\w+|AROR_\w+) (\w+) =/g,'let $1 =')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/\.Count\(\)/g,'.length')
      .replace(/\.Insert\(/g,'.push(').replace(/\.Compare\(/g,'.localeCompare(');
    for(const method of methods)body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(', 'g'),'this.'+method+'(');
    body=body.replace(/(?<![.\w])(OnRegistered|OnUnregistered|OnControlChanged|OnRegistryChanged)(?=\s*\))/g,'this.$1');
    body=body.replace('seed + AROR_DefenceLayout.RepairOffset(attempt)','seed.map((v,i)=>v+AROR_DefenceLayout.RepairOffset(attempt)[i])');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  // Use actual manager methods to exercise arrival inside the site, rather than
  // only an ordinary approach through the activation boundary.
  let playerPosition=[1001,0,0],playerDead=false,ready=true;
  const player={GetOrigin:()=>playerPosition,GetCharacterController:()=>({IsDead:()=>playerDead})};
  const centre={GetCenter:()=>[0,0,0]};
  const playerManager={GetPlayers:out=>out.push(1),GetPlayerControlledEntity:()=>player};
  let clock=10000;
  const game={GetPlayerManager:()=>playerManager,GetWorld:()=>({GetWorldTime:()=>clock})};
  Object.assign(env,{GetGame:()=>game,ChimeraCharacter:{Cast:x=>x},
    AROR_GameFlowComponent:{Cast:x=>x},AROR_GarrisonComponent:{Cast:x=>x},
    Math:{...env.Math,floor:Math.floor}});
  const playerNear=compile(sources.manager,'PlayerNear');
  const playerClearance=compile(sources.garrison,'HasPlayerClearance');
  playerPosition=[0,0,0];
  check(playerClearance.call({m_fCrewPlayerClearance:3},[4,0,0],4,6),'standing on the flag does not suppress its four-metre crew staging ring');
  check(!playerClearance.call({m_fCrewPlayerClearance:3},[2,0,0],4,6),'crew still cannot spawn directly against a player');
  check(!playerClearance.call({m_fCrewPlayerClearance:3},[4,0,0],0,6),'infantry pads retain the normal player exclusion');
  playerPosition=[1001,0,0];
  const allowedCentre=compile(sources.garrison,'IsDefenceCenterAllowed');
  const anchored={m_bFixedDefenceAnchor:true,m_Location:{GetCenter:()=>[0,0,0],GetRadius:()=>250}};
  check(allowedCentre.call(anchored,[100,0,0],24),'anchored compact defence fits inside canonical capture circle');
  check(!allowedCentre.call(anchored,[230,0,0],24),'anchored perimeter cannot leave the capture circle');
  check(!allowedCentre.call(anchored,[700,0,0],26),'distant resource clearing cannot replace a fixed strategic anchor');
  check(allowedCentre.call({m_bFixedDefenceAnchor:false},[700,0,0],26),'ordinary outpost relocation is not constrained by its old capture circle');
  check(!playerNear.call({m_fActivationRadius:1000},centre,[playerPosition]),'outside 1km does not activate');
  playerPosition=[1000,0,0];check(playerNear.call({m_fActivationRadius:1000},centre,[playerPosition]),'1km boundary activates');
  playerPosition=[0,0,0];check(playerNear.call({m_fActivationRadius:1000},centre,[playerPosition]),'teleport directly onto flag activates without a boundary event');
  check(!playerNear.call({m_fActivationRadius:1000},centre,[]),'empty living-player snapshot does not activate');
  let assets=0,groups=0,lastClearance=0,lastGroupTime=-1,terrainFits=true,active=false,reserved=9,retirementTime=-1;
  const teleportSite={...centre,GetControllingRole:()=>1,GetOwner:()=>({FindComponent:()=>defence}),GetId:()=>'test'};
  let starts=0,scans=0,retired=0;
  const defence={IsEligible:()=>true,IsGarrisonActive:()=>active,Observe_S:()=>{},CanRetire:()=>false,
    HasDeploymentStarted:()=>starts>0,BeginDeployment_S:()=>starts++,IsDeploymentClosed:()=>false,UpdateDeployment_S:()=>{},BindAssets_S:()=>true,
    IsCapacityWaiting:()=>false,CommittedPersonnel:()=>reserved,NextPersonnelCost:()=>4,SetCapacityWait_S:()=>{},
    Retire_S:()=>retired++,OutsideLongEnough:(outside,now)=>{retirementTime=now;return false;},RefreshPending_S:()=>{},ClearAssignments_S:()=>{},
    HasRoster:()=>true,GetRosterRole:()=>1,GetLocation:()=>teleportSite,ReservedPersonnel:()=>reserved,
    HasSuitableDefenceTerrain_S:()=>terrainFits,
    SpawnNextAsset_S:(binding,clearance)=>{assets++;lastClearance=clearance;return false;},
    SpawnNextGroup_S:(now,clearance)=>{groups++;lastClearance=clearance;lastGroupTime=now;return true;}};
  const callbacks={Insert:()=>{},Remove:()=>{}};
  env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocations:out=>out.push(teleportSite),GetOnLocationRegistered:()=>callbacks,GetOnLocationUnregistered:()=>callbacks,GetOnLocationControlChanged:()=>callbacks})};
  const managerTick=compile(sources.manager,'Tick'),scan=compile(sources.manager,'ScanProximity_S');
  const managerState={m_bGarrisonsActive:true,m_iNextProximityAt:0,m_iCursor:0,m_fActivationRadius:1000,m_fProximityInterval:1,m_aGarrisons:[defence],m_aNearby:[],
    m_Flow:{IsPlaythroughReady:()=>ready},EnsureRegistry_S:()=>true,CanDefendRole:()=>true,
    m_fPlayerSpawnClearance:6,m_fRetirementRadius:2500,m_iRetirementDelay:120,m_iMaxPersonnel:96,
    PlayerNear:playerNear,FactionFor:()=>({}),HasRecentCombat:()=>false,NearestPlayerDistance:compile(sources.manager,'NearestPlayerDistance'),ScanProximity_S(...args){scans++;return scan.call(this,...args);}};
  for(const name of ['DefenceLevel','InfantryTeams','StaticCount','MannedCount','IsEnemyRole'])env.AROR_GarrisonPolicy[name]=compile(sources.policy,name);
  playerPosition=[0,0,0];managerTick.call(managerState);
  check(assets===0&&groups===1&&lastClearance===6,'teleport onto flag admits infantry before optional hulls within the proximity scan interval');
  for(let i=0;i<3;i++){clock+=250;managerTick.call(managerState);}
  check(scans===1&&groups===4,'four spawn ticks reuse one proximity scan');
  clock=17500;active=true;managerTick.call(managerState);
  check(retirementTime===17500&&lastGroupTime===17500,'late callbacks use the actual game clock for retirement and queue timestamps');active=false;
  terrainFits=false;const started=assets;managerTick.call(managerState);check(assets===started,'terrain rejection still prevents physical construction');terrainFits=true;
  reserved=97;managerTick.call(managerState);check(assets===started,'personnel cap is enforced');reserved=9;
  playerPosition=[1001,0,0];clock+=1000;managerTick.call(managerState);check(assets===started,'proximity refresh removes distant candidates');playerPosition=[0,0,0];
  ready=false;managerTick.call(managerState);check(assets===started,'campaign readiness gates admission');ready=true;
  server=false;managerTick.call(managerState);check(assets===started,'clients cannot construct garrisons');server=true;
  playerDead=true;clock+=1000;managerTick.call(managerState);check(assets===started&&managerState.m_aNearby.length===0,'dead controlled characters are excluded from the living proximity snapshot');playerDead=false;
  for(const roll of [0,1,2,99]){
    check(env.AROR_GarrisonPolicy.DefenceLevel(types.OUTPOST,roll)===1,'outposts cannot roll vehicle recipes');
    check(env.AROR_GarrisonPolicy.DefenceLevel(types.AIRPORT,roll)===4,'airbases always heavy');
    check(env.AROR_GarrisonPolicy.DefenceLevel(types.MILITARY_BASE,roll)===3,'military outpost tier');
    check(env.AROR_GarrisonPolicy.DefenceLevel(types.TOWN,roll)===0,'no town garrison recipe');
  }
  check([0,1,2].map(r=>env.AROR_GarrisonPolicy.DefenceLevel(types.RESOURCES,r)).join(',')==='1,2,3','resources roll three stored defence levels');
  const faction={m_eRole:1,m_sFactionKey:'AROR_OCCUPIERS',m_sAssetFactionKey:'US',m_sInfantry:'rifle',m_sMarksman:'',m_sStaticMG:'mg',m_sCrew:'crew',m_sParkedArmour:'lav',m_aMannedVehicles:['lav','humvee']};
  const add=compile(sources.garrison,'AddSlot'),reserve=compile(sources.garrison,'ReservedPersonnel'),generate=compile(sources.garrison,'Generate_S');
  function roster(type,override=0,staticOverride=0,omitParked=false,extraTeams=0,parked=[],posts=0){
    const state={m_aSlots:[],m_bGenerated:false,m_iDefenceLevel:override,m_iStaticWeaponCountOverride:staticOverride,m_bOmitParkedArmour:omitParked,IsEligible:()=>true,
      m_aExtraParkedVehicles:parked,GetExtraInfantryTeams:()=>extraTeams,SentryPosts:()=>Array.from({length:posts},()=>({})),GetOwner:()=>({}),stocked:0,StockStores_S(entity,depth){this.stocked++;},GetExtraParkedCount:()=>parked.length,
      m_Location:{GetControllingRole:()=>1,GetLocationType:()=>type,GetId:()=>'site'},AddSlot:add,ReservedPersonnel:reserve,InfantryPrefab:compile(sources.garrison,'InfantryPrefab'),StaticPrefab:compile(sources.garrison,'StaticPrefab'),GetStaticWeaponCount:compile(sources.garrison,'GetStaticWeaponCount')};
    generate.call(state,faction);return state;
  }
  const towered=roster(types.OUTPOST,1,0,false,0,[],2),plain=roster(types.OUTPOST,1);
  check(towered.stocked===1&&plain.stocked===1,'stores are stocked once, when the roster is generated');
  generate.call(plain,faction);check(plain.stocked===1,'an existing roster never restocks its stores');
  const sentries=towered.m_aSlots.filter(s=>s.m_iKind===0&&s.m_iVariant>=1000);
  check(sentries.length===2&&sentries.map(s=>s.m_iVariant).join(',')==='1000,1001','one marksman per guard tower, recording its post');
  check(new Set(sentries.map(s=>s.m_iGroup)).size===2&&sentries.every(s=>s.m_iGroup>Math.max(...plain.m_aSlots.map(p=>p.m_iGroup))),'each sentry is its own group after every authored group');
  check(sentries.every(s=>s.m_sPrefab==='rifle'),'sentry falls back to the infantry character without a marksman');
  check(JSON.stringify(towered.m_aSlots.slice(0,plain.m_aSlots.length).map(s=>[s.m_iKind,s.m_iGroup]))===JSON.stringify(plain.m_aSlots.map(s=>[s.m_iKind,s.m_iGroup])),'towers leave the team, gun and crew ordinals unchanged');
  const outpost=roster(types.OUTPOST,4);
  check(outpost.m_aSlots.filter(s=>s.m_iKind===2||s.m_iKind===3).length===0,'even strong outpost overrides never add vehicles');
  const military=roster(types.MILITARY_BASE);
  check(military.m_aSlots.filter(s=>s.m_iKind===2).length===1,'military outpost includes parked unmanned armour');
  check(military.m_aSlots.filter(s=>s.m_iKind===3).length===1,'military outpost includes one random manned hull');
    const compact=roster(types.MILITARY_BASE,3,4,true);
    check(compact.m_aSlots.filter(s=>s.m_iKind===2).length===0&&compact.m_aSlots.filter(s=>s.m_iKind===3).length===1,'compact military compound omits only the spare uncrewed hull');
    check(compact.m_aSlots.filter(s=>s.m_iKind===0).length===24&&compact.m_aSlots.filter(s=>s.m_iKind===1).length===4,'compact layout retains its infantry and four statics');
    check(reserve.call(compact)===32,'compact military reserves infantry and all static and vehicle crews');
  const airport=roster(types.AIRPORT,1);
  check(airport.m_iLevel===4&&airport.m_aSlots.filter(s=>s.m_iKind===3).length===3,'airbase stays large under low override');
  check(reserve.call(airport)===48,'airbase reserves all infantry, static and possible hull crew before spawning');
  const stronghold=roster(types.AIRPORT,4,6,false,4,['heli','heli','heli']);
  check(stronghold.m_aSlots.filter(s=>s.m_iKind===0).length===48,'stronghold adds four infantry teams to the airport roster');
  const extraHulls=stronghold.m_aSlots.filter(s=>s.m_iKind===2&&s.m_iVariant>0);
  check(extraHulls.length===3&&extraHulls.map(s=>s.m_iVariant).join(',')==='1,2,3','extra parked hulls retain distinct saved variants');
  const parkedPrefab=compile(sources.garrison,'ParkedPrefab');
  for(const hull of extraHulls)check(parkedPrefab.call(stronghold,faction,hull.m_iVariant)==='heli','saved helicopter variant restores as helicopter');
  const before=airport.m_aSlots.length;check(!generate.call(airport,faction)&&airport.m_aSlots.length===before,'active recipe cannot reroll');
  server=false;check(!generate.call({...military,m_bGenerated:false},faction),'client cannot generate roster');server=true;

  faction.m_aStaticVariants=['medium-mg','heavy-specialist'];
  const equippedOutpost=roster(types.OUTPOST,2);
  check(equippedOutpost.m_aSlots.filter(s=>s.m_iKind===1).map(s=>s.m_sPrefab).join(',')==='mg,medium-mg','ordinary equipped outpost uses heavy and medium statics');
  const equippedMilitary=roster(types.MILITARY_BASE,3,4),guns=equippedMilitary.m_aSlots.filter(s=>s.m_iKind===1);
  check(guns.map(s=>s.m_sPrefab).join(',')==='mg,medium-mg,heavy-specialist,mg','military uses four crewed positions with bounded variant rotation');
  check(guns.map(s=>s.m_iVariant).join(',')==='0,1,2,0','logical static choices are recorded for serialization');
  check(equippedMilitary.m_aSlots.filter(s=>s.m_iKind===0).length===24,'extra static does not increase the infantry recipe');
  check(equippedMilitary.ReservedPersonnel()===32,'extra gun reserves its crew before physical spawning');
  const staticPrefab=compile(sources.garrison,'StaticPrefab');
  check(staticPrefab(faction,99)==='mg','removed static variants safely use the current default gun');
  check(staticPrefab({...faction,m_aStaticVariants:['']},1)==='mg','empty static variant retains a usable default');
  const bindStatic=compile(sources.garrison,'BindAssets_S');
  const staticRestore={m_aSlots:guns.map(s=>({...s,m_bLost:s.m_iVariant===1})),m_eRosterRole:1,m_bAssetsBound:false,StaticPrefab:staticPrefab};
  check(bindStatic.call(staticRestore,{...faction,m_aStaticVariants:['new-medium','new-heavy']})&&staticRestore.m_aSlots[2].m_sPrefab==='new-heavy','restored static slots bind their recorded variant to current role assets');
  check(staticRestore.m_aSlots[1].m_bLost,'static asset rebinding never replenishes a destroyed gun');
  delete faction.m_aStaticVariants;

  const advance=compile(sources.support,'ApplyRegionalSupport_S');
  function support(r,o,i){return {m_fResistance:r,m_fOccupiers:o,m_fInvaders:i,m_Location:{IsControlFixed:()=>false},
    SetValues(a,r,b,o,c,i){this.m_fResistance=r;this.m_fOccupiers=o;this.m_fInvaders=i;this.UpdateControl_S();},UpdateControl_S(){this.checked=(this.checked||0)+1;}};}
  const town=support(0,100,0);
  for(let n=0;n<26;n++)advance.call(town,1,0,0,2);
  check(town.m_fResistance===52&&town.m_fOccupiers===48&&town.m_fInvaders===0,'regional majority emerges gradually without donating support to absent third role');
  const mixed=support(10,60,30);advance.call(mixed,1,1,1,2);
  check(Math.abs(mixed.m_fResistance+mixed.m_fOccupiers+mixed.m_fInvaders-100)<1e-9,'three-role distribution stays normalized');
  check(Math.max(Math.abs(mixed.m_fResistance-10),Math.abs(mixed.m_fOccupiers-60),Math.abs(mixed.m_fInvaders-30))<=2.000001,'step caps every role');
  const untouched=support(0,100,0);advance.call(untouched,0,0,0,2);check(untouched.m_fOccupiers===100&&!untouched.checked,'no installations preserves support');
  untouched.m_Location.IsControlFixed=()=>true;advance.call(untouched,1,0,0,2);check(untouched.m_fResistance===0,'fixed control preserves support');
  const majority=support(100,0,0);advance.call(majority,1,0,0,2);check(majority.checked===1,'existing majority rechecks expired flip cooldown');
  server=false;advance.call(town,0,1,0,2);check(town.m_fResistance===52,'clients cannot influence support');server=true;

  const region=compile(sources.region,'RegionFor');
  const loc=(id,type,pos,override='')=>({GetId:()=>id,GetLocationType:()=>type,GetCenter:()=>pos,GetSupportRegionId:()=>override});
  const a=loc('a',types.TOWN,[-10,0,0]),b=loc('b',types.CITY,[10,0,0]),site=loc('site',types.OUTPOST,[0,0,0]);
  check(region(site,[b,a])==='a'&&region(site,[a,b])==='a','nearest-region ties use stable IDs, independent of registration');
  check(region(loc('site',types.OUTPOST,[0,0,0],'manual'),[a,b])==='manual','military authored region overrides distance');
  check(region(a,[a,b])==='a','settlement defaults to own stable region ID');
  const shared=loc('a',types.TOWN,[-10,0,0],'larger');check(region(site,[b,shared])==='larger','nearest settlement propagates larger shared region');
  check(region(site,[])==='','missing settlements invent no region');

  const outside=compile(sources.garrison,'OutsideLongEnough'),state={m_iOutsideSince:-1};
  check(!outside.call(state,true,100,120),'leaving radius starts grace');
  check(!outside.call(state,true,219,120)&&outside.call(state,true,220,120),'retirement requires entire grace interval');
  check(!outside.call(state,false,221,120)&&state.m_iOutsideSince===-1,'returning player resets grace');
  check(!outside.call(state,true,230,120),'new exit cannot inherit old grace');

  // Observe actual recorded assets: a retired living entity is not a death; missing physical assets are losses.
  Object.assign(env,{DamageManagerComponent:{Cast:x=>x},ChimeraCharacter:{Cast:e=>e&&e.character?e:null}});
  const observe=compile(sources.garrison,'Observe_S');
  let destroyed=false,health=0.6,foreign=false;
  const entity={FindComponent:()=>({IsDestroyed:()=>destroyed,GetHealthScaled:()=>health})};
  const survivor={m_bLost:false,m_bSpawned:true,m_Entity:entity,m_fHealth:1};
  const missing={m_bLost:false,m_bSpawned:true,m_Entity:null,m_fHealth:1};
  const virtual={m_bLost:false,m_bSpawned:false,m_Entity:null,m_fHealth:1};
  const garrison={m_aGroups:[],m_aSlots:[survivor,missing,virtual],IsPlayerOrForeignAsset:()=>foreign,HasDormantReserve:()=>false};
  observe.call(garrison,false);check(survivor.m_fHealth===0.6&&!survivor.m_bLost,'damage sampled without declaring survivor dead');
  check(missing.m_bLost&&!virtual.m_bLost,'only formerly physical missing assets become losses');
  health=1;observe.call(garrison,false);check(survivor.m_fHealth===0.6,'virtualisation cannot manufacture healing');
  destroyed=true;observe.call(garrison,false);check(survivor.m_bLost,'confirmed destruction retained');
  destroyed=false;survivor.m_bLost=false;foreign=true;observe.call(garrison,false);
  check(survivor.m_bLost&&survivor.m_Entity===null,'captured/player assets detach from cleanup and cannot respawn');
	missing.m_bLost=false;garrison.HasDormantReserve=()=>true;observe.call(garrison,false);
	check(!missing.m_bLost,'native dormant reserve is never treated as a missing combat casualty');
	const pending=compile(sources.garrison,'IsDefencePending');
	server=false;check(pending.call({m_bPendingDefense:false})===false,'client cleared state comes from replication rather than ungenerated local roster');
	check(pending.call({m_bPendingDefense:true})===true,'client honours replicated deployment lock');server=true;
  const unresolved=compile(sources.garrison,'HasUnresolvedQueue');
  const fallen={m_iKind:0,m_iGroup:0,m_bLost:true,m_bSpawned:true,m_Entity:null};
  const blocked={m_iKind:1,m_iGroup:1,m_bLost:false,m_bSpawned:false,m_Entity:null,m_iVehicle:-1,m_iPlacementFailureSince:-1};
  const guard={IsEligible:()=>true,m_bGenerated:true,m_eRosterRole:1,m_Location:{GetControllingRole:()=>1,GetId:()=>'site'},
    m_aSlots:[fallen,blocked],m_aGroups:[]};
  check(pending.call(guard),'unresolved initial MG retains only the bounded deployment gate');blocked.m_bPlacementUnavailable=true;
  check(!pending.call(guard),'omitted unqueued MG cannot lock a cleared defence');
  const queuedCrew={m_iKind:4,m_bLost:false,m_bSpawned:false,m_iVehicle:1,m_iGroup:1};
  guard.m_aSlots.push(queuedCrew);guard.m_aGroups=[{m_iId:1,m_aSlots:[2]}];
  check(pending.call(guard),'unresolved native personnel queue still protects against premature capture');
  queuedCrew.m_bSpawned=true;check(!pending.call(guard),'resolved queue restores physical-hostile capture rule');
  guard.m_aGroups=[];guard.m_aSlots=[{...fallen,m_bLost:false,m_bSpawned:false}];
  check(pending.call(guard),'capture cannot race an entirely unspawned initial defence');
  blocked.m_bPlacementUnavailable=false;
  let placementTime=0;
  env.GetGame=()=>({GetWorld:()=>({GetWorldTime:()=>placementTime})});
  const placementFailure=compile(sources.garrison,'ReportPlacementFailure');
  const placementState={m_aSlots:[blocked,{...queuedCrew,m_bSpawned:false,m_iVehicle:0}],m_aGroups:[],m_iPlacementRetrySeconds:30,
    m_Location:{GetId:()=>'site'},ReportFailure:()=>{}};
  placementFailure.call(placementState,0,'blocked');placementTime=29999;placementFailure.call(placementState,0,'blocked');
  check(!blocked.m_bPlacementUnavailable,'failed placement remains retryable during its bounded window');
  placementTime=30000;placementFailure.call(placementState,0,'blocked');
  check(blocked.m_bPlacementUnavailable&&placementState.m_aSlots[1].m_bPlacementUnavailable,'failed hull/MG and its unqueued crew omit together');
  check(!blocked.m_bLost&&!placementState.m_aSlots[1].m_bLost,'omitting a never-spawned asset creates no casualties');
  const queuedSlot={...blocked,m_bPlacementUnavailable:false,m_iPlacementFailureSince:0};
  placementState.m_aSlots=[queuedSlot];placementState.m_aGroups=[{m_iId:1}];placementTime=60000;
  placementFailure.call(placementState,0,'blocked');check(!queuedSlot.m_bPlacementUnavailable,'native queued personnel cannot be discarded by pad timeout');
  placementState.m_aGroups=[];queuedSlot.m_bSpawned=true;placementFailure.call(placementState,0,'blocked');
  check(!queuedSlot.m_bPlacementUnavailable,'live assets cannot be omitted by placement timeout');
  queuedSlot.m_bSpawned=false;server=false;placementFailure.call(placementState,0,'blocked');
  check(!queuedSlot.m_bPlacementUnavailable,'clients cannot omit defence assets');server=true;
  check(sources.location.includes('support.GetSupport(role) <= 50'),'canonical town setter requires majority, including direct external calls');
  const deploymentReason=compile(sources.garrison,'GetDefenceCaptureBlockReason');
  const deployment={IsDefencePending:()=>true,m_bDefenceTerrainChecked:true,m_bDefenceTerrainFits:false,m_bDeploymentDeferred:true};
  check(deploymentReason.call(deployment)==='Defence site unsuitable - relocation required','rejected terrain is not labelled as living hostiles');
  deployment.m_bDefenceTerrainFits=true;check(deploymentReason.call(deployment)==='Defence deployment delayed','obstructed placement has distinct feedback');
  deployment.m_bDeploymentDeferred=false;check(deploymentReason.call(deployment)==='Defenders awaiting deployment','queued defenders have explicit feedback');
  deployment.IsDefencePending=()=>false;check(deploymentReason.call(deployment)==='','cleared roster removes deployment capture gate');
  server=false;check(deploymentReason.call({m_sPendingDefenseReason:'Defence deployment delayed'})==='Defence deployment delayed','client uses replicated reason');server=true;
  let bumps=0;env.Replication.BumpMe=()=>bumps++;
  const status={m_bPendingDefense:true,m_sPendingDefenseReason:'Defenders awaiting deployment',IsDefencePending:()=>true,GetDefenceCaptureBlockReason:()=>'Defence site unsuitable - relocation required'};
  status.UpdateDeployment_S=()=>{};compile(sources.garrison,'RefreshPending_S').call(status);check(bumps===1&&status.m_sPendingDefenseReason.startsWith('Defence site unsuitable'),'reason-only changes replicate while pending remains true');
  let livingHostile=false;Object.assign(env,{AROR_HostilesQuery:{HasHostilesInRadius:()=>livingHostile},GetGame:()=>({GetWorld:()=>({GetWorldTime:()=>10000})})});
  const physical=compile(sources.capture,'HasHostiles'),captureState={m_fLastHostileCheckTime:0,m_bHostilesPresent:false,GetLocation:()=>({GetCenter:()=>[0,0,0],GetRadius:()=>100})};
  check(!physical.call(captureState,{},false),'virtual/unspawned roster is not evidence of a physical hostile');
  livingHostile=true;check(physical.call(captureState,{},false),'living hostile query still blocks capture');
  check(sources.capture.includes('garrison.GetDefenceCaptureBlockReason()'),'deployment remains a separate authoritative capture check');
	const dormant=compile(sources.garrison,'HasDormantReserve');
	const sleeping={m_aGroups:[{m_iId:2,m_Group:{IsDormant:()=>true,GetDormantAliveCount:()=>3}}]};
	check(dormant.call(sleeping,{m_iGroup:2,m_iKind:0}),'native dormant infantry keeps its reserve');
	check(!dormant.call(sleeping,{m_iGroup:2,m_iKind:3}),'dormant crew never excuses a missing hull');
	Object.assign(env,{AROR_GarrisonComponent:{Cast:x=>x},ESerializeResult:{OK:1,DEFAULT:0}});
	const write=compile(sources.serializer,'Serialize'),read=compile(sources.serializer,'Deserialize');
	const saved={};let observed=false,restored=null;
	const snapshot=military.m_aSlots;Object.assign(snapshot[0],{m_bLost:true,m_fHealth:0,m_bPlaced:true,m_vPosition:[1,2,3],m_vAngles:[0,45,0]});
	const original={HasRoster:()=>true,Observe_S:orders=>{check(orders===false,'saving never mounts/orders/spawns actors');observed=true;},
	  IsRestoreQuarantined:()=>false,GetSlots:out=>out.push(...snapshot),GetRosterRole:()=>1,GetRosterFaction:()=>faction.m_sFactionKey,GetAssetFaction:()=>faction.m_sAssetFactionKey,GetLevel:()=>3};
	check(write(null,original,{WriteValue:(key,value)=>saved[key]=value})===1&&observed,'versioned roster snapshot writes');
	let quarantine='';const loaded={QuarantineRoster_S:reason=>quarantine=reason,RestoreRoster_S:(...args)=>restored=args},context={ReadValue:(key,set)=>{if(!(key in saved))return false;set(saved[key]);return true;}};
	check(read(null,loaded,context)&&restored[4].length===snapshot.length,'saved recipe restores all slots');
	check(restored[4][0].m_bLost&&restored[4][0].m_fHealth===0&&!restored[4][0].m_bPlaced,'casualty and damage survive while positions are replanned');
	const equippedSave={};let equippedRestore;
	const equippedSlots=equippedMilitary.m_aSlots.map(slot=>({...slot}));
	const destroyedStatic=equippedSlots.find(slot=>slot.m_iKind===1&&slot.m_iVariant===1);
	destroyedStatic.m_bLost=true;destroyedStatic.m_fHealth=0;
	write(null,{...original,GetSlots:out=>out.push(...equippedSlots)},{WriteValue:(key,value)=>equippedSave[key]=value});
	const equippedLoaded={QuarantineRoster_S:()=>{throw Error('Unexpected equipped roster quarantine');},RestoreRoster_S:(...args)=>equippedRestore=args};
	check(read(null,equippedLoaded,{ReadValue:(key,set)=>{if(!(key in equippedSave))return false;set(equippedSave[key]);return true;}}),'equipped roster round-trips through the actual serializer');
	check(equippedRestore[4].filter(slot=>slot.m_iKind===1).map(slot=>slot.m_iVariant).join(',')==='0,1,2,0','all four static variants survive save/load');
	check(equippedRestore[4].some(slot=>slot.m_iKind===1&&slot.m_iVariant===1&&slot.m_bLost&&slot.m_fHealth===0),'destroyed medium gun stays lost on restore');
	check(equippedRestore[4].filter(slot=>slot.m_iKind===0).length===24,'equipped save preserves the original infantry count');
	saved.version=99;restored=null;check(read(null,loaded,context)&&!restored&&quarantine,'unsupported headers quarantine without generating replacements');saved.version=1;
	check(read(null,loaded,context)&&!restored[4][0].m_bPlaced&&restored[4][0].m_bLost&&restored[4][0].m_fHealth===0,'v1 migration keeps losses/damage while invalidating scattered placement');
	saved.slot0_kind=4;saved.slot0_vehicle=999;check(read(null,loaded,context)&&restored[4][0].m_iVehicle===-1,'orphaned crew restore on foot instead of rejecting the whole roster');

  // Absolute deployment clocks cover empty capacity/terrain failures as well as active queues.
  let worldTime=1000;
  const playerIds=new Map();
  env.GetGame=()=>({GetWorld:()=>({GetWorldTime:()=>worldTime}),GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:e=>playerIds.get(e)||0})});
  env.AROR_QRFDefenseComponent={Cast:x=>x};env.SCR_EntityHelper={};
  const lifecycleNames=['BeginDeployment_S','UpdateDeployment_S','CloseDeployment_S','HasUnresolvedQueue','CancelQueuedGroup_S','DetachOrphanedCrew_S','QuarantineRoster_S','BindAssets_S','InfantryPrefab','StaticPrefab','ParkedPrefab','GetStaticWeaponCount'];
  const lifecycle=Object.fromEntries(lifecycleNames.map(n=>[n,compile(sources.garrison,n)]));
  function newDeployment(slots=[]){
    return {...lifecycle,m_aSlots:slots,m_aGroups:[],GetExtraParkedCount:()=>0,m_iDeploymentStartedAt:-1,m_iDeploymentGraceSeconds:60,m_iSpawnQueueTimeoutSeconds:30,
      m_iCapacityWaitSeconds:180,m_iCapacityWaitSince:-1,m_iCapacityWaited:0,m_iNextCapacityLog:0,m_bCapacityLimited:false,m_bHadDeployedPersonnel:false,
      m_bDeploymentClosed:false,m_bRestoreQuarantined:false,m_eRosterRole:1,m_bGenerated:true,
      m_Location:{GetControllingRole:()=>1,GetId:()=>'failure-site'},IsEligible:()=>true,AssignMembers:()=>{},IsPlayerOrForeignAsset:e=>!!e.foreign};
  }
  const waiting=newDeployment([new env.AROR_GarrisonSlot()]);
  waiting.BeginDeployment_S(worldTime);waiting.BeginDeployment_S(50000);
  check(waiting.m_iDeploymentStartedAt===1000,'proximity retries cannot restart the deployment deadline');
  waiting.UpdateDeployment_S(60999);check(!waiting.m_bDeploymentClosed&&pending.call(waiting),'capacity/terrain wait remains pending only inside its grace window');
  waiting.UpdateDeployment_S(61000);
  check(waiting.m_bDeploymentClosed&&!pending.call(waiting),'an entirely empty blocked site becomes capturable when its bounded window expires');
  check(waiting.m_aSlots[0].m_bPlacementUnavailable&&!waiting.m_aSlots[0].m_bLost,'deployment expiry omits virtual slots without inventing deaths');
  check(!compile(sources.garrison,'SpawnNextAsset_S').call(waiting,faction,6)&&!compile(sources.garrison,'SpawnNextGroup_S').call(waiting,62000,6),'closed activation cannot construct late assets or submit new groups');
  check(physical.call(captureState,{},false),'deployment timeout never bypasses living-hostile detection');
  const changedOwner=newDeployment();changedOwner.BeginDeployment_S(1000);changedOwner.m_Location.GetControllingRole=()=>0;changedOwner.UpdateDeployment_S(1001);
  check(changedOwner.m_bDeploymentClosed,'ownership change closes producers immediately');

  // Terrain failure may recover during the same bounded deployment, not only after retirement.
  let terrainSamples=0,terrainGood=false;
  env.AROR_DefenceLayout={TerrainFits:()=>{terrainSamples++;return terrainGood;}};
  const terrainState={m_bDefenceTerrainChecked:false,m_bDefenceTerrainFits:false,m_iTerrainRetryAt:0,
    IsDefenceCenterAllowed:()=>true,GetDefenceCenter:()=>[0,0,0],GetDefenceHalfExtent:()=>18,GetDefenceHalfDepth:()=>18,
    GetOwner:()=>({GetYawPitchRoll:()=>[0,0,0]}),ReportFailure:()=>{},m_fMaxDefenceRelief:2};
  const terrainCheck=compile(sources.garrison,'HasSuitableDefenceTerrain_S');
  worldTime=0;check(!terrainCheck.call(terrainState),'failed terrain measurement rejects construction');
  worldTime=4999;check(!terrainCheck.call(terrainState)&&terrainSamples===1,'failed terrain retry is throttled');
  worldTime=5000;terrainGood=true;check(terrainCheck.call(terrainState)&&terrainSamples===2,'failed terrain is rechecked and can recover');
  worldTime=7000;terrainCheck.call(terrainState);check(terrainSamples===2,'accepted terrain is cached for the activation');

  // Cancel the actual old group after detaching survivors; the old queue cannot reappear after capture.
  const deleted=[],detached=[],rehomed=[];
  env.SCR_EntityHelper.DeleteEntityAndChildren=e=>{if(e.attached&&e.attached.length)throw Error('Deleted a group with attached survivors');deleted.push(e);};
  env.AROR_QRFDefenseComponent={Cast:x=>x};
  const live={GetOrigin:()=>[1,0,1]},foreignAI={foreign:true,GetOrigin:()=>[2,0,1]};
  const previousGroup={attached:[live,foreignAI],m_aUnitPrefabSlots:['crew','crew'],
    SetMaxUnitsToSpawn:n=>previousGroup.max=n,SetNumberOfMembersToSpawn:n=>previousGroup.wanted=n,
    GetAgents:out=>out.push(...previousGroup.attached.map(e=>({GetControlledEntity:()=>e}))),
    RemoveAgentFromControlledEntity:e=>{detached.push(e);previousGroup.attached.RemoveItem(e);},FindComponent:()=>null,GetPlayerCount:()=>0};
  const partial=newDeployment([
    {...new env.AROR_GarrisonSlot(),m_bSpawned:true,m_Entity:live},
    new env.AROR_GarrisonSlot()
  ]);
  partial.CreateGroup_S=()=>({AddAgentFromControlledEntity:e=>rehomed.push(e)});
  const queued={m_iId:0,m_iQueuedAt:1000,m_aSlots:[0,1],m_Group:previousGroup,m_bQueueClosed:false};
  partial.m_aGroups=[queued];partial.BeginDeployment_S(1000);
  partial.UpdateDeployment_S(30999);check(!queued.m_bQueueClosed,'queue remains eligible before its own timeout');
  partial.UpdateDeployment_S(31000);
  check(queued.m_bQueueClosed&&deleted.includes(previousGroup)&&previousGroup.max===0&&previousGroup.wanted===0&&previousGroup.m_aUnitPrefabSlots.length===0,'timed-out native source group is stopped and deleted');
  check(detached.includes(live)&&detached.includes(foreignAI)&&rehomed.length===1&&rehomed[0]===live,'owned survivors regroup while foreign actors are preserved separately');
  check(partial.m_aSlots[1].m_bPlacementUnavailable&&!partial.m_aSlots[1].m_bLost,'queued member never produced is omitted without a replacement loss');
  check(!pending.call(partial)&&partial.m_aSlots[0].m_Entity===live,'resolved timeout unlocks only the virtual gate and retains physical defenders');

  // Lost hulls retain crew as on-foot survivors on subsequent activation and still reserve personnel.
  const orphanHull={...new env.AROR_GarrisonSlot(),m_iKind:3,m_iGroup:1,m_bLost:true};
  const orphanCrew={...new env.AROR_GarrisonSlot(),m_iKind:4,m_iGroup:1,m_iVehicle:0,m_iSeat:0,m_fHealth:0.6};
  const orphan=newDeployment([orphanHull,orphanCrew]);orphan.DetachOrphanedCrew_S();
  check(orphanCrew.m_iVehicle===-1&&orphanCrew.m_iSeat===-1&&!orphanCrew.m_bLost&&orphanCrew.m_fHealth===0.6,'surviving crew detach from their lost parent without damage/reset');
  check(reserve.call(orphan)===1,'orphaned crew still count toward the personnel cap');
  let requested=0;
  Object.assign(orphan,{m_iNextGroup:0,PlaceSlot:()=>true,CreateGroup_S:()=>({m_aUnitPrefabSlots:[],FindComponent:()=>null,RequestSpawn:n=>requested=n}),m_bActive:true});
  env.AROR_GarrisonGroup=class{constructor(){this.m_aSlots=[];}};
  check(compile(sources.garrison,'SpawnNextGroup_S').call(orphan,5000,6)&&requested===1,'orphaned survivors can re-enter the native queue on foot');

  // Schema 3 stores results and logical catalogue choices, not resource paths or stale positions.
  check(saved.version===1||saved.version===3,'serializer fixture remains a supported migration version');
  saved.version=3;delete saved.quarantined;saved.slot0_kind=0;saved.slot0_vehicle=-1;
  delete saved.slot1_health;restored=null;quarantine='';
  check(read(null,loaded,context)&&restored&&restored[4][1].m_bLost&&!restored[4][2].m_bLost,'one damaged slot is quarantined without rejecting its neighbouring survivors');
  check(restored[4][0].m_bLost,'partial record recovery preserves an independent saved casualty');
  const newSave={};write(null,original,{WriteValue:(k,v)=>newSave[k]=v});
  check(!Object.keys(newSave).some(k=>/(prefab|position|angles|placed)$/.test(k)),'new roster saves contain no prefab or transform fields');
  const rebinding=newDeployment(restored[4]);rebinding.m_bAssetsBound=false;
  const changedFaction={...faction,m_sFactionKey:'NEW_OCCUPIERS',m_sAssetFactionKey:'NEW',m_sInfantry:'new-rifle',m_sCrew:'new-crew',m_sStaticMG:'new-mg',m_sParkedArmour:'new-armour',m_aMannedVehicles:['new-hull']};
  check(rebinding.BindAssets_S(changedFaction)&&rebinding.m_aSlots[2].m_sPrefab==='new-rifle','restored survivors bind to current role assets');
  check(rebinding.m_aSlots[0].m_bLost&&rebinding.m_aSlots[1].m_bLost&&!rebinding.m_aSlots[2].m_bPlaced,'asset remapping neither refills casualties nor restores obsolete pads');
  const invalidRoster=newDeployment();invalidRoster.m_bActive=false;invalidRoster.QuarantineRoster_S('unsupported fixture');
  check(invalidRoster.m_bGenerated&&invalidRoster.m_bRestoreQuarantined&&invalidRoster.m_bDeploymentClosed,'unrecoverable save section cannot silently generate a new roster');
  check(!generate.call({...invalidRoster,IsEligible:()=>true},faction),'quarantined section blocks automatic recipe regeneration');
  const crewHull={...new env.AROR_GarrisonSlot(),m_iKind:3,m_iGroup:2,m_bCrewRecorded:true};
  const oldCrew={...new env.AROR_GarrisonSlot(),m_iKind:4,m_iGroup:2,m_iVehicle:0,m_iSeat:0,m_bLost:true};
  env.CargoCompartmentSlot={Cast:s=>s.cargo?s:null};
  const seatState={m_aSlots:[crewHull,oldCrew],AddSlot:add};
  compile(sources.garrison,'ReconcileCrew_S').call(seatState,0,[{},{}],faction);
  check(seatState.m_aSlots[1].m_bLost&&seatState.m_aSlots[2].m_bLost,'a replacement catalogue hull with extra seats grants no free crew');
  seatState.m_aSlots[1].m_bLost=false;seatState.m_aSlots[2].m_bLost=false;
  compile(sources.garrison,'ReconcileCrew_S').call(seatState,0,[{}],faction);
  check(seatState.m_aSlots[2].m_iVehicle===-1&&!seatState.m_aSlots[2].m_bLost,'fewer seats retain excess surviving crew on foot');

  // Cache region membership once, but re-read ownership and rebuild after registry changes.
  const applied=[];
  const supportA={ApplyRegionalSupport_S:(...args)=>applied.push(args)};
  const regionTown={...loc('town',types.TOWN,[0,0,0]),GetOwner:()=>({FindComponent:()=>supportA})};
  let regionalOwner=1,lookups=0;
  const regionSite={...loc('base',types.MILITARY_BASE,[10,0,0]),GetControllingRole:()=>regionalOwner};
  let regionLocations=[regionTown,regionSite];
  env.AROR_SettlementComponent={Cast:x=>x};env.AROR_CaptureLocationAction={IsCapturableType:t=>[2,4,5,6,7,8].includes(t)};
  env.AROR_RegionalSupportRegion=class{constructor(){this.m_aSites=[];this.m_aSettlements=[];}};
  const cachedRegion={m_bRegionalSupport:true,m_bRegionsDirty:true,m_aRegions:[],m_fStep:2,EnsureRegistry_S:()=>true,m_Flow:{IsPlaythroughReady:()=>true},
    m_Locations:{GetLocations:out=>out.push(...regionLocations)},RegionFor(...args){lookups++;return region(...args);}};
  cachedRegion.RebuildRegions_S=compile(sources.region,'RebuildRegions_S');
  const regionalTick=compile(sources.region,'Tick');
  regionalTick.call(cachedRegion);const firstLookups=lookups;regionalOwner=0;regionalTick.call(cachedRegion);
  check(firstLookups===2&&lookups===firstLookups,'support cycles reuse precomputed region membership');
  check(applied[0][1]===1&&applied[1][0]===1,'cached regions still use current military ownership');
  regionLocations=[regionTown];compile(sources.region,'OnRegistryChanged').call(cachedRegion,regionSite);regionalTick.call(cachedRegion);
  check(lookups===firstLookups+1&&applied[2].slice(0,3).every(n=>n===0),'registry removal invalidates membership without inventing military influence');



  // Registration caches component lookup; deletion removes weak references and proximity candidates.
  let flowLookups=0,componentLookups=0,registryReads=0,subscriptions=0;
  const invoker={push:()=>subscriptions++,Remove:()=>subscriptions--};
  const cachedSite={GetOwner:()=>({FindComponent:()=>{componentLookups++;return defence;}})};
  const registry={GetLocations:out=>{registryReads++;out.push(cachedSite);},GetOnLocationRegistered:()=>invoker,GetOnLocationUnregistered:()=>invoker,GetOnLocationControlChanged:()=>invoker};
  env.AROR_LocationManagerComponent.GetInstance=()=>registry;
  const cacheManager={m_Flow:null,m_Locations:null,m_aGarrisons:[],m_aNearby:[],GetOwner:()=>({FindComponent:()=>{flowLookups++;return {IsPlaythroughReady:()=>true};}})};
  for(const name of ['OnRegistered','OnUnregistered','OnControlChanged'])cacheManager[name]=compile(sources.manager,name);
  const ensure=compile(sources.manager,'EnsureRegistry_S');
  check(ensure.call(cacheManager)&&ensure.call(cacheManager),'registry cache can initialise and then be reused');
  check(flowLookups===1&&componentLookups===1&&registryReads===1&&subscriptions===3,'repeated manager ticks do not repeat game-flow/location component lookups');
  cacheManager.m_aNearby=[defence];cacheManager.OnUnregistered(teleportSite);
  check(cacheManager.m_aGarrisons.length===0&&cacheManager.m_aNearby.length===0,'unregistered locations leave neither component nor proximity cache entries');

  // Native asset failures share the same bounded retry path as pad failures.
  const badAsset={...new env.AROR_GarrisonSlot(),m_iKind:1,m_sPrefab:'missing-mg'};
  const failing=newDeployment([badAsset]);
  Object.assign(failing,{m_iNextAsset:0,m_iPlacementRetrySeconds:30,PlaceSlot:()=>true,Spawn:()=>null,ReportFailure:()=>{},ReportPlacementFailure:compile(sources.garrison,'ReportPlacementFailure')});
  worldTime=0;compile(sources.garrison,'SpawnNextAsset_S').call(failing,faction,6);
  worldTime=29999;compile(sources.garrison,'SpawnNextAsset_S').call(failing,faction,6);
  check(!badAsset.m_bPlacementUnavailable&&badAsset.m_iPlacementFailureSince===0,'failed asset loads do not reset their retry start');
  worldTime=30000;compile(sources.garrison,'SpawnNextAsset_S').call(failing,faction,6);
  check(badAsset.m_bPlacementUnavailable&&!badAsset.m_bLost,'repeated resource failure resolves without a fabricated casualty');


  // Shared capacity follows actual submitted work, not an entire virtual roster.
  const committed=compile(sources.garrison,'CommittedPersonnel'),nextCost=compile(sources.garrison,'NextPersonnelCost');
  const requestKnown=compile(sources.garrison,'IsGroupRequested');
  const virtualTeam=Array.from({length:4},()=>Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:0,m_iGroup:0}));
  const allocation={m_aSlots:virtualTeam,m_aGroups:[],IsGroupRequested:requestKnown};
  check(committed.call(allocation)===0&&nextCost.call(allocation)===4,'unspawned recipes consume no actor capacity but require complete fireteams');
  allocation.m_aGroups=[{m_iId:0}];
  check(committed.call(allocation)===4,'submitted native team reserves capacity before its members become physical');
  virtualTeam[0].m_bSpawned=true;
  check(committed.call(allocation)===4,'physical observation cannot double-count a queued member');
  virtualTeam[0].m_bLost=true;check(committed.call(allocation)===3,'confirmed personnel losses release only their own actor allocation');
  const budgetHull=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:3,m_iGroup:1,m_bSpawned:true});
  const budgetCrew=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:4,m_iGroup:1,m_iVehicle:0,m_bSpawned:true});
  const hullAllocation={m_aSlots:[budgetHull,budgetCrew],m_aGroups:[{m_iId:1}],IsGroupRequested:requestKnown};
  check(committed.call(hullAllocation)===4,'crewed hull allowance already includes its native crew slots');
  budgetHull.m_bLost=true;check(committed.call(hullAllocation)===1,'surviving lost-hull crew retain their own allocation');
  budgetHull.m_bLost=false;budgetCrew.m_bSpawned=false;
  const mixedOrphan=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:4,m_iGroup:1,m_iVehicle:-1});
  hullAllocation.m_aSlots.push(mixedOrphan);hullAllocation.m_aGroups=[];
  check(nextCost.call(hullAllocation)===1,'a migrated mixed crew group must reserve an orphaned survivor beyond the hull allowance');
  const capacityWait=compile(sources.garrison,'SetCapacityWait_S');
  const capacitySite=newDeployment([Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:0,m_iGroup:0})]);
  Object.assign(capacitySite,{NextPersonnelCost:()=>4,SetCapacityWait_S:capacityWait});
  capacitySite.BeginDeployment_S(0);capacitySite.SetCapacityWait_S(1000,true,96,96);
  capacitySite.UpdateDeployment_S(61000);
  check(!capacitySite.m_bDeploymentClosed,'a new empty site does not exhaust its ordinary deployment window while capacity is occupied elsewhere');
  capacitySite.SetCapacityWait_S(91000,false,88,96);capacitySite.UpdateDeployment_S(149999);
  check(!capacitySite.m_bDeploymentClosed,'capacity wait is deducted from deployment elapsed time');
  capacitySite.UpdateDeployment_S(150000);check(capacitySite.m_bDeploymentClosed,'capacity extensions retain a finite overall deadline');
  const saturated=newDeployment([Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:0,m_iGroup:0})]);
  Object.assign(saturated,{NextPersonnelCost:()=>4,SetCapacityWait_S:capacityWait});saturated.BeginDeployment_S(0);saturated.SetCapacityWait_S(0,true,96,96);
  saturated.UpdateDeployment_S(239999);check(!saturated.m_bDeploymentClosed,'maximum capacity grace remains available before its boundary');
  saturated.UpdateDeployment_S(240000);check(saturated.m_bDeploymentClosed&&!saturated.m_aSlots[0].m_bLost,'persistent saturation resolves without fabricated casualties or an infinite capture lock');
  const established=newDeployment([Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:0,m_iGroup:0,m_bSpawned:true})]);
  established.BeginDeployment_S(0);established.m_bCapacityLimited=true;established.m_bHadDeployedPersonnel=true;established.UpdateDeployment_S(1000);
  check(established.m_bDeploymentClosed,'an established partial defence stops optional additions before clearance can create late pop-ins');
  let partialCommitted=0,waited=false;
  const partialSite={...defence,IsCapacityWaiting:()=>waited,CommittedPersonnel:()=>partialCommitted,ReservedPersonnel:()=>31,NextPersonnelCost:()=>4,
    SetCapacityWait_S(now,value){waited=value;},SpawnNextGroup_S(now,clearance,free){if(free<4)return false;partialCommitted+=4;return true;}};
  const occupied={CommittedPersonnel:()=>88};
  const partialManager={...managerState,m_aGarrisons:[occupied,partialSite],m_aNearby:[partialSite],m_iNextProximityAt:Infinity,m_iCursor:0};
  managerTick.call(partialManager);managerTick.call(partialManager);managerTick.call(partialManager);
  check(partialCommitted===8&&waited,'a 31-person virtual recipe can establish eight defenders in the remaining eight slots instead of staying empty');
  check(partialCommitted+occupied.CommittedPersonnel()===96,'incremental admission cannot exceed the hard global cap');
  const enriched={...faction,m_aInfantryVariants:['automatic','grenadier','leader']};
  const infantryPrefab=compile(sources.garrison,'InfantryPrefab');
  check([0,1,2,3].map(v=>infantryPrefab(enriched,v)).join(',')==='rifle,automatic,grenadier,leader','configured teams combine rifleman, automatic rifleman, grenadier and leader');
  check(infantryPrefab(faction,3)==='rifle','extenders without specialist configuration retain their existing rifleman');
  // Moving from Chotain to Levie must release safely distant actors before the
  // new site's extended deployment window expires, without deleting combatants nearby.
  env.GetGame=()=>game;playerPosition=[0,0,0];playerDead=false;
  let released=0,recentCombat=false,pressureActive=true;
  const oldLocation={GetCenter:()=>[2000,0,0],GetControllingRole:()=>1,GetId:()=>'old-site'};
  const oldGarrison={...defence,m_iOutsideSince:-1,m_Location:oldLocation,m_aSlots:[{m_Entity:{GetOrigin:()=>[2000,0,0]}}],
    GetLocation:()=>oldLocation,IsGarrisonActive:()=>pressureActive,HasDeploymentStarted:()=>pressureActive,IsCapacityWaiting:()=>false,
    CommittedPersonnel:()=>pressureActive?88:0,CanRetire:compile(sources.garrison,'CanRetire'),OutsideLongEnough:compile(sources.garrison,'OutsideLongEnough'),
    Retire_S(){released++;pressureActive=false;}};
  const awaiting={...defence,IsGarrisonActive:()=>false,IsCapacityWaiting:()=>true,CommittedPersonnel:()=>0};
  const pressureManager={...managerState,m_aGarrisons:[oldGarrison,awaiting],m_aNearby:[],m_fPressureRetirementRadius:1200,m_iPressureRetirementDelay:15,
    HasRecentCombat:()=>recentCombat};
  scan.call(pressureManager,0);scan.call(pressureManager,14999);
  check(released===0,'capacity pressure still requires continuous safe distance');
  scan.call(pressureManager,15000);check(released===1&&oldGarrison.CommittedPersonnel()===0,'distant retained garrison releases its actor allocation before the new deployment deadline');
  pressureActive=true;oldGarrison.m_iOutsideSince=-1;recentCombat=true;
  scan.call(pressureManager,20000);scan.call(pressureManager,50000);
  check(released===1,'recent validated combat prevents accelerated retirement');
  recentCombat=false;oldGarrison.m_aSlots[0].m_Entity.GetOrigin=()=>[500,0,0];
  scan.call(pressureManager,60000);scan.call(pressureManager,90000);
  check(released===1,'a tracked actor near any player prevents retirement even when its parent site is distant');
  const fallback=compile(sources.garrison,'PrepareInfantryFallback_S');
  const localInfantry=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:0,m_iGroup:0});
  const omittedHull=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:3,m_iGroup:1});
  const omittedCrew=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:4,m_iGroup:1,m_iVehicle:1});
  const orphaned=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:4,m_iGroup:2,m_iVehicle:-1});
  const liveHull=Object.assign(new env.AROR_GarrisonSlot(),{m_iKind:1,m_iGroup:3,m_bSpawned:true});
  const localFallback=newDeployment([localInfantry,omittedHull,omittedCrew,orphaned,liveHull]);
  check(fallback.call(localFallback)&&localFallback.m_bInfantryTerrainFallback,'scenario may select an explicit infantry-only fallback after a failed full-footprint check');
  check(omittedHull.m_bPlacementUnavailable&&omittedCrew.m_bPlacementUnavailable&&!localInfantry.m_bPlacementUnavailable&&!orphaned.m_bPlacementUnavailable,'fallback omits unspawned heavy assets and attached crew while retaining infantry and orphaned survivors');
  check(!liveHull.m_bPlacementUnavailable&&localFallback.m_aSlots.every(s=>!s.m_bLost),'terrain fallback never deletes existing actors or invents casualties');
  localFallback.m_bDefenceTerrainChecked=true;localFallback.m_bDefenceTerrainFits=false;localFallback.IsDefencePending=()=>true;
  check(deploymentReason.call(localFallback)!=='Defence site unsuitable - relocation required','accepted infantry fallback does not falsely report a complete terrain block to players');
  env.AROR_DefenceLayout={Facing:()=>0,RepairOffset:()=>[0,0,0]};
  env.Vector=(...values)=>values;
  env.GetGame=()=>({...game,GetWorld:()=>({...game.GetWorld(),GetSurfaceY:()=>0})});
  const existingPlace=compile(sources.garrison,'PlaceSlot');let localChecks=0;
  const unsafe={...localFallback,HasSuitableDefenceTerrain_S:()=>false,GetOwner:()=>({GetYawPitchRoll:()=>[0,0,0],VectorToParent:v=>v}),
    PositionOrdinal:()=>0,IsSentry:()=>false,DefenceMarker:()=>({GetOrigin:()=>[0,0,0],GetYawPitchRoll:()=>[0,0,0],FindComponent:type=>type===env.AROR_SentryPostComponent?null:({GetElevation:()=>0})}),GetDefenceCenter:()=>[0,0,0],GetDefenceHalfExtent:()=>18,GetDefenceHalfDepth:()=>18,IsClear:()=>{localChecks++;return false;}};
  // The same individual pad gate remains in production; non-infantry exits before native terrain access.
  check(!existingPlace.call(unsafe,1,6),'full-footprint failure cannot admit a heavy hull through the infantry exception');
  check(!existingPlace.call(unsafe,0,6)&&localChecks===25,'infantry fallback still rejects every obstructed or unwalkable local pad within its search bound');
  unsafe.IsClear=()=>true;
  check(existingPlace.call(unsafe,0,6)&&localInfantry.m_bPlaced,'infantry fallback can admit a locally validated pad despite an unsuitable compound corner');
  // Exercise the actual manager and team-submission methods under a crowded
  // arrival. Native member creation is delayed; every submitted member must
  // already count against the same cap while the next site takes its turn.
  const submitTeam=compile(sources.garrison,'SpawnNextGroup_S');let nativeRequests=0,maximumCommitted=0;
  const crowdedSites=Array.from({length:13},(_,siteIndex)=>{
    const position=[siteIndex*100,0,0],logical=roster(types.OUTPOST).m_aSlots.filter(s=>s.m_iKind===0);
    logical.forEach(s=>s.m_vPosition=position);
    const site=newDeployment(logical);
    Object.assign(site,{m_bActive:false,m_iNextGroup:0,m_bAssetsBound:false,m_iOutsideSince:-1,
      m_Location:{GetCenter:()=>position,GetControllingRole:()=>1,GetId:()=>'crowded-'+siteIndex},
      IsGarrisonActive:()=>site.m_bActive,HasRoster:()=>site.m_bGenerated,GetRosterRole:()=>site.m_eRosterRole,
      HasDeploymentStarted:()=>site.m_iDeploymentStartedAt>=0,IsDeploymentClosed:()=>site.m_bDeploymentClosed,
      IsCapacityWaiting:()=>site.m_iCapacityWaitSince>=0&&!site.m_bDeploymentClosed,GetLocation:()=>site.m_Location,
      CommittedPersonnel:committed,NextPersonnelCost:nextCost,IsGroupRequested:requestKnown,SetCapacityWait_S:capacityWait,
      Observe_S(){},RefreshPending_S(){site.UpdateDeployment_S(clock);},ClearAssignments_S(){},HasSuitableDefenceTerrain_S:()=>true,
      CanRetire:compile(sources.garrison,'CanRetire'),OutsideLongEnough:compile(sources.garrison,'OutsideLongEnough'),Retire_S(){throw Error('Nearby defence retired under pressure');},
      SpawnNextGroup_S:submitTeam,SpawnNextAsset_S:()=>false,PlaceSlot:()=>true,
      CreateGroup_S(){return {m_aUnitPrefabSlots:[],FindComponent:()=>null,m_iReadyAt:clock+500,RequestSpawn(){nativeRequests++;}};},
      AssignMembers(pending){if(clock<pending.m_Group.m_iReadyAt)return;for(const i of pending.m_aSlots){const slot=site.m_aSlots[i];if(slot.m_bLost||slot.m_bPlacementUnavailable)continue;slot.m_bSpawned=true;slot.m_Entity={GetOrigin:()=>position};site.m_bHadDeployedPersonnel=true;}}});
    return site;
  });
  playerPosition=[650,0,0];clock=0;env.GetGame=()=>game;
  const crowdedManager={...managerState,FactionFor:()=>faction,m_aGarrisons:crowdedSites,m_aNearby:[],m_iNextProximityAt:0,m_iCursor:0,m_fPressureRetirementRadius:1200,m_iPressureRetirementDelay:15};
  for(let tick=0;tick<120;tick++){
    clock=tick*250;managerTick.call(crowdedManager);
    maximumCommitted=Math.max(maximumCommitted,crowdedSites.reduce((sum,s)=>sum+s.CommittedPersonnel(),0));
  }
  check(maximumCommitted<=96&&maximumCommitted>0,'crowded incremental/native-queue admission never exceeds 96 committed personnel: observed='+maximumCommitted+', requests='+nativeRequests);
  check(crowdedSites.every(s=>s.m_aSlots.some(slot=>slot.m_bSpawned)),'every nearby outpost receives physical defenders before optional second teams fill the cap');
  const submissionsBeforeClear=nativeRequests;
  crowdedSites.forEach(s=>s.m_aSlots.forEach(slot=>{if(slot.m_bSpawned){slot.m_bLost=true;slot.m_Entity=null;}}));
  for(let tick=120;tick<180;tick++){clock=tick*250;managerTick.call(crowdedManager);}
  check(nativeRequests===submissionsBeforeClear,'free capacity after clearance cannot manufacture reinforcements or refill casualties');
  const reset=compile(sources.garrison,'ResetRoster_S');
  const retaken={...newDeployment([new env.AROR_GarrisonSlot()]),m_bActive:false,m_bDeploymentClosed:true,m_iDeploymentStartedAt:1000,m_iOutsideSince:2000};
  reset.call(retaken);
  check(!retaken.m_bGenerated&&!retaken.m_bDeploymentClosed&&retaken.m_iDeploymentStartedAt===-1&&retaken.m_iOutsideSince===-1,'safely retired ownership change starts a fresh deployment window');
  const stillActive={...retaken,m_bActive:true,m_bGenerated:true,m_bDeploymentClosed:true};
  reset.call(stillActive);check(stillActive.m_bGenerated&&stillActive.m_bDeploymentClosed,'ownership reset cannot overwrite active defenders');
  check(deploymentReason.call({IsDefencePending:()=>false})==='','terminal deployment outcomes expose no stale capture rejection');
  return {assertions,note:'Mocked native services; Workbench compile, crew seating, placement, replication and save/load remain required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
  const sources={};for(const [key,name]of Object.entries({manager:'Garrisons/AROR_GarrisonManagerComponent',policy:'Garrisons/AROR_GarrisonPolicy',garrison:'Garrisons/AROR_GarrisonComponent',serializer:'Garrisons/AROR_GarrisonComponentSerializer',support:'Settlements/AROR_SettlementComponent',region:'Settlements/AROR_RegionalSupportComponent',location:'Locations/AROR_LocationComponent',capture:'Capture/AROR_CaptureLocationAction'}))sources[key]=fs.readFileSync(path.join(root,name+'.c'),'utf8');
  console.log(JSON.stringify(runGarrisonsAndSupportTests(sources)));
}
