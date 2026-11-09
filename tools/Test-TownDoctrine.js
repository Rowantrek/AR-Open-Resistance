// Executes selected actual Enforce method bodies with native services mocked.
// This is a policy regression harness, not an Enforce compiler or navigation test.
// Run: node tools/Test-TownDoctrine.js
const fs = require('fs'), path = require('path');
const root = path.resolve(__dirname, '../AROpenResistance/Open Resistance/Scripts/Game');
const read = file => fs.readFileSync(path.join(root, file), 'utf8');
const town = read('Towns/AROR_TownWavesComponent.c');
const tactical = read('Operations/AROR_QRFTacticalCommander.c');
const managerSource = read('Operations/AROR_OperationManagerComponent.c');
const executor = read('Operations/AROR_OperationExecutorComponent.c');
const capture = read('Capture/AROR_CaptureLocationAction.c');
let assertions = 0, server = true, manager = null, randomHigh = false;
const check = (ok, label) => { assertions++; if (!ok) throw Error(label); };
const add = (a,b) => a.map((v,i) => v+b[i]);
const sub = (a,b) => a.map((v,i) => v-b[i]);
const mul = (a,n) => a.map(v => v*n);
const distance = (a,b) => Math.hypot(a[0]-b[0], a[2]-b[2]);
const normalize = v => mul(v, 1 / Math.hypot(...v));
const roles = {RESISTANCE:0, OCCUPIERS:1, INVADERS:2};
const states = {PLANNED:0, SPAWNING:1, ACTIVE:2, SUCCEEDED:3, FAILED:4, CANCELLED:5};
for (const [name,fn] of Object.entries({Count(){return this.length;}, IsEmpty(){return !this.length;},
  Insert(v){this.push(v);}, Clear(){this.length=0;}, Contains(v){return this.includes(v);},
  Copy(v){this.splice(0,this.length,...v);}})) Object.defineProperty(Array.prototype,name,{value:fn});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length;}});
const env = {
  Math:{Max:Math.max, Min:Math.min, Clamp:(v,a,b)=>Math.max(a,Math.min(b,v)), Ceil:Math.ceil,
    Mod:(a,b)=>a%b, Atan2:Math.atan2, Sin:Math.sin, Cos:Math.cos, PI2:Math.PI*2,
    RandomInt:(a,b)=>randomHigh?b-1:a},
  vector:{DistanceXZ:distance}, Vector:(...v)=>v, add, sub, mul, normalize,
  Replication:{IsServer:()=>server,BumpMe:()=>{}}, Print:()=>{},
  LogLevel:{NORMAL:0,WARNING:1}, string:{Empty:'',Format:()=>''},
  AROR_EFactionRole:roles, AROR_ELocationType:{TOWN:0,CITY:1}, AROR_EOperationState:states,
  AROR_OperationManagerComponent:{GetInstance:()=>manager},
  AROR_CommanderManagerComponent:{GetInstance:()=>({IsCampaignActive:()=>true})},
  AROR_GarrisonPolicy:{IsEnemyRole:r=>r===1||r===2},
  AROR_UIColors:{DANGER:1},
  AROR_TownWavesComponent:{Cast:x=>x},
  FactionAffiliationComponent:{Cast:x=>x},
  AROR_FactionRolesComponent:{GetInstance:()=>({GetRoleOfFaction:(f)=>f.role===1})},
  AROR_QRFSearchPlan:class {ReportContact_S(){} BeginSearch_S(){}},
  EDamageType:{HEALING:1,REGENERATION:2},InstigatorType:{INSTIGATOR_GM:9},NowSeconds:()=>100,
};
// GetRoleOfFaction uses an out enum. The contact tests use Occupiers (1); emulate assignment below.
function compile(source,name) {
  const match = source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if (!match) throw Error('Missing method '+name);
  let body = match[2]
    .replace(/foreach \([^,\n]+ (\w+), [^:\n]+ (\w+) : ([^)]+)\)/g,'for (const [$1,$2] of $3.entries())')
    .replace(/foreach \([^:\n]+ (\w+) : ([^)]+)\)/g,'for (const $1 of $2)')
    .replace(/(?:ref )?array<[^>]+> (\w+) = \{\};/g,'let $1 = [];')
    .replace(/vector (\w+);/g,'let $1 = [0,0,0];')
    .replace(/\b(?:AROR_\w+|FactionAffiliationComponent|Faction|IEntity|bool|int|float|string|vector) (\w+)/g,'let $1')
    .replace(/let (\w+);/g,'let $1 = 0;')
    .replace(/"0 0 1"/g,'[0,0,1]')
    .replace(/\bheading\.Length\(\)/g,'MathNative.hypot(...heading)')
    .replace(/heading\.Normalize\(\);/g,'heading = normalize(heading);')
    .replace(/heading = m_vOrigin - center;/g,'heading = sub(m_vOrigin, center);')
    .replace(/point = center \+ heading \* m_fCoverDistance \+ right \* \(\(rank - \(count - 1\) \* 0\.5\) \* 20\);/g,
      'point = add(add(center, mul(heading, m_fCoverDistance)), mul(right, ((rank - (count - 1) * 0.5) * 20)));')
    .replace(/point = center \+ heading \* \(m_fFlankDistance \* 0\.35\) \+ right \* \(side \* flank\);/g,
      'point = add(add(center, mul(heading, m_fFlankDistance * 0.35)), mul(right, side * flank));')
    .replace(/point = center \+ Vector\(Math.Sin\(angle\), 0, Math.Cos\(angle\)\) \* radius;/g,
      'point = add(center, mul(Vector(Math.Sin(angle), 0, Math.Cos(angle)), radius));')
    .replace(/point = m_vTown \+ \(point - m_vTown\) \* \(limit \/ distance\);/g,
      'point = add(m_vTown, mul(sub(point, m_vTown), limit / distance));')
    .replace(/\(crackdown \+ 1\) \/ 2/g,'MathNative.floor((crackdown + 1) / 2)')
    .replace(/roles.GetRoleOfFaction\(victimFaction, victimRole\)/g,'((victimRole = victimFaction.role), roles.GetRoleOfFaction(victimFaction))')
    .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
    .replace(/(?<![.\w])(?:PHASE_IDLE|PHASE_ALERT|PHASE_WAVES|STAGING_RETRY_SECONDS|STAGING_ATTEMPTS|CONTEST_LOG_SECONDS)\b/g,x=>'this.'+x);
  const methods = ['UsesShockDoctrine','ActiveSectorIndex','ActiveSectorCount','IsTownCoverElement','IsTargetLost',
    'BoundTownPoint','ResistanceSupport','Executor','SkipWave','WaveElements','FindStaging','PostPosition',
    'CompassSide','TrackOperation','NextInterval','LogResponseEnd','ResetResponse','ClearWatches','IsReady',
    'RefreshWatches','EndResponse','KillThreshold','IsResistanceNear','StartResponse','ReportContact_S',
    'ResistancePlayersNear','ContactNames','Headline'];
  body = body.replace(new RegExp('(?<![.\\w])('+methods.join('|')+')\\(', 'g'),'this.$1(');
  const args = match[1].replace(/\b(?:AROR_\w+|BaseDamageContext|int|float|bool|vector|string) /g,'');
  return new Function(...Object.keys(env),'MathNative','return function('+args+'){'+body+'}')(...Object.values(env), Math);
}
const configure=compile(town,'ConfigureCommander_S');
function component() {
  return {m_bFactionDoctrine:true,m_eRole:roles.INVADERS,m_fShockTempo:0.65,m_iShockSplitFromWave:1,
    m_fShockDismountDistance:160,m_iShockRetaskSeconds:10,m_iDeliberateRetaskSeconds:25,
    m_iDoctrineContactMaxAge:45,m_fDoctrineFlankDistance:70,m_fDeliberateSupportDistance:100,
    m_fShockWithdrawFraction:0.25,m_fDeliberateWithdrawFraction:0.45,
    m_iWaveInterval:150,m_iWaveIntervalJitter:30,m_iWaves:3,m_iExtraElements:0,
    m_Location:{GetLocationType:()=>0,GetId:()=> 'town'}, UsesShockDoctrine:compile(town,'UsesShockDoctrine')};
}
const interval=compile(town,'NextInterval'), size=compile(town,'WaveElements');
const c=component(), shock={m_eRole:2}, deliberate={m_eRole:1};
configure.call(c,shock); configure.call(c,deliberate);
check(shock.m_bShockAssault && shock.m_iRetaskSeconds===10 && shock.m_fWithdrawFraction===0.25,'Invader shock settings');
check(!deliberate.m_bShockAssault && deliberate.m_iRetaskSeconds===25 && deliberate.m_fWithdrawFraction===0.45,'Occupier deliberate settings');
check(shock.m_iContactMaxAge===45 && deliberate.m_iContactMaxAge===45,'Both armies expire contacts');
c.m_bFactionDoctrine=false; const legacy={}; configure.call(c,legacy);
check(!legacy.m_bTownDoctrine,'Opt-out preserves ordinary tactics'); c.m_bFactionDoctrine=true;
server=false;const client={};configure.call(c,client);check(!client.m_bTownDoctrine,'Client cannot configure tactical state');server=true;
for(const [role,min,max] of [[2,78,117],[1,120,180]]) {
  c.m_eRole=role;randomHigh=false;check(interval.call(c)===min,'Minimum wave interval role '+role);
  randomHigh=true;check(interval.call(c)===max,'Maximum wave interval role '+role);
}
for(let level=1;level<=5;level++) {
  check(size.call(c,1,level)===[2,2,3,3,4][level-1],'Crackdown finite size '+level);
  check(size.call(c,3,level)===[3,3,4,4,5][level-1],'Last wave adds one '+level);
}
function commander(role,count=3) {
  const result={m_eRole:role,m_iPackages:count,m_aInitial:Array(count).fill(6),m_aLost:Array(count).fill(false),
    m_vTown:[0,0,0],m_vContact:[0,0,0],m_vOrigin:[0,0,1000],m_bHasContact:true,
    m_fTownRadius:250,m_fMargin:150,m_fSecurityRadius:300,m_bKnowsTargetControl:true,m_eTargetControl:0};
  for(const method of ['ActiveSectorCount','ActiveSectorIndex','IsTargetLost','IsTownCoverElement','TownInfantryPoint','BoundTownPoint','UpdateContact_S'])
    result[method]=compile(tactical,method);
  configure.call(c,result);return result;
}
const s=commander(2),d=commander(1);
check(!s.IsTownCoverElement(0) && d.IsTownCoverElement(0),'Only deliberate force holds a covering team');
check(!commander(1,2).IsTownCoverElement(0),'Small forces keep both teams advancing');
const left=s.TownInfantryPoint(1,0),right=s.TownInfantryPoint(2,0);
check(left[0]*right[0]<0 && distance(left,right)>130,'Shock teams receive opposite flanks');
check(distance(s.TownInfantryPoint(1,1),[0,0,0])<13,'Shock flank closes on contact after ingress');
check(distance(d.TownInfantryPoint(0,0),[0,0,0])>=100,'Deliberate covering element holds back');
const large=commander(2,8);
check(distance(large.TownInfantryPoint(1,0),large.TownInfantryPoint(5,0))>40,'Same-side flanking teams receive distinct ingress points');
s.m_vContact=[200,0,0];check(distance(s.TownInfantryPoint(1,2),s.m_vTown)<13,'Assault returns to flag after contact task');
for(const role of [1,2])for(let count=1;count<=8;count++)for(let index=0;index<count;index++)for(let step=0;step<6;step++) {
  const cmd=commander(role,count);cmd.m_vContact=[900,0,900];
  check(distance(cmd.TownInfantryPoint(index,step),cmd.m_vTown)<=300.001,'Every task stays within security boundary');
}
s.m_iNextRetask=0;s.m_iContactVersion=0;s.m_vLastTaskContact=[0,0,0];
s.UpdateContact_S({m_vPosition:[100,0,0],m_iSequence:1},10);
s.UpdateContact_S(null,11);check(s.m_bHasContact,'Retask throttle avoids order churn');
s.UpdateContact_S(null,20);check(!s.m_bHasContact,'Expired contact clears at next allowed retask');
const report={m_eRole:2,m_vPosition:[0,0,0],m_iTime:100,m_iSequence:1};
const contactManager={IsReady:()=>true,m_aCombatContacts:[report],m_iClock:145};
const getFresh=compile(managerSource,'GetRecentCombatContact');
check(getFresh.call(contactManager,2,[0,0,0],300,45)===report,'Contact valid at age boundary');
contactManager.m_iClock=146;check(!getFresh.call(contactManager,2,[0,0,0],300,45),'Contact expires beyond 45 seconds');
contactManager.m_iClock=100;check(!getFresh.call(contactManager,1,[0,0,0],300,45),'Reports never cross army roles');
function launchFixture(role=2) {
  const t=component();t.m_eRole=role;
  Object.assign(t,{PHASE_IDLE:0,PHASE_WAVES:2,STAGING_RETRY_SECONDS:20,STAGING_ATTEMPTS:6,
    m_iClock:100,m_iStartedAt:0,m_iStagingFailures:0,m_iSplitMinElements:3,m_iAxisIndex:0,
    m_fDismountDistance:220,m_aLastAxes:[],m_fStagingMinDistance:1000,m_fStagingMaxDistance:2000,
    WaveElements:wave=>wave===3?4:3,PostPosition:()=>[0,0,0],CompassSide:()=> 'north',TrackOperation:()=>{},
    NextInterval:()=>100,SkipWave(){this.skipped=true;this.m_iStagingFailures=0;},
    FindStaging(executor,post,axis,avoid,staging){staging[0]=(axis+1)*1000;return true;},
    Executor:()=>({FindTownWaveRecipeId_S:()=> 'recipe'})});
  const requests=[];
  manager={GetReadinessLevel:()=>3,GetCrackdownLevel:()=>3,RequestTownWave_S(...args){requests.push(args);return 'op'+requests.length;}};
  // GetInstance is evaluated at runtime, so this fixture manager is shared by the extracted method.
  return {t,requests};
}
const launch=compile(town,'LaunchWave');
for(const [role,wave,columns] of [[2,1,2],[1,1,1],[1,3,2]]) {
  const {t,requests}=launchFixture(role);launch.call(t,wave);
  check(requests.length===columns,'Doctrine column count '+role+'/'+wave);
  check(requests.reduce((n,r)=>n+r[6],0)===(wave===3?4:3),'Split preserves total element budget');
  check(requests.every(r=>r[7]===(role===2?160:220)),'Doctrine dismount distance');
}
for(const failure of ['roads','request','services']) {
  const {t}=launchFixture();
  if(failure==='roads')t.FindStaging=()=>false;
  if(failure==='request')manager.RequestTownWave_S=()=>'';
  if(failure==='services')t.Executor=()=>null;
  for(let attempt=0;attempt<7;attempt++)launch.call(t,1);
  check(t.skipped,'Bounded launch failure: '+failure);
}
{
  const {t,requests}=launchFixture();let searches=0;
  t.FindStaging=(executor,post,axis,avoid,staging)=>{searches++;staging[0]=1000;return searches===1;};
  launch.call(t,1);
  check(requests.length===1 && requests[0][6]===3 && requests[0][8]===0,'One safe road retains the complete finite wave');
}
let cancelled=[];
manager={GetOperation:id=>({IsTerminal:()=>id==='done'}),ResolveOperation_S:(id)=>cancelled.push(id)};
const ending={PHASE_IDLE:0,m_iPhase:2,m_iWavesSent:1,m_aOperationIds:['planned','active','done'],
  m_Location:{GetId:()=> 'town'},LogResponseEnd:()=>{},m_bContestActive:false,ResetResponse:()=>{},
  m_aSeen:[],m_aContactPlayers:[],ClearWatches:()=>{},m_iClock:100,m_iRearmSeconds:1800};
env.s_ActiveResponse=null;const endResponse=compile(town,'EndResponse');endResponse.call(ending,true);
check(cancelled.join(',')==='planned,active','Town end cancels only its outstanding child operations');
check(ending.m_iRearmAt===1900,'Response cooldown starts at end');
const post=component();Object.assign(post,{m_iPhase:0,m_iClock:100,m_iRearmAt:0,ResistanceSupport:()=>55});
let escalation=0;
manager={RecordTownEscalation_S:()=>escalation++};
const postLocation={GetId:()=> 'town',GetOwner:()=>({FindComponent:()=>post})};
compile(town,'RecordPostTaken_S').call({PHASE_IDLE:0},postLocation,1,25);
check(post.m_bPostTakenTrigger && post.m_eRole===1 && post.m_fPostSupportBefore===25,'Post queues original holder with pre-capture support');
check(escalation===1,'Taking the post escalates once');
let started=false;
Object.assign(post,{m_Location:{GetControllingRole:()=>0,GetId:()=> 'town'},PHASE_IDLE:0,PHASE_ALERT:1,CONTEST_LOG_SECONDS:30,
  RefreshWatches:()=>{},KillThreshold:()=>3,m_iKills:0,IsResistanceNear:()=>true,StartResponse:()=>{started=true;}});
compile(town,'Tick').call(post);
check(started,'Immediate support flip and zero processed kills cannot swallow post response');
post.m_iAlertDelayMin=30;post.m_iAlertDelayMax=90;post.m_iLockdownSeconds=600;
post.m_aSeen=[];post.ResetResponse=()=>{};post.ResistancePlayersNear=()=>1;post.ContactNames=()=>'';post.Headline=()=>{};
manager=null;
compile(town,'StartResponse').call(post);
check(post.m_fSupportAtContact===25 && !post.m_bPostTakenTrigger,'Response rollback baseline precedes capture support reward');
let contacts=[];
manager={ReportCombatContact_S:(...args)=>contacts.push(args)};
const victimFaction={role:1,IsFactionEnemy:other=>other.role!==1};
const victim={FindComponent:()=>({GetAffiliatedFaction:()=>victimFaction})};
let attackerRole=0;
const attacker={FindComponent:()=>({GetAffiliatedFaction:()=>({role:attackerRole})}),GetOrigin:()=>[80,0,40]};
const damage={damageValue:5,damageType:0,instigator:{GetInstigatorEntity:()=>attacker,GetInstigatorType:()=>0,GetInstigatorPlayerID:()=>1}};
const watch={m_sReportingTownId:'town',m_Entity:victim,m_eReportingRole:1,
  ReportContact_S:compile(town,'ReportContact_S')};
const onDamage=compile(town,'OnDamage');
onDamage.call(watch,damage);
check(contacts.length===1 && contacts[0][0]===1 && contacts[0][3]===false,'Hostile hit shares one snapshot without double escalation');
check(watch.m_iPlayerId===1 && watch.m_iHitAt===100,'Contact reporting preserves kill credit');
attackerRole=1;watch.m_iPlayerId=0;onDamage.call(watch,damage);
check(contacts.length===1 && watch.m_iPlayerId===0,'Friendly hit sends no combat report or defender kill credit');
attackerRole=0;damage.damageType=1;onDamage.call(watch,damage);check(contacts.length===1,'Healing sends no report');damage.damageType=0;
server=false;onDamage.call(watch,damage);check(contacts.length===1,'Client damage callback cannot publish reports');server=true;
watch.m_sReportingTownId='';onDamage.call(watch,damage);check(contacts.length===1,'Civilian watches send no military reports');
check(capture.includes('RecordPostTaken_S(location, holder, supportBefore)'),'Post capture preserves pre-reward support baseline');
check(town.includes('!m_bPostTakenTrigger && m_iKills < KillThreshold()'),'Taking post cannot bypass relief trigger');
check(executor.includes('if (operation.m_bAssaultWave || operation.GetType() == AROR_EOperationType.ATTACK)'),'Waves and attack orders cannot also pacify support');
check(executor.includes('commander.IsTownCoverElement(force.m_iPackageIndex)'),'Cover task uses defend waypoint');
check(executor.includes('force.m_iTacticalNextOrder = Math.Min(force.m_iTacticalNextOrder, m_iExecutorClock)'),'Fresh doctrine retask bypasses an old security dwell');
check(executor.includes('contactAge = commander.m_iContactMaxAge'),'Executor applies doctrine report expiry');
for(const source of [town,read('Towns/AROR_TownLifeComponent.c')]) {
  const code=source.replace(/\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"/g,'');
  check(!code.includes('%'),'Town code has no unsupported remainder operator');
}
console.log(JSON.stringify({assertions,nativeVerified:false,note:'Actual-source policy tests; Workbench compile, navigation, combat and dedicated-server replication remain unverified.'}));
