/* Two-axis QRF progression against actual source methods and Chicken recipes.
 * Native actors, replication and save services are mocked. Run with Node where installed.
 */
function runQRFProgressionTests(sources) {
  let assertions=0,server=true,bumps=0;
  const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
  for(const [name,fn]of Object.entries({Insert(x){this.push(x);},Clear(){this.length=0;},Contains(x){return this.includes(x);},Count(){return this.length;},IsEmpty(){return !this.length;}}))
    Object.defineProperty(Array.prototype,name,{value:fn,configurable:true});
  Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length;},configurable:true});
  const roles={RESISTANCE:0,OCCUPIERS:1,INVADERS:2},types={MOTORIZED:0,MECHANIZED:1,ARMOUR:2,AIRBORNE:3,INFANTRY:4};
  const env={Math:{Clamp:(x,a,b)=>Math.max(a,Math.min(b,x)),Min:Math.min,Max:Math.max,Floor:Math.floor,Round:Math.round},
    AROR_EFactionRole:roles,AROR_EForceType:types,AROR_EOperationType:{ATTACK:0,QRF:1},AROR_ELocationType:{TOWN:0,CITY:3,AIRPORT:2,FACTORY:4,MILITARY_BASE:5,PORT:6,RESOURCES:7,OUTPOST:8},
    Replication:{IsServer:()=>server,BumpMe:()=>bumps++},Print(){},LogLevel:{NORMAL:0},string:{Empty:'',Format:(s,...a)=>s.replace(/%([1-9])/g,(_,n)=>a[n-1])},
    AROR_OperationManagerComponent:{Cast:x=>x},ESerializeResult:{OK:1},VERSION:Number(sources.serializer.match(/VERSION\s*=\s*(\d+)/)[1]),
    AROR_LocationManagerComponent:{GetInstance:()=>({GetLocationById:()=>({GetLocationType:()=>8})})}};
  env.AROR_EResponseType={AIR_GROUND:7};
  const names=['PressureForLevel','ReadinessLevel','ResponseLevel','Importance','GetReadinessPressure','GetReadinessLevel','GetCrackdown','GetCrackdownLevel','GetCrackdownPercent','HasThreatState',
    'IsEnemyRole','GetQRFDispatchCount','RestoreQRFResponseMix_S','SetReadinessPressure_S','SetReadinessLevel_S','SetCrackdown_S','RestoreQRFStrategy_S','RecordStrategicEvidence','RecordCaptureEscalation_S','UpdateCrackdown_S','GetCrackdownForSave',
    'IsValid','AllowsReadinessLevel','HasNativeExecutor','GetWeight','GetGroupBudget','GetVehicleBudget','VehicleLossValue','ReplacementQuote','IsReady','HasLocation','Resolve',
    'FindStrategicGroundRecipe','QRFPoolReadinessLevel','GetReservedGroups','GetReservedMembers','GetReservedVehicles','GetSpottingKind'];
  function compile(source,name){
    const match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
    if(!match)throw Error('Missing production method '+name);
    const args=match[1].replace(/\b(?:notnull |const )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
    let body=match[2].replace(/\b(?:const )?array<[^>]+>\s+(\w+)\s*=\s*\{\s*\};/g,'let $1=[];')
      .replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^\)]+)\)/g,'for(let $1 of $2)')
      .replace(/\b(?:const )?(?:bool|int|float|string|IEntity|AROR_\w+)\s+(\w+)/g,'let $1')
      .replace(/let (\w+);/g,'let $1=0;').replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/context\.ReadValue\("([^"]+)", (\w+)\)/g,'(($2=context.values["$1"])!==undefined)')
      .replace(/context\.ReadValue\((\w+), (\w+)\)/g,'(($2=context.values[$1])!==undefined)');
    // Parent engine hooks and the shared timer are mocked; retain the actual startup policy.
    if(name==='OnPostInit')body=body.replace(/super\.OnPostInit\(owner\);/,'').replace(/(?<![.\w])s_Instance/g,'this.s_Instance');
    for(const n of names)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
  }
  const policy={};env.AROR_QRFStrategicPolicy=policy;
  env.AROR_QRFResponseMix={CountsValid:compile(sources.mix,'CountsValid')};
  env.AROR_ResponsePolicy={};
  for(const name of ['IsRecipeEligible','IsProfileUnlocked'])env.AROR_ResponsePolicy[name]=compile(sources.responsePolicy,name);
  for(const name of ['PressureForLevel','ReadinessLevel','ResponseLevel','Importance','VehicleValue','LossCrackdown'])policy[name]=compile(sources.policy,name);
  const makeManager=()=>{
    const manager={m_iClock:1000,m_bQRFStrategicEconomy:true,m_bInvaderOperationsEnabled:true,m_bThreatStateReady:false,
      m_iOccupierQRFDispatches:0,m_iOccupierAirDispatches:0,m_iInvaderQRFDispatches:0,m_iInvaderAirDispatches:0,
      m_iOccupierPressure:0,m_iInvaderPressure:0,m_iOccupierCrackdown:200,m_iInvaderCrackdown:200,
      m_iOccupierEvidenceAt:0,m_iInvaderEvidenceAt:0,m_iOccupierLastThreat:1000,m_iInvaderLastThreat:1000,
      m_iCaptureReadinessPressure:25,m_iCaptureCrackdown:150,m_iCrackdownQuietTime:600,m_iCrackdownDecay:20,m_iNextCrackdownDecay:0,
      m_iAppliedOccupierLoss:0,m_iAppliedInvaderLoss:0,m_aOperations:[],m_aQRFThreats:[],
      m_QRFFinance:{m_iOccupierLossValue:0,m_iInvaderLossValue:0,debts:{1:0,2:0},pending:{1:0,2:0},PendingLossValue(r){return this.pending[r]||0;},SetDebt_S(r,n){this.debts[r]=n;}},IsReady:()=>server,HasLocation:()=>true};
    for(const name of ['IsEnemyRole','GetQRFDispatchCount','RestoreQRFResponseMix_S','GetReadinessPressure','GetReadinessLevel','GetCrackdown','GetCrackdownLevel','GetCrackdownPercent','HasThreatState','SetReadinessPressure_S','SetReadinessLevel_S','SetCrackdown_S',
      'RestoreQRFStrategy_S','RecordStrategicEvidence','RecordCaptureEscalation_S','UpdateCrackdown_S','GetCrackdownForSave'])manager[name]=compile(sources.manager,name);
    return manager;
  };
  const manager=makeManager();
  manager.SetReadinessLevel_S(1,5);check(manager.GetReadinessLevel(1)===5&&manager.GetCrackdownLevel(1)===2,'readiness changes equipment progress without changing response intensity');
  manager.SetCrackdown_S(1,1000);check(manager.GetReadinessLevel(1)===5&&manager.GetCrackdownLevel(1)===5,'crackdown changes intensity independently');
  check(manager.GetReadinessLevel(2)===1&&manager.GetCrackdownLevel(2)===2,'both faction states remain independent');
  for(const pressure of [0,199,200,399,400,599,600,799,800,1000]){
    manager.SetCrackdown_S(2,pressure);
    check(manager.GetCrackdownLevel(2)===Math.min(5,1+Math.floor(manager.GetCrackdownPercent(2)/20)),'displayed crackdown thresholds agree with the response level');
  }
  const before=bumps;manager.SetCrackdown_S(1,1000);check(bumps===before,'unchanged crackdown does not create redundant replication');
  manager.SetCrackdown_S(0,1000);check(bumps===before&&manager.GetCrackdown(0)===0,'unsupported roles cannot acquire enemy crackdown or cause replication');
  manager.SetCrackdown_S(1,-5);manager.SetCrackdown_S(2,2000);check(manager.GetCrackdown(1)===0&&manager.GetCrackdown(2)===1000,'trusted crackdown writes are bounded');
  const clocked=makeManager();clocked.SetReadinessLevel_S(1,3);const persistentReadiness=clocked.GetReadinessPressure(1);
  clocked.RecordStrategicEvidence(1);clocked.RecordStrategicEvidence(1);
  check(clocked.GetCrackdown(1)===205&&clocked.GetReadinessPressure(1)===persistentReadiness,'combat evidence is throttled and cannot unlock equipment');
  clocked.m_iClock+=30;clocked.RecordStrategicEvidence(1);check(clocked.GetCrackdown(1)===210,'fresh combat evidence may increase crackdown after its throttle');
  clocked.m_iClock+=599;clocked.UpdateCrackdown_S();check(clocked.GetCrackdown(1)===210,'quiet delay protects recent crackdown');
  clocked.m_iClock+=61;clocked.UpdateCrackdown_S();check(clocked.GetCrackdown(1)===190&&clocked.GetReadinessPressure(1)===persistentReadiness,'quiet time cools crackdown without losing equipment progress');
  clocked.UpdateCrackdown_S();check(clocked.GetCrackdown(1)===190,'cooling is paced at most once per minute');
  clocked.m_QRFFinance.m_iOccupierLossValue=500;clocked.UpdateCrackdown_S();
  check(clocked.GetCrackdown(1)===195&&clocked.GetReadinessPressure(1)===persistentReadiness,'confirmed QRF losses affect crackdown, never equipment unlocks');
  clocked.UpdateCrackdown_S();check(clocked.GetCrackdown(1)===195,'a confirmed loss contributes crackdown once');
  clocked.m_QRFFinance.pending[1]=2999;clocked.m_QRFFinance.m_iOccupierLossValue+=100;
  const beforeSnapshot=bumps;
  check(clocked.GetCrackdownForSave(1)===225&&clocked.GetCrackdown(1)===195&&bumps===beforeSnapshot,'save includes bounded pending/unapplied loss crackdown without changing live state');
  clocked.m_iAppliedOccupierLoss=clocked.m_QRFFinance.m_iOccupierLossValue;clocked.m_QRFFinance.pending[1]=0;
  check(clocked.GetCrackdownForSave(1)===195&&clocked.GetCrackdownForSave(2)===clocked.GetCrackdown(2),'settled losses cannot be added twice or credited to the other faction');
  check(policy.LossCrackdown(2999)===29&&policy.LossCrackdown(0)===0,'loss crackdown uses explicit integer bounds');
  env.AROR_QRFThreat=function(){Object.assign(this,{m_iReports:0,m_iNextEvidence:0,m_bCaptured:false});};
  const queue=compile(sources.manager,'QueueQRFThreat_S');
  const capture=makeManager();queue.call(capture,1,'outpost',true);
  check(capture.GetReadinessPressure(1)===25&&capture.GetCrackdown(1)===355,'one capture increments slow readiness progress and faster crackdown separately');
  queue.call(capture,1,'outpost',true);check(capture.GetReadinessPressure(1)===25&&capture.GetCrackdown(1)===355,'duplicate queued capture cannot farm escalation');
  queue.call(capture,1,'another-outpost',true);check(capture.GetReadinessPressure(1)===50&&capture.GetCrackdown(1)===505,'a distinct capture can raise response intensity while readiness remains early');
  const existing={IsTerminal:()=>false,GetRole:()=>1,GetType:()=>1,GetTargetLocationId:()=> 'active-site',GetId:()=> 'active',m_bCaptureThreatRecorded:false};
  capture.m_aOperations=[existing];queue.call(capture,1,'active-site',true);const capturePressure=capture.GetReadinessPressure(1);
  queue.call(capture,1,'active-site',true);check(capture.GetReadinessPressure(1)===capturePressure&&existing.m_bCaptureThreatRecorded,'active response records a capture escalation only once');
  const restored=makeManager();restored.RestoreQRFStrategy_S(450,100,20,30,10,900);
  check(restored.GetReadinessLevel(1)===4&&restored.GetCrackdownLevel(1)===1&&restored.GetReadinessLevel(2)===2&&restored.GetCrackdownLevel(2)===5,'restore retains independently opposed equipment and intensity values');
  server=false;const replicated=bumps;restored.SetCrackdown_S(1,900);restored.SetReadinessLevel_S(2,5);restored.RestoreQRFStrategy_S(0,0,0,0,0,0);restored.UpdateCrackdown_S();
  check(restored.GetCrackdown(1)===10&&restored.GetReadinessLevel(2)===2&&bumps===replicated,'clients cannot write, restore or cool progression');server=true;
  const serialize=compile(sources.serializer,'Serialize'),deserialize=compile(sources.serializer,'Deserialize');
  const saved={};restored.GetQRFFinance=()=>({Debt:r=>r*10,PendingLossValue:r=>r});
  serialize(null,restored,{WriteValue:(k,v)=>saved[k]=v});const roundTrip=makeManager();
  check(deserialize(null,roundTrip,{values:saved})&&roundTrip.GetReadinessLevel(1)===4&&roundTrip.GetCrackdown(1)===10&&roundTrip.m_QRFFinance.debts[2]===22,'version 3 round-trip retains both axes and pending liabilities');
  const legacy={...saved,version:1};delete legacy.occupierCrackdown;delete legacy.invaderCrackdown;
  const migrated=makeManager();check(deserialize(null,migrated,{values:legacy})&&migrated.GetReadinessPressure(1)===450&&migrated.GetCrackdown(1)===450,'version 1 migrates the old shared pressure without resetting campaign progress');
  const partial={...saved};delete partial.invaderCrackdown;const safe=makeManager();
  check(!deserialize(null,safe,{values:partial})&&safe.GetReadinessPressure(1)===0,'partial version 3 cannot apply half a restore');
  check(!deserialize(null,safe,{values:{...saved,version:5}}),'future save formats are rejected');
  const aggressionSave={...saved,version:2,occupierAggression:saved.occupierCrackdown,invaderAggression:saved.invaderCrackdown};delete aggressionSave.occupierCrackdown;delete aggressionSave.invaderCrackdown;
  const renamed=makeManager();check(deserialize(null,renamed,{values:aggressionSave})&&renamed.GetCrackdown(1)===10&&renamed.GetReadinessLevel(1)===4,'version 2 saves load their old aggression keys as crackdown');
  check(!deserialize(null,safe,{values:{...saved,occupierCrackdown:-1}}),'malformed new crackdown cannot silently invoke legacy migration');
  let scheduled=0;env.GetGame=()=>({InPlayMode:()=>true,GetCallqueue:()=>({CallLater(){scheduled++;}})});env.Tick=()=>{};
  const init=compile(sources.manager,'OnPostInit');
  const defaults=()=>Object.fromEntries([...sources.manager.matchAll(/\[Attribute\("([\d.]+)",[^\r\n]*\)\]\s*(?:protected )?(int|float|bool) (m_\w+)/g)].map(m=>[m[3],Number(m[1])]));
  const fresh=Object.assign(makeManager(),defaults());init.call(fresh,{});
  check(scheduled===1&&fresh.HasThreatState()&&fresh.GetReadinessLevel(1)===1&&fresh.GetCrackdown(1)===0&&fresh.GetCrackdownLevel(1)===1,'real component defaults start new campaigns at readiness one and response tier one');
  for(const role of [1,2]){
    const paced=Object.assign(makeManager(),defaults());paced.SetCrackdown_S(1,0);paced.SetCrackdown_S(2,0);
    for(let captures=1;captures<=20;captures++){
      paced.RecordCaptureEscalation_S(role);
      check(paced.GetCrackdownLevel(role)===Math.min(5,1+Math.floor(captures/4)),'capture-only intensity advances one tier every four losses: '+[role,captures]);
      check(paced.GetReadinessPressure(role)===captures*25,'linear intensity retains independent equipment progression');
      check(paced.GetCrackdown(role===1?2:1)===0,'one army capture escalation cannot raise the other army');
    }
  }
  const loaded=Object.assign(makeManager(),defaults());loaded.RestoreQRFStrategy_S(700,250,10,20,50,800);init.call(loaded,{});
  check(scheduled===2&&loaded.GetReadinessLevel(1)===5&&loaded.GetCrackdown(1)===50&&loaded.GetReadinessLevel(2)===3&&loaded.GetCrackdown(2)===800,'startup cannot overwrite restored axes with new scenario defaults');
  const invalid=Object.assign(makeManager(),defaults(),{m_iStartingCrackdown:101});init.call(invalid,{});
  check(scheduled===2&&!invalid.HasThreatState()&&!invalid.s_Instance,'invalid crackdown configuration disables startup without scheduling a partially initialized manager');
  const duplicate=Object.assign(makeManager(),defaults(),{s_Instance:fresh});init.call(duplicate,{});
  check(scheduled===2&&!duplicate.HasThreatState(),'duplicate scheduler cannot initialize another progression authority');
  const clientInit=Object.assign(makeManager(),defaults());server=false;init.call(clientInit,{});server=true;
  check(scheduled===2&&!clientInit.HasThreatState(),'client startup cannot seed authoritative progression');
  function fields(block){const result={};for(const m of block.matchAll(/^\s+(m_\w+)\s+(?:"([^"\r\n]*)"|(-?[\d.]+))\s*$/gm))result[m[1]]=m[2]??Number(m[3]);return result;}
  const profiles=[...sources.layer.matchAll(/^ {4}AROR_ForceProfile \{\r?\n([\s\S]*?)^ {4}\}/gm)].map(m=>{
    const p={m_iMinimumReadinessLevel:1,m_iEscortCount:1,m_iEscortLossValue:2000,m_iPersonnelLossValue:25,...fields(m[1])};
    for(const n of ['IsValid','HasNativeExecutor','GetWeight','GetGroupBudget','GetVehicleBudget','VehicleLossValue','ReplacementQuote','GetSpottingKind'])p[n]=compile(sources.profile,n);return p;
  });
  const recipes=[...sources.layer.matchAll(/^ {4}AROR_GroundQRFRecipe \{\r?\n([\s\S]*?)^ {4}\}/gm)].map(m=>{
    const r={m_iMinimumReadinessLevel:1,m_iMaximumReadinessLevel:5,...fields(m[1]),m_aProfileIds:[...m[1].match(/m_aProfileIds\s*\{([^}]*)\}/)[1].matchAll(/"([^"]+)"/g)].map(v=>v[1])};
    for(const n of ['IsValid','AllowsReadinessLevel','Resolve'])r[n]=compile(sources.recipe,n);return r;
  });
  check(profiles.length===12&&recipes.length===62,'Chicken contains light/escorted air profiles and complete ground/joint readiness bands');
  check(/m_iStartingCrackdown\s+0\b/.test(sources.layer)&&/m_iCaptureCrackdown\s+50\b/.test(sources.layer),'Chicken overrides agree with the tier-one linear pacing defaults');
  for(const role of [1,2])for(const band of ['light','mechanized','ground','joint_r2','joint_r3','joint']){
    const prefix=(role===1?'occupier':'invader')+'_'+band+'_l';let lastPersonnel=0,lastCollateral=0;
    for(let tier=1;tier<=5;tier++){
      const recipe=recipes.find(r=>r.m_eRole===role&&r.m_sId===prefix+tier),elements=[];
      check(recipe&&recipe.Resolve(profiles,role,role===1?'AROR_OCCUPIERS':'AROR_INVADERS',elements,recipe.m_iMinimumReadinessLevel),'each army has a complete linear recipe band');
      check(elements.length===tier+(['ground','joint_r2','joint_r3','joint'].includes(band)?1:0),'each successive tier adds exactly one complete package: '+[role,band,tier]);
      const personnel=elements.reduce((sum,p)=>sum+p.m_iPersonnelBudget,0),collateral=elements.reduce((sum,p)=>sum+p.ReplacementQuote(),0);
      check(personnel>lastPersonnel&&collateral>lastCollateral,'each tier mounts in funded strength rather than shrinking or merely relabelling a force');
      check(elements.filter(p=>p.m_eType!==3&&p.m_eType!==4).every(p=>p.m_bFillPassengerSeats),'linear tiers keep full-load ground profiles');
      lastPersonnel=personnel;lastCollateral=collateral;
    }
  }
  check(profiles.every(p=>p.IsValid())&&recipes.every(r=>r.IsValid()),'every authored progression profile and recipe passes production configuration validation');
  check(profiles.filter(p=>/_light_support$/.test(p.m_sId)).every(p=>p.GetSpottingKind()===5)&&profiles.find(p=>p.m_sId==='us_armour').GetSpottingKind()===2,'light escorts report their own intelligence class while genuine armour keeps its class');
  check(profiles.filter(p=>/_light_support$/.test(p.m_sId)).every(p=>p.GetWeight(0)===0&&p.GetWeight(1)>0),'new QRF escorts do not change the separate scheduled-attack pool');
  let funds=100000,debt=0;
  const context={m_aGroundQRFRecipes:recipes,m_aForceProfiles:profiles,m_iMaxGroups:20,m_iMaxMembers:Number(sources.layer.match(/m_iMaxMembers (\d+)/)[1]),m_iMaxVehicles:10,
    m_Operations:{GetReadinessLevel:()=>1,GetCrackdownLevel:()=>5,GetQRFFinance:()=>({Available:()=>funds,Debt:()=>debt}),QRFOverhead:t=>50+t*10,AllowsMinimumCaptureQRF:()=>true},
    GetReservedGroups:()=>0,GetReservedMembers:()=>0,GetReservedVehicles:()=>0};
  context.FindStrategicGroundRecipe=compile(sources.executor,'FindStrategicGroundRecipe');context.QRFPoolReadinessLevel=compile(sources.executor,'QRFPoolReadinessLevel');
  const select=compile(sources.executor,'SelectStrategicGroundRecipe');
  for(const role of [1,2])for(let readinessLevel=1;readinessLevel<=5;readinessLevel++)for(let intensity=1;intensity<=5;intensity++){
    const faction=role===1?'AROR_OCCUPIERS':'AROR_INVADERS';
    const r=context.FindStrategicGroundRecipe(role,faction,intensity,false,readinessLevel),elements=[];
    check(r&&r.m_iLevel===intensity,'each readiness/intensity/faction cell has a funded complete response: '+[role,readinessLevel,intensity,r?.m_sId]);
    check(r.Resolve(profiles,role,faction,elements,readinessLevel)&&elements.every(p=>p.m_iMinimumReadinessLevel<=readinessLevel),'every selected element respects the equipment unlock');
    if(readinessLevel<=2)check(elements.every(p=>/motorized|light_support/.test(p.m_sId)),'even maximum early crackdown excludes APC/armour/aircraft');
    if(readinessLevel>=3&&readinessLevel<=4)check(elements.some(p=>/mechanized/.test(p.m_sId))&&elements.every(p=>!/_(armour|airborne)$/.test(p.m_sId)),'middle readiness introduces APCs without late support/aircraft');
    const groups=elements.reduce((n,p)=>n+p.GetGroupBudget(),0),personnel=elements.reduce((n,p)=>n+p.m_iPersonnelBudget,0);
    check(groups<=20&&personnel<=128&&elements.length<=10,'complete responses fit existing executor limits');
    funds=0;debt=500;
    const minimum=context.FindStrategicGroundRecipe(role,faction,1,true,readinessLevel),minimumElements=[];
    check(minimum&&minimum.m_iLevel===1&&minimum.Resolve(profiles,role,faction,minimumElements,readinessLevel)&&minimumElements.every(p=>p.m_iMinimumReadinessLevel<=readinessLevel),'zero-funds floor cannot bypass equipment gating');
    check(!context.FindStrategicGroundRecipe(role,faction,intensity,false,readinessLevel),'debt still blocks ordinary paid selection');funds=100000;debt=0;
  }
  for(const role of [1,2])for(let readinessLevel=1;readinessLevel<=5;readinessLevel++)for(let intensity=1;intensity<=5;intensity++){
    const faction=role===1?'AROR_OCCUPIERS':'AROR_INVADERS';let previous=0;
    for(funds=0;funds<=6500;funds+=250){
      const selected=context.FindStrategicGroundRecipe(role,faction,intensity,false,readinessLevel);
      const actual=selected?.m_iLevel||0,elements=[];
      check(actual>=previous&&actual<=intensity,'increasing available funds cannot lower intensity or exceed requested crackdown');previous=actual;
      if(selected){
        check(selected.Resolve(profiles,role,faction,elements,readinessLevel)&&elements.every(p=>p.m_iMinimumReadinessLevel<=readinessLevel),'every affordability downgrade remains inside the equipment pool');
        check(elements.reduce((sum,p)=>sum+p.ReplacementQuote(),50+actual*10)<=funds,'downgraded response collateral and overhead fit the actual funds');
      }
    }
  }
  funds=100000;const emptyCapacity={...context,GetReservedGroups:()=>20};
  check(!context.FindStrategicGroundRecipe.call(emptyCapacity,1,'AROR_OCCUPIERS',1,true,1),'unsecured early baseline cannot exceed an exhausted group budget');
  emptyCapacity.GetReservedGroups=()=>0;emptyCapacity.GetReservedMembers=()=>context.m_iMaxMembers-7;
  check(!context.FindStrategicGroundRecipe.call(emptyCapacity,1,'AROR_OCCUPIERS',1,true,1),'unsecured baseline still needs the complete truck personnel reservation');
  const captured={m_iPoolReadinessLevel:0,m_iRequestedTier:5,m_bCaptureThreatRecorded:true,GetRole:()=>1};
  const chosen=select.call(context,captured,{GetFactionKey:()=> 'AROR_OCCUPIERS'});
  context.m_Operations.GetReadinessLevel=()=>5;context.m_Operations.GetCrackdownLevel=()=>1;
  check(chosen.m_sId===select.call(context,captured,{GetFactionKey:()=> 'AROR_OCCUPIERS'}).m_sId&&captured.m_iPoolReadinessLevel===1&&captured.m_iRequestedTier===5,'bound admission cannot change its equipment or intensity while waiting');
  const malicious=recipes.find(r=>r.m_sId==='occupier_light_l1');malicious.m_aProfileIds=['us_mechanized'];
  check(!malicious.Resolve(profiles,1,'AROR_OCCUPIERS',[],1),'profile-level equipment gate rejects a mislabelled early recipe');
  malicious.m_aProfileIds=['us_motorized'];
  // Advisory planning must honor the same equipment gate even though it cannot dispatch.
  env.AROR_QRFSelectionDecision=function(){this.m_Recipe=null;};
  const preview=compile(sources.selection,'Select');
  const previewContext={m_iReadinessLevel:1,m_iDesiredTier:5,m_iMinimumTier:1,m_iLocationTierCeiling:5,m_bAllowDowngrade:true,m_iArrivalDeadlineSeconds:180};
  const availability={m_eRole:1,m_sFactionKey:'AROR_OCCUPIERS',m_bGroundRouteReady:true,m_iArrivalSeconds:60,m_iFreeGroups:20,m_iFreePersonnel:128,m_iFreeVehicles:10,
    m_sOriginLocationId:'friendly-origin',IsValidAt:()=>true,GetReadyPackages:()=>10};
  check(preview(recipes,profiles,1,'AROR_OCCUPIERS',previewContext,availability,100).m_Recipe.m_sId==='occupier_light_l5','readiness preview cannot unlock APCs under high early crackdown');
  previewContext.m_iReadinessLevel=5;check(preview(recipes,profiles,1,'AROR_OCCUPIERS',previewContext,availability,100).m_Recipe.m_sId==='occupier_ground_l5','readiness preview can select unlocked combined arms');
  previewContext.m_iReadinessLevel=0;check(!preview(recipes,profiles,1,'AROR_OCCUPIERS',previewContext,availability,100).m_Recipe,'preview rejects an invalid equipment state');
  env.AROR_EconomyComponent={GetInstance:()=>({})};
  const air=compile(sources.reaper,'AffordableReaperTier'),helicopter=profiles.find(p=>p.m_sId==='us_airborne');
  check(air.call(context,helicopter,5,1)===0,'even an explicitly configured air adapter cannot buy locked aircraft through high crackdown');
  check(air.call(context,helicopter,5,5)===5,'unlocked experimental air path retains existing paid intensity/capacity limits');
  check(sources.app.includes('AROR_EThreatScale.READINESS')&&sources.app.includes('AROR_EThreatScale.CRACKDOWN')&&sources.app.includes('AROR_ThreatLabels'),'phone binds independent equipment and intensity cards using army threat labels');
  check(!sources.app.includes('SetCrackdown_S('),'phone remains read-only');
  return {assertions,note:'Actual source and all Chicken readiness/intensity cells; native compile, driving, crew, replication and balance remain unverified.'};
}
if(typeof module!=='undefined')module.exports={runQRFProgressionTests};
if(typeof require!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.resolve(__dirname,'../AROpenResistance/Open Resistance');
  const read=p=>fs.readFileSync(path.join(root,p),'utf8');
  console.log(runQRFProgressionTests({mix:read('Scripts/Game/Operations/AROR_QRFResponseMix.c'),policy:read('Scripts/Game/Operations/AROR_QRFStrategicPolicy.c'),manager:read('Scripts/Game/Operations/AROR_OperationManagerComponent.c'),
    serializer:read('Scripts/Game/Operations/AROR_OperationManagerComponentSerializer.c'),profile:read('Scripts/Game/Operations/AROR_ForceProfile.c'),recipe:read('Scripts/Game/Operations/AROR_GroundQRFRecipe.c'),responsePolicy:read('Scripts/Game/Operations/AROR_ResponsePolicy.c'),
    executor:read('Scripts/Game/Operations/AROR_OperationExecutorComponent.c'),selection:read('Scripts/Game/Operations/AROR_QRFSelectionPolicy.c'),reaper:read('Scripts/Game/Operations/AROR_ReaperQRFExecutor.c'),
    app:read('Scripts/Game/Phone/AROR_RebIntelApp.c'),layer:read('Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer')}));
}
