/* Actual editor audit orchestration with mocked native geometry/editor services.
 * No native compile, terrain or placement acceptance is implied.
 */
function runDefenceSiteAuditTests(sources) {
 let assertions=0;
 const check=(ok,message)=>{assertions++;if(!ok)throw Error(message);};
 Object.defineProperty(String.prototype,'IsEmpty',{value:function(){return this.length===0;},configurable:true});
 function body(source,name){
  const found=source.match(new RegExp('^\\t[^\\r\\n]*\\b'+name+'\\([^\\r\\n]*\\)\\r?\\n\\t\\{([\\s\\S]*?)^\\t\\}', 'm'));
  if(!found)throw Error('Missing actual method '+name);
  return found[1];
 }
 const denseSource=body(sources.layout,'DenseSiteSearchOffset')
  .replace(/\b(?:int|float) (\w+) =/g,'let $1 =')
  .replace('remaining / (ring * 2)','Math.floor(remaining / (ring * 2))');
 // Compile to a function with native integer division made explicit.
 const offset=new Function('Vector','vector','Math','return function(candidate){'+denseSource+'}')(
  (...v)=>v,{Zero:[0,0,0]},{...Math,floor:Math.floor,Mod:(a,b)=>a%b});
 const overlapSource=body(sources.layout,'FootprintsOverlap').replace(/\b(?:int|float) (\w+) =/g,'let $1 =');
 const overlap=new Function('Math','return function(center,halfExtent,yaw,otherCenter,otherHalfExtent,otherYaw,gap=8){'+overlapSource+'}')(
  {DEG2RAD:Math.PI/180,AbsFloat:Math.abs,Cos:Math.cos,Sin:Math.sin});
 let ids=Array.from({length:27},(_,i)=>i===5?'everon_outpost_andres_beacon':'everon_fixture_'+i);
 let logs=[],calls=[],failId='',missingFlag='',anchoredId='',wideSearch=false,scene='AROR_Everon_Chicken.ent';
 const sites=ids.map((id,i)=>{
  const garrison={GetDefenceHalfExtent:()=>i===0?24:18,GetDefenceHalfDepth:()=>i===0?24:18,GetMaxDefenceRelief:()=>i===0?3:2,GetEditorSearchRadius:()=>wideSearch&&i===26?1200:800,IsDefenceCenterAllowed:p=>anchoredId!==id||Math.hypot(p[0]-i*2000,p[2]-1000)<=50};
  const root={GetYawPitchRoll:()=>[0,0,0],FindComponent:()=>garrison};
  root.flag={GetOrigin:()=>[i*2000,10,1000]};
  const site={GetId:()=>id,GetOwner:()=>root,GetLocationType:()=>8};
  return {source:{GetName:()=>'AROR_Location_'+id},root,site,garrison};
 });
 const world={GetSurfaceY:()=>10};
 const api={IsEditMode:()=>true,IsPrefabEditMode:()=>false,GetWorldPath:()=>scene,
  GetEditorEntityCount:()=>ids.length,GetEditorEntity:i=>sites[i].source,
  SourceToEntity:source=>sites.find(s=>s.source===source).root,GetWorld:()=>world};
 const siteCount=sources.audit.match(/MILITARY_SITE_COUNT = (\d+);/);
 check(siteCount&&+siteCount[1]===27,'audit expects the 27 capturable Chicken sites');
 const env={MILITARY_SITE_COUNT:+siteCount[1],WorldEditor:{},Workbench:{GetModule:()=>({GetApi:()=>api})},
  AROR_LocationComponent:{Cast:x=>x},AROR_GarrisonComponent:{Cast:x=>x},
  AROR_CaptureLocationAction:{IsCapturableType:()=>true},
  AROR_DefenceLayout:{HalfExtent:()=>18,DenseSiteSearchOffset:offset,FootprintsOverlap:overlap},
  vector:{Zero:[0,0,0],DistanceXZ:(a,b)=>Math.hypot(a[0]-b[0],a[2]-b[2])},
  string:{Empty:'',Format:(message,...args)=>message.replace(/%(\d+)/g,(_,n)=>String(args[n-1]))},
  Print:message=>logs.push(message),LogLevel:{NORMAL:0,WARNING:1,ERROR:2},vadd:(a,b)=>a.map((v,i)=>v+b[i])};
 for(const item of sites)item.root.FindComponent=type=>type===env.AROR_LocationComponent?item.site:item.garrison;
 let runSource=body(sources.audit,'RunSiteAudit')
  .replace(/array<AROR_LocationComponent> (\w+) = \{\};/g,'let $1=[];')
  .replace(/array<(?:vector|float)> (\w+) = \{\};/g,'let $1=[];')
  .replace(/foreach \(AROR_LocationComponent (\w+) : (\w+)\)/g,'for(const $1 of $2)')
  .replace(/\b(?:WorldEditorAPI|WorldEditor|IEntitySource|IEntity|BaseWorld|AROR_LocationComponent|AROR_GarrisonComponent|vector|float|int|bool|string) (\w+);/g,'let $1=0;')
  .replace(/\b(?:WorldEditorAPI|WorldEditor|IEntitySource|IEntity|BaseWorld|AROR_LocationComponent|AROR_GarrisonComponent|vector|float|int|bool|string) (\w+) =/g,'let $1 =')
  .replace('let blocked, badArcs;','let blocked=0, badArcs=0;')
  .replace('int blocked, badArcs;','let blocked=0, badArcs=0;')
  .replace('api.GetWorldPath(path);','path=api.GetWorldPath();')
  .replace(/\.Count\(\)/g,'.length').replace(/\.Insert\(/g,'.push(')
  .replace(/\.StartsWith\(/g,'.startsWith(').replace(/\.Contains\(/g,'.includes(')
  .replace('center + offset','vadd(center,offset)');
 for(const name of ['FindFlag','CheckSite','OverlapsOtherSite'])runSource=runSource.replace(new RegExp('(?<![.\\w])'+name+'\\(', 'g'),'this.'+name+'(');
 runSource=runSource.replace('let clear = this.CheckSite(world, root, flag, site, proposed, half, proposedYaw, relief, blocked, badArcs, reportFailures);',
  'let measurement=this.CheckSite(world,root,flag,site,proposed,half,proposedYaw,reportFailures); relief=measurement.relief; blocked=measurement.blocked; badArcs=measurement.badArcs; let clear=measurement.clear;');
 const run=new Function(...Object.keys(env),'return function(targetId,searchAlternatives=true){'+runSource+'}')( ...Object.values(env));
 const state={FindFlag:(api,root)=>sites.find(s=>s.root===root).site.GetId()===missingFlag?null:root.flag,
  CheckSite:(world,root,flag,site,point,half,yaw,reportFailures)=>{calls.push({id:site.GetId(),point,half,yaw,reportFailures});return {clear:site.GetId()!==failId,relief:1,blocked:0,badArcs:0};},
  OverlapsOtherSite:()=>false};
 run.call(state,'');
 check(calls.length===27&&new Set(calls.map(c=>c.id)).size===27,'full audit checks every military site');
 check(logs.filter(l=>l.includes('progress=')).length===27,'full audit reports progress for all sites');
 check(calls[0].half===24&&logs.some(l=>l.includes('limit=3')),'audit uses configured footprint and terrain allowance');
 check(logs.some(l=>l.includes('begin reviewed=27 grid=25 radius=per-site'))&&logs.some(l=>l.includes('searchRadius=800')),'full dense scan declares its per-site bounds and run boundary');
 logs=[];calls=[];run.call(state,'everon_outpost_andres_beacon');
 check(calls.length===1&&calls[0].id==='everon_outpost_andres_beacon','target command still restricts itself to Andres');
 check(logs.some(l=>l.includes('reviewed=1.')),'target summary remains separately importable');
 logs=[];calls=[];
 const originalMeasure=state.CheckSite,originalSecondOrigin=sites[1].root.flag.GetOrigin;
 sites[1].root.flag.GetOrigin=()=>[1000,10,1000];
 state.CheckSite=(...args)=>{
  const result=originalMeasure(...args),id=args[3].GetId(),p=args[4];
  if(id===ids[0])result.clear=p[0]===600&&p[2]===1000;
  if(id===ids[1])result.clear=(p[0]===625||p[0]===475)&&p[2]===1000;
  return result;
 };
 run.call(state,'');
 check(logs.some(l=>l.includes('id='+ids[0]+' proposed=600,10,1000')),'first unresolved site reserves its measured proposed footprint');
 check(logs.some(l=>l.includes('id='+ids[1]+' proposed=475,10,1000')),'later site skips a conflicting first choice and selects a separated candidate');
 check(!logs.some(l=>l.includes('id='+ids[1]+' proposed=625,10,1000')),'audit cannot emit overlapping independent resource proposals');
 state.CheckSite=originalMeasure;sites[1].root.flag.GetOrigin=originalSecondOrigin;
 logs=[];calls=[];failId=ids[26];run.call(state,'');
 const failedCalls=calls.filter(c=>c.id===failId),centre=sites[26].root.flag.GetOrigin();
 check(failedCalls.length>25000,'unresolved sites receive the dense grid, not the former 129-point search');
 check(failedCalls.every(c=>Math.hypot(c.point[0]-centre[0],c.point[2]-centre[2])<=800),'every sampled orientation remains within 800m');
 check(failedCalls.some(c=>c.point[0]-centre[0]===50&&c.point[2]-centre[2]===25),'full audit samples gaps between the former eight rays');
 check(logs.some(l=>l.includes('current suitable=26 suggested relocations=0 reviewed=27.')),'failed site does not become an accepted placement');
 logs=[];calls=[];run.call(state,'',false);
 check(calls.length===27&&calls.every(c=>c.reportFailures),'current-placement command checks each actual pose exactly once with diagnostics');
 check(logs.some(l=>l.includes('mode=current-only'))&&logs.some(l=>l.includes('no relocation search requested')),'current-only audit identifies its mode and unresolved footprint');
 check(!logs.some(l=>l.includes(' proposed='))&&logs.some(l=>l.includes('current suitable=26 suggested relocations=0 reviewed=27.')),'current-only failures never become proposed or accepted sites');
 logs=[];calls=[];wideSearch=true;run.call(state,'');
 const wideCalls=calls.filter(c=>c.id===failId);
 check(wideCalls.some(c=>Math.hypot(c.point[0]-centre[0],c.point[2]-centre[2])>800),'configured resource search samples beyond the exhausted 800m boundary');
 check(wideCalls.every(c=>Math.hypot(c.point[0]-centre[0],c.point[2]-centre[2])<=1200),'expanded search remains bounded to 1200m');
 check(logs.some(l=>l.includes('id='+failId+' searchRadius=1200')),'expanded bounds appear in native audit evidence');wideSearch=false;
 logs=[];calls=[];anchoredId=failId;run.call(state,'');
 const anchoredCalls=calls.filter(c=>c.id===failId);
 check(anchoredCalls.length>0&&anchoredCalls.length<1000,'fixed-anchor sites do not sample distant clearing candidates');
 check(anchoredCalls.every(c=>Math.hypot(c.point[0]-centre[0],c.point[2]-centre[2])<=50),'canonical anchored capture bound applies before native geometry checks');anchoredId='';
 logs=[];calls=[];failId='';missingFlag=ids[5];run.call(state,'');
 check(logs.some(l=>l.includes('missing attached capture flag'))&&logs.some(l=>l.includes('reviewed=26.')),'missing flag cannot yield a complete 27-site result');
 logs=[];calls=[];missingFlag='';ids=ids.slice(0,26);run.call(state,'');
 check(!calls.length&&logs.some(l=>l.includes('Expected 27 military sites, found 26')),'incomplete scene refuses audit');
 ids.push('last');logs=[];calls=[];scene='Rowan.ent';run.call(state,'');
 check(!calls.length,'other scenes are never audited by Chicken command');

 // Audit the actual role-pad loop, including designer overrides before a roster exists.
 const types={OUTPOST:8,AIRPORT:2,MILITARY_BASE:5};
 const maths={Clamp:(x,a,b)=>Math.min(b,Math.max(a,x)),Ceil:Math.ceil,AbsFloat:Math.abs};
 const adapt=code=>code
  .replace(/array<IEntity> (\w+) = \{\};/g,'let $1=[];')
  .replace('array<IEntity> flagOnly = {flag};','let flagOnly=[flag];')
  .replace(/\b(?:AROR_GarrisonComponent|IEntity|vector|int|float|bool) (\w+);/g,'let $1=0;')
  .replace(/\b(?:AROR_GarrisonComponent|IEntity|vector|int|float|bool) (\w+) =/g,'let $1 =');
 const levelFn=new Function('AROR_GarrisonComponent','AROR_ELocationType','Math','return function(location){'+adapt(body(sources.audit,'AuditDefenceLevel'))+'}')(env.AROR_GarrisonComponent,types,maths);
 const policy=sources.policy;
 if(!policy)throw Error('Supply actual garrison policy source for role-pad coverage');
 const nativePolicy={};
 for(const name of ['InfantryTeams','StaticCount','MannedCount'])nativePolicy[name]=new Function('Math','return function(level){'+adapt(body(policy,name))+'}')(maths);
 const geometry={TerrainFits:()=>true,LocalPosition:(kind,ordinal)=>[kind,0,ordinal],Facing:()=>0,ClearMGArc:()=>true};
 let checkSiteBody=adapt(body(sources.audit,'CheckSite'));
 for(const name of ['AuditDefenceLevel','CollectChildren','WorldOffset','AuthoredPosition','ClearBox'])checkSiteBody=checkSiteBody.replace(new RegExp('(?<![.\\w])'+name+'\\(', 'g'),'this.'+name+'(');
 const checkSite=new Function('AROR_GarrisonComponent','AROR_ELocationType','AROR_GarrisonPolicy','AROR_DefenceLayout','Math','Vector','Print','string','LogLevel','return function(world,root,flag,location,center,half,yaw,relief,blocked,badArcs,reportFailures=false){'+checkSiteBody+'}')(env.AROR_GarrisonComponent,types,nativePolicy,geometry,maths,(...v)=>v,env.Print,env.string,env.LogLevel);
 for(const fixture of [
  {type:8,override:0,teams:2,statics:1,hulls:0},
  {type:8,override:2,teams:4,statics:2,hulls:0},
  {type:8,override:3,teams:6,statics:3,hulls:0},
  {type:5,override:3,teams:6,statics:3,hulls:2},
  {type:5,override:3,staticOverride:4,teams:6,statics:4,hulls:2},
  {type:5,override:3,staticOverride:4,omitParked:true,teams:6,statics:4,hulls:1},
  {type:2,override:2,teams:8,statics:4,hulls:4},
  {type:2,override:4,extraTeams:4,extraParked:3,staticOverride:6,teams:12,statics:6,hulls:7}
 ]){
  const garrison={GetDefenceHalfExtent:()=>32,GetDefenceHalfDepth:()=>32,GetExtraInfantryTeams:()=>fixture.extraTeams||0,GetExtraParkedCount:()=>fixture.extraParked||0,GetDefenceLevelOverride:()=>fixture.override,GetMaxDefenceRelief:()=>2,GetStaticWeaponCount:level=>fixture.staticOverride||nativePolicy.StaticCount(level),AllowsParkedArmour:()=>!fixture.omitParked};
  const root={FindComponent:()=>garrison},flag={};
  const location={GetLocationType:()=>fixture.type,GetOwner:()=>root,GetId:()=>'fixture'};
  const pads=[];
  const context={AuditDefenceLevel:levelFn,CollectChildren:()=>{},WorldOffset:(center,local)=>local,AuthoredPosition:()=>null,
   ClearBox:(world,point,size,excludes)=>{if(excludes[0]===flag)pads.push(point[0]);return true;}};
  check(checkSite.call(context,world,root,flag,location,[0,0,0],32,0,0,0,0),'clear authored recipe passes mocked geometry');
  check(pads.filter(k=>k===0).length===fixture.teams,'all infantry pads are audited for recipe '+fixture.type+'/'+fixture.override);
  check(pads.filter(k=>k===1).length===fixture.statics,'all static pads are audited for recipe '+fixture.type+'/'+fixture.override);
  check(pads.filter(k=>k===2||k===3).length===fixture.hulls,'audit does not invent outpost vehicles and retains military hull coverage');
  if(fixture.omitParked){
   logs=[];context.ClearBox=()=>false;geometry.ClearMGArc=()=>false;
   check(!checkSite.call(context,world,root,flag,location,[0,0,0],32,0,0,0,0,true),'blocked actual placement fails');
   check(logs.filter(l=>l.includes('wallSide=')).length===8,'scene-wall diagnostics are bounded');
   check(logs.some(l=>l.includes('kind=0 ordinal=0')&&l.includes('blocked spawn pad'))&&logs.filter(l=>l.includes('blocked gun arc')).length===4,'diagnostics identify infantry pads and each obstructed gun');
   logs=[];checkSite.call(context,world,root,flag,location,[0,0,0],32,0,0,0,0,false);
   check(logs.length===0,'relocation searches do not flood the log with candidate diagnostics');geometry.ClearMGArc=()=>true;
  }
 }
 return {assertions,note:'Editor orchestration and geometry services mocked; native compile and dense scan still required.'};
}
if(typeof require==='function'&&typeof module!=='undefined'&&require.main===module){
 const fs=require('fs'),path=require('path'),root=path.join(__dirname,'../AROpenResistance/Open Resistance/Scripts');
 console.log(runDefenceSiteAuditTests({audit:fs.readFileSync(path.join(root,'WorkbenchGame/Plugins/AROR_AuditDefenceSitesPlugin.c'),'utf8'),layout:fs.readFileSync(path.join(root,'Game/Garrisons/AROR_DefenceLayout.c'),'utf8'),policy:fs.readFileSync(path.join(root,'Game/Garrisons/AROR_GarrisonPolicy.c'),'utf8')}));
}
