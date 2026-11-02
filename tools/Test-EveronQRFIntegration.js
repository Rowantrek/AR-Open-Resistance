/* Actual-source location/QRF integration with mocked campaign services.
 * Run: node tools/Test-EveronQRFIntegration.js
 */
function runEveronQRFIntegrationTests(sources) {
  let assertions=0;
  const check=(ok,msg)=>{assertions++;if(!ok)throw Error(msg);};
  const types={TOWN:0,HQ:1,AIRPORT:2,CITY:3,FACTORY:4,MILITARY_BASE:5,PORT:6,RESOURCES:7,OUTPOST:8};
  let target={GetId:()=>'site',type:types.OUTPOST,owner:0,GetLocationType(){return this.type;},GetControllingRole(){return this.owner;}};
  const env={AROR_ELocationType:types,AROR_EFactionRole:{RESISTANCE:0,OCCUPIERS:1,INVADERS:2},AROR_EOperationType:{QRF:1},string:{Empty:'',Format:()=>''},Print:()=>{},LogLevel:{NORMAL:0},
    AROR_LocationManagerComponent:{GetInstance:()=>({GetLocationById:()=>target})},AROR_InvaderCampaignComponent:{GetInstance:()=>null},
    AROR_FactionRolesComponent:{GetInstance:()=>({GetFaction:role=>({role,IsFactionEnemy:other=>role!==other.role})})},
    Replication:{IsServer:()=>true},AROR_QRFOriginPolicy:{},AROR_CaptureLocationAction:{},AROR_SettlementComponent:{Cast:x=>x}};
  function compile(source,name){
    const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!m)throw Error('Missing '+name);
    const args=m[1].replace(/\b(?:int|bool|string|vector|AROR_\w+)\s+/g,'');
    let body=m[2].replace(/\b(?:bool|int|string|Faction|AROR_\w+)\s+(\w+)/g,'let $1').replace(/let (\w+);/g,'let $1=0;')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x);
    for(const method of ['IsReady','IsEnemyRole','QueueQRFThreat_S','RequestOperation_S','HasPendingQRFThreat','ReportLocationCapture_S','IsCapturableType','IsCapturableLocation','ReportAttack_S','ReportCombatContact_S','GetOperation','RecordStrategicEvidence','RecordCaptureEscalation_S','SetReadinessPressure_S','GetReadinessPressure'])
      body=body.replace(new RegExp('(?<![.\\w])'+method+'\\(', 'g'),'this.'+method+'(');
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  const origin=compile(sources.policy,'IsOriginType');env.AROR_QRFOriginPolicy.IsOriginType=origin;
  env.AROR_CaptureLocationAction.IsCapturableType=compile(sources.capture,'IsCapturableType');
  for(const type of Object.values(types))check(origin(type)===[2,5,8].includes(type),'origin type gate '+type);
  const capture=compile(sources.manager,'ReportLocationCapture_S');
  let requested=0,queued=0;
  const manager={m_bTownCaptureQRFs:true,m_bOutpostCaptureQRFs:true,m_bQRFStrategicEconomy:false,m_iQRFReaction:10,
    IsReady:()=>true,IsEnemyRole:r=>r===1||r===2,RequestOperation_S(){requested++;return 'operation';},
    QueueQRFThreat_S(){queued++;return 'queued';},HasPendingQRFThreat:()=>false,GetOperation:()=>null};
  for(const type of [types.TOWN,types.CITY,types.OUTPOST,types.MILITARY_BASE,types.AIRPORT,types.FACTORY,types.PORT,types.RESOURCES]){
    target.type=type;check(capture.call(manager,1,'site')==='operation','capture admission: '+type);
  }
  check(requested===8,'one request per supported capture');
  for(const type of [types.HQ]){
    target.type=type;check(capture.call(manager,1,'site')==='','unwired type ignored: '+type);
  }
  target.type=types.OUTPOST;manager.m_bOutpostCaptureQRFs=false;
  check(capture.call(manager,1,'site')==='','outpost opt-out respected');manager.m_bOutpostCaptureQRFs=true;
  target.type=types.CITY;manager.m_bTownCaptureQRFs=false;
  check(capture.call(manager,1,'site')==='','settlement opt-out respected');manager.m_bTownCaptureQRFs=true;
  target.owner=1;check(capture.call(manager,1,'site')==='','unchanged ownership ignored');target.owner=0;
  check(capture.call(manager,0,'site')==='','Resistance does not acquire an AI response');
  manager.IsReady=()=>false;check(capture.call(manager,1,'site')==='','setup/restore not treated as attack');manager.IsReady=()=>true;
  manager.m_bQRFStrategicEconomy=true;check(capture.call(manager,1,'site')==='queued'&&queued===1,'paid capture uses bounded strategic queue');
  manager.ReportLocationCapture_S=(...args)=>capture.apply(manager,args);
  check(compile(sources.manager,'ReportTownCapture_S').call(manager,1,'site')==='queued','legacy settlement API retained');
  let combatContacts=0;
  manager.m_bCombatQRFs=false;
  manager.ReportAttack_S=compile(sources.manager,'ReportAttack_S');
  manager.ReportCombatContact_S=()=>combatContacts++;
  manager.GetOperation=()=>null;
  const damageAt=compile(sources.manager,'ReportAttackAt_S'),beforeDamageQueue=queued,beforeDamageRequests=requested;
  check(manager.ReportAttack_S(1,'site')===''&&queued===beforeDamageQueue&&requested===beforeDamageRequests,'combat disabled blocks plain damage request');
  check(damageAt.call(manager,1,'site',[0,0,0])===''&&queued===beforeDamageQueue&&combatContacts===1,'combat dispatch disabled still permits a trusted contact snapshot for local defence, without requesting a QRF');
  check(capture.call(manager,1,'site')==='queued'&&queued===beforeDamageQueue+1,'capture still queues a paid response with combat reports disabled');
  manager.m_bCombatQRFs=true;
  check(damageAt.call(manager,1,'site',[0,0,0])==='queued'&&queued===beforeDamageQueue+2&&combatContacts===2,'explicit combat opt-in retains strategic response path');
  manager.m_bQRFStrategicEconomy=false;
  check(manager.ReportAttack_S(1,'site')==='operation'&&requested===beforeDamageRequests+1,'explicit combat opt-in retains non-strategic response path');
  let invaderPressure=100,evidence=0;
  manager.m_bInvaderOperationsEnabled=false;manager.RecordStrategicEvidence=()=>evidence++;manager.RecordCaptureEscalation_S=()=>invaderPressure+=25;manager.GetReadinessPressure=()=>invaderPressure;manager.SetReadinessPressure_S=(r,n)=>invaderPressure=n;
  env.AROR_InvaderCampaignComponent.GetInstance=()=>({IsAdvanceActive:()=>true});const requestsBefore=requested,queuesBefore=queued;
  check(capture.call(manager,2,'site')===''&&invaderPressure===125&&evidence===1&&requested===requestsBefore&&queued===queuesBefore,'Resistance capture raises territorial Invader progress without physical dispatch');
  target.owner=1;capture.call(manager,2,'site');check(invaderPressure===125,'AI-versus-AI capture cannot escalate the territorial war by itself');target.owner=0;
  env.AROR_InvaderCampaignComponent.GetInstance=()=>null;capture.call(manager,2,'site');check(invaderPressure===125,'disabled territorial campaign retains Invader admission opt-out');
  manager.m_bInvaderOperationsEnabled=true;manager.m_bQRFStrategicEconomy=true;const invaderQueued=queued;
  check(capture.call(manager,2,'site')==='queued'&&queued===invaderQueued+1&&invaderPressure===125,'enabled Invader capture uses strategic dispatch without duplicate fallback progress');
  const invaderDamageQueued=queued;manager.m_bCombatQRFs=false;
  check(manager.ReportAttack_S(2,'site')===''&&queued===invaderDamageQueued,'enabled Invader admission still rejects combat-driven QRFs');
  check(!sources.observer.includes('SetControllingRole_S'),'observer does not change territory authority');
  const actions={IsCapturableType:compile(sources.capture,'IsCapturableType'),IsCapturableLocation:compile(sources.capture,'IsCapturableLocation')};
  const captureFlag=compile(sources.capture,'CaptureLocation_S');
  check(!sources.location.includes('AllowsSettlementFlagCapture')&&!sources.location.includes('m_bSettlementFlagCapture'),'obsolete settlement flag API removed');
  let supportCalls=0,controlCalls=0;
  const site={m_eType:types.CITY,owner:1,allowed:true,
    GetLocationType(){return this.m_eType;},CanBeTakenBy(){return this.allowed;},
    GetControllingRole(){return this.owner;},SetControllingRole_S(role){controlCalls++;this.owner=role;return true;},
    GetOwner:()=>({FindComponent:()=>settlement})};
  let settlement={SetSupport_S(role,value){supportCalls++;check(value===100,'flag capture sets coherent majority support');site.owner=role;}};
  check(!captureFlag.call(actions,site,0)&&supportCalls===0&&controlCalls===0,'legacy opted-in city cannot use flag capture');
  check(!actions.IsCapturableLocation(site),'unmodified cities remain support-only');
  check(!captureFlag.call(actions,site,0),'non-opted-in city flag rejected');
  site.allowed=false;check(!captureFlag.call(actions,site,0),'capture cooldown/fixed control respected');site.allowed=true;
  settlement=null;check(!captureFlag.call(actions,site,0),'missing settlement cannot create inconsistent city control');
	for(const type of [types.TOWN,types.CITY]){site.m_eType=type;check(!actions.IsCapturableLocation(site),'settlements never enter military recapture: '+type);}
  site.m_eType=types.OUTPOST;check(captureFlag.call(actions,site,0)&&controlCalls===1,'outpost flag retains direct control authority');
  site.m_eType=types.MILITARY_BASE;check(captureFlag.call(actions,site,0)&&controlCalls===2,'military outpost flag retains direct control authority');
  site.m_eType=types.HQ;check(!actions.IsCapturableLocation(site),'HQ cannot opt into settlement flag capture');
  env.Replication.IsServer=()=>false;site.m_eType=types.OUTPOST;
  check(!captureFlag.call(actions,site,0)&&controlCalls===2,'client cannot capture through helper');
  check(sources.capture.includes('if (!IsCapturableLocation(location) || location.IsControlFixed())'),'action visibility uses the military-only gate');
  return {assertions,note:'Native events, faction manager and response execution mocked; Play verification required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
  const sources={};for(const [key,name]of Object.entries({policy:'AROR_QRFOriginPolicy',manager:'AROR_OperationManagerComponent',observer:'AROR_QRFCaptureObserver',capture:'../Capture/AROR_CaptureLocationAction',location:'../Locations/AROR_LocationComponent'}))sources[key]=fs.readFileSync(path.join(root,name+'.c'),'utf8');
  console.log(JSON.stringify(runEveronQRFIntegrationTests(sources)));
}
