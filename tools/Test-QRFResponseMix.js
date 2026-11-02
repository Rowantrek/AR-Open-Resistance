// Actual-source response-mix policy, dispatch accounting, routing and save migration.
// Native services are mocked; this is not Enforce compilation or flight acceptance.
const fs=require('fs'),path=require('path');
const root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
const src=Object.fromEntries(['AROR_QRFResponseMix','AROR_Operation','AROR_OperationManagerComponent','AROR_OperationManagerComponentSerializer','AROR_OperationExecutorComponent','AROR_ReaperQRFExecutor'].map(n=>[n,fs.readFileSync(path.join(root,n+'.c'),'utf8')]));
let assertions=0,server=true;
const check=(ok,label)=>{assertions++;if(!ok)throw Error(label)};
for(const[n,fn]of Object.entries({Count(){return this.length},Insert(x){this.push(x)},IsEmpty(){return !this.length},Contains(x){return this.includes(x)}}))Object.defineProperty(Array.prototype,n,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
const env={Math:{Round:Math.round,Max:Math.max,Min:Math.min},Replication:{IsServer:()=>server},Print(){},LogLevel:{NORMAL:0},
 AROR_EOperationState:{PLANNED:0,SPAWNING:1,ACTIVE:2,SUCCEEDED:3,FAILED:4,CANCELLED:5},AROR_EOperationType:{ATTACK:0,QRF:1},
 AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},AROR_EForceType:{AIRBORNE:3},AROR_EResponseType:{AIR_GROUND:7},
 AROR_ReaperAirForce:{Cast:x=>x?.air?x:null},AROR_OperationManagerComponent:{Cast:x=>x},ESerializeResult:{OK:1},VERSION:4,
 string:{Format:(s,...args)=>s.replace(/%(\d+)/g,(_,i)=>args[i-1])}};
const names=new Set(Object.values(src).flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])));
function compile(file,name){
 const match=src[file].match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
 if(!match)throw Error('Missing '+name);
 const args=match[1].replace(/\b(?:notnull |const )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
 let body=match[2].replace(/\bconst\s+(?=AROR_)/g,'').replace(/\barray<[^>]+>\s+(\w+)\s*=\s*\{\s*\};/g,'let $1=[];')
  .replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^)]+)\)/g,'for(const $1 of $2)')
  .replace(/\b(?:bool|int|float|string|IEntity|GenericComponent|AROR_\w+)\s+(\w+)/g,'let $1')
  .replace(/let (\w+);/g,'let $1=0;').replace(/(?<![.\w])[ms]_\w+/g,x=>'this.'+x)
  .replace(/context\.ReadValue\("([^"]+)", (\w+)\)/g,'(($2=context.values["$1"])!==undefined)')
  .replace(/context\.ReadValue\((\w+), (\w+)\)/g,'(($2=context.values[$1])!==undefined)')
  .replace(/super\.(\w+)\(/g,'this.Base$1(');
 for(const name of names)body=body.replace(new RegExp('(?<![.\\w])'+name+'\\(','g'),'this.'+name+'(');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
const managerFile='AROR_OperationManagerComponent',executorFile='AROR_OperationExecutorComponent',reaperFile='AROR_ReaperQRFExecutor';
env.AROR_QRFResponseMix={};
for(const name of ['CountsValid','PreferAir','IsCampaignQRF'])env.AROR_QRFResponseMix[name]=compile('AROR_QRFResponseMix',name);
function manager(){
 const m={m_iAirborneQRFShare:30,m_iOccupierQRFDispatches:0,m_iOccupierAirDispatches:0,m_iInvaderQRFDispatches:0,m_iInvaderAirDispatches:0,m_iClock:10,
  IsEnemyRole:r=>r===1||r===2,NotifyChanged(){},GetReadinessPressure:()=>0,GetCrackdownForSave:()=>0,GetQRFFinance:()=>({Debt:()=>0,PendingLossValue:()=>0}),RestoreQRFStrategy_S(){this.restored=true;}};
 for(const name of ['GetQRFDispatchCount','PreferAirQRF','RestoreQRFResponseMix_S','RecordQRFDispatchMix_S','ChangeState_S'])m[name]=compile(managerFile,name);
 m.s_Instance=m;return m;
}
function operation(role=1){return {m_bDebugRequest:false,m_bFundingWaived:false,m_sRequestedProfileId:'',m_sRequestedRecipeId:'',m_iResponseMixBucket:-1,
 m_bResponseMixTracked:true,m_bResponseMixCounted:false,m_bResponseMixAirborne:false,m_eState:1,m_iRequestedTier:0,m_iPoolReadinessLevel:0,
 GetRole:()=>role,GetType:()=>1,GetId:()=> 'qrf',GetTargetLocationId:()=> 'target',GetState(){return this.m_eState;},IsTerminal(){return this.m_eState>=3;},
 Transition_S(state){if(this.m_eState>=3)return false;this.m_eState=state;return true;}}}
// Both armies maintain independent nearest-attainable running shares.
for(const percent of [0,30,100]){
 const m=manager();m.m_iAirborneQRFShare=percent;
 for(const role of [1,2])for(let n=1;n<=100;n++){
  const o=operation(role);o.m_bResponseMixAirborne=m.PreferAirQRF(role);m.ChangeState_S(o,2,'dispatch');
  check(m.GetQRFDispatchCount(role)===n,'only one unit of history per complete QRF, not per vehicle');
  check(Math.abs(m.GetQRFDispatchCount(role,true)-n*percent/100)<=0.500001,'running share stays at nearest attainable quota');
  m.RecordQRFDispatchMix_S(o);check(m.GetQRFDispatchCount(role)===n,'duplicate ACTIVE handling cannot double-count');
 }
 check(m.GetQRFDispatchCount(1,true)===percent&&m.GetQRFDispatchCount(2,true)===percent,'long-run split is exact and faction-local');
}
for(const [label,change]of [['debug',o=>o.m_bDebugRequest=true],['scripted',o=>o.m_bFundingWaived=true],['explicit profile',o=>o.m_sRequestedProfileId='air'],['explicit recipe',o=>o.m_sRequestedRecipeId='joint'],['attack',o=>o.GetType=()=>0],['untracked',o=>o.m_bResponseMixTracked=false]]){
 const m=manager(),o=operation();change(o);m.ChangeState_S(o,2,'manual');check(m.GetQRFDispatchCount(1)===0,'exclude '+label+' from campaign share');
}
{
 const m=manager(),failed=operation();m.ChangeState_S(failed,4,'failed before deployment');check(m.GetQRFDispatchCount(1)===0,'failed preparation never spends quota');
 const o=operation();m.NotifyChanged=op=>{if(op.GetState()===2)m.ChangeState_S(op,3,'nested terminal');};m.ChangeState_S(o,2,'dispatch');
 check(m.GetQRFDispatchCount(1)===1&&o.IsTerminal(),'record dispatch before reentrant terminal subscribers');
 server=false;m.RecordQRFDispatchMix_S(operation());check(m.GetQRFDispatchCount(1)===1,'clients cannot change history');server=true;
}
// Persist the aggregate counts; old saves begin an empty mix without discarding escalation.
const serialize=compile('AROR_OperationManagerComponentSerializer','Serialize'),deserialize=compile('AROR_OperationManagerComponentSerializer','Deserialize');
{
 const m=manager();m.RestoreQRFResponseMix_S(10,3,20,6);const saved={};serialize(null,m,{WriteValue:(key,value)=>saved[key]=value});
 const loaded=manager();check(deserialize(null,loaded,{values:saved})&&loaded.GetQRFDispatchCount(2,true)===6&&loaded.GetQRFDispatchCount(1)===10,'version 4 retains both army totals and air/joint counts');
 for(const version of [1,2,3]){
  const old={...saved,version,occupierAggression:0,invaderAggression:0};for(const key of ['occupierQRFDispatches','occupierAirDispatches','invaderQRFDispatches','invaderAirDispatches'])delete old[key];
  const migrated=manager();check(deserialize(null,migrated,{values:old})&&migrated.restored&&migrated.GetQRFDispatchCount(1)===0,'older saves migrate without invented dispatch history');
 }
 for(const change of [s=>delete s.invaderAirDispatches,s=>s.occupierAirDispatches=11,s=>s.invaderQRFDispatches=-1,s=>s.occupierQRFDispatches=1000000001]){
  const bad={...saved};change(bad);const safe=manager();check(!deserialize(null,safe,{values:bad})&&!safe.restored,'malformed mix rejects before applying any campaign state');
 }
}
// Actual backend routing: choose one bucket, honor feasibility, and never convert committed assets.
env.AROR_CommanderManagerComponent={GetInstance:()=>({GetCommander:()=>({GetFactionKey:()=> 'ARMY'})})};
const route=compile(reaperFile,'StartMixedCampaignQRF'),fallback=compile(reaperFile,'FallbackMixedQRFToGround');
function router(preferAir){
 const calls=[];const ctx={m_Operations:{PreferAirQRF:()=>preferAir,GetCrackdownLevel:()=>2,GetReadinessLevel:()=>2},m_aForces:[],m_bAllowAirborneResponses:true,
  m_sReaperPlanningOperation:'',FindForce:()=>null,ReaperSettingsValid:()=>true,HasMarsOrigin_S:()=>true,QRFPoolReadinessLevel:compile(executorFile,'QRFPoolReadinessLevel'),
  m_aForceProfiles:[{m_bEnabled:true,m_eType:3,m_eRole:1,m_sFactionKey:'ARMY',IsValid:()=>true,GetWeight:()=>1}],
  FindStrategicGroundRecipe:()=>({}),AffordableReaperTier:()=>2,FallbackMixedQRFToGround:fallback,ReportSpawnWait(){},
  StartGroundQRFRecipe:o=>calls.push(o.m_iResponseMixBucket===1?'joint':'ground'),StartReaperQRF:()=>calls.push('air'),BaseStartForce:()=>calls.push('ground')};
 return {ctx,calls};
}
for(const [label,prefer,change,expected]of [
 ['ground share',false,()=>{},'ground'],['joint share',true,()=>{},'joint'],['standalone air',true,c=>c.FindStrategicGroundRecipe=()=>null,'air'],
 ['no funds',true,c=>{c.FindStrategicGroundRecipe=()=>null;c.AffordableReaperTier=()=>0;},'ground'],['adapter disabled',true,c=>c.m_bAllowAirborneResponses=false,'ground'],
 ['airspace occupied',true,c=>c.m_aForces=[{air:true}],'ground'],['no origin',true,c=>c.HasMarsOrigin_S=()=>false,'ground']]){
 const {ctx,calls}=router(prefer),o=operation();change(ctx);route.call(ctx,o);check(calls.join(',')===expected,'route '+label+' through correct backend');
 check(o.m_bResponseMixTracked&&!o.m_bResponseMixCounted,'selection alone cannot increment dispatched history');
}
{
 const {ctx,calls}=router(true),o=operation();o.m_Funding={};o.m_iResponseMixBucket=1;route.call(ctx,o);check(!calls.length&&o.m_iResponseMixBucket===1,'already funded backend cannot be replaced');
 delete o.m_Funding;ctx.m_sReaperPlanningOperation=o.GetId();ctx.m_ReaperLandingSearch={};check(fallback.call(ctx,o,'no LZ')&&o.m_iResponseMixBucket===0&&ctx.m_ReaperLandingSearch===null,'pre-spawn LZ failure releases planning ownership for ground fallback');
 route.call(ctx,o);check(calls.join(',')==='ground','ground fallback stays frozen instead of retrying impossible air every tick');
 o.m_iResponseMixBucket=1;ctx.FindForce=()=>({});check(!fallback.call(ctx,o,'unsafe'),'materialized force cannot be abandoned by bucket fallback');
}
// Existing recipe selector must enforce both buckets without changing unrestricted previews.
{
 const start=compile(reaperFile,'StartForce'),{ctx,calls}=router(true),o=operation();
 o.m_bResponseMixTracked=false;ctx.m_Operations.IsQRFStrategyEnabled=()=>true;ctx.StartMixedCampaignQRF=op=>route.call(ctx,op);ctx.GetGroundQRFRecipeId=()=> 'fixture';
 start.call(ctx,o);check(calls.join(',')==='joint'&&o.m_bResponseMixTracked,'production StartForce enters the mix before the fixed ground recipe path');
 const explicit=operation();explicit.m_bResponseMixTracked=false;explicit.m_sRequestedRecipeId='manual';start.call(ctx,explicit);
 check(!explicit.m_bResponseMixTracked,'explicit requests bypass the production mix hook');
}
// Existing recipe selector must enforce both buckets without changing unrestricted previews.
{
 env.AROR_ResponsePolicy={IsRecipeEligible:r=>!!r};
 const ground={m_iLevel:2,m_eResponseType:5,Resolve:(p,r,f,out)=>{out.push(profile);return true;}},joint={...ground,m_eResponseType:7};
 const profile={ReplacementQuote:()=>100,GetGroupBudget:()=>2,GetVehicleBudget:()=>1,m_iPersonnelBudget:8};
 const ctx={m_aGroundQRFRecipes:[ground,joint],m_aForceProfiles:[profile],m_bAllowJointResponses:true,m_bAllowAirborneResponses:true,m_bQRFTacticalCommander:true,
  m_Operations:{GetReadinessLevel:()=>2,GetQRFFinance:()=>({Available:()=>10000,Debt:()=>0}),QRFOverhead:()=>60},GetReservedGroups:()=>0,GetReservedMembers:()=>0,GetReservedVehicles:()=>0,m_iMaxGroups:20,m_iMaxMembers:160,m_iMaxVehicles:10};
 const find=compile(executorFile,'FindStrategicGroundRecipe');
 check(find.call(ctx,1,'ARMY',2,false,2,0)===ground,'ground bucket cannot inherit unconditional joint preference');
 check(find.call(ctx,1,'ARMY',2,false,2,1)===joint,'air bucket can use a funded joint recipe');
}
console.log(JSON.stringify({assertions,note:'Actual-source mix, routing, accounting and save checks; native execution remains mocked.'}));
