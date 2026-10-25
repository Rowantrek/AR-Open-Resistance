/* Actual-source policy regression checks. Native services are mocks, not an Enforce compiler.
 * Node: node tools/Test-QRFStrategy.js
 * The same runQRFTests(sourceMap) body can run in a V8 tool host without Node.
 */
function runQRFTests(sources) {
  let assertions = 0;
  function check(value, message) { assertions++; if (!value) throw Error(message); }
  for (const [name, fn] of Object.entries({Count(){return this.length;}, Insert(x){this.push(x);}, Clear(){this.length=0;}, Remove(i){this.splice(i,1);}, RemoveItem(x){const i=this.indexOf(x);if(i>=0)this.splice(i,1);}, Contains(x){return this.includes(x);}})) {
    Object.defineProperty(Array.prototype, name, {value:fn, configurable:true});
  }
  function extract(source, name) {
    const escaped = name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    const match = source.match(new RegExp('^\\t[^\\r\\n]*\\b'+escaped+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if (!match) throw Error('Missing source method '+name);
    return {args:match[1],body:match[2]};
  }
  const bank = {balances:{1:10000,2:10000}, payments:[], GetFactionFunds(role){return this.balances[role]||0;}, SpendFactionFunds_S(role,amount){if(amount<0||this.GetFactionFunds(role)<amount)return false;this.balances[role]-=amount;this.payments.push([role,amount]);return true;}};
  const env = {Replication:{IsServer:()=>true}, AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2}, AROR_EForceType:{MOTORIZED:0,MECHANIZED:1,ARMOUR:2,AIRBORNE:3,INFANTRY:4}, AROR_ELocationType:{TOWN:0,HQ:1,AIRPORT:2,CITY:3,FACTORY:4,MILITARY_BASE:5,PORT:6,RESOURCES:7,OUTPOST:8}, Math:{...Object.fromEntries(['min','max','floor'].map(n=>[n,Math[n]])), Min:Math.min,Max:Math.max,Clamp:(x,a,b)=>Math.max(a,Math.min(b,x))}, Print:()=>{},LogLevel:{NORMAL:0},string:{Format:(...xs)=>xs.join(' ')}, AROR_EconomyComponent:{GetInstance:()=>bank}, ChimeraCharacter:{Cast:x=>x?.character?x:null},DamageManagerComponent:{Cast:x=>x},FactionAffiliationComponent:{Cast:x=>x}};
  const methodNames = ['PressureForLevel','WarLevel','Importance','Priority','DesiredTier','VehicleValue','Debt','SetDebt_S','Available','Reserve_S','Charge_S','Activate_S','Observe_S','Tick_S','Register_S','ObserveLosses_S','Retire_S','Reserved','IsLost','VehicleLossValue','ReplacementQuote','Find','RecordLook','InView','GetReservedGroups','GetReservedMembers','GetReservedVehicles','IsReady','IsEnemyRole','HasLocation','RecordStrategicEvidence','SetWarPressure_S','GetWarPressure','GetWarLevel','GetOperation','RequestOperation_S','GetOwner','Clear','HasOperatingQRF','HasPendingQRF','FailOperation'];
  function compile(source,name) {
    let {args,body} = extract(source,name);
    args = args.replace(/\b(?:notnull |const )?(?:[A-Za-z_]\w*)(?:<[^>]+>)?\s+(\w+)/g,'$1');
    body=body.replace(/\b(?:const )?array<[^>]+>\s+(\w+)\s*=\s*\{([^}]*)\};/g,'let $1=[$2];')
      .replace(/foreach\s*\(\w+\s+(\w+)\s*:\s*([^\)]+)\)/g,'for (let $1 of $2)')
      .replace(/\b(?:const )?(?:bool|int|float|string|IEntity|ChimeraCharacter|DamageManagerComponent|FactionAffiliationComponent|AROR_\w+)\s+(\w+)/g,'let $1')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/(?<![.\w])s_\w+/g,x=>'this.'+x)
      .replace(/\b([\w.]+)\.IsEmpty\(\)/g,'($1.length === 0)');
    // Enforce zero-initializes local integers; JavaScript does not.
    body=body.replace(/let (\w+);/g,'let $1=0;');
    body=body.replace(/context\.ReadValue\("([^"]+)", (\w+)\)/g,'(($2=context.values["$1"])!==undefined)');
    for(const method of methodNames)body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(', 'g'),'this.'+method+'(');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  function makeClass(name,methods,initialize) {
    const ctor=function(){initialize?.call(this);};
    env[name]=ctor;
    const source=sources[name];
    for(const method of methods)ctor.prototype[method]=compile(source,method);
    return ctor;
  }
  const policy={};env.AROR_QRFStrategicPolicy=policy;
  for(const name of methodNames.slice(0,6))policy[name]=compile(sources.AROR_QRFStrategicPolicy,name);
  for(let pressure=0;pressure<=1000;pressure++) {
    const level=policy.WarLevel(pressure);
    check(level>=1&&level<=5,'bounded war level');
    check(pressure>=policy.PressureForLevel(level),'pressure threshold');
    if(level<5)check(pressure<policy.PressureForLevel(level+1),'next threshold');
  }
  for(let war=1;war<=5;war++)for(let roll=0;roll<100;roll++)for(const captured of [false,true]) {
    const tier=policy.DesiredTier(war,0,captured,roll);
    check(tier>=1&&tier<=5,'tier bounds');
    check(tier<=Math.min(5,war+(captured?1:0)),'war ceiling');
    check(tier>=Math.max(1,war-1),'no excessive downgrade');
  }
  check(policy.DesiredTier(2,3,false,50)===3,'sustained attack escalates');
  check(policy.DesiredTier(2,0,false,50,180)===3,'strategic location strengthens response');
  check(policy.Priority(100,1,true,0)>policy.Priority(100,1,false,0),'capture priority');
  check(policy.Priority(100,1000,false,10000)===250,'bounded reports and aging');
  check(policy.Importance(env.AROR_ELocationType.FACTORY)>policy.Importance(env.AROR_ELocationType.TOWN),'economic importance');
  const Asset=makeClass('AROR_QRFAssetAccount',['IsLost'],function(){this.m_bSettled=false;});
  const Funding=makeClass('AROR_QRFFunding',['Register_S','ObserveLosses_S','Retire_S','Reserved'],function(){Object.assign(this,{m_aAssets:[],m_iLosses:0,m_iRecovered:0,m_bActivated:false,m_bClosed:false,m_iQuote:0,m_iOverhead:0});});
  const Finance=makeClass('AROR_QRFFinance',['Debt','SetDebt_S','Available','Reserve_S','Charge_S','Activate_S','Observe_S','Tick_S','PendingLossValue'],function(){Object.assign(this,{m_aDeployments:[],m_iOccupierDebt:0,m_iInvaderDebt:0,m_iOccupierLossValue:0,m_iInvaderLossValue:0});});
  const operation=(id,role)=>({GetId:()=>id,GetRole:()=>role});
  const finance=new Finance();
  let account=finance.Reserve_S(operation('one',1),940,60);
  check(account&&finance.Available(1)===9000,'whole quote reserved');
  check(finance.Available(2)===10000,'separate faction collateral');
  check(!finance.Reserve_S(operation('too expensive',1),10000,60),'overcommit rejected');
  check(finance.Reserve_S(operation('one',1),940,60)===account,'duplicate booking idempotent');
  finance.Activate_S(account);finance.Activate_S(account);
  check(bank.balances[1]===9940&&bank.payments.length===1,'overhead exactly once');
  function entity(character=false,key='AROR_OCCUPIERS') {
    const value={character,dead:false,destroyed:false,key,GetOrigin:()=>[0,0,0]};
    value.GetCharacterController=()=>({IsDead:()=>value.dead});
    value.FindComponent=type=>type===env.DamageManagerComponent?{IsDestroyed:()=>value.destroyed}:{GetAffiliatedFactionKey:()=>value.key};
    return value;
  }
  const soldier=entity(true),truck=entity();
  account.Register_S(soldier,25,false,'AROR_OCCUPIERS','US');
  account.Register_S(truck,200,true,'AROR_OCCUPIERS','US');
  account.Register_S(truck,200,true,'AROR_OCCUPIERS','US');
  check(account.m_aAssets.length===2,'one asset registration');
  soldier.dead=true;check(finance.PendingLossValue(1)===25&&finance.PendingLossValue(2)===0,'pending confirmed loss snapshot is role scoped');check(bank.balances[1]===9940,'save snapshot does not mutate bank');finance.Observe_S(account);finance.Observe_S(account);
  check(finance.PendingLossValue(1)===0,'paid losses excluded from pending save debt');
  check(bank.balances[1]===9915,'death charged exactly once');
  check(finance.m_iOccupierLossValue===25,'unique casualty pressure input');
  truck.key='';finance.Observe_S(account);check(bank.balances[1]===9915,'empty transport not capture');
  truck.key='US';finance.Observe_S(account);check(bank.balances[1]===9915,'native transport not capture');
  truck.key='AROR_OCCUPIERS';account.Retire_S(truck);truck.destroyed=true;finance.Observe_S(account);
  check(bank.balances[1]===9915,'normal retirement never charged as destruction');
  account.m_bClosed=true;finance.Tick_S();check(finance.m_aDeployments.length===0,'closed settled allocation pruned');
  account=finance.Reserve_S(operation('cancel',1),940,60);account.m_bClosed=true;finance.Tick_S();
  check(bank.balances[1]===9915,'pre-active cancellation free');
  bank.balances[0]=500;finance.Charge_S(0,100);check(bank.GetFactionFunds(0)===500&&finance.Debt(0)===0,'Resistance not billed');
  account=finance.Reserve_S(operation('loss',1),1000,60);finance.Activate_S(account);
  const armour=entity();account.Register_S(armour,1000,true,'AROR_OCCUPIERS','US');
  bank.balances[1]=100;armour.destroyed=true;finance.Observe_S(account);
  check(bank.balances[1]===0&&finance.Debt(1)===900,'external spend shortfall becomes debt');
  check(!finance.Reserve_S(operation('blocked by debt',1),0,0),'debt blocks even zero-price response');
  bank.balances[1]=500;finance.Tick_S();check(finance.Debt(1)===400&&bank.balances[1]===0,'income services debt');
  bank.balances[1]=500;finance.Tick_S();check(finance.Debt(1)===0&&bank.balances[1]===100,'debt repaid without negative balance');
  const captured=entity(true,'AROR_RESISTANCE'),nativeSoldier=entity(true,'US');
  for(const e of [captured,nativeSoldier]){const asset=new Asset();Object.assign(asset,{m_Entity:e,m_sFaction:'AROR_OCCUPIERS',m_sNativeFaction:'US',m_bVehicle:false});check(asset.IsLost(),'captured personnel is a loss');}
  const unknown=new Asset();check(!unknown.IsLost(),'missing/dormant entity is not confirmed dead');
  const Profile=makeClass('AROR_ForceProfile',['VehicleLossValue','ReplacementQuote','GetGroupBudget'],function(){Object.assign(this,{m_eType:0,m_iVehicleLossValue:-1,m_iPersonnelLossValue:25,m_iPersonnelBudget:8,m_iEscortCount:1,m_iEscortLossValue:2000});});
  const profile=new Profile();check(profile.ReplacementQuote()===400,'motorized collateral');profile.m_eType=3;profile.m_iPersonnelBudget=20;check(profile.ReplacementQuote()===4100,'aircraft and all crew quoted');
  profile.m_iVehicleLossValue=700;check(profile.VehicleLossValue()===700,'designer price override');
  const Spot=makeClass('AROR_QRFSpotting',['Find','RecordLook','InView','SetMapOpen_S'],function(){this.m_aSightings=[];this.m_iNextId=0;this.m_aMapObserversBlocked=[];});
  env.AROR_QRFSighting=function(){};
  // RecordLook was compiled before this constructor existed in env; recompile its bindings.
  Spot.prototype.RecordLook=compile(sources.AROR_QRFSpotting,'RecordLook');
  const spot=new Spot(),target=entity();
  spot.SetMapOpen_S(7,true);spot.SetMapOpen_S(7,true);check(spot.m_aMapObserversBlocked.length===1,'map suppresses observer once');spot.SetMapOpen_S(7,false);check(spot.m_aMapObserversBlocked.length===0,'map close restores observer');
  // Enforce initializes plain object fields to zero/false.
  env.AROR_QRFSighting=function(){Object.assign(this,{m_iLastLook:0,m_bConfirmed:false});};
  Spot.prototype.RecordLook=compile(sources.AROR_QRFSpotting,'RecordLook');
  spot.RecordLook(target,1,7,100,90);check(!spot.m_aSightings[0].m_bConfirmed,'one glance not enough');
  spot.RecordLook(target,1,8,101,90);check(!spot.m_aSightings[0].m_bConfirmed,'other player cannot complete first glance');
  spot.RecordLook(target,1,7,102,90);check(spot.m_aSightings[0].m_bConfirmed,'second visual observation confirms');
  spot.RecordLook(target,1,8,130,90);check(spot.m_aSightings[0].m_iLastSeen===130,'continuous reports work beyond first15secs');
  check(Spot.prototype.InView.call(spot,500,0.95,800,0.9),'view geometry accepted');
  check(!Spot.prototype.InView.call(spot,900,1,800,0.9),'range rejects');
  check(!Spot.prototype.InView.call(spot,500,0.5,800,0.9),'behind/outside view rejected');
  // Execute actual ground selection against bounded authored-recipe mocks.
  const groundCtx={m_Operations:{GetQRFFinance:()=>({Available:()=>bank.balances[1],Debt:()=>0}),QRFOverhead:t=>50+t*10},m_iMaxGroups:20,m_iMaxMembers:128,m_iMaxVehicles:10,GetReservedGroups:()=>0,GetReservedMembers:()=>0,GetReservedVehicles:()=>0,m_aForceProfiles:[],m_aGroundQRFRecipes:[]};
  const motor=new Profile();
  for(let tier=1;tier<=5;tier++)groundCtx.m_aGroundQRFRecipes.push({m_iLevel:tier,Resolve:(profiles,role,faction,out)=>{if(role!==1||faction!=='AROR_OCCUPIERS')return false;for(let i=0;i<tier;i++)out.push(motor);return true;}});
  const choose=compile(sources.AROR_OperationExecutorComponent,'FindStrategicGroundRecipe');
  for(let funds=0;funds<=10000;funds+=250)for(let desired=1;desired<=5;desired++){
    bank.balances[1]=funds;const recipe=choose.call(groundCtx,1,'AROR_OCCUPIERS',desired);
    if(recipe){check(recipe.m_iLevel<=desired,'recipe respects intent');check(recipe.m_iLevel*400+50+recipe.m_iLevel*10<=funds,'complete quote affordable');}
    else check(funds<460,'only reject if no affordable tier1');
  }
  check(!choose.call(groundCtx,2,'AROR_INVADERS',5),'other role recipes rejected');
  const air=compile(sources.AROR_ReaperQRFExecutor,'AffordableReaperTier');
  Object.assign(profile,{m_iVehicleLossValue:1600,m_eRole:1});bank.balances[1]=10000;
  check(air.call(groundCtx,profile,5)===2,'air downgrades to two complete affordable pairs');
  groundCtx.m_iMaxVehicles=1;check(air.call(groundCtx,profile,5)===0,'air needs two slots');groundCtx.m_iMaxVehicles=10;
  // Queue coalescing, disabled role, affordability wait, priority and expiry.
  env.AROR_EOperationType={QRF:1,ATTACK:0};env.Math.RandomInt=()=>50;
  env.AROR_LocationManagerComponent={GetInstance:()=>({GetLocationById:id=>({GetLocationType:()=>id==='factory'?4:0})})};
  env.AROR_OperationExecutorComponent={Cast:x=>x};
  env.AROR_QRFThreat=function(){Object.assign(this,{m_iNextAttempt:0,m_iNextWaitLog:0,m_iReports:0,m_iNextEvidence:0,m_iDesiredTier:0,m_iTierRoll:-1,m_bCaptured:false});};
  env.AROR_QRFThreat.prototype.Score=function(now){return policy.Priority(this.m_iImportance,this.m_iReports,this.m_bCaptured,now-this.m_iFirstSeen);};
  const manager={m_iClock:100,m_bInvaderOperationsEnabled:false,m_aQRFThreats:[],m_aOperations:[],m_iQRFThreatLifetime:180,m_iQRFReaction:10,m_QRFFinance:finance,IsReady:()=>true,IsEnemyRole:r=>r===1||r===2,HasLocation:id=>id!=='missing',RecordStrategicEvidence(){},GetWarPressure:()=>100,SetWarPressure_S(){},GetWarLevel:()=>2};
  const queue=compile(sources.AROR_OperationManagerComponent,'QueueQRFThreat_S');
  queue.call(manager,1,'town',false);queue.call(manager,1,'town',false);
  check(manager.m_aQRFThreats.length===1&&manager.m_aQRFThreats[0].m_iReports===1,'coalesce and throttle reports');
  queue.call(manager,2,'town',false);queue.call(manager,1,'missing',false);check(manager.m_aQRFThreats.length===1,'disabled Invader and missing location rejected');
  queue.call(manager,1,'factory',true);check(manager.m_aQRFThreats.length===2,'separate strategic target queued');
  let affordable=false,admitted=[];
  manager.GetOwner=()=>({FindComponent:()=>({CanFundQRFThreat_S:()=>affordable})});
  manager.RequestOperation_S=(role,type,target)=>{admitted.push(target);return target;};
  manager.GetOperation=id=>id?{m_iRequestedTier:0}:null;
  const process=compile(sources.AROR_OperationManagerComponent,'ProcessQRFThreats');
  process.call(manager);check(admitted.length===0&&manager.m_aQRFThreats.length===2,'unaffordable threats retained');
  affordable=true;manager.m_iClock=116;process.call(manager);check(admitted[0]==='factory','captured factory outranks town');
  manager.m_iClock=1000;process.call(manager);check(manager.m_aQRFThreats.length===0,'stale pending threats expire');
  // Shared commander recovery does not fabricate losses or trigger reserve release.
  const recovered=compile(sources.AROR_QRFTacticalCommander,'MarkRecovered');
  const tactical={m_iPackages:2,m_aLost:[false,false],m_aInitial:[5,7],m_aLiving:[5,7],m_iLosses:0};
  recovered.call(tactical,0);recovered.call(tactical,0);
  check(tactical.m_iLosses===0&&tactical.m_aInitial[0]===0&&tactical.m_aLiving[0]===0,'idempotent recovery differs from casualty');
  env.AROR_GroundOperationForce={Cast:x=>x.ground?x:null};
  env.vector={DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])};
  const hasReturned=compile(sources.AROR_OperationExecutorComponent,'TacticalForceReturned');
  const returnCtx={m_fQRFRecoveryRadius:100,m_fQRFVehicleRecoveryRadius:125};
  const returnCommander={m_bSafeWithdrawalOrigin:true,m_vOrigin:[0,0,0]};
  const returnedSoldier=entity(true),returnedHull=entity();returnedHull.GetOrigin=()=>[200,0,0];
  const returnForce={ground:true,m_Group:{GetPlayerCount:()=>0},m_Vehicle:returnedHull,GetOwnedMembers:out=>out.push(returnedSoldier)};
  check(!hasReturned.call(returnCtx,returnForce,returnCommander),'vehicle must actually return');
  returnedHull.GetOrigin=()=>[0,0,0];returnedSoldier.GetOrigin=()=>[160,0,0];
  check(!hasReturned.call(returnCtx,returnForce,returnCommander),'infantry must actually return');
  returnedSoldier.GetOrigin=()=>[0,0,0];check(hasReturned.call(returnCtx,returnForce,returnCommander),'all survivors near friendly origin');
  returnCommander.m_bSafeWithdrawalOrigin=false;check(!hasReturned.call(returnCtx,returnForce,returnCommander),'lost origin cannot recover');
  env.AROR_EOperationState={ACTIVE:2,SUCCEEDED:3};let finished=0,failed=0;
  const finish=compile(sources.AROR_OperationExecutorComponent,'FinishGroundQRFIfIdle');
  const endCtx={m_Operations:{ResolveOperation_S:()=>finished++},HasOperatingQRF:()=>false,HasPendingQRF:()=>false,FailOperation:()=>failed++};
  const ended={IsTerminal:()=>false,GetState:()=>2,GetId:()=> 'ended',m_iRecoveredPackages:1};
  finish.call(endCtx,ended);check(finished===1&&failed===0,'returned survivor completes task');
  ended.m_iRecoveredPackages=0;finish.call(endCtx,ended);check(failed===1,'no survivors fails task');
  endCtx.HasPendingQRF=()=>true;finish.call(endCtx,ended);check(failed===1,'virtual reserve prevents premature terminal state');
  // Client packet bounds and static last-seen snapshots; no entity tracking on the client.
  env.RplSession={Mode:()=>0};env.RplMode={Dedicated:1};env.GetGame=()=>({GetWorld:()=>({GetWorldTime:()=>100000})});
  const cache={s_aIds:[],s_aPositions:[],s_aKinds:[],s_aSides:[],s_aAges:[],Clear(){this.s_aIds=[];this.s_aPositions=[];this.s_aKinds=[];this.s_aSides=[];this.s_aAges=[];}};
  const receive=compile(sources.AROR_QRFMapIntel,'Receive');
  receive.call(cache,['one'],[[1,2,3]],[1],[10],90,[1]);check(cache.s_aIds.length===1&&cache.s_fReceivedAt===100,'last seen snapshot received');
  receive.call(cache,['bad'],[],[1],[10],90,[1]);check(cache.s_aIds[0]==='one','malformed packet rejected');
  receive.call(cache,['bad'],[[1,2,3]],[9],[10],90,[1]);check(cache.s_aIds.length===0,'invalid type filtered');
  receive.call(cache,['stale'],[[1,2,3]],[4],[90],90,[2]);check(cache.s_aIds.length===0,'expired report filtered');
  check(!sources.AROR_QRFMapIntel.includes('m_Entity'),'map stores snapshots only');
  // Actual serializer bodies preserve pending bills without mutating another component during save.
  env.AROR_OperationManagerComponent={Cast:x=>x};env.ESerializeResult={OK:1};env.VERSION=1;
  const saveFinance=new Finance(),saveAccount=saveFinance.Reserve_S(operation('save',1),500,60),saveSoldier=entity(true);
  saveAccount.Register_S(saveSoldier,25,false,'AROR_OCCUPIERS','US');saveSoldier.dead=true;saveFinance.SetDebt_S(1,100);
  const saved={},saveManager={GetWarPressure:role=>role===1?100:250,GetQRFFinance:()=>saveFinance};
  const serialize=compile(sources.AROR_OperationManagerComponentSerializer,'Serialize');
  const beforeSave=bank.balances[1];
  check(serialize(null,saveManager,{WriteValue:(key,value)=>saved[key]=value})===1,'serialize policy state');
  check(saved.occupierDebt===125&&saved.invaderDebt===0,'save includes only unbilled confirmed losses');
  check(bank.balances[1]===beforeSave&&!saveAccount.m_aAssets[0].m_bSettled,'save is read only');
  let restored=null;const loadManager={RestoreQRFStrategy_S:(...args)=>restored=args};
  const deserialize=compile(sources.AROR_OperationManagerComponentSerializer,'Deserialize');
  check(deserialize(null,loadManager,{values:saved})&&restored.join(',')==='100,250,125,0','restore pressure and liabilities');
  check(!deserialize(null,loadManager,{values:{...saved,version:2}}),'unknown version rejected');
  check(!deserialize(null,loadManager,{values:{version:1}}),'partial state rejected');
  const payments=bank.payments.length;env.Replication.IsServer=()=>false;
  finance.Charge_S(1,100);check(!finance.Reserve_S(operation('client',1),1,1)&&bank.payments.length===payments,'client cannot bill or reserve');
  env.Replication.IsServer=()=>true;
  check(!sources.AROR_QRFSpotting.includes('RplRcver.Broadcast'),'spot data not broadcast to opposing clients');
  check(sources.AROR_QRFSpotting.includes('TraceFlags.VISIBILITY'),'native visibility occlusion required');
  check(sources.AROR_ModdedPlayerController.includes('RplRcver.Owner'),'owner-targeted delivery');
  check(!sources.AROR_ModdedPlayerController.includes('RpcAsk_QRFIntel'),'no client intelligence authority');
  check(sources.AROR_OperationManagerComponentSerializer.includes('occupierDebt')&&sources.AROR_OperationManagerComponentSerializer.includes('invaderPressure'),'policy/debt persistence hooks');
  return {assertions, note:'Actual-source bodies; native lifecycle, perception, replication and serialization remain mocked/unverified.'};
}
if(typeof module!=='undefined')module.exports={runQRFTests};
if(typeof require!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),sources={};
  const root=path.resolve(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game');
  for(const folder of ['Operations','GameFlow','Locations'])for(const file of fs.readdirSync(path.join(root,folder))){
    if(file.endsWith('.c'))sources[file.slice(0,-2)]=fs.readFileSync(path.join(root,folder,file),'utf8');
  }
  console.log(runQRFTests(sources));
}
