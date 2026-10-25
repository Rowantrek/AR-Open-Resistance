/* Actual-source town hold and stalled-route policy, with native services mocked.
 * Run: node tools/Test-QRFTownHold.js
 */
function runQRFTownHoldTests(sources) {
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  const env={Math:{Max:Math.max,Min:Math.min},vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
    Print:()=>{},string:{Format:()=>''},LogLevel:{WARNING:1},AROR_EOperationType:{QRF:1}};
  function compile(source,name){
    const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!m)throw Error('Missing '+name);
    let body=m[2].replace(/vector next;/g,'let next=[0,0,0];')
      .replace(/\b(?:int|bool|float|vector)\s+(\w+)/g,'let $1')
      .replace(/let (\w+);/g,'let $1=0;')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/(?<![.\w])(FindTacticalApproach|ReplaceTacticalWaypoint)\(/g,'this.$1(');
    const args=m[1].replace(/\b(?:int|bool|float|AROR_\w+)\s+/g,'');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  const begin=compile(sources.commander,'BeginTownHold'),withdraw=compile(sources.commander,'ShouldWithdraw');
  const c={m_iTownHoldSeconds:900,m_iTownHoldStarted:-1,m_iFirstDispatch:0,m_bWithdraw:false,m_iMissionSeconds:600,
    m_aInitial:[20],m_aLiving:[1],m_fWithdrawFraction:0.4};
  check(!withdraw.call(c,601,false),'old mission timer cannot truncate fixed hold');
  check(begin.call(c,700),'first town arrival starts hold');
  check(!begin.call(c,800)&&c.m_iTownHoldStarted===700,'later arrivals do not restart operation hold');
  check(!withdraw.call(c,1599,false),'remain for complete 900 seconds despite casualty retreat threshold');
  check(withdraw.call(c,1600,false),'withdraw at hold expiry');
  c.m_bWithdraw=false;c.m_iTownHoldSeconds=0;c.m_iTownHoldStarted=-1;
  check(!begin.call(c,1),'disabled fixed hold does not start');
  check(withdraw.call(c,601,true),'legacy duration restored when disabled');
  c.m_bWithdraw=true;c.m_iTownHoldSeconds=900;check(withdraw.call(c,0,true),'explicit safety withdrawal respected');
  const timeout=compile(sources.manager,'ActiveTimeoutFor');
  const op={GetType:()=>1,m_TacticalCommander:{m_iTownHoldSeconds:900}};
  check(timeout.call({m_iActiveTimeout:900},op)===2700,'manager allows ingress plus full hold plus withdrawal');
  op.GetType=()=>0;check(timeout.call({m_iActiveTimeout:900},op)===900,'unrelated attacks retain timeout');
  const route=compile(sources.executor,'RecoverStalledQRFRoute');
  let queries=0,orders=0;
  const ctx={m_iExecutorClock:0,m_fQRFObjectiveDeliveryRadius:100,m_sMoveWaypointPrefab:'move',
    FindTacticalApproach(f,c,p,d,out,offset){queries++;out[0]=60;out[1]=0;out[2]=0;return true;},
    ReplaceTacticalWaypoint(){orders++;return true;}};
  const force={RestoreNearbyDriver_S:()=>false,m_iTacticalNextRouteCheck:0,m_iTacticalRouteRetries:0,m_vTacticalProgressPosition:[0,0,0],
    m_Vehicle:{GetOrigin:()=>[0,0,0]},m_iPackageIndex:0,m_sOperationId:'test'};
  route.call(ctx,force,{});check(queries===1&&orders===1,'stalled route gets a replacement road objective');
  check(force.m_vApproachPosition[0]===60,'new road objective tracked');
  ctx.m_iExecutorClock=1;route.call(ctx,force,{});check(queries===1,'no per-frame route searches');
  for(let now=30;now<=300;now+=30){ctx.m_iExecutorClock=now;route.call(ctx,force,{});}
  check(queries===4&&orders===4,'route retries bounded per package');
  force.m_iTacticalRouteRetries=0;force.m_iTacticalNextRouteCheck=0;force.m_Vehicle.GetOrigin=()=>[40,0,0];
  route.call(ctx,force,{});check(queries===4,'moving package not repeatedly replanned');
  check(sources.executor.includes('force.m_iTacticalExitStarted'),'unload recovery age survives repeated deadlines');
  return {assertions,note:'Native navigation, dismounting and town occupation still require Play verification.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
  const names={commander:'AROR_QRFTacticalCommander',executor:'AROR_OperationExecutorComponent',manager:'AROR_OperationManagerComponent'};
  const sources={};for(const [key,name]of Object.entries(names))sources[key]=fs.readFileSync(path.join(root,name+'.c'),'utf8');
  console.log(JSON.stringify(runQRFTownHoldTests(sources)));
}
