/* Actual-source road spacing and replicated flag transform checks; native services mocked.
 * Run: node tools/Test-CapturePlacement.js
 */
function runCapturePlacementTests(sources) {
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  const env={LAST_RESORT_ROAD_RADIUS:600,Math:{Min:Math.min,Max:Math.max},vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
    AROR_GroundOperationForce:{Cast:x=>x},Print:()=>{},string:{Format:()=>''},LogLevel:{NORMAL:0,WARNING:1}};
  function compile(source,name,returnOutput=false){
    const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!m)throw Error('Missing '+name);
    const args=m[1].replace(/\bout\s+/g,'').replace(/\b(?:vector|float|int|bool|AROR_\w+)\s+/g,'');
    let body=m[2].replace(/\bfloat\.MAX/g,'Number.MAX_VALUE')
      .replace(/vector (\w+)\[4\];/g,'let $1=[];').replace(/vector (\w+);/g,'let $1=[0,0,0];')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
      .replace(/\b(?:float|int|bool|vector|AROR_\w+)\s+(\w+)/g,'let $1')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
    for(const method of ['TacticalRoadPoint','GetOwner'])body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(', 'g'),'this.'+method+'(');
    if(returnOutput)body=body.replace(/return true;/g,'return {ok:true,point:approach};').replace(/return false;/g,'return {ok:false};');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  const find=compile(sources.executor,'FindTacticalApproach',true);
  let candidates=[[60,0,0]],queries=0;
  const ctx={m_iQRFApproachAttempts:3,m_fQRFSearchStopSpacing:50,m_fQRFObjectiveDeliveryRadius:100,m_aForces:[],
    TacticalRoadPoint(origin,desired,out){queries++;out[0]=desired[0];out[1]=desired[1];out[2]=desired[2];return true;}};
  const commander={m_vTown:[0,0,0],m_fTownRadius:250,m_fMargin:100,ApproachCandidate:(index,origin,distance,attempt)=>candidates[Math.min(attempt,candidates.length-1)]};
  const force={m_iPackageIndex:1,m_sOperationId:'one'};
  const run=()=>find.call(ctx,force,commander,[-300,0,0],60,[0,0,0],0);
  check(run().ok&&queries===1,'preferred free delivery point accepted');
  ctx.m_aForces=[{m_sOperationId:'one',m_vApproachPosition:[50,0,0]}];queries=0;
  candidates=[[60,0,0],[70,0,0],[80,0,0]];
  let result=run();check(result.ok&&result.point[0]===80,'compact fallback chooses largest safe separation');
  check(queries===3,'compact fallback uses existing query budget');
  candidates=[[60,0,0]];check(!run().ok,'ten-metre collision-prone spacing rejected');
  candidates=[[200,0,0]];check(!run().ok,'compact fallback cannot relax town objective bound');
  candidates=[[100,0,0]];queries=0;check(run().ok&&queries===1,'preferred spacing remains first choice');
  ctx.m_aForces[0].m_sOperationId='other';candidates=[[60,0,0]];
  check(run().ok,'unrelated operations do not reserve this response road points');
  const apply=compile(sources.flag,'ApplyPlacement');
  let position=null,updates=0;
  const owner={GetWorldTransform:t=>{t[3]=[10,-50,20];},SetWorldTransform:t=>{position=t[3];},Update:()=>updates++};
  const component={m_bPlaced:false,m_vPlacedPosition:[10,85.1,20],GetOwner:()=>owner};
  apply.call(component);check(position===null,'unreplicated placement not applied');
  component.m_bPlaced=true;apply.call(component);
  check(position[1]===85.1&&updates===1,'flag child world height and static collision committed');
  component.m_vPlacedPosition=[15,86.1,22];apply.call(component);
  check(position[0]===15&&updates===2,'later replicated position applied to flag itself');
  check(sources.flag.includes('if (!Replication.IsServer() || m_bPlaced)'),'only server searches placement');
  check(sources.flag.includes('m_iAttempt < 33'),'flag search has finite candidate limit');
  check(sources.flag.includes('trace.Exclude = GetOwner()'),'flag does not block its own clearance test');
  check(sources.commander.includes('if (attempt >= 4)'),'road alternatives include whole-town ring');
  return {assertions,note:'Native traces, navmesh access, replication and driving still require Workbench verification.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
  const sources={};for(const [key,name]of Object.entries({executor:'Operations/AROR_OperationExecutorComponent',commander:'Operations/AROR_QRFTacticalCommander',flag:'Capture/AROR_CaptureFlagPlacementComponent'}))sources[key]=fs.readFileSync(path.join(root,name+'.c'),'utf8');
  console.log(JSON.stringify(runCapturePlacementTests(sources)));
}
