/* Actual-source ingress regressions with native services mocked.
 * Run: node tools/Test-QRFIngress.js. Does not compile Enforce or certify native routing.
 */
function runQRFIngressTests(sources) {
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  const methods=['GetOwner','IsSeatedInOwnVehicle','TacticalLaunchPoint','TacticalRoadPoint','CanDismountOnFoot','IsLaunchPositionSafe','FailOperation','FailGroundPlanning','ResolveQRFOrigin','FindGroundRoadLaunch','ConfigureForceFunding','FindTacticalApproach','ConvertReservation','SpawnGroundVehicle','SpawnEmptyGroup','SpawnWaypoint','ConfigureGroup'];
  methods.push('GetMountRejection');
  const env={AROR_ForceSpawner:{IsAlive:x=>!!x&&!x.dead},string:{Empty:'',Format:()=>''},LAST_RESORT_ROAD_RADIUS:600,UNDISPATCHED_CLEANUP_DISTANCE:300,GetGame:()=>({GetPlayerManager:()=>({GetPlayerIdFromControlledEntity:e=>e?.player?1:0})})};
  if(!String.prototype.IsEmpty)Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
  function compile(source,name) {
    const match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!match)throw Error('Missing '+name);
    let args=match[1].replace(/\bout\s+/g,'').replace(/\b(?:bool|vector|float|int|IEntity|AROR_\w+)\s+/g,'');
    let body=match[2].replace(/array<\w+> (\w+) = \{\};/g,'let $1=[];')
      .replace(/new TraceBox\(\)/g,'({})')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
      .replace(/vector (\w+);/g,'let $1=[0,0,0];')
      .replace(/\b(?:bool|float|int|vector|IEntity|ResourceName|Resource|NavmeshWorldComponent|ChimeraAIWorld|BaseWorld|CompartmentAccessComponent|ChimeraCharacter|BaseCompartmentSlot|BaseCompartmentManagerComponent|SCR_AIVehicleUsageComponent|AROR_\w+|TraceBox)\s+(\w+)/g,'let $1')
      .replace(/let (\w+);/g,'let $1=0;')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
    for(const method of methods)body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(', 'g'),'this.'+method+'(');
    // V8 does not implement Enforce's vector arithmetic. Represent the bounded
    // candidate offset explicitly; all acceptance decisions remain actual source.
    body=body.replace('seed + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius','seed.map((v,i)=>v+[Math.sin(angle)*radius,0,Math.cos(angle)*radius][i])')
      .replace('origin - commander.m_vTown','origin.map((v,i)=>v-commander.m_vTown[i])')
      .replace('commander.m_vOrigin + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius','commander.m_vOrigin.map((v,i)=>v+[Math.sin(angle)*radius,0,Math.cos(angle)*radius][i])')
      .replace('(attempt - 12) / 8 + 3','Math.floor((attempt - 12) / 8) + 3')
      .replace('trace.Start + Vector(0, 0.05, 0)','[trace.Start[0],trace.Start[1]+0.05,trace.Start[2]]')
      .replace('commander.m_vTown + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius','commander.m_vTown.map((v,i)=>v+[Math.sin(angle)*radius,0,Math.cos(angle)*radius][i])')
      .replace(/approach = (reachable|fallback);/g,'approach.splice(0,3,...$1);')
      .replace('point = road;','point.splice(0,3,...road);'); // Enforce out-vector argument
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  env.CompartmentAccessComponent={Cast:x=>x};
  env.ChimeraCharacter={Cast:x=>x};
  env.SCR_AIVehicleUsageComponent={Cast:x=>x};
  env.BaseCompartmentManagerComponent={Cast:x=>x};
  const vehicle={FindComponent:type=>type===env.BaseCompartmentManagerComponent?{GetCompartments:()=>{}}:usage},otherVehicle={};
  let occupant={},inSeat=true,gettingIn=false,gettingOut=false,seatVehicle=vehicle;
  const access={IsInCompartment:()=>inSeat,IsGettingIn:()=>gettingIn,IsGettingOut:()=>gettingOut,
    GetCompartment:()=>({GetVehicle:()=>seatVehicle,GetOccupant:()=>occupant})};
  const person={FindComponent:()=>access,GetCharacterController:()=>({IsDead:()=>false})};occupant=person;
  const ownSeat=compile(sources.ground,'IsSeatedInOwnVehicle');
  const force={m_Vehicle:vehicle,m_bMountRequested:false,m_CrewGroup:{GetPlayerCount:()=>0,GetAgents:a=>a.push({GetControlledEntity:()=>person})},
    m_aPassengers:[person],IsSeatedInOwnVehicle:e=>ownSeat.call(force,e)};
  let pilot=person;
  const usage={GetPilotCompartmentSlot:()=>({GetOccupant:()=>pilot})};
  const complete=compile(sources.ground,'IsMountComplete');
  force.GetMountRejection=compile(sources.ground,'GetMountRejection');
  check(!complete.call(force),'no readiness before initial seat request');
  force.m_bMountRequested=true;check(complete.call(force),'driver and all members in own seats allow readiness');
  inSeat=false;check(!complete.call(force),'accepted teleport request without physical seating cannot activate');inSeat=true;
  gettingIn=true;check(!complete.call(force),'in-progress seating waits for later tick');gettingIn=false;
  gettingOut=true;check(!complete.call(force),'member already exiting cannot activate');gettingOut=false;
  pilot=null;check(!complete.call(force),'empty driver seat blocks dispatch');pilot=person;
  seatVehicle=otherVehicle;check(!complete.call(force),'wrong vehicle compartment cannot satisfy readiness');seatVehicle=vehicle;
  occupant={};check(!complete.call(force),'stale compartment pointer without matching occupant cannot satisfy readiness');occupant=person;
  force.m_aPassengers=[null];check(!complete.call(force),'missing passenger blocks dispatch');force.m_aPassengers=[person];

  let queries=0,blocked=false,slope=false,water=false,roadOK=true,road=[100,10,100];
  const world={GetSurfaceY:(x,z)=>slope&&x>100?15:10,GetOceanHeight:()=>water?10:0,TraceMove:()=>blocked?0:1};
  let launchLogs=[];
  Object.assign(env,{GetGame:()=>({GetWorld:()=>world}),Vector:(...v)=>v,TraceFlags:{ENTS:1},
    Math:{Atan2:Math.atan2,Sin:Math.sin,Cos:Math.cos,AbsFloat:Math.abs,Min:Math.min,Max:Math.max,Mod:(a,b)=>a%b,floor:Math.floor,sin:Math.sin,cos:Math.cos},float:{MAX:Infinity},vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
    AROR_GroundOperationForce:{Cast:x=>x},Print:message=>launchLogs.push(message),string:{Format:(format,...args)=>format.replace(/%(\d+)/g,(_,n)=>String(args[n-1]))},LogLevel:{NORMAL:0,WARNING:1}});
  const launch=compile(sources.executor,'FindGroundRoadLaunch');
  Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length;},configurable:true});
  for(const [n,f] of [['Contains',function(x){return this.includes(x);}],['Insert',function(x){this.push(x);}]])Object.defineProperty(Array.prototype,n,{value:f,configurable:true});
  const context={m_aForces:[],m_aRoadlessOrigins:[],m_fQRFRoadLaunchRadius:150,IsLaunchPositionSafe:()=>true,TacticalLaunchPoint:()=>[100,10,100],TacticalRoadPoint:(origin,desired,out)=>{queries++;out.splice(0,3,...road);return roadOK;}};
  const commander={m_vOrigin:[100,10,100],m_vTown:[1000,10,1000],m_sOriginLocationId:'farm'},point=[0,0,0];
  const run=()=>launch.call(context,commander,0,point);
  check(run()&&point[1]===10&&queries===1,'clear level road pad accepted and grounded');
  const centreRoad=context.TacticalRoadPoint;context.TacticalRoadPoint=(origin,desired,out)=>{if(origin===commander.m_vOrigin)return false;out.splice(0,3,...road);return true;};
  check(run()&&!context.m_aRoadlessOrigins.length,'a base centre cut off on the navmesh still launches from a road beside the probe');context.TacticalRoadPoint=centreRoad;
  queries=0;blocked=true;check(!run()&&queries===36,'obstructed pads rejected within expanded finite road-query budget');blocked=false;
  check(launchLogs.some(l=>l.includes('obstruction=36')),'failed launch reports obstruction count');
  check(!context.m_aRoadlessOrigins.length,'a launch blocked by obstructions never marks the base as roadless');
  roadOK=false;check(!run(),'raw outpost position never substitutes for missing road');roadOK=true;
  check(context.m_aRoadlessOrigins.join()==='farm'&&launchLogs.some(l=>l.includes('no longer used as a launch site')),'a base with no reachable road at all is remembered and reported');
  roadOK=false;run();roadOK=true;check(context.m_aRoadlessOrigins.length===1,'the base is recorded only once');context.m_aRoadlessOrigins=[];
  road=[300,10,100];check(!run(),'road outside origin vicinity rejected');road=[100,10,100];
  water=true;check(!run(),'underwater road candidate rejected');water=false;
  slope=true;check(!run(),'steep pad rejected');slope=false;
  context.m_aForces=[{m_Vehicle:{GetOrigin:()=>[110,10,100]}}];check(!run(),'second vehicle cannot overlap an existing package');
  context.m_aForces=[{m_Vehicle:{GetOrigin:()=>[140,10,100]}}];check(run(),'separated vehicle pad accepted');
  // Quarry reproduction: nearby queries snap back onto the first two hulls.
  const nativeLaunchRoad=context.TacticalRoadPoint;
  context.m_aForces=[{m_Vehicle:{GetOrigin:()=>[100,10,100]}},{m_Vehicle:{GetOrigin:()=>[140,10,100]}}];
  queries=0;const desiredPoints=[];
  context.TacticalRoadPoint=(origin,desired,out)=>{queries++;desiredPoints.push(desired);out.splice(0,3,...(queries<=12?[100,10,100]:desired));return true;};
  check(run()&&queries===13&&point[2]===190,'third package finds a clear outer-origin pad after the former search is exhausted');
  check(context.m_aForces.every(f=>env.vector.DistanceXZ(point,f.m_Vehicle.GetOrigin())>=25),'outer search preserves separation from both earlier hulls');
  queries=0;roadOK=false;context.TacticalRoadPoint=nativeLaunchRoad;context.m_aForces=[];launchLogs=[];
  check(!run()&&queries===72&&launchLogs.some(l=>l.includes('noRoad=36')),'missing connectivity remains bounded (two road queries per probe) and reports its reason');roadOK=true;
  queries=0;context.IsLaunchPositionSafe=p=>p[0]>=120;
  context.TacticalRoadPoint=(origin,desired,out)=>{queries++;out.splice(0,3,...(queries===1?[100,10,100]:[130,10,100]));return true;};
  check(run()&&queries===2&&point[0]===130,'unsafe player/target candidate is skipped instead of rejecting an otherwise usable origin');
  context.IsLaunchPositionSafe=()=>true;context.m_fQRFRoadLaunchRadius=75;queries=0;let farthest=0;
  context.TacticalRoadPoint=(origin,desired,out)=>{queries++;farthest=Math.max(farthest,env.vector.DistanceXZ(origin,desired));out.splice(0,3,200,10,100);return true;};
  check(!run()&&queries===36&&farthest<=75.001,'configured launch radius bounds outer rings and rejects projected roads outside it');
  context.m_fQRFRoadLaunchRadius=150;context.TacticalRoadPoint=nativeLaunchRoad;
  // Levie's off-road capture point: use an actual planned road stop, then walk in.
  const approach=compile(sources.executor,'FindTacticalApproach'),arrival=compile(sources.executor,'IsQRFDeliveryReached');
  let roadQueries=0,footPad=true,nativeRoad=true,roadPoint=[60,10,0];
  const roadForce={m_sOperationId:'capture',m_iPackageIndex:0,m_fTacticalDeliveryRadius:0};
  const roadCommander={m_vTown:[0,10,0],m_fTownRadius:150,m_fMargin:50,ApproachCandidate:()=>[60,10,0]};
  const roadContext={m_iQRFApproachAttempts:12,m_fQRFObjectiveDeliveryRadius:100,m_fQRFMaximumRoadDismount:300,m_fQRFVehicleArrivalTolerance:20,m_fQRFSearchStopSpacing:30,m_aForces:[],
    CanDismountOnFoot:()=>footPad,TacticalRoadPoint(origin,desired,out){roadQueries++;out.splice(0,3,...roadPoint);return nativeRoad;}};
  const roadStop=[0,0,0],plan=()=>approach.call(roadContext,roadForce,roadCommander,[500,10,0],60,roadStop);
  check(plan()&&roadQueries===1&&roadForce.m_fTacticalDeliveryRadius===0,'normal objective road delivery remains preferred');
  roadQueries=0;roadPoint=[160,10,0];
  check(plan()&&roadQueries<=24&&roadForce.m_fTacticalDeliveryRadius===180,'off-road objective permits a bounded planned road dismount with a foot approach');
  roadQueries=0;
  check(approach.call(roadContext,roadForce,roadCommander,[500,10,0],60,roadStop,3,true)&&roadQueries===1,'detour recovery tries the bounded road/foot stop before inner objective roads');
  roadQueries=0;roadPoint=[60,10,0];footPad=false;
  check(approach.call(roadContext,roadForce,roadCommander,[500,10,0],60,roadStop,3,true)&&roadQueries===13,'unsafe foot approach falls back to normal objective road delivery');
  footPad=true;roadPoint=[160,10,0];roadForce.m_fTacticalDeliveryRadius=180;
  check(!arrival.call(roadContext,10,160,200),'unplanned remote road proximity cannot authorise unloading');
  check(arrival.call(roadContext,10,160,200,roadForce.m_fTacticalDeliveryRadius),'physical arrival at the planned fallback stop authorises dismount');
  check(!arrival.call(roadContext,21,160,200,180),'fallback still requires proximity to the actual selected stop');
  check(!arrival.call(roadContext,10,301,200,999),'a forged oversized planned radius cannot exceed the configured fallback limit');
  roadQueries=0;footPad=false;check(!plan()&&roadQueries===25,'road without a usable infantry dismount is rejected within the total query bound (24 probes and one last-resort query)');footPad=true;
  // Walled compound or off-road site: every ring probe fails; the nearest launch-reachable road within the dismount limit is used.
  const ringRoad=roadContext.TacticalRoadPoint;roadContext.TacticalRoadPoint=function(origin,desired,out,radius){roadQueries++;if(desired!==roadCommander.m_vTown)return false;out.splice(0,3,...roadPoint);return env.vector.DistanceXZ(roadPoint,desired)<=(radius??75);};
  roadQueries=0;roadPoint=[250,10,0];roadForce.m_fTacticalDeliveryRadius=0;
  check(plan()&&roadQueries===25&&roadForce.m_fTacticalDeliveryRadius===270,'when every probe fails, the nearest reachable road within the dismount limit becomes a foot-approach stop');
  check(arrival.call(roadContext,10,250,200,roadForce.m_fTacticalDeliveryRadius),'arrival at that last-resort stop authorises the dismount within the configured limit');
  roadQueries=0;roadPoint=[350,10,0];check(!plan(),'a road beyond the dismount limit is never used as a last resort');
  roadContext.TacticalRoadPoint=ringRoad;roadPoint=[160,10,0];
  roadQueries=0;nativeRoad=false;check(!plan()&&roadQueries===25,'missing road connectivity does not teleport vehicles or manufacture arrival (the last-resort query fails too)');nativeRoad=true;
  roadQueries=0;roadPoint=[400,10,0];check(!plan()&&roadQueries===25,'distant roads cannot expand the fallback beyond its bounded radius, last resort included');
  roadContext.m_fQRFMaximumRoadDismount=0;roadQueries=0;check(!plan()&&roadQueries===12,'scenario can disable foot approach fallback');roadContext.m_fQRFMaximumRoadDismount=300;
  roadPoint=[60,10,0];roadContext.m_aForces=[{m_sOperationId:'capture',m_vApproachPosition:[78,10,0]}];roadQueries=0;
  check(plan()&&roadQueries===12&&roadStop[0]===60,'compact safe spacing is accepted before considering distant road stops');
  roadContext.m_aForces=[{m_sOperationId:'capture',m_vApproachPosition:[75,10,0]}];
  check(!plan(),'neighbouring packages cannot overlap below minimum spacing');
  // A failed delivery preflight must never construct an empty vehicle or consume
  // an actor reservation. This reproduces the failed Levie launch from the log.
  env.AROR_GroundOperationForce=function(){};env.AROR_GroundOperationForce.Cast=x=>x;env.AROR_InfantryRoster={Apply_S(){}};
  env.AROR_EFactionRole={OCCUPIERS:1,INVADERS:2};env.AROR_EForceType={MOTORIZED:0,ARMOUR:2};env.AROR_EOperationType={QRF:1};
  env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocationById:()=>({GetCenter:()=>[0,10,0]})})};
  let createdVehicles=0,converted=0,planningFailures=0;
  const preflight={m_vOccupiersSpawnPosition:[500,10,0],m_fQRFObjectiveDeliveryRadius:100,m_aForces:[],TacticalLaunchPoint:()=>[500,10,0],IsLaunchPositionSafe:()=>true,
    FindGroundRoadLaunch(c,i,out){out.splice(0,3,500,10,0);return true;},ConfigureForceFunding(){},FindTacticalApproach:()=>false,
    FailGroundPlanning(){planningFailures++;},ConvertReservation(){converted++;},SpawnGroundVehicle(){createdVehicles++;}};
  const failedRequest={GetRole:()=>1,GetTargetLocationId:()=>'levie',GetId:()=>'failed-capture',m_TacticalCommander:{m_vTown:[0,10,0]},m_iPlanningRetries:0};
  const failedProfile={m_eType:0,m_fDismountDistance:60,GetGroupBudget:()=>2,m_iPersonnelBudget:8,m_sAssetFactionKey:'US'};
  compile(sources.executor,'StartGroundForce').call(preflight,failedRequest,{GetFactionKey:()=>'OWN'},failedProfile,0,{});
  check(planningFailures===1&&createdVehicles===0&&converted===0&&preflight.m_aForces.length===0,'delivery failure leaves no empty hull, personnel queue or converted actor reservation');
  let plannedRoadPads=0;
  preflight.IsLaunchPositionSafe=p=>p[0]===520;
  preflight.FindGroundRoadLaunch=(c,i,out)=>{plannedRoadPads++;out.splice(0,3,520,10,0);return true;};
  compile(sources.executor,'StartGroundForce').call(preflight,failedRequest,{GetFactionKey:()=>'OWN'},failedProfile,0,{});
  check(plannedRoadPads===1&&planningFailures===2,'unsafe coastal formation seed still reaches the bounded safe-road planner before delivery preflight');
  env.AROR_GarrisonComponent={Cast:x=>x};
  const strategicCentre=[0,0,0],flagPosition=[80,1,0];let flagInside=true,flagGarrison={IsEligible:()=>true,GetDefenceCenter:()=>flagPosition};
  const flagTarget={GetOwner:()=>({FindComponent:()=>flagGarrison}),GetCenter:()=>strategicCentre,IsPositionInside:()=>flagInside};
  const objectivePoint=compile(sources.executor,'QRFObjectivePoint');
  check(objectivePoint(flagTarget)===flagPosition,'military tactical objective follows an authored flag offset instead of an unrelated strategic anchor');
  flagInside=false;check(objectivePoint(flagTarget)===strategicCentre,'out-of-bounds flag cannot redirect the canonical response');flagInside=true;
  flagGarrison=null;check(objectivePoint(flagTarget)===strategicCentre,'support-only or legacy locations keep their canonical objective centre');
  // Execute the actual entry guard without invoking downstream waypoint/native services.
  const guard=sources.executor.match(/protected void TacticalInfantryOrders[^]*?\n\t\tAROR_QRFTacticalCommander commander/)[0];
  const guardBody=guard.substring(guard.indexOf('{')+1,guard.lastIndexOf('\n\t\tAROR_QRFTacticalCommander commander'))
    .replace('AROR_GroundOperationForce transport','let transport');
  const allows=new Function('AROR_GroundOperationForce','force','operation',guardBody+'return true;');
  const transport={m_bDismounted:false,m_bTacticalDropStarted:false,ArePassengersOut:()=>false};
  check(!allows(env.AROR_GroundOperationForce,transport,{}),'seated and mixed passenger group cannot receive premature foot orders');
  transport.m_bTacticalDropStarted=true;check(allows(env.AROR_GroundOperationForce,transport,{}),'authorized drop permits early dismount investigation');
  transport.m_bTacticalDropStarted=false;transport.ArePassengersOut=()=>true;
  check(allows(env.AROR_GroundOperationForce,transport,{}),'fully dismounted survivors retain investigation');
  check(allows(env.AROR_GroundOperationForce,null,{}),'standalone infantry retains normal orders');
  return {assertions,note:'Mocked native services; Workbench compile and airport driving/unloading required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
  const sources={ground:fs.readFileSync(path.join(root,'AROR_GroundOperationForce.c'),'utf8'),executor:fs.readFileSync(path.join(root,'AROR_OperationExecutorComponent.c'),'utf8')};
  console.log(JSON.stringify(runQRFIngressTests(sources)));
}
