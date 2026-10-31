// Actual Enforce method bodies with mocked native services; this is not a script compiler.
// Run: node tools/Test-MissionLifecycle.js
function runMissionLifecycleTests(sources) {
  let assertions = 0;
  const check = (ok, message) => { assertions++; if (!ok) throw Error(message); };
  const array = values => Object.assign(values || [], {
    Count() { return this.length; }, IsEmpty() { return !this.length; },
    Contains(x) { return this.includes(x); }, Insert(x) { this.push(x); },
    Clear() { this.length = 0; }, RemoveItem(x) { const i=this.indexOf(x); if(i>=0)this.splice(i,1); }
  });
  const result = {ONGOING:0, SUCCEEDED:1, FAILED:2};
  const deleted = [], replies = [];
  let server = true;
  const env = {
    AROR_EMissionResult:result, AROR_EMissionType:{CONVOY_AMBUSH:1},
    AROR_MissionBoardComponent:{IsDestroyed:x=>!x||x.destroyed,
      CountLiving:(g,out)=>{out.push(...(g?.members || []).filter(x=>!x.dead));return out.length;}},
    AROR_HighCommandComponent:{GetInstance:()=>({IsHost:id=>id===1})},
    Replication:{IsServer:()=>server},
    GetGame:()=>({GetPlayerManager:()=>({GetPlayerName:id=>'player'+id,GetPlayers:out=>out.push(1,2,3)}),
      GetWorld:()=>({GetWorldTime:()=>123})}),
    SCR_EntityHelper:{DeleteEntityAndChildren:x=>deleted.push(x)},
    vector:{DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
    string:{Format:(s,...args)=>s.replace(/%(\d+)/g,(_,i)=>args[i-1])},
    Math:{Max:Math.max,Round:Math.round}, Print:()=>{},LogLevel:{NORMAL:0,WARNING:1},
    ARRIVAL_RADIUS:120,MARKER_INTERVAL:30,PRISONERS:3,array
  };
  function compile(source, name) {
    const match=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\(([^\\r\\n]*)\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
    if(!match)throw Error('Missing '+name);
    const args=match[1].replace(/\b\w+\s+(\w+)(?=,|$)/g,'$1');
    let body=match[2]
      .replace(/foreach \(\w+ (\w+), \w+ (\w+) : ([^)]+)\)/g,'for (const [$1, $2] of $3.entries())')
      .replace(/foreach \(\w+ (\w+) : ([^)]+)\)/g,'for (const $1 of $2)')
      .replace(/array<\w+> (\w+) = \{\};/g,'let $1 = array();')
      .replace(/\b(?:int|bool|float|vector|string|IEntity|AIAgent|SCR_AIGroup|AROR_LocationComponent|AROR_HighCommandComponent|AROR_Mission)\s+(\w+)/g,'let $1')
      .replace(/let (\w+);/g,'let $1 = 0;')
      .replace(/(?<![.\w])m_\w+/g,x=>'this.'+x)
      .replace(/(?<![.\w])(?:UpdateGroups_S|GetLocationName|GetDestinationName|TrackMarker|CanBeFreed|Free_S|FindActive|IsLeader|Reply|Finish_S|Clean|HasPlayerAboard|GetLocation|PublishSnapshot_S)\(/g,x=>'this.'+x);
    return new Function(...Object.keys(env),'return function('+args+'){'+body+'}')( ...Object.values(env));
  }
  const update=compile(sources.convoy,'Update_S');
  function convoy() {
    const vehicle={GetOrigin:()=>[0,0,0]}, group={members:[],GetAgents(out){out.push(...this.members.map(x=>({GetControlledEntity:()=>x})));}};
    let seated=false, attempts=0, waypoints=0;
    const board={GetUpdateInterval:()=>5, SeatGroup_S:()=>{attempts++;return seated;},FindRoadPosition_S:()=>false,
      AddWaypoint_S:()=>waypoints++,AddLog:()=>{}};
    const mission={m_eEnemyRole:1,m_iPhase:0,m_aVehicles:array([vehicle]),m_aCrews:array([group]),m_aCrewSizes:array([2]),
      m_aObservedCrew:array(),m_aSpawnedCrewCounts:array([0]),m_vDestination:[2000,0,0],m_iMarkerSeconds:0,
      UpdateGroups_S:()=>{},GetLocationName:()=>'',GetDestinationName:()=>'',TrackMarker:()=>{}};
    return {mission,board,vehicle,group,run:()=>update.call(mission,board),seat:()=>seated=true,counts:()=>({attempts,waypoints})};
  }
  let f=convoy();
  for(let i=0;i<30;i++)check(f.run()===result.ONGOING,'dormant crew cannot yield success after 120 seconds');
  check(f.mission.m_iPhase===0&&f.counts().waypoints===0,'dormant convoy never dispatched');
  f.group.members.push({dead:false});check(f.run()===result.ONGOING,'partially spawned crew waits');
  f.group.members.push({dead:false});
  for(let i=0;i<30;i++)check(f.run()===result.ONGOING,'dropped seating requests keep retrying');
  check(f.counts().attempts>30&&f.mission.m_iPhase===0,'no boarding timeout dispatch');
  f.seat();check(f.run()===result.ONGOING&&f.mission.m_iPhase===1,'confirmed seating dispatches');
  f.group.members.forEach(x=>x.dead=true);check(f.run()===result.SUCCEEDED,'observed crew casualties count as destruction');
  f=convoy();f.vehicle.destroyed=true;check(f.run()===result.SUCCEEDED,'wrecked hull needs no crew to spawn');
  f=convoy();f.group.members.push({dead:false},{dead:true});f.seat();
  check(f.run()===result.ONGOING&&f.mission.m_iPhase===1,'boarding tolerates observed casualties');
  f.vehicle.GetOrigin=()=>[2000,0,0];check(f.run()===result.FAILED,'arrival still fails convoy mission');

  const rescue=compile(sources.prisoner,'Update_S');
  const prisoners=[0,1,2].map(i=>({dead:false,GetOrigin:()=>[i===0?0:200,0,0]}));
  const group={members:prisoners,GetAgents(out){out.push(...this.members.map(x=>({GetControlledEntity:()=>x})));}};
  const mission={m_Prisoners:group,m_aObservedPrisoners:array(),m_iPrisonersSeen:0,m_iPhase:1,m_vDestination:[0,0,0],
    UpdateGroups_S:()=>{},TrackMarker:()=>{}};
  const board={GetLocation:()=>null};
  check(rescue.call(mission,board)===result.ONGOING&&mission.m_iRescued===1,'first arrival does not pay for stragglers');
  prisoners[1].GetOrigin=()=>[0,0,0];prisoners[2].dead=true;
  check(rescue.call(mission,board)===result.SUCCEEDED&&mission.m_iRescued===2,'only arrived survivors rewarded');
  prisoners.forEach(x=>x.dead=true);
  check(rescue.call(mission,board)===result.FAILED,'all prisoners killed fails');
  group.members=[];mission.m_aObservedPrisoners=array();
  check(rescue.call(mission,board)===result.ONGOING,'unspawned prisoners cannot succeed');

  const abort=compile(sources.board,'RequestAbort_S');
  let active={m_iLeaderId:0,m_sCodename:'TEST'},finished=0;
  const owner={FindActive:()=>active,IsLeader:id=>id!==3,Reply:(...x)=>replies.push(x),
    Finish_S:()=>finished++,Clean:x=>x};
  abort.call(owner,2,7);check(finished===1,'current leader can abort restored mission');
  abort.call(owner,3,7);check(finished===1,'nonleader cannot abort restored mission');
  active.m_iLeaderId=1;abort.call(owner,2,7);check(finished===1,'another leader cannot abort owned mission');
  abort.call(owner,1,7);check(finished===2,'host can abort');
  server=false;abort.call(owner,1,7);check(finished===2,'client cannot abort');server=true;
  const hint=compile(sources.board,'ReplyLeader');replies.length=0;
  hint.call(owner,{m_iLeaderId:0,m_sCodename:'TEST'},'progress');
  check(replies.length===2&&replies.every(x=>x[0]>0&&x[0]!==3),'restored progress reaches current leaders');

  const deploy=compile(sources.board,'TryDeploy_S');
  const member={},deadMember={},spawnedGroup={GetAgents:out=>out.push(...[member,deadMember].map(x=>({GetControlledEntity:()=>x})))};
  const truck={},occupied={};
  const partial={Deploy_S:()=>false,m_aGroups:array([spawnedGroup]),m_aGroupRoles:array([1]),
    m_aAffiliated:array([member]),m_aEntities:array([truck,occupied]),m_iDeployAttempts:0};
  deploy.call({HasPlayerAboard:x=>x===occupied},partial);
  check(deleted.includes(spawnedGroup)&&deleted.includes(member)&&deleted.includes(deadMember)&&deleted.includes(truck),'partial spawn removed immediately including dead members');
  check(!deleted.includes(occupied),'occupied vehicle preserved');
  check(partial.m_aGroups.length===0&&partial.m_aEntities.length===0&&partial.m_iDeployAttempts===1,'retry clears tracking and counts failure');
  check(sources.cache.includes('m_aEntities.RemoveItem(m_Crate)'),'successful cache excluded from cleanup');
  check(sources.board.includes('!mission.m_bRestored && mission.DeploysAtOnce()'),'loaded convoy obeys proximity');
  check(sources.board.includes('!mission.m_bDeployed && !mission.IsStillValid_S(this)'),'undeployed target revalidated');
  return {assertions,note:'Native spawning, seating and replication mocked; Workbench compile and Play required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
  const fs=require('fs'),path=require('path'),dir=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts/Game/Missions');
  const sources={};
  for(const [key,name] of Object.entries({convoy:'ConvoyAmbushMission',prisoner:'PrisonerRescueMission',board:'MissionBoardComponent',cache:'CacheRecoveryMission'}))
    sources[key]=fs.readFileSync(path.join(dir,'AROR_'+name+'.c'),'utf8');
  console.log(JSON.stringify(runMissionLifecycleTests(sources)));
}
