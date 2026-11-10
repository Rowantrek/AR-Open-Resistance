// Read-only gameplay review: execute current source counterexamples and existing mocked suites.
// No Enforce compilation or native load/performance test. Generated suite artifacts go to TEMP.
const fs=require('fs'),path=require('path'),cp=require('child_process'),assert=require('assert/strict'),os=require('os');
const root=path.resolve(__dirname,'..'),core='AROpenResistance/Open Resistance/';
const read=file=>fs.readFileSync(path.join(root,file),'utf8');
const git=(...args)=>cp.execFileSync('git',args,{cwd:root,encoding:'utf8'}).trim();
const snapshot=git('rev-parse','HEAD');
const cutoff='2026-11-09T07:47:09Z';
const commits=git('log','chicken-dev','--since='+cutoff,'--format=%H%x09%cI%x09%s').split('\n').filter(Boolean).map(line=>{
 const [sha,committedAt,subject]=line.split('\t');
 return {sha,committedAt,subject,claude:/Co-Authored-By: Claude/i.test(git('show','-s','--format=%B',sha))};
});
const tmp=fs.mkdtempSync(path.join(os.tmpdir(),'AROR-review-20261110-'));
const hook=path.join(tmp,'redirect-artifacts.cjs');
fs.writeFileSync(hook,`const fs=require('fs'),path=require('path');
const originalRead=fs.readFileSync,originalWrite=fs.writeFileSync;
const root=${JSON.stringify(root)},tmp=${JSON.stringify(tmp)};
function redirected(p){if(typeof p!=='string')return p;const abs=path.resolve(p),rel=path.relative(root,abs);return rel.startsWith('docs'+path.sep)&&!rel.startsWith('..')?path.join(tmp,rel):p;}
fs.writeFileSync=function(p,...args){const out=redirected(p);if(out!==p)fs.mkdirSync(path.dirname(out),{recursive:true});return originalWrite.call(this,out,...args);};
fs.readFileSync=function(p,...args){const out=redirected(p);return originalRead.call(this,out!==p&&fs.existsSync(out)?out:p,...args);};
`);
const results=[];
function run(name,args=[]){
 const ps=name.endsWith('.ps1');
 const p=cp.spawnSync(ps?'powershell.exe':process.execPath,ps?['-NoProfile','-ExecutionPolicy','Bypass','-File',path.join(__dirname,name),...args]:['--require',hook,path.join(__dirname,name),...args],
  {cwd:root,encoding:'utf8',timeout:180000,windowsHide:true,env:{...process.env,ELECTRON_RUN_AS_NODE:'1'}});
 const output=((p.stdout||'')+(p.stderr||'')).replace(/^.*crashpad.*(?:\r?\n|$)/gm,'').trim();
 const assertions=Number(output.match(/["']?assertions["']?\s*:\s*(\d+)/)?.[1]||0);
 const completed=ps?/Checked|OK|Verified|passed/i.test(output):assertions>0||/checks passed/i.test(output);
 const passed=p.status===0&&!p.error&&completed&&!/^\w*Error(?: \[[^\]]+\])?:/m.test(output);
 const result={name,passed,assertions,exitCode:p.status,error:p.error?.message,output};results.push(result);
 console.log(`${passed?'PASS':'FAIL'} ${name} ${assertions}`);
}
const militia=read(core+'Scripts/Game/Towns/AROR_TownMilitiaComponent.c');
const site=read(core+'Scripts/Game/Locations/AROR_SiteLifeComponent.c');
const high=read(core+'Scripts/Game/HighCommand/AROR_HighCommandComponent.c');
const force=read(core+'Scripts/Game/Common/AROR_ForceSpawner.c');
const executor=read(core+'Scripts/Game/Operations/AROR_OperationExecutorComponent.c');
const names=new Set([militia,site,high,force,executor].flatMap(s=>[...s.matchAll(/^\t[^\r\n]*?\b(\w+)\([^\r\n]*\)\r?\n\t\{/gm)].map(m=>m[1])));
for(const [n,fn]of Object.entries({Count(){return this.length},Find(v){return this.indexOf(v)},IsEmpty(){return !this.length},Insert(v){this.push(v)},Clear(){this.length=0},Remove(i){this.splice(i,1)},RemoveItem(v){const i=this.indexOf(v);if(i>=0)this.splice(i,1)},Contains(v){return this.includes(v)}}))Object.defineProperty(Array.prototype,n,{value:fn,configurable:true});
Object.defineProperty(String.prototype,'IsEmpty',{value(){return !this.length},configurable:true});
let deleted=[],requests=[];
const component={access:{},compartments:{},usage:{}};
const env={FORM_UP_SECONDS:30,Replication:{IsServer:()=>true},Print(){},LogLevel:{NORMAL:0},string:{Empty:'',Format:()=>''},
 Math:{Floor:Math.floor,Min:Math.min,Max:Math.max,Clamp:(v,a,b)=>Math.max(a,Math.min(b,v))},AROR_ELocationType:{TOWN:0,CITY:3},
 AROR_EFactionRole:{RESISTANCE:0},AROR_EMilitiaKind:{INFANTRY:0,APC:1,ARMOUR:2},AROR_EHighCommandEmbarkPhase:{TO_SITE:1},
 AROR_EHighCommandOrder:{HOLD:0},AROR_ForceSpawner:{IsAlive:e=>!!e&&!e.dead,IsDestroyed:e=>!e||e.destroyed},
 AROR_GroundOperationForce:{Cast:f=>f?.ground||f?.air?f:null},AROR_ReaperAirForce:{Cast:f=>f?.air?f:null},
 SCR_EntityHelper:{DeleteEntityAndChildren:e=>deleted.push(e)},
 CompartmentAccessComponent:{Cast:x=>x},BaseCompartmentManagerComponent:{Cast:x=>x},SCR_AIVehicleUsageComponent:{Cast:x=>x},
 PilotCompartmentSlot:{Cast:s=>s?.driver?s:null},TurretCompartmentSlot:{Cast:s=>s?.turret?s:null},CargoCompartmentSlot:{Cast:s=>s?.cargo?s:null},
 ECloseDoorAfterActions:{CLOSE_DOOR:1,INVALID:0},EGetOutType:{TELEPORT:0},
 vector:{Zero:[0,0,0],DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},Vector:(...v)=>v};
function method(src,name,track=false){
 const m=src.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}','m'));assert(m,'actual method '+name);
 let body=m[2].replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g,'for(const [$1,$2] of $3.entries())')
 .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for(const $1 of $2)')
 .replace(/array<[^>]+> (\w+) = \{\};/g,'let $1=[];')
 .replace(/\bvector (\w+);/g,'let $1=[0,0,0];')
 .replace(/\b(?:bool|int|float|string|IEntity|AIAgent|SCR_\w+|Base\w+|CompartmentAccessComponent|AROR_\w+) (\w+)/g,'let $1')
 .replace(/let (\w+);/g,'let $1=0;').replace(/(?<![.\w])m_\w+/g,n=>'this.'+n)
 .replace(/(?<![.\w])(\w+)\(/g,(s,n)=>names.has(n)?'this.'+s:s);
 if(track)body=body.replace('return best >= 0;','return {found:best>=0,lead,air,landing};');
 if(name==='WaveElements')body=body.replace('(crackdown + 1) / 2','Math.Floor((crackdown + 1) / 2)'); // Enforce int division
 const args=m[1].replace(/\b(?:out |notnull )?\w+(?:<[^>]+>)?\s+(\w+)/g,'$1');
 return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')(...Object.values(env));
}
env.AROR_ForceSpawner.CountLiving=method(force,'CountLiving');
const observations=[];
function observe(id,condition,detail){assert(condition,'counterexample not reproduced: '+id);observations.push({id,reproduced:true,...detail});}
const losses=method(militia,'CountLosses_S');
const dormant={GetAgents:()=>{},IsDormant:()=>true,GetDormantAliveCount:()=>12};
const book={m_aInfantry:[12],m_aApcs:[0],m_aArmour:[0]};
losses.call(book,{m_aGroups:[dormant],m_aVehicles:[]},0);
observe('militia_dormancy_erases_purchase',book.m_aInfantry[0]===0,{savedInfantry:12,dormantSurvivors:12,observedInfantry:book.m_aInfantry[0]});
const survivingCrew={GetAgents:()=>{},manualLifecycle:true};
const post={m_aGroups:[],m_aVehicles:[{destroyed:true}],m_aVehicleKinds:[1],m_aCrews:[survivingCrew],m_aSeated:[true]};
losses.call({m_aInfantry:[0],m_aApcs:[1],m_aArmour:[0]},post,0);
observe('destroyed_militia_vehicle_forgets_live_crew',post.m_aCrews.length===0&&!deleted.includes(survivingCrew),{trackedCrews:post.m_aCrews.length,crewDeleted:false});
const standDown=method(militia,'StandDown_S'),claimed={claimed:true,playerAboard:true};deleted=[];
standDown.call({m_aTownIds:['town'],m_iClock:0,m_aPosts:[],CountLosses_S:losses},
 {m_sTownId:'town',m_iRaisedAt:0,m_aGroups:[],m_aCrews:[],m_aVehicles:[claimed]});
observe('militia_retirement_deletes_claimed_player_vehicle',deleted.includes(claimed),{claimed:true,playerAboard:true,deletionRequested:true});
deleted=[];const oldSite={m_aPatrols:[{id:1}],m_aWorkers:[{id:2}],m_aPatrolLostAt:[],m_aWorkerLostAt:[],m_Truck:{id:3},m_aHelicopters:[{id:4}],m_aHelicopterLostAt:[0]};
method(site,'Forget').call(oldSite);
observe('site_handover_discards_live_ownership',!oldSite.m_aPatrols.length&&!oldSite.m_aWorkers.length&&!oldSite.m_Truck&&!oldSite.m_aHelicopters[0]&&!deleted.length,{retainedGroups:0,retainedVehicles:0,cleanupCalls:deleted.length});
const driver={driver:true,IsCompartmentAccessible:()=>true,IsOccupied:()=>false,GetOccupant:()=>null};
const cargo={cargo:true,IsCompartmentAccessible:()=>true,IsOccupied:()=>false,GetOccupant:()=>null};
const actors=Array.from({length:3},(_,id)=>({id,FindComponent:()=>({GetCompartment:()=>null,IsInCompartment:()=>false,IsGettingIn:()=>false,IsGettingOut:()=>false,
 GetInVehicle:(v,slot)=>{requests.push({id,slot:slot===driver?'driver':'cargo'});return true}})}));
const vehicle={FindComponent:t=>t===env.BaseCompartmentManagerComponent?{GetCompartments:out=>out.push(driver,cargo)}:{GetPilotCompartmentSlot:()=>driver}};
method(high,'SeatSquad_S')({m_Vehicle:vehicle,m_Group:{GetAgents:out=>out.push(...actors.map(e=>({GetControlledEntity:()=>e})))}});
observe('hicomm_boarding_reuses_unacknowledged_driver_seat',requests.length===3&&requests.every(r=>r.slot==='driver'),{acceptedAsyncRequests:requests});
let finished;
method(high,'UpdateEmbark_S').call({m_iBoardTimeout:60,SeatSquad_S:()=>false,CountMounted_S:()=>1,VehicleName:()=> 'truck',FinishEmbark_S:(s,m,mounted)=>finished=mounted},
 {m_eEmbarkPhase:2,m_Vehicle:{},m_bFormed:true,m_iEmbarkStarted:1000,m_iAlive:6},62001);
observe('hicomm_passenger_only_counts_as_mounted',finished===true,{livingMembers:6,driverPresent:false,seatedPassengers:1,reportedMounted:finished});
const tracked=method(executor,'GetOperationTrack_S',true).call({m_aForces:[
 {air:true,m_sOperationId:'joint',m_vLanding:[500,0,0],m_Vehicle:{GetOrigin:()=>[1000,0,0]}},
 {ground:true,m_sOperationId:'joint',m_Vehicle:{GetOrigin:()=>[100,0,0]}}
]},'joint',[0,0,0],[0,0,0],false,[0,0,0]);
observe('joint_track_mislabels_ground_head_as_air',tracked.air&&tracked.lead[0]===100,{published:tracked,observationRequired:false});
const {parseConfig,objects,value}=require('./Audit-WCSAssets');
function walk(nodes,predicate,out=[]){for(const n of nodes){if(predicate(n))out.push(n);walk(n.children||[],predicate,out)}return out;}
const managers=parseConfig(read(core+'Worlds/AROR_Everon/AROR_Everon_Chicken_Layers/Managers.layer'));
const profiles=walk(managers,n=>n.head[0]==='AROR_ForceProfile');
const profileById=new Map(profiles.map(n=>[value(n,'m_sId'),n]));
const recipes=walk(managers,n=>n.head[0]==='AROR_GroundQRFRecipe').map(n=>({id:value(n,'m_sId'),role:Number(value(n,'m_eRole')),wave:Number(value(n,'m_iAssaultWave',0)),
 minReadiness:Number(value(n,'m_iMinimumReadinessLevel',1)),maxReadiness:Number(value(n,'m_iMaximumReadinessLevel',5)),
 elements:(n.children.find(c=>c.head[0]==='m_aProfileIds')?.children||[]).flatMap(c=>c.head)}));
const waveCounts=method(read(core+'Scripts/Game/Towns/AROR_TownWavesComponent.c'),'WaveElements');
const matrix=[];
for(const role of [1,2])for(const readiness of [1,2,3,4,5])for(const crackdown of [1,2,3,4,5])for(const city of [false,true]){
 const waves=[];
 for(const wave of [1,2,3]){
  const r=recipes.find(r=>r.role===role&&r.wave===wave&&readiness>=r.minReadiness&&readiness<=r.maxReadiness);
  const count=waveCounts.call({m_iExtraElements:0,m_iWaves:3,m_Location:{GetLocationType:()=>city?3:0}},wave,crackdown);
  const ids=(r?.elements||[]).slice(0,count);
  waves.push({wave,requestedElements:count,resolvedElements:ids.length,personnelReservation:ids.reduce((s,id)=>s+Number(value(profileById.get(id),'m_iPersonnelBudget',0)),0),profiles:ids});
 }
 matrix.push({role,readiness,crackdown,city,waves,totalGroundPackages:waves.reduce((s,w)=>s+w.resolvedElements,0),totalGroundReservation:waves.reduce((s,w)=>s+w.personnelReservation,0)});
}
const suiteSource=read('tools/Validate-QRFRelease.js');
const suites=[...suiteSource.match(/const suites=\[([\s\S]*?)\];/)[1].matchAll(/'([^']+\.js)'/g)].map(m=>m[1]);
for(const name of [...new Set([...suites,'Test-HighCommandSimulation.js','Test-PhoneSession.js','Test-GarrisonHelicopterProximity.js','Test-PlayerSquads.js','Test-PlayerSquadMutations.js'])])if(fs.existsSync(path.join(__dirname,name)))run(name);
run('Test-QRFSourceStructure.ps1');run('Test-ExecutorSettings.ps1');run('Test-TownSites.ps1');
const report={generatedAt:new Date().toISOString(),snapshot,cutoff,commits,claudeCommits:commits.filter(c=>c.claude).length,nativeVerified:false,
 scope:'Local chicken-dev; actual-source counterexamples with mocked native services, static force matrix, existing local suites. No native compilation or dedicated-server stress test.',
 observations,matrix,recipes,results,temporaryArtifacts:tmp,
 summary:{suites:results.length,passing:results.filter(r=>r.passed).length,failing:results.filter(r=>!r.passed).map(r=>r.name),counterexamples:observations.length}};
const output=path.join(root,'docs/validation/CHICKEN_DEV_REVIEW_2026-11-10.json');fs.writeFileSync(output,JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({report:output,claudeCommits:report.claudeCommits,...report.summary}));
