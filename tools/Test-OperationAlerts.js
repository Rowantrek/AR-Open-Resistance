/* Actual-source checks for the QRF/attack dispatch alerts: arrival estimates, headline wording, the replicated
 * threat board round trip and the map marker captions. Native widgets, replication and notifications are mocked.
 * Run: node tools/Test-OperationAlerts.js
 */
function runOperationAlertsTests(sources) {
 let assertions=0,printed=[],sent=[],bumps=0,tick=0;
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 const extend=(proto,name,fn)=>Object.defineProperty(proto,name,{value:fn,configurable:true});
 extend(Array.prototype,'Count',function(){return this.length;});
 extend(Array.prototype,'Clear',function(){this.length=0;});
 extend(Array.prototype,'Insert',function(x){this.push(x);});
 extend(String.prototype,'IsEmpty',function(){return this.length===0;});
 extend(String.prototype,'ToInt',function(){return parseInt(this,10)||0;});
 extend(String.prototype,'Split',function(sep,out,removeEmpty){out.length=0;for(const part of this.split(sep))if(!removeEmpty||part.length)out.push(part);});
 const roles={RESISTANCE:0,OCCUPIERS:1,INVADERS:2},types={ATTACK:0,QRF:1},states={PLANNED:0,SPAWNING:1,ACTIVE:2,SUCCEEDED:3,FAILED:4,CANCELLED:5};
 let targets={};
 class Notification{constructor(type,text,icon,color,kicker){Object.assign(this,{type,text,color,kicker});}}
 class ThreatInfo{constructor(){this.m_iEtaSeconds=-1;}}
 const env={AROR_EFactionRole:roles,AROR_EOperationType:types,AROR_EOperationState:states,
  Replication:{IsServer:()=>true,BumpMe:()=>bumps++},System:{GetTickCount:()=>tick},
  Math:{Max:Math.max,Min:Math.min,Round:Math.round,Ceil:Math.ceil,floor:Math.floor},vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
  string:{Empty:'',Format:(s,...a)=>s.replace(/%(\d+)/g,(_,n)=>String(a[n-1]))},Print:x=>printed.push(x),LogLevel:{NORMAL:0},
  AROR_LocationManagerComponent:{GetInstance:()=>({GetLocationById:id=>targets[id]||null})},
  AROR_RebIntelHud:{ArmyName:r=>r===1?'US ARMY':'SOVIET ARMY'},AROR_FactionPresetsComponent:{GetInstance:()=>({GetPresetId:r=>r===1?'US_ARMY':(r===2?'USSR_ARMY':'OTHER')})},AROR_UIColors:{WARN:1,DANGER:2,Get:c=>'color'+c},
  AROR_NotificationManagerComponent:{Send_S:n=>sent.push(n)},AROR_Notification:Notification,AROR_ENotificationType:{HEADLINE:0},
  AROR_OperationThreatInfo:ThreatInfo,THREAT_ROW:';',THREAT_FIELD:'|',THREAT_BOARD_RESYNC:15,
  GROUND_SPEED:11,ROAD_FACTOR:1.35,GROUND_DISMOUNT:30,AIR_SPEED:40,AIR_LANDING:60,
  AROR_OperationManagerComponent:{ThreatArmyName:r=>r===1?'NATO':'USSR'},  AROR_GroundOperationForce:{Cast:f=>f&&f.ground?f:null},AROR_ReaperAirForce:{Cast:f=>f&&f.air?f:null}};
 const methods=new Set(['ThreatArmyName','Estimate','Describe','Countdown','DispatchHeadline','PublishThreatBoard_S','OnThreatBoardReplicated','AnnounceDispatch_S','GetEtaRemaining','EstimateArrival']);
 function compile(source,name){
  const m=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));
  if(!m)throw Error('Missing actual method '+name);
  const args=m[1].replace(/\bnotnull /g,'').replace(/array<[^>]+>\s+/g,'').replace(/\b(?:vector|bool|float|int|string|IEntity|AROR_\w+)\s+/g,'');
  let body=m[2].replace(/array<[^>]+> (\w+) = \{\};/g,'let $1=[];')
   .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
   .replace(/int (\w+) = (\w+) \/ 60;/g,'let $1 = Math.floor($2 / 60);')
   .replace(/\bstring (\w+);/g,"let $1='';").replace(/\b(?:bool|int|float) (\w+);/g,'let $1=0;')
   .replace(/\b(?:vector|IEntity|AROR_\w+) (\w+);/g,'let $1=null;')
   .replace(/\b(?:bool|int|float|string|vector|IEntity|AROR_\w+) (\w+) =/g,'let $1 =')
   .replace(/super\.(\w+)\(/g,'this.Base$1(')
   .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x).replace(/(?<![.\w])s_Instance/g,'this.s_Instance');
  for(const n of methods)body=body.replace(new RegExp('(?<![.\\w])'+n+'\\(','g'),'this.'+n+'(');
  return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
 }
 const eta={};for(const n of ['Estimate','Describe','Countdown'])eta[n]=compile(sources.eta,n);env.AROR_OperationEta=eta;

 // Arrival estimates and wording
 check(eta.Estimate(2200,false)===Math.round(2200*1.35/11)+30,'ground estimate: road allowance at about 40 km/h plus unloading');
 check(eta.Estimate(4000,true)===160&&eta.Estimate(4000,true)<eta.Estimate(4000,false),'helicopters are faster than the road over the same distance');
 check(eta.Estimate(-1,false)===-1,'no distance, no estimate');
 check(eta.Describe(-1)===''&&eta.Describe(45)==='ETA UNDER 1 MIN'&&eta.Describe(60)==='ETA ~1 MIN'&&eta.Describe(181)==='ETA ~4 MIN','headline estimate rounds up to whole minutes');
 check(eta.Countdown(185)==='ETA 3:05'&&eta.Countdown(600)==='ETA 10:00'&&eta.Countdown(-5)==='ETA 0:00','map countdown is minutes and seconds');

 // Remaining estimate on the operation
 const remaining=compile(sources.operation,'GetEtaRemaining');
 check(remaining.call({m_iEtaSeconds:-1,m_iEtaSetAt:0},50)===-1&&remaining.call({m_iEtaSeconds:120,m_iEtaSetAt:100},150)===70&&remaining.call({m_iEtaSeconds:120,m_iEtaSetAt:100},400)===0,'the estimate counts down from dispatch and stops at zero');

 // The executor times each lead package from where it is
 const estimateArrival=compile(sources.executor,'EstimateArrival');
 const at=(x,z)=>({GetOrigin:()=>[x,0,z]});
 const op=(id,role,type,target,state)=>({id,role,type,target,state,m_TacticalCommander:null,m_iEtaSeconds:-1,m_iEtaSetAt:0,m_bDebugRequest:false,
  GetId(){return this.id;},GetRole(){return this.role;},GetType(){return this.type;},GetTargetLocationId(){return this.target;},GetState(){return this.state;},
  IsTerminal(){return this.state>=states.SUCCEEDED;},UsesTacticalResponse:()=>true,GetEtaRemaining(now){return remaining.call(this,now);}});
 const tactical={m_vTown:[0,0,0],m_vOrigin:[0,0,5000],IsReserve:i=>i===2};
 const qrf=op('operation_1',1,types.QRF,'levie',states.SPAWNING);qrf.m_TacticalCommander=tactical;
 const executor={m_aForces:[
  {m_sOperationId:'operation_1',m_iPackageIndex:0,ground:true,m_Vehicle:at(0,3000)},
  {m_sOperationId:'operation_1',m_iPackageIndex:1,ground:true,air:true,m_Group:at(0,4000)},
  {m_sOperationId:'operation_1',m_iPackageIndex:2,ground:true,m_Vehicle:at(0,200)},
  {m_sOperationId:'operation_1',m_iPackageIndex:3,ground:true,m_bReservation:true,m_Vehicle:at(0,100)},
  {m_sOperationId:'other',m_iPackageIndex:0,ground:true,m_Vehicle:at(0,50)}]};
 check(estimateArrival.call(executor,qrf)===160,'the first lead package to arrive sets the estimate (the helicopter here)');
 executor.m_aForces.splice(1,1);
 check(estimateArrival.call(executor,qrf)===eta.Estimate(3000,false),'reserve waves, reservations and other operations are ignored');
 executor.m_aForces=[];
 check(estimateArrival.call(executor,qrf)===eta.Estimate(5000,false),'with no package located the launch site is used');

 // Dispatch headline once a response goes ACTIVE
 const army='US ARMY';
 const headline=compile(sources.alerts,'DispatchHeadline');
 check(headline(army,types.QRF,'Levie',240)==='US ARMY QRF DISPATCHED / Levie  ·  ETA ~4 MIN','QRF headline names the army, the target and the estimate');
 check(headline('SOVIET ARMY',types.ATTACK,'Quarry',30)==='SOVIET ARMY ATTACK INCOMING / Quarry  ·  ETA UNDER 1 MIN','attack headline');
 check(headline(army,types.QRF,'Levie',-1)==='US ARMY QRF DISPATCHED / Levie','no estimate, no ETA');
 targets={levie:{GetName:()=>'Levie',GetCenter:()=>[0,0,0]},quarry:{GetName:()=>'Quarry',GetCenter:()=>[0,0,0]}};
 const manager={m_aOperations:[],m_iClock:100,m_sThreatBoard:'',m_sThreatBoardStructure:'',m_iThreatBoardResync:0,m_aThreatInfos:[],m_iThreatBoardReceivedAt:0,s_Instance:null,BaseNotifyChanged(){},BaseTick(){}};
 manager.s_Instance=manager;
 for(const n of ['ThreatArmyName','NotifyChanged','AnnounceDispatch_S','PublishThreatBoard_S','OnThreatBoardReplicated','GetThreats','GetThreatEtaRemaining','Tick'])manager[n]=compile(sources.alerts,n);
 manager.DispatchHeadline=headline;
 manager.m_aOperations.push(qrf);qrf.state=states.PLANNED;manager.NotifyChanged(qrf);
 check(!sent.length,'nothing is announced while a response is only being prepared');
 qrf.state=states.ACTIVE;qrf.m_iEtaSeconds=240;qrf.m_iEtaSetAt=100;manager.NotifyChanged(qrf);
 check(sent.length===1&&sent[0].text==='NATO QRF DISPATCHED / Levie  ·  ETA ~4 MIN'&&sent[0].color==='color1','dispatch headline in warning amber for a QRF');
 const attack=op('operation_2',2,types.ATTACK,'quarry',states.ACTIVE);attack.m_iEtaSeconds=90;attack.m_iEtaSetAt=100;manager.m_aOperations.push(attack);
 manager.NotifyChanged(attack);check(sent[1].text.startsWith('USSR ATTACK INCOMING / Quarry')&&sent[1].color==='color2','attack headline in danger red');
 qrf.state=states.SUCCEEDED;manager.NotifyChanged(qrf);check(sent.length===2,'ending a response is not announced as a dispatch');
 targets.gone=null;const lost=op('operation_3',1,types.QRF,'gone',states.ACTIVE);manager.AnnounceDispatch_S(lost);check(sent.length===2,'an unknown target is never announced');

 // The board round trip: what clients draw
 qrf.state=states.ACTIVE;tick=0;manager.m_iThreatBoardResync=0;manager.PublishThreatBoard_S(true);
 const infos=[];manager.GetThreats(infos);
 check(infos.length===2&&infos[0].m_sTargetId==='levie'&&infos[0].m_eType===types.QRF&&infos[0].m_eRole===1&&infos[0].m_eState===states.ACTIVE&&infos[0].m_iEtaSeconds===240,'the board carries type, army, target, state and the estimate');
 check(infos[1].m_eType===types.ATTACK&&infos[1].m_iEtaSeconds===90,'every outstanding response is on the board');
 tick=30000;check(manager.GetThreatEtaRemaining(infos[0])===210,'clients count down locally from when the board arrived');
 tick=999000;check(manager.GetThreatEtaRemaining(infos[1])===0,'the local countdown stops at zero');
 const published=bumps;manager.m_iClock=105;manager.m_iThreatBoardResync=5;manager.PublishThreatBoard_S(false);
 check(bumps===published,'a countdown alone does not republish between resyncs');
 manager.m_iThreatBoardResync=0;manager.PublishThreatBoard_S(false);
 check(bumps===published+1&&manager.m_sThreatBoard.includes('1|1|levie|2|235'),'the resync republishes the countdown for late joiners');
 attack.state=states.FAILED;manager.m_iThreatBoardResync=5;manager.PublishThreatBoard_S(false);manager.GetThreats(infos);
 check(infos.length===1&&infos[0].m_sTargetId==='levie','a finished response leaves the board at once');
 qrf.state=states.CANCELLED;manager.PublishThreatBoard_S(false);manager.GetThreats(infos);
 check(!infos.length&&manager.m_sThreatBoard==='','the board empties when nothing is outstanding');
 const idle=bumps;manager.m_iThreatBoardResync=0;manager.PublishThreatBoard_S(false);check(bumps===idle,'an empty board is never republished');
 const planned=op('operation_4',1,types.ATTACK,'quarry',states.PLANNED);manager.m_aOperations.push(planned);manager.PublishThreatBoard_S(false);manager.GetThreats(infos);
 check(infos.length===1&&infos[0].m_iEtaSeconds===-1,'a prepared attack shows on the map before it has an estimate');

 // Marker captions
 const caption=compile(sources.markers,'Caption');
 check(manager.ThreatArmyName(1)==='NATO'&&manager.ThreatArmyName(2)==='USSR'&&manager.ThreatArmyName(0)==='SOVIET ARMY','alerts call the US Army NATO and the Soviet Army USSR, other presets by name');
 check(caption({m_eRole:1,m_eType:types.ATTACK,m_eState:states.PLANNED},-1)==='NATO ATTACK PREPARING','preparing attack caption');
 check(caption({m_eRole:1,m_eType:types.ATTACK,m_eState:states.ACTIVE},185)==='UNDER NATO ATTACK  ·  ETA 3:05','an incoming attack marks the site as under attack with the countdown');
 check(caption({m_eRole:2,m_eType:types.ATTACK,m_eState:states.ACTIVE},0)==='UNDER USSR ATTACK','the under-attack caption stays after arrival for as long as the attack runs');
 check(caption({m_eRole:2,m_eType:types.QRF,m_eState:states.ACTIVE},185)==='USSR QRF INBOUND  ·  ETA 3:05','en-route QRF caption counts down');
 check(caption({m_eRole:1,m_eType:types.QRF,m_eState:states.ACTIVE},0)==='NATO QRF ON SITE','arrived QRF caption');

 return {assertions,note:'Native headlines, map widgets and replication require Workbench Play.'};
}
if(typeof module!=='undefined')module.exports={runOperationAlertsTests};
if(typeof require!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path'),root=path.resolve(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Operations');
 const read=name=>fs.readFileSync(path.join(root,name+'.c'),'utf8');
 console.log(JSON.stringify(runOperationAlertsTests({eta:read('AROR_OperationEta'),operation:read('AROR_Operation'),executor:read('AROR_OperationExecutorComponent'),
  alerts:read('AROR_OperationAlerts'),markers:read('AROR_OperationMapMarkers')})));
}
