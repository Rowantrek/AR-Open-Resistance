/* Actual-source ingress regressions with native services mocked.
 * Run: node tools/Test-QRFIngress.js. Does not compile Enforce or certify native routing.
 */
function runQRFIngressTests(sources) {
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  const methods=['GetOwner','IsSeatedInOwnVehicle','TacticalLaunchPoint','TacticalRoadPoint'];
  const env={};
  function compile(source,name) {
    const match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!match)throw Error('Missing '+name);
    let args=match[1].replace(/\bout\s+/g,'').replace(/\b(?:vector|float|int|IEntity|AROR_\w+)\s+/g,'');
    let body=match[2].replace(/array<AIAgent> (\w+) = \{\};/g,'let $1=[];')
      .replace(/new TraceBox\(\)/g,'({})')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
      .replace(/vector (\w+);/g,'let $1=[0,0,0];')
      .replace(/\b(?:bool|float|int|vector|IEntity|BaseWorld|CompartmentAccessComponent|BaseCompartmentSlot|SCR_AIVehicleUsageComponent|AROR_\w+|TraceBox)\s+(\w+)/g,'let $1')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
    for(const method of methods)body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(', 'g'),'this.'+method+'(');
    // V8 does not implement Enforce's vector arithmetic. Represent the bounded
    // candidate offset explicitly; all acceptance decisions remain actual source.
    body=body.replace('seed + Vector(Math.Sin(angle), 0, Math.Cos(angle)) * radius','seed.map((v,i)=>v+[Math.sin(angle)*radius,0,Math.cos(angle)*radius][i])')
      .replace('trace.Start + Vector(0, 0.05, 0)','[trace.Start[0],trace.Start[1]+0.05,trace.Start[2]]')
      .replace('point = road;','point.splice(0,3,...road);'); // Enforce out-vector argument
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  env.CompartmentAccessComponent={Cast:x=>x};
  env.SCR_AIVehicleUsageComponent={Cast:x=>x};
  const vehicle={FindComponent:()=>usage},otherVehicle={};
  let occupant={},inSeat=true,gettingIn=false,gettingOut=false,seatVehicle=vehicle;
  const access={IsInCompartment:()=>inSeat,IsGettingIn:()=>gettingIn,IsGettingOut:()=>gettingOut,
    GetCompartment:()=>({GetVehicle:()=>seatVehicle,GetOccupant:()=>occupant})};
  const person={FindComponent:()=>access};occupant=person;
  const ownSeat=compile(sources.ground,'IsSeatedInOwnVehicle');
  const force={m_Vehicle:vehicle,m_bMountRequested:false,m_CrewGroup:{GetAgents:a=>a.push({GetControlledEntity:()=>person})},
    m_aPassengers:[person],IsSeatedInOwnVehicle:e=>ownSeat.call(force,e)};
  let pilot=person;
  const usage={GetPilotCompartmentSlot:()=>({GetOccupant:()=>pilot})};
  const complete=compile(sources.ground,'IsMountComplete');
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
  Object.assign(env,{GetGame:()=>({GetWorld:()=>world}),Vector:(...v)=>v,TraceFlags:{ENTS:1},
    Math:{Sin:Math.sin,Cos:Math.cos,AbsFloat:Math.abs,sin:Math.sin,cos:Math.cos},vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
    AROR_GroundOperationForce:{Cast:x=>x},Print:()=>{},string:{Format:()=>''},LogLevel:{NORMAL:0}});
  const launch=compile(sources.executor,'FindGroundRoadLaunch');
  const context={m_aForces:[],TacticalLaunchPoint:()=>[100,10,100],TacticalRoadPoint:(origin,desired,out)=>{queries++;out.splice(0,3,...road);return roadOK;}};
  const commander={m_vOrigin:[100,10,100],m_sOriginLocationId:'farm'},point=[0,0,0];
  const run=()=>launch.call(context,commander,0,point);
  check(run()&&point[1]===10&&queries===1,'clear level road pad accepted and grounded');
  queries=0;blocked=true;check(!run()&&queries===12,'obstructed pads rejected within finite road-query budget');blocked=false;
  roadOK=false;check(!run(),'raw outpost position never substitutes for missing road');roadOK=true;
  road=[300,10,100];check(!run(),'road outside origin vicinity rejected');road=[100,10,100];
  water=true;check(!run(),'underwater road candidate rejected');water=false;
  slope=true;check(!run(),'steep pad rejected');slope=false;
  context.m_aForces=[{m_Vehicle:{GetOrigin:()=>[110,10,100]}}];check(!run(),'second vehicle cannot overlap an existing package');
  context.m_aForces=[{m_Vehicle:{GetOrigin:()=>[140,10,100]}}];check(run(),'separated vehicle pad accepted');
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
